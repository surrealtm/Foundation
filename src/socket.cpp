#include "socket.h"
#include "strings.h"


/* ------------------------------------------- Win32 Implementation ------------------------------------------- */

#if FOUNDATION_WIN32
# include <winsock2.h>
# include <ws2tcpip.h>

typedef sockaddr_in Win32_Remote_Socket;

static_assert(sizeof(Remote_Socket) >= sizeof(Win32_Remote_Socket), "Remote_Socket was smaller than expected.");

static s64 wsa_references = 0;

static
string wsa_error_string(s32 error_code) {
    string result;

#define error(value) case value: result = strltr(#value); break;
    
    switch(error_code) {
        error(WSAEINTR);
        error(WSAEBADF);
        error(WSAEACCES);
        error(WSAEFAULT);
        error(WSAEINVAL);
        error(WSAEMFILE);
        error(WSAEWOULDBLOCK);
        error(WSAEINPROGRESS);
        error(WSAEALREADY);
        error(WSAENOTSOCK);
        error(WSAEDESTADDRREQ);
        error(WSAEMSGSIZE);
        error(WSAEPROTOTYPE);
        error(WSAENOPROTOOPT);
        error(WSAEPROTONOSUPPORT);
        error(WSAESOCKTNOSUPPORT);
        error(WSAEOPNOTSUPP);
        error(WSAEAFNOSUPPORT);
        error(WSAEADDRINUSE);
        error(WSAEADDRNOTAVAIL);
        error(WSAENETDOWN);
        error(WSAENETUNREACH);
        error(WSAENETRESET);
        error(WSAECONNABORTED);
        error(WSAECONNRESET);
        error(WSAENOBUFS);
        error(WSAEISCONN);
        error(WSAENOTCONN);
        error(WSAESHUTDOWN);
        error(WSAETOOMANYREFS);
        error(WSAETIMEDOUT);
        error(WSAECONNREFUSED);
        error(WSAELOOP);
        error(WSAENAMETOOLONG);
        error(WSAEHOSTDOWN);
        error(WSAEHOSTUNREACH);
        error(WSAENOTEMPTY);
        error(WSAEPROCLIM);
        error(WSAEUSERS);
        error(WSAEDQUOT);
        error(WSAESTALE);
        error(WSAEREMOTE);
        error(WSASYSNOTREADY);
        error(WSAVERNOTSUPPORTED);
        error(WSANOTINITIALISED);
        error(WSAEDISCON);
        error(WSAENOMORE);
        error(WSAECANCELLED);
        error(WSAEINVALIDPROCTABLE);
        error(WSAEINVALIDPROVIDER);
        error(WSAEPROVIDERFAILEDINIT);
        error(WSASYSCALLFAILURE);
        error(WSASERVICE_NOT_FOUND);
        error(WSATYPE_NOT_FOUND);
        error(WSA_E_NO_MORE);
        error(WSA_E_CANCELLED);
        error(WSAEREFUSED);
        error(WSAHOST_NOT_FOUND);
        error(WSATRY_AGAIN);
        error(WSANO_RECOVERY);
        error(WSANO_DATA);
    default: result = "<UnknownWsaError>"_s; break;
    }

#undef error

    return result;
}

static
string last_wsa_error_string() {
    return wsa_error_string(WSAGetLastError());
}

static
s32 win32_socket_type_for_connection_type(Connection_Type type) {
    s32 type_id;

    switch(type) {
    case CONNECTION_TCP: type_id = SOCK_STREAM; break;
    case CONNECTION_UDP: type_id = SOCK_DGRAM; break;
    }

    return type_id;
}

static
s32 win32_protocol_type_for_connection_type(Connection_Type type) {
    s32 type_id;

    switch(type) {
    case CONNECTION_TCP: type_id = IPPROTO_TCP; break;
    case CONNECTION_UDP: type_id = IPPROTO_UDP; break;
    }
    
    return type_id;
}

static
void win32_copy_remote(Win32_Remote_Socket *dst, Win32_Remote_Socket *src) {
    *dst = *src;
}

