#pragma once

#include "foundation.h"
#include "memutils.h"
#include "strings.h"
#include "error.h"

typedef u32 Client_Id;

#define INVALID_CLIENT_ID ((Client_Id) -1)
#define DEFAULT_VIRTUAL_CONNECTION_MAGIC 75

#define PACKET_SIZE        1028 // The total size of a packet (one unit being sent over the network). Should be small enough to avoid fragmentation during routing but big enough to hold the biggest message.
#define PACKET_HEADER_SIZE 20 // The serialized (packed) size of the Packet_Header struct.
#define PACKET_BODY_SIZE   (PACKET_SIZE - PACKET_HEADER_SIZE)

#define VIRTUAL_CONNECTION_BUFFER_SIZE PACKET_SIZE
#define VIRTUAL_CONNECTION_NON_BLOCKING true // Calls to the underlying socket APIs can be blocking or non-blocking. We use non-blocking by default here to use it as an immediate mode API (where we check if some data has arrived, instead of waiting until some data arrives)

typedef u64 Socket; // SOCKET on win32 is a UINT_PTR.
typedef u8 Remote_Socket[16]; // sockaddr_in on win32 is a struct of 16 bytes.

enum Socket_Result {
    SOCKET_Error    = 0,
    SOCKET_New_Data = 1,
    SOCKET_No_Data  = 2,
};

enum Connection_Status {
    CONNECTION_Closed     = 0,
    CONNECTION_Connecting = 1,
    CONNECTION_Good       = 2,
};

enum Connection_Type {
    CONNECTION_Unknown = 0,
    CONNECTION_TCP     = 1,
    CONNECTION_UDP     = 2,
};

enum Packet_Type {
    PACKET_Unknown                = 0x0,
    PACKET_Connection_Request     = 0x1,
    PACKET_Connection_Established = 0x2,
    PACKET_Connection_Closed      = 0x3,
    PACKET_Ping                   = 0x4,
    PACKET_Message                = 0x5,
};

struct Packet_Header {
    u16 packet_size; // The total size in bytes of this packet, including the header. Used in TCP streams to read the complete packet before parsing it.
    u8 magic; // If the magic number of an incoming packet does not match the magic of a virtual connection, that packet is silently ignored.
    u32 client_id; // The client that sent this packet, only valid after the handshake is complete. Used to map packets to clients in the server.
    u32 sequence_id; // The sequential id for this packet by the sender.
    u32 remote_ack_id; // The highest packet id that arrived from the receiver on the sender.
    u32 remote_ack_field; // The bitfield for the last 32 packet ids (starting from remote_ack_id), 1 if the packet has been received, 0 if it hasn't.
    u8 packet_type; // One of Packet_Type, used for filtering system packets from user-defined messages.
};

struct Packet {
    Packet_Header header;
    u8 body[PACKET_BODY_SIZE];
    s64 body_size = 0; // Used for creating and parsing the body.
};

struct Virtual_Connection_Info {
    u8 magic              = DEFAULT_VIRTUAL_CONNECTION_MAGIC; // Remote packets with a different magic will be silently ignored.
    u32 client_id         = INVALID_CLIENT_ID; // The client id this connection was assigned to after the handshake.
    u32 remote_ack_id     = (u32) -1; // The sequence id of the latest remote packet that has reached local.
    u32 remote_ack_field  = 0; // A bit field representing the latest 32 packets (starting from remote_ack_id), 1 means it arrived at remote, 0 means it hasn't (yet).
    u32 local_sequence_id = 0; // The sequential id of the next packet to be created locally.
    u32 local_ack_id      = (u32) -1; // The sequence id of the latest local packet that has been acked to local by remote.
    u32 local_ack_field   = 0; // A bit field representing the latest 32 packets (starting from local_ack_id), 1 means it has been acked by remote, 0 means it hasn't (yet).
};

struct Virtual_Connection {
    Connection_Type type;
    Connection_Status status;
    Virtual_Connection_Info info;

    Packet incoming_packet; // Once a complete packet has been read by the underlying protocol, the data in th ebuffer gets extracted into this packet, and read_packet returns true. At that point, the caller can safely access this packet until the next call to read_packet().

    //
    // For tcp and udp sockets, data received via the socket gets stored in this buffer.
    // The udp protocol ensures that a sent datagram gets received at once at whole, so this buffer is only used
    // inside read_packet.
    // The tcp protocol however is a stream, therefore we might not receive a whole packet inside a single read
    // call. This buffer therefore keeps the received part of a packet stored until we have read all of it.
    // It might also happen that we read more than one packet in a single read call (if the packets are smaller
    // than the available buffer size), so this buffer might be emptied in subsequent calls to read_packet.
    //
    Socket socket;
    u8 incoming_buffer[VIRTUAL_CONNECTION_BUFFER_SIZE];
    s64 incoming_buffer_size = 0; // The number of valid bytes in the incoming_buffer.
    s64 incoming_buffer_offset = 0; // Read offset into the incoming_buffer in bytes.
    u8 outgoing_buffer[VIRTUAL_CONNECTION_BUFFER_SIZE]; // This buffer is used for serializing packets inside send_packet, to avoid having to stack allocate it there every time.
    s64 outgoing_buffer_size = 0;
    
    //
    // The following members are only used for udp sockets, since tcp has it's own concept of connections that
    // we don't manually need to handle. Tcp also ensures that we do not lose packets, so we only need to
    // remember reliable packets for udp, until we either send them again if they have been dropped, or we
    // have received an ack for them.
    //
    Remote_Socket remote;
    Linked_List<Packet> unacked_reliable_packets;
};



/* ------------------------------------------- Connection Handling ------------------------------------------- */

Error_Code create_client_connection(Virtual_Connection *connection, Connection_Type type, string host, u16 port);
Error_Code create_server_connection(Virtual_Connection *connection, Connection_Type type, u16 port);
void destroy_connection(Virtual_Connection *connection);

Virtual_Connection create_remote_client_connection(Virtual_Connection *server); // Creates a virtual connection object around the current remote socket of a UDP server, to "fake" an actual connection which doesn't exist in the UDP protocol.
b8 accept_remote_client_connection(Virtual_Connection *server, Virtual_Connection *client); // Tries to accept an incoming TCP client connection.

void send_packet(Virtual_Connection *connection, Packet *packet);
void send_reliable_packet(Virtual_Connection *connection, Packet *packet);
b8 read_packet(Virtual_Connection *connection);



/* --------------------------------------------- Packet Handling --------------------------------------------- */

//
// The system packets have a 'spam_count' parameter, which essentially just means 'send this packet x times'.
// This is done to fight potential packet loss during the handshake (or destruction), since these packets may
// be lost, but without a connection we cannot do the proper UDP reliable packets handling.
// Therefore we just send the packet multiple times and hope that at least one does eventually arrive, at which
// point the other ones will just be ignored by the receiver.
//

void send_connection_request_packet(Virtual_Connection *connection, s64 spam_count = 1);
void send_connection_established_packet(Virtual_Connection *connection, s64 spam_count = 1);
void send_connection_closed_packet(Virtual_Connection *connection, s64 spam_count = 1);
void send_ping_packet(Virtual_Connection *connection);
b8 wait_until_connection_established(Virtual_Connection *connection, f32 timeout_in_seconds = 0.f);



/* ------------------------------------------ Lower Level Utilities ------------------------------------------ */

b8 remote_sockets_equal(Remote_Socket lhs, Remote_Socket rhs);