static
b8 win32_initialize() {
    if(wsa_references > 0) return true;

    WORD version = MAKEWORD(2, 2);
    WSADATA wsadata;

    wsa_references = WSAStartup(version, &wsadata) == 0;

    return wsa_references > 0;
}

static
void win32_cleanup() {
    --wsa_references;
    if(wsa_references == 0) WSACleanup();
}

static
b8 win32_remote_sockets_equal(Win32_Remote_Socket *lhs, Win32_Remote_Socket *rhs) {
    return lhs->sin_family == rhs->sin_family && lhs->sin_port == rhs->sin_port && lhs->sin_addr.S_un.S_addr == rhs->sin_addr.S_un.S_addr;
}

static
Socket win32_create_socket(Connection_Type type) {
    Socket id = socket(AF_INET, win32_socket_type_for_connection_type(type), 0);
    return id; // id may be INVALID_SOCKET!
}

static
b8 win32_set_socket_to_non_blocking(Socket socket) {
    if(socket == INVALID_SOCKET) return false;

    u_long nonblocking = true;
    return ioctlsocket(socket, FIONBIO, &nonblocking) == 0;
}

static
void win32_destroy_socket(Socket *socket) {
    closesocket(*socket);
    *socket = INVALID_SOCKET;
}

static
Socket win32_create_server_socket(Connection_Type type, u16 port) {
    if(!win32_initialize()) return INVALID_SOCKET;

    int result;
    Socket socket = win32_create_socket(type);
    if(socket == INVALID_SOCKET) return INVALID_SOCKET;
    
    sockaddr_in address;
    address.sin_family = AF_INET; // IPv4
    address.sin_port   = htons(port);
    address.sin_addr.S_un.S_addr = INADDR_ANY;

    result = bind(socket, (sockaddr *) &address, sizeof(sockaddr_in));
    if(result == SOCKET_ERROR) {
        win32_destroy_socket(&socket);
        return INVALID_SOCKET;
    }

    if(type == CONNECTION_TCP) {
        result = listen(socket, SOMAXCONN);
        if(result == SOCKET_ERROR) {
            win32_destroy_socket(&socket);
            return INVALID_SOCKET;
        }
    }

#if VIRTUAL_CONNECTION_NON_BLOCKING
    if(!win32_set_socket_to_non_blocking(socket)) {
        win32_destroy_socket(&socket);
        return INVALID_SOCKET;
    }
#endif

    return socket;
}

static
Socket win32_create_client_socket(Connection_Type type, string host, u16 port, Win32_Remote_Socket *out_remote) {
    if(!win32_initialize()) return INVALID_SOCKET;

    int result;
    Socket socket = win32_create_socket(type);
    if(socket == INVALID_SOCKET) return INVALID_SOCKET;
    
    addrinfo hints;
    hints.ai_family = AF_INET; // IPv4
    hints.ai_socktype = win32_socket_type_for_connection_type(type);
    hints.ai_protocol = win32_protocol_type_for_connection_type(type);

    char port_string[8];
    sprintf_s(port_string, sizeof(port_string), "%d", port);

    char *host_string = to_cstring(Default_Allocator, host);
    defer { free_cstring(Default_Allocator, host_string); };

    Win32_Remote_Socket remote;
    addrinfo *host_address;

    result = getaddrinfo(host_string, port_string, &hints, &host_address);
    defer { freeaddrinfo(host_address); };

    if(result != 0) {
        win32_destroy_socket(&socket);
        return INVALID_SOCKET;
    }

    if(type == CONNECTION_UDP) {
        remote = *((sockaddr_in *) host_address->ai_addr);
    }

    result = connect(socket, host_address->ai_addr, (int) host_address->ai_addrlen);
    if(result != 0) {
        win32_destroy_socket(&socket);
        return INVALID_SOCKET;
    }

#if VIRTUAL_CONNECTION_NON_BLOCKING
    if(!win32_set_socket_to_non_blocking(socket)) {
        win32_destroy_socket(&socket);
        return INVALID_SOCKET;
    }
#endif

    *out_remote = remote;
    return socket;
}

static
Socket_Result win32_accept_incoming_client_socket(Socket server_socket, Socket *client_socket) {
    if(server_socket == INVALID_SOCKET) return SOCKET_Error;

    sockaddr client_address;
    int client_address_size = sizeof(sockaddr);

    *client_socket = accept(server_socket, &client_address, &client_address_size);
    Socket_Result result = SOCKET_No_Data;

    if(*client_socket == INVALID_SOCKET) {
        // If the server socket is set to non-blocking, it will return with error code WSAEWOULDBLOCK if no
        // incoming client connection is available. In that case, we just want to continue on normally.
        int error = WSAGetLastError();
        if(error != WSAEWOULDBLOCK) {
            result = SOCKET_Error;
        }
    } else {
        result = SOCKET_New_Data;
        
#if VIRTUAL_CONNECTION_NON_BLOCKING
        if(!win32_set_socket_to_non_blocking(*client_socket)) {
            win32_destroy_socket(client_socket);
            result = SOCKET_Error;
        }
#endif
    }
    
    return result;
}

static
Socket_Result win32_receive_socket_data_udp(Socket socket, u8 *buffer, s64 buffer_size, s64 *read, Win32_Remote_Socket *remote) {
    int remote_size = sizeof(Win32_Remote_Socket);

    *read = (s64) recvfrom(socket, (char *) buffer, (int) buffer_size, 0, (sockaddr *) remote, &remote_size);
    Socket_Result result = SOCKET_No_Data;

    if(*read < 0) {
        int error = WSAGetLastError();

        // If the socket is set to non-blocking, it will return with error code WSAEWOULDBLOCK if no
        // incoming data is available. In that case, we just want to continue on normally.
        if(error != WSAEWOULDBLOCK) {
            result = SOCKET_Error;
        }
    } else if(*read == 0) {
        // If recv returned 0, it means the remote closed the connection gracefully, which means we should
        // also shut down.
        result = SOCKET_Error;
    } else if(*read > 0) {
        result = SOCKET_New_Data;
    }

    return result;
}

static
Socket_Result win32_receive_socket_data_tcp(Socket socket, u8 *buffer, s64 buffer_size, s64 *read) {
    *read = recv(socket, (char *) buffer, (int) buffer_size, 0);
    Socket_Result result = SOCKET_No_Data;

    if(*read < 0) {
        int error = WSAGetLastError();

        // If the socket is set to non-blocking, it will return with error code WSAEWOULDBLOCK if no
        // incoming data is available. In that case, we just want to continue on normally.
        if(error != WSAEWOULDBLOCK) {
            result = SOCKET_Error;
        }        
    } else if(*read == 0) {
        // If recv returned 0, it means the remote closed the connection gracefully, which means we should
        // also shut down.
        result = SOCKET_Error;
    } else if(*read > 0) {
        result = SOCKET_New_Data;
    }

    return result;
}

static
b8 win32_send_socket_data_udp(Socket socket, Win32_Remote_Socket *remote, u8 *buffer, s64 buffer_size) {
    int sent = sendto(socket, (char *) buffer, (int) buffer_size, 0, (sockaddr *) remote, sizeof(Win32_Remote_Socket));
    return sent != SOCKET_ERROR;
}

static
b8 win32_send_socket_data_tcp(Socket socket, u8 *buffer, s64 buffer_size) {
    int sent = send(socket, (char *) buffer, (int) buffer_size, 0);
    return sent != SOCKET_ERROR;
}

#endif



/* ---------------------------------------------- Packet Helpers ---------------------------------------------- */

template<typename T>
static
void serialize(Virtual_Connection *connection, T const &data) {
    memcpy(&connection->outgoing_buffer[connection->outgoing_buffer_size], &data, sizeof(T));
    connection->outgoing_buffer_size += sizeof(T);
}

static
void serialize(Virtual_Connection *connection, void *src, s64 bytes) {
    memcpy(&connection->outgoing_buffer[connection->outgoing_buffer_size], src, bytes);
    connection->outgoing_buffer_size += bytes;
}

template<typename T>
static
T deserialize(Virtual_Connection *connection) {
    T value;
    memcpy(&value, &connection->incoming_buffer[connection->incoming_buffer_offset], sizeof(T));
    connection->incoming_buffer_offset += sizeof(T);
    return value;
}

static
void deserialize(Virtual_Connection *connection, void *dst, s64 bytes) {
    memcpy(dst, &connection->incoming_buffer[connection->incoming_buffer_offset], bytes);
    connection->incoming_buffer_offset += bytes;
}

static
b8 extract_incoming_packet_from_buffer_if_available(Virtual_Connection *connection) {
    if(connection->incoming_buffer_size < PACKET_HEADER_SIZE) return false; // We haven't even read in a complete packet header yet, so there is no way a complete packet is there.

    connection->incoming_buffer_offset = 0;
    u16 peek_packet_size = deserialize<u16>(connection);
    connection->incoming_buffer_offset = 0;
    
    if(peek_packet_size > connection->incoming_buffer_size) return false;

    connection->incoming_packet.header.packet_size      = deserialize<u16>(connection);
    connection->incoming_packet.header.magic            = deserialize<u8>(connection);
    connection->incoming_packet.header.client_id        = deserialize<u32>(connection);
    connection->incoming_packet.header.sequence_id      = deserialize<u32>(connection);
    connection->incoming_packet.header.remote_ack_id    = deserialize<u32>(connection);
    connection->incoming_packet.header.remote_ack_field = deserialize<u32>(connection);
    connection->incoming_packet.header.packet_type      = deserialize<u8>(connection);
    deserialize(connection, connection->incoming_packet.body, connection->incoming_packet.header.packet_size - PACKET_HEADER_SIZE);

    // Move any additional data in the buffer to the front for the next read_packet call.
    memmove(connection->incoming_buffer, &connection->incoming_buffer[connection->incoming_packet.header.packet_size], connection->incoming_buffer_size - connection->incoming_packet.header.packet_size);
    connection->incoming_buffer_size -= connection->incoming_packet.header.packet_size;
    
    return true;
}



/* ------------------------------------------- Connection Handling ------------------------------------------- */

b8 create_client_connection(Virtual_Connection *connection, Connection_Type type, string host, u16 port) {
    connection->socket = win32_create_client_socket(type, host, port, (Win32_Remote_Socket *) &connection->remote);
    if(connection->socket == INVALID_SOCKET) return false;

    connection->type   = type;
    connection->status = CONNECTION_Connecting;
    connection->info   = Virtual_Connection_Info();
    connection->incoming_buffer_size = 0;
    connection->outgoing_buffer_size = 0;
    connection->unacked_reliable_packets = Linked_List<Packet>();
    connection->unacked_reliable_packets.allocator = Default_Allocator;
    
    return true;
}

b8 create_server_connection(Virtual_Connection *connection, Connection_Type type, u16 port) {
    connection->socket = win32_create_server_socket(type, port);
    if(connection->socket == INVALID_SOCKET) return false;

    connection->type    = type;
    connection->status = CONNECTION_Good;
    connection->info   = Virtual_Connection_Info();
    connection->incoming_buffer_size = 0;
    connection->outgoing_buffer_size = 0;
    connection->unacked_reliable_packets = Linked_List<Packet>();
    connection->unacked_reliable_packets.allocator = Default_Allocator;

    return true;
}

void destroy_connection(Virtual_Connection *connection) {
    win32_destroy_socket(&connection->socket);
    connection->status = CONNECTION_Closed;
    connection->type   = CONNECTION_Unknown;
}

Virtual_Connection create_remote_client_connection(Virtual_Connection *server) {
    Virtual_Connection client{};
    client.status = CONNECTION_Good;
    client.socket = server->socket;
    client.type   = server->type;
    win32_copy_remote((Win32_Remote_Socket *) client.remote, (Win32_Remote_Socket *) server->remote);
    client.info.magic = server->info.magic;
    return client;
}

b8 accept_remote_client_connection(Virtual_Connection *server, Virtual_Connection *client) {
    if(server->status == CONNECTION_Closed) return false;

    Socket client_socket;
    Socket_Result result = win32_accept_incoming_client_socket(server->socket, &client_socket);
    if(result == SOCKET_Error) {
        // If the platform's accept() failed due to an error, then the server socket got closed and so should
        // this connection.
        destroy_connection(server);
        return false;
    }

    if(result == SOCKET_No_Data) return false;

    *client = Virtual_Connection{};
    client->socket = client_socket;
    client->status = CONNECTION_Good;
    client->type   = server->type;
    client->info.magic = server->info.magic;
    return true;
}

void send_packet(Virtual_Connection *connection, Packet *packet) {
    if(connection->status == CONNECTION_Closed) return;

    assert(PACKET_HEADER_SIZE + packet->body_size <= PACKET_SIZE);

    packet->header.packet_size      = (u16) (PACKET_HEADER_SIZE + packet->body_size);
    packet->header.magic            = connection->info.magic;
    packet->header.client_id        = connection->info.client_id;
    packet->header.remote_ack_id    = connection->info.remote_ack_id;
    packet->header.remote_ack_field = connection->info.remote_ack_field;
    packet->header.sequence_id      = connection->info.local_sequence_id;

    ++connection->info.local_sequence_id;
    connection->outgoing_buffer_size = 0;
    
    serialize(connection, packet->header.packet_size);
    serialize(connection, packet->header.magic);
    serialize(connection, packet->header.client_id);
    serialize(connection, packet->header.remote_ack_id);
    serialize(connection, packet->header.remote_ack_field);
    serialize(connection, packet->header.packet_type);
    serialize(connection, packet->body, packet->body_size);

    assert(connection->outgoing_buffer_size == packet->header.packet_size);

    b8 success;
    
    switch(connection->type) {
    case CONNECTION_UDP: success = win32_send_socket_data_udp(connection->socket, (Win32_Remote_Socket *) connection->remote, connection->outgoing_buffer, connection->outgoing_buffer_size); break;
    case CONNECTION_TCP: success = win32_send_socket_data_tcp(connection->socket, connection->outgoing_buffer, connection->outgoing_buffer_size); break;
    }

    if(!success) destroy_connection(connection);
}

b8 read_packet(Virtual_Connection *connection) {
    if(connection->status == CONNECTION_Closed) return false;

    // If the connection is based on TCP, we may still have a complete packet in the buffer from the last
    // network read. Try to parse that one first.
    if(extract_incoming_packet_from_buffer_if_available(connection)) return true;

    // No more space in the incoming buffer. If we got here, it means extract_incoming_packet_from_buffer_if_available
    // has failed, but we also cannot read in additional data, which probably means we are soft-locked?
    if(connection->incoming_buffer_size == sizeof(connection->incoming_buffer)) return false;

    Socket_Result result = SOCKET_No_Data;
    Remote_Socket remote;
    s64 received;

    switch(connection->type) {
    case CONNECTION_UDP:
        result = win32_receive_socket_data_udp(connection->socket, &connection->incoming_buffer[connection->incoming_buffer_size], sizeof(connection->incoming_buffer) - connection->incoming_buffer_size, &received, (Win32_Remote_Socket *) &remote);
        break;
    case CONNECTION_TCP:
        result = win32_receive_socket_data_tcp(connection->socket, &connection->incoming_buffer[connection->incoming_buffer_size], sizeof(connection->incoming_buffer) - connection->incoming_buffer_size, &received);
        break;
    }

    if(result == SOCKET_No_Data) return false;

    connection->incoming_buffer_size += received;
    connection->status = CONNECTION_Good; // Since we've received something, assume that the connection is fine.
    win32_copy_remote((Win32_Remote_Socket *) &connection->remote, (Win32_Remote_Socket *) &remote);
    
    return extract_incoming_packet_from_buffer_if_available(connection);
}
