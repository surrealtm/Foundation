#include "socket.h"
#include "os_specific.h"


#if SOCKET_PACKET_LOSS > 0
# include "random.h"
#endif

// @Incomplete: Make serialization respect the endianess


/* ------------------------------------------- Win32 Implementation ------------------------------------------- */

#if FOUNDATION_WIN32
# include <winsock2.h>
# include <ws2tcpip.h>

typedef sockaddr_in Win32_Remote_Socket;

static_assert(sizeof(Remote_Socket) >= sizeof(Win32_Remote_Socket), "Remote_Socket was smaller than expected.");

static s64 wsa_references = 0;

//
// :WSAErrorHandling
// WSA has some terrible error handling. Most of the procedures return success or set
// the LastError on failure, (which can then easily be queried), the other procedures however
// return the error code and don't set the last error.
// That would require a lot more effort to propagate the error message through the abstraction
// layers, so for the second kind of procedures, we just manually set the last wsa error...
// Definitely not very clean, but should do the trick for now.
//
static
string win32_error_string(s32 error_code) {
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
string win32_last_error_string() {
    return win32_error_string(WSAGetLastError());
}

static
Error_Code win32_get_error_code() {
    Error_Code error_code;
    s32 wsa = WSAGetLastError();

    switch(wsa) {
    case WSAEADDRINUSE:     error_code = ERROR_SOCKET_Address_In_Use;        break;
    case WSAEADDRNOTAVAIL:  error_code = ERROR_SOCKET_Address_Not_Available; break;
    case WSAENETDOWN:       error_code = ERROR_SOCKET_Network_Down;          break;
    case WSAENETUNREACH:    error_code = ERROR_SOCKET_Network_Unreachable;   break;
    case WSAENETRESET:      error_code = ERROR_SOCKET_Network_Reset;         break;
    case WSAECONNABORTED:   error_code = ERROR_SOCKET_Connection_Aborted;    break;
    case WSAECONNRESET:     error_code = ERROR_SOCKET_Connection_Reset;      break;
    case WSAECONNREFUSED:   error_code = ERROR_SOCKET_Connection_Refused;    break;
    case WSAEHOSTDOWN:      error_code = ERROR_SOCKET_Host_Down;             break;
    case WSAEHOSTUNREACH:   error_code = ERROR_SOCKET_Host_Unreachable;      break;
    case WSAHOST_NOT_FOUND: error_code = ERROR_SOCKET_Host_Not_Found;        break;
        
    default:
        set_custom_error_message(win32_error_string(wsa));
        error_code = ERROR_Custom_Error_Message;
        break;
    }

    return error_code;
}

static
s32 win32_socket_type_for_connection_type(Connection_Protocol protocol) {
    s32 type_id;

    switch(protocol) {
    case CONNECTION_TCP: type_id = SOCK_STREAM; break;
    case CONNECTION_UDP: type_id = SOCK_DGRAM;  break;
    }

    return type_id;
}

static
s32 win32_protocol_type_for_connection_type(Connection_Protocol protocol) {
    s32 type_id;

    switch(protocol) {
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
b8 win32_remote_sockets_equal(Win32_Remote_Socket *lhs, Win32_Remote_Socket *rhs) {
    return lhs->sin_family == rhs->sin_family && lhs->sin_port == rhs->sin_port && lhs->sin_addr.S_un.S_addr == rhs->sin_addr.S_un.S_addr;
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
void win32_destroy_socket(Socket *socket) {
    closesocket(*socket);
    *socket = INVALID_SOCKET;
}

static
Error_Code win32_create_server_socket(Socket *sock, Connection_Protocol protocol, u16 port) {
    if(!win32_initialize()) return win32_get_error_code();

    int result;
    *sock = socket(AF_INET, win32_socket_type_for_connection_type(protocol), 0);
    if(*sock == INVALID_SOCKET) return win32_get_error_code();
    
    sockaddr_in address;
    address.sin_family = AF_INET; // IPv4
    address.sin_port   = htons(port);
    address.sin_addr.S_un.S_addr = INADDR_ANY;

    result = bind(*sock, (sockaddr *) &address, sizeof(sockaddr_in));
    if(result == SOCKET_ERROR) {
        Error_Code error = win32_get_error_code();
        win32_destroy_socket(sock);
        return error;
    }

    if(protocol == CONNECTION_TCP) {
        result = listen(*sock, SOMAXCONN);
        if(result == SOCKET_ERROR) {
            Error_Code error = win32_get_error_code();
            win32_destroy_socket(sock);
            return error;
        }
    }
    
#if VIRTUAL_CONNECTION_NON_BLOCKING
    u_long nonblocking = true;
    if(ioctlsocket(*sock, FIONBIO, &nonblocking) != 0) {
        Error_Code error = win32_get_error_code();
        win32_destroy_socket(sock);
        return error;
    }
#endif

    return Success;
}

static
Error_Code win32_create_client_socket(Socket *sock, Connection_Protocol protocol, string host, u16 port, Win32_Remote_Socket *out_remote) {
    if(!win32_initialize()) return win32_get_error_code();

    int result;
    *sock = socket(AF_INET, win32_socket_type_for_connection_type(protocol), 0);
    if(*sock == INVALID_SOCKET) return win32_get_error_code();
    
    addrinfo hints{};
    hints.ai_family = AF_INET; // IPv4
    hints.ai_socktype = win32_socket_type_for_connection_type(protocol);
    hints.ai_protocol = win32_protocol_type_for_connection_type(protocol);

    char port_string[8];
    sprintf_s(port_string, sizeof(port_string), "%u", port);

    char *host_string = to_cstring(Default_Allocator, host);
    defer { free_cstring(Default_Allocator, host_string); };

    Win32_Remote_Socket remote;
    addrinfo *host_address;

    result = GetAddrInfoA(host_string, port_string, &hints, &host_address);
    defer { freeaddrinfo(host_address); };

    if(result != 0) {
        Error_Code error = win32_get_error_code();
        win32_destroy_socket(sock);
        return error;
    }

    if(protocol == CONNECTION_UDP) {
        remote = *((sockaddr_in *) host_address->ai_addr);
    }

    result = connect(*sock, host_address->ai_addr, (int) host_address->ai_addrlen);
    if(result != 0) {
        Error_Code error = win32_get_error_code();
        win32_destroy_socket(sock);
        return error;
    }

#if VIRTUAL_CONNECTION_NON_BLOCKING
    u_long nonblocking = true;
    if(ioctlsocket(*sock, FIONBIO, &nonblocking) != 0) {
        Error_Code error = win32_get_error_code();
        win32_destroy_socket(sock);
        return error;
    }
#endif

    *out_remote = remote;
    return Success;
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
        u_long nonblocking = true;
        if(ioctlsocket(*client_socket, FIONBIO, &nonblocking) != 0) {
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

#elif FOUNDATION_LINUX
# include <errno.h>
# include <unistd.h>
# include <sys/socket.h>
# include <netdb.h>
# include <fcntl.h>

typedef struct sockaddr_in Linux_Remote_Socket;

static
string linux_error_string(s32 error_code) {
    string result;

#define error(value) case value: result = strltr(#value); break;

    switch(error_code) {
        error(EPERM);
        error(ENOENT);
        error(ESRCH);
        error(EINTR);
        error(EIO);
        error(ENXIO);
        error(E2BIG);
        error(ENOEXEC);
        error(EBADF);
        error(ECHILD);
        error(EAGAIN);
        error(ENOMEM);
        error(EACCES);
        error(EFAULT);
        error(ENOTBLK);
        error(EBUSY);
        error(EEXIST);
        error(EXDEV);
        error(ENODEV);
        error(ENOTDIR);
        error(EISDIR);
        error(EINVAL);
        error(ENFILE);
        error(EMFILE);
        error(ENOTTY);
        error(ETXTBSY);
        error(EFBIG);
        error(ENOSPC);
        error(ESPIPE);
        error(EROFS);
        error(EDOM);
        error(ERANGE);
        error(EDEADLK);
        error(ENAMETOOLONG);
        error(ENOLCK);
        error(ENOSYS);
        error(ENOTEMPTY);
        error(ELOOP);
        error(ENOMSG);
        error(EIDRM);
        error(ECHRNG);
        error(EL2NSYNC);
        error(EL3HLT);
        error(EL3RST);
        error(ELNRNG);
        error(EUNATCH);
        error(ENOCSI);
        error(EL2HLT);
        error(EBADE);
        error(EBADR);
        error(EXFULL);
        error(ENOANO);
        error(EBADRQC);
        error(EBADSLT);
        error(EBFONT);
        error(ENOSTR);
        error(ENODATA);
        error(ETIME);
        error(ENOSR);
        error(ENONET);
        error(ENOPKG);
        error(EREMOTE);
        error(ENOLINK);
        error(EADV);
        error(ESRMNT);
        error(ECOMM);
        error(EPROTO);
        error(EMULTIHOP);
        error(EDOTDOT);
        error(EBADMSG);
        error(EOVERFLOW);
        error(ENOTUNIQ);
        error(EBADFD);
        error(EREMCHG);
        error(ELIBACC);
        error(ELIBBAD);
        error(ELIBSCN);
        error(ELIBMAX);
        error(ELIBEXEC);
        error(EILSEQ);
        error(ERESTART);
        error(ESTRPIPE);
        error(EUSERS);
        error(ENOTSOCK);
        error(EDESTADDRREQ);
        error(EMSGSIZE);
        error(EPROTOTYPE);
        error(ENOPROTOOPT);
        error(EPROTONOSUPPORT);
        error(ESOCKTNOSUPPORT);
        error(EOPNOTSUPP);
        error(EPFNOSUPPORT);
        error(EAFNOSUPPORT);
        error(EADDRINUSE);
        error(EADDRNOTAVAIL);
        error(ENETDOWN);
        error(ENETUNREACH);
        error(ENETRESET);
        error(ECONNABORTED);
        error(ECONNRESET);
        error(ENOBUFS);
        error(EISCONN);
        error(ENOTCONN);
        error(ESHUTDOWN);
        error(ETOOMANYREFS);
        error(ETIMEDOUT);
        error(ECONNREFUSED);
        error(EHOSTDOWN);
        error(EHOSTUNREACH);
        error(EALREADY);
        error(EINPROGRESS);
        error(ESTALE);
        error(EUCLEAN);
        error(ENOTNAM);
        error(ENAVAIL);
        error(EISNAM);
        error(EREMOTEIO);
        error(EDQUOT);
        error(ENOMEDIUM);
        error(EMEDIUMTYPE);
        error(ECANCELED);
        error(ENOKEY);
        error(EKEYEXPIRED);
        error(EKEYREVOKED);
        error(EKEYREJECTED);
        error(EOWNERDEAD);
        error(ENOTRECOVERABLE);
        error(ERFKILL);
        error(EHWPOISON);
    default: result = "<UnknownSockError>"_s; break;
    }

#undef error
    
    return result;
}

static
string linux_last_error_string() {
    return linux_error_string(errno);
}

static
Error_Code linux_get_error_code() {
    Error_Code error_code;
    
    switch(errno) {
    case EADDRINUSE:     error_code = ERROR_SOCKET_Address_In_Use;        break;
    case EADDRNOTAVAIL:  error_code = ERROR_SOCKET_Address_Not_Available; break;
    case ENETDOWN:       error_code = ERROR_SOCKET_Network_Down;          break;
    case ENETUNREACH:    error_code = ERROR_SOCKET_Network_Unreachable;   break;
    case ENETRESET:      error_code = ERROR_SOCKET_Network_Reset;         break;
    case ECONNABORTED:   error_code = ERROR_SOCKET_Connection_Aborted;    break;
    case ECONNRESET:     error_code = ERROR_SOCKET_Connection_Reset;      break;
    case ECONNREFUSED:   error_code = ERROR_SOCKET_Connection_Refused;    break;
    case EHOSTDOWN:      error_code = ERROR_SOCKET_Host_Down;             break;
    case EHOSTUNREACH:   error_code = ERROR_SOCKET_Host_Unreachable;      break;
    case HOST_NOT_FOUND: error_code = ERROR_SOCKET_Host_Not_Found;        break;

    default:
        set_custom_error_message(linux_error_string(errno));
        error_code = ERROR_Custom_Error_Message;
    }
    
    return error_code;
}

static
s32 linux_socket_type_for_connection_type(Connection_Protocol protocol) {
    s32 type_id;

    switch(protocol) {
    case CONNECTION_TCP: type_id = SOCK_STREAM; break;
    case CONNECTION_UDP: type_id = SOCK_DGRAM;  break;
    default: type_id = -1; break;
    }

    return type_id;
}

static
void linux_copy_remote(Linux_Remote_Socket *dst, Linux_Remote_Socket *src) {
    *dst = *src;
}

static
b8 linux_remote_sockets_equal(Linux_Remote_Socket *lhs, Linux_Remote_Socket *rhs) {
    return lhs->sin_family == rhs->sin_family && lhs->sin_port == rhs->sin_port && lhs->sin_addr.s_addr == rhs->sin_addr.s_addr;
}

static
void linux_destroy_socket(Socket *socket) {
    close(*socket);
    *socket = -1;
}

static
Error_Code linux_create_server_socket(Socket *sock, Connection_Protocol protocol, u16 port) {
    int result;
    *sock = socket(AF_INET, linux_socket_type_for_connection_type(protocol), 0);
    if(*sock == -1) return linux_get_error_code();

    struct sockaddr_in address;
    address.sin_family      = AF_INET; // IPv4
    address.sin_port        = htons(port);
    address.sin_addr.s_addr = INADDR_ANY;
    
    result = bind(*sock, (sockaddr *) &address, sizeof(sockaddr_in));
    if(result == -1) {
        Error_Code error = linux_get_error_code();
        linux_destroy_socket(sock);
        return error;
    }

    if(protocol == CONNECTION_TCP) {
        result = listen(*sock, SOMAXCONN);
        if(result == -1) {
            Error_Code error = linux_get_error_code();
            linux_destroy_socket(sock);
            return error;
        }
    }

#if VIRTUAL_CONNECTION_NON_BLOCKING
    result = fcntl(*sock, F_SETFL, fcntl(*sock, F_GETFL, 0) | O_NONBLOCK);
    if(result == -1) {
        Error_Code error = linux_get_error_code();
        linux_destroy_socket(sock);
        return error;
    }
#endif

    return Success;
}

static
Error_Code linux_create_client_socket(Socket *sock, Connection_Protocol protocol, string host, u16 port, Linux_Remote_Socket *out_remote) {
    int result;
    *sock = socket(AF_INET, linux_socket_type_for_connection_type(protocol), 0);
    if(*sock == -1) return linux_get_error_code();

    char *host_string = to_cstring(Default_Allocator, host);
    defer { free_cstring(Default_Allocator, host_string); };

    struct hostent *host_entry = gethostbyname(host_string);
    if(host_entry == null) {
        Error_Code error = linux_get_error_code();
        linux_destroy_socket(sock);
        return error;
    }

    Linux_Remote_Socket remote;
    struct sockaddr_in host_address;

    host_address.sin_family = AF_INET; // IPv4
    host_address.sin_port   = htons(port);
    memcpy(&host_address.sin_addr, host_entry->h_addr_list[0], host_entry->h_length);

    if(protocol == CONNECTION_UDP) {
        remote = host_address;
    }
    
    result = connect(*sock, (sockaddr *) &host_address, sizeof(host_address));
    if(result == -1) {
        Error_Code error = linux_get_error_code();
        linux_destroy_socket(sock);
        return error;
    }

#if VIRTUAL_CONNECTION_NON_BLOCKING
    result = fcntl(*sock, F_SETFL, fcntl(*sock, F_GETFL, 0) | O_NONBLOCK);
    if(result == -1) {
        Error_Code error = linux_get_error_code();
        linux_destroy_socket(sock);
        return error;
    }
#endif

    *out_remote = remote;
    
    return Success;    
}

static
Socket_Result linux_accept_incoming_client_socket(Socket server_socket, Socket *client_socket) {
    if(server_socket == -1) return SOCKET_Error;

    sockaddr client_address;
    socklen_t client_address_size = sizeof(sockaddr);

    *client_socket = accept(server_socket, &client_address, &client_address_size);
    Socket_Result result = SOCKET_No_Data;

    if(*client_socket == -1) {
        // If the server socket is set to non-blocking, it will return with error code WSAEWOULDBLOCK if no
        // incoming client connection is available. In that case, we just want to continue on normally.
        if(errno != EWOULDBLOCK) {
            result = SOCKET_Error;
        }
    } else {
        result = SOCKET_New_Data;

#if VIRTUAL_CONNECTION_NON_BLOCKING
        if(fcntl(*client_socket, F_SETFL, fcntl(*client_socket, F_GETFL, 0) | O_NONBLOCK) == -1) {
            linux_destroy_socket(client_socket);
            result = SOCKET_Error;
        }
#endif        
    }
    
    return result;
}

static
Socket_Result linux_receive_socket_data_udp(Socket socket, u8 *buffer, s64 buffer_size, s64 *read, Linux_Remote_Socket *remote) {
    socklen_t remote_size = sizeof(Linux_Remote_Socket);

    *read = (s64) recvfrom(socket, (char *) buffer, (int) buffer_size, 0, (sockaddr *) remote, &remote_size);
    Socket_Result result = SOCKET_No_Data;

    if(*read < 0) {
        // If the socket is set to non-blocking, it will return with error code WSAEWOULDBLOCK if no
        // incoming data is available. In that case, we just want to continue on normally.
        if(errno != EWOULDBLOCK) {
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
Socket_Result linux_receive_socket_data_tcp(Socket socket, u8 *buffer, s64 buffer_size, s64 *read) {
    *read = recv(socket, (char *) buffer, (int) buffer_size, 0);
    Socket_Result result = SOCKET_No_Data;

    if(*read < 0) {
        // If the socket is set to non-blocking, it will return with error code WSAEWOULDBLOCK if no
        // incoming data is available. In that case, we just want to continue on normally.
        if(errno != EWOULDBLOCK) {
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
b8 linux_send_socket_data_udp(Socket socket, Linux_Remote_Socket *remote, u8 *buffer, s64 buffer_size) {
    int sent = sendto(socket, (char *) buffer, (int) buffer_size, 0, (sockaddr *) remote, sizeof(Linux_Remote_Socket));
    return sent > 0;
}

static
b8 linux_send_socket_data_tcp(Socket socket, u8 *buffer, s64 buffer_size) {
    int sent = send(socket, (char *) buffer, (int) buffer_size, 0);
    return sent > 0;
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
b8 is_complete_packet_in_incoming_buffer(Virtual_Connection *connection) {
    if(connection->incoming_buffer_size < PACKET_HEADER_SIZE) return false;

    connection->incoming_buffer_offset = 0;
    u16 peek_packet_size = deserialize<u16>(connection);
    connection->incoming_buffer_offset = 0;

    return connection->incoming_buffer_size >= peek_packet_size;
}



/* ------------------------------------------ Packet Acknowledgement ------------------------------------------ */

static
Linked_List_Node<Packet> *find_unacked_reliable_packet_iterator(Virtual_Connection *connection, u32 sequence_id) {
    for(auto *it = connection->unacked_reliable_packets.head; it != null; it = it->next) {
        if(it->data.header.sender_sequence_id == sequence_id) return it;
    }
    
    return null;
}

static
b8 query_ack_field(Virtual_Connection *connection, u32 index_in_field, u32 *sequence_id) {
    u32 ack_field_size  = min(connection->info.ack_id_for_local_packets, 32);
    u32 packet_offset   = ack_field_size - index_in_field - 1;
    b8 packet_was_acked = (connection->info.ack_field_for_local_packets & (0x1 << packet_offset)) != 0;
    *sequence_id        = connection->info.ack_id_for_local_packets - packet_offset;
    return packet_was_acked;
}

void update_virtual_connection_information_for_packet(Virtual_Connection *connection, Packet_Header *header) {
    if(header->sender_sequence_id > connection->info.ack_id_for_remote_packets) {
        //
        // This packet is the latest one we have received from remote. UDP does not ensure the order,
        // so we might very well read packets out of order, but that means that the ack data in these
        // packets is outdated, even if we receive them later. Ignore them.
        //
        u32 ack_delta_for_remote_packets = header->sender_sequence_id - connection->info.ack_id_for_remote_packets;
        u32 ack_delta_for_local_packets  = header->ack_id_for_remote_packets - max(connection->info.ack_id_for_local_packets, 32); // If the ack id is smaller than 32, we can still store additional bits in the ack field and may not need to give up packets just yet.
        
        //
        // Look for potentially dropped packets. We consider a packet dropped if we haven't received an ack
        // for that package, but for a package with a sequence id at least 32 bigger (since we can keep
        // 32 acks in the ack field).
        //
        if(header->ack_id_for_remote_packets > 32) {
            // Look at all the packets in the ack field which will no longer fit into the ack field after
            // this packet's information (ack_delta_for_local_packets). If the ack for that packet has not
            // been received, consider it dropped.
            
            for(u32 i = 0; i < ack_delta_for_local_packets; ++i) {
                u32 packet_sequence_id;
                b8 packet_was_dropped = !query_ack_field(connection, i, &packet_sequence_id);

                if(packet_was_dropped) {
#if SOCKET_DEBUG_PRINT
                    printf("Packet '%u' was dropped.\n", packet_sequence_id); // We are missing the 7, 8, 9....
#endif
                        
                    auto *it = find_unacked_reliable_packet_iterator(connection, packet_sequence_id);
                    if(it) { // We should always find it, ... I think.
                        // Resend the packet. This updates the packet header stored in the list.
                        send_packet(connection, &it->data, (Packet_Type) it->data.header.packet_type);
                    }
                }
            }
        }
        
        //
        // Update the connection info based on the packet header.
        //
        connection->info.ack_id_for_remote_packets    = header->sender_sequence_id;
        connection->info.ack_field_for_remote_packets = (connection->info.ack_field_for_remote_packets << ack_delta_for_remote_packets) | 0x1;
        connection->info.ack_id_for_local_packets     = header->ack_id_for_remote_packets;
        connection->info.ack_field_for_local_packets  = header->ack_field_for_remote_packets;
    } else if(connection->info.ack_id_for_remote_packets < header->sender_sequence_id + 32) {
        // This packet isn't the latest one we have received from remote. But we still want to ack that packet,
        // so that remote won't send it again to us. Note that if the sequence id is "older" than 32, we ignore
        // it because remote (probably) has considered it dropped by now.
        u32 delta = connection->info.ack_id_for_remote_packets - header->sender_sequence_id;
        connection->info.ack_field_for_remote_packets = connection->info.ack_field_for_remote_packets | (0x1 << delta);
    }

    //
    // Find packets that have been ack'ed through this packet header (meaning they haven't been acked before,
    // but are now). We now know they have arrived at remote, and we don't need to bother with them anymore.
    //
    for(u32 i = 0; i < min(connection->info.ack_id_for_local_packets, 32); ++i) {
        u32 packet_sequence_id;
        b8 packet_was_acked = query_ack_field(connection, i, &packet_sequence_id);

        if(packet_was_acked) {
            auto *it = find_unacked_reliable_packet_iterator(connection, packet_sequence_id);
            if(it) { // We should always find it, ... I think.
#if SOCKET_DEBUG_PRINT
                printf("Packet '%u' was acked.\n", packet_sequence_id);
#endif
                connection->unacked_reliable_packets.remove_node(it);
            }
        }
    }
}



/* ------------------------------------------- Connection Handling ------------------------------------------- */

Error_Code create_client_connection(Virtual_Connection *connection, Connection_Protocol protocol, string host, u16 port) {
#if FOUNDATION_WIN32
    Error_Code result = win32_create_client_socket(&connection->socket, protocol, host, port, (Win32_Remote_Socket *) &connection->remote);
#elif FOUNDATION_LINUX
    Error_Code result = linux_create_client_socket(&connection->socket, protocol, host, port, (Linux_Remote_Socket *) &connection->remote);
#endif
    
    if(result != Success) return result;

    connection->type     = CONNECTION_Client;
    connection->protocol = protocol;
    connection->status   = CONNECTION_Connecting;
    connection->info     = Virtual_Connection_Info();
    connection->incoming_buffer_size = 0;
    connection->outgoing_buffer_size = 0;
    connection->unacked_reliable_packets = Linked_List<Packet>();
    connection->unacked_reliable_packets.allocator = Default_Allocator;
    
    return Success;
}

Error_Code create_server_connection(Virtual_Connection *connection, Connection_Protocol protocol, u16 port) {
#if FOUNDATION_WIN32
    Error_Code result = win32_create_server_socket(&connection->socket, protocol, port);
#elif FOUNDATION_LINUX
    Error_Code result = linux_create_server_socket(&connection->socket, protocol, port);
#endif
    
    if(result != Success) return result;

    connection->type     = CONNECTION_Server;
    connection->protocol = protocol;
    connection->status   = CONNECTION_Good;
    connection->info     = Virtual_Connection_Info();
    connection->incoming_buffer_size = 0;
    connection->outgoing_buffer_size = 0;
    connection->unacked_reliable_packets = Linked_List<Packet>();
    connection->unacked_reliable_packets.allocator = Default_Allocator;

    return Success;
}

void destroy_connection(Virtual_Connection *connection) {
    if(connection->type != CONNECTION_Udp_Remote_Client) {
#if FOUNDATION_WIN32
        win32_destroy_socket(&connection->socket);
#elif FOUNDATION_LINUX
        linux_destroy_socket(&connection->socket);
#endif
    }
    
    connection->unacked_reliable_packets.clear();
    connection->status   = CONNECTION_Closed;
    connection->protocol = CONNECTION_Unknown;
    connection->type     = CONNECTION_Undefined;
}

Virtual_Connection create_remote_client_connection(Virtual_Connection *server) {
    Virtual_Connection client{};
    client.type     = CONNECTION_Udp_Remote_Client;
    client.status   = CONNECTION_Good;
    client.socket   = server->socket;
    client.protocol = server->protocol;
    
#if FOUNDATION_WIN32
    win32_copy_remote((Win32_Remote_Socket *) client.remote, (Win32_Remote_Socket *) server->remote);
#elif FOUNDATION_LINUX
    linux_copy_remote((Linux_Remote_Socket *) client.remote, (Linux_Remote_Socket *) server->remote);
#endif
    
    client.info.magic = server->info.magic;
    client.unacked_reliable_packets = Linked_List<Packet>();
    client.unacked_reliable_packets.allocator = Default_Allocator;
    return client;
}

b8 accept_remote_client_connection(Virtual_Connection *server, Virtual_Connection *client) {
    if(server->status == CONNECTION_Closed) return false;

    Socket client_socket;

#if FOUNDATION_WIN32
    Socket_Result result = win32_accept_incoming_client_socket(server->socket, &client_socket);
#elif FOUNDATION_LINUX
    Socket_Result result = linux_accept_incoming_client_socket(server->socket, &client_socket);    
#endif
    
    if(result == SOCKET_Error) {
        // If the platform's accept() failed due to an error, then the server socket got closed and so should
        // this connection.
        destroy_connection(server);
        return false;
    }

    if(result == SOCKET_No_Data) return false;

    *client = Virtual_Connection{};
    client->type       = CONNECTION_Tcp_Remote_Client;
    client->status     = CONNECTION_Good;
    client->socket     = client_socket;
    client->protocol   = server->protocol;
    client->info.magic = server->info.magic;
    return true;
}

void send_packet(Virtual_Connection *connection, Packet *packet, Packet_Type packet_type) {
    if(connection->status == CONNECTION_Closed) return;

    assert(PACKET_HEADER_SIZE + packet->body_size <= PACKET_SIZE);

    packet->header.packet_size                  = (u16) (PACKET_HEADER_SIZE + packet->body_size);
    packet->header.magic                        = connection->info.magic;
    packet->header.sender_client_id             = connection->info.client_id;
    packet->header.sender_sequence_id           = connection->info.sequence_id_for_local_packets;
    packet->header.ack_id_for_remote_packets    = connection->info.ack_id_for_remote_packets;
    packet->header.ack_field_for_remote_packets = connection->info.ack_field_for_remote_packets;
    packet->header.packet_type                  = packet_type;
    
    ++connection->info.sequence_id_for_local_packets;
    connection->outgoing_buffer_size = 0;
    
    serialize(connection, packet->header.packet_size);
    serialize(connection, packet->header.magic);
    serialize(connection, packet->header.sender_client_id);
    serialize(connection, packet->header.sender_sequence_id);
    serialize(connection, packet->header.ack_id_for_remote_packets);
    serialize(connection, packet->header.ack_field_for_remote_packets);
    serialize(connection, packet->header.packet_type);
    assert(connection->outgoing_buffer_size == PACKET_HEADER_SIZE);
    serialize(connection, packet->body, packet->body_size);
    assert(connection->outgoing_buffer_size == packet->header.packet_size);

#if (SOCKET_PACKET_LOSS > 0)
    u32 chance = get_random_u32(0, 100);
    if(chance < SOCKET_PACKET_LOSS) {
#if SOCKET_DEBUG_PRINT
        printf("Dropping packet '%u'.\n", packet->header.sender_sequence_id);
#endif
        return;
    }
#endif
    
#if SOCKET_DEBUG_PRINT
    printf("Sending packet '%u'.\n", packet->header.sender_sequence_id);
#endif
    
    b8 success;

#if FOUNDATION_WIN32
    switch(connection->protocol) {
    case CONNECTION_UDP: success = win32_send_socket_data_udp(connection->socket, (Win32_Remote_Socket *) connection->remote, connection->outgoing_buffer, connection->outgoing_buffer_size); break;
    case CONNECTION_TCP: success = win32_send_socket_data_tcp(connection->socket, connection->outgoing_buffer, connection->outgoing_buffer_size); break;
    }
#elif FOUNDATION_LINUX
    switch(connection->protocol) {
    case CONNECTION_UDP: success = linux_send_socket_data_udp(connection->socket, (Linux_Remote_Socket *) connection->remote, connection->outgoing_buffer, connection->outgoing_buffer_size); break;
    case CONNECTION_TCP: success = linux_send_socket_data_tcp(connection->socket, connection->outgoing_buffer, connection->outgoing_buffer_size); break;
    default: break; // So that clang doesn't complain
    }
#endif

    if(!success) destroy_connection(connection); // Errors when sending data usually hint at a local error, in which case we just assume our local endpoint is dead.
}

void send_reliable_packet(Virtual_Connection *connection, Packet *packet, Packet_Type packet_type) {
    if(connection->status == CONNECTION_Closed) return;

    send_packet(connection, packet, packet_type);

    const s64 too_many_reliable_packets = 64;
    
#if SOCKET_DEBUG_PRINT
    if(connection->unacked_reliable_packets.count >= too_many_reliable_packets) {
        printf("Too many unacked reliable packets in virtual connection, dropping the oldest one.\n");
    }
#endif
    
    if(connection->unacked_reliable_packets.count >= too_many_reliable_packets) {
        connection->unacked_reliable_packets.remove(0);
    }
    
    connection->unacked_reliable_packets.add(*packet); // send_packet fills out information like the sequence_id, which the packet needs.
}

b8 read_packet(Virtual_Connection *connection) {
    if(connection->status == CONNECTION_Closed) return false;

    // Only actually read from the network if two conditions are met:
    //   1. There isn't a complete packet in the incoming buffer already (which mostly happens on tcp, where
    //      the reads don't correspond 1:1 to the sends).
    //   2. There is still some space in the incoming buffer.
    // We might actually softlock ourselves if the incoming buffer is too small to hold a complete packet,
    // but I don't care about that right now.
    if(!is_complete_packet_in_incoming_buffer(connection) && connection->incoming_buffer_size < sizeof(connection->incoming_buffer)) {
        Socket_Result result = SOCKET_No_Data;
        Remote_Socket remote;
        s64 received;

#if FOUNDATION_WIN32
        switch(connection->protocol) {
        case CONNECTION_UDP:
            result = win32_receive_socket_data_udp(connection->socket, &connection->incoming_buffer[connection->incoming_buffer_size], sizeof(connection->incoming_buffer) - connection->incoming_buffer_size, &received, (Win32_Remote_Socket *) &remote);
            break;
        case CONNECTION_TCP:
            result = win32_receive_socket_data_tcp(connection->socket, &connection->incoming_buffer[connection->incoming_buffer_size], sizeof(connection->incoming_buffer) - connection->incoming_buffer_size, &received);
            break;
        }
#elif FOUNDATION_LINUX
        switch(connection->protocol) {
        case CONNECTION_UDP:
            result = linux_receive_socket_data_udp(connection->socket, &connection->incoming_buffer[connection->incoming_buffer_size], sizeof(connection->incoming_buffer) - connection->incoming_buffer_size, &received, (Linux_Remote_Socket *) &remote);
            break;
        case CONNECTION_TCP:
            result = linux_receive_socket_data_tcp(connection->socket, &connection->incoming_buffer[connection->incoming_buffer_size], sizeof(connection->incoming_buffer) - connection->incoming_buffer_size, &received);
            break;
        default: break; // So that clang doesn't complain
        }
#endif

        if(result == SOCKET_New_Data) {
            connection->incoming_buffer_size += received;
            connection->status = CONNECTION_Good; // Since we've received something, assume that the connection is fine.

#if FOUNDATION_WIN32
            win32_copy_remote((Win32_Remote_Socket *) &connection->remote, (Win32_Remote_Socket *) &remote);
#elif FOUNDATION_LINUX
            linux_copy_remote((Linux_Remote_Socket *) &connection->remote, (Linux_Remote_Socket *) &remote);
#endif
        } else if(result == SOCKET_Error && connection->type != CONNECTION_Server) {
            // Close the connection if this isn't a server. A server may lose connection to one client, but should still
            // serve the other clients.
            destroy_connection(connection);
        }
    }

    b8 actually_read_packet = is_complete_packet_in_incoming_buffer(connection);
    if(actually_read_packet) {
        // Copy the packet data from the incoming buffer into the readable packet in the connection.
        connection->incoming_packet.header.packet_size                  = deserialize<u16>(connection);
        connection->incoming_packet.header.magic                        = deserialize<u8>(connection);
        connection->incoming_packet.header.sender_client_id             = deserialize<u32>(connection);
        connection->incoming_packet.header.sender_sequence_id           = deserialize<u32>(connection);
        connection->incoming_packet.header.ack_id_for_remote_packets    = deserialize<u32>(connection);
        connection->incoming_packet.header.ack_field_for_remote_packets = deserialize<u32>(connection);
        connection->incoming_packet.header.packet_type                  = deserialize<u8>(connection);
        assert(connection->incoming_buffer_offset == PACKET_HEADER_SIZE);
        deserialize(connection, connection->incoming_packet.body, connection->incoming_packet.header.packet_size - PACKET_HEADER_SIZE);
        connection->incoming_packet.body_offset = 0;
        connection->incoming_packet.body_size   = connection->incoming_packet.header.packet_size - PACKET_HEADER_SIZE;
        
        // Move any additional data in the buffer to the front for the next read_packet call.
        memmove(connection->incoming_buffer, &connection->incoming_buffer[connection->incoming_packet.header.packet_size], connection->incoming_buffer_size - connection->incoming_packet.header.packet_size);
        connection->incoming_buffer_size -= connection->incoming_packet.header.packet_size;

#if SOCKET_DEBUG_PRINT
        printf("Received packet '%u'.\n", connection->incoming_packet.header.sender_sequence_id);
#endif
    }
    
    return actually_read_packet && connection->incoming_packet.header.magic == connection->info.magic;
}



/* --------------------------------------------- Packet Handling --------------------------------------------- */

void send_connection_request_packet(Virtual_Connection *connection, s64 spam_count) {
    Packet packet;
    packet.body_size = 0;

    for(s64 i = 0; i < spam_count; ++i) {
        send_packet(connection, &packet, PACKET_Connection_Request);
    }
}

void send_connection_established_packet(Virtual_Connection *connection, s64 spam_count) {
    Packet packet;
    packet.body_size = 0;

    for(s64 i = 0; i < spam_count; ++i) {
        send_packet(connection, &packet, PACKET_Connection_Established);
    }
}

void send_connection_closed_packet(Virtual_Connection *connection, s64 spam_count) {
    Packet packet;
    packet.body_size = 0;

    for(s64 i = 0; i < spam_count; ++i) {
        send_packet(connection, &packet, PACKET_Connection_Closed);
    }
}

void send_ping_packet(Virtual_Connection *connection) {
    Packet packet;
    packet.body_size = 0;
    send_packet(connection, &packet, PACKET_Ping);
}

b8 wait_until_connection_established(Virtual_Connection *connection, f32 timeout_in_seconds) {
    Hardware_Time start = os_get_hardware_time();

    b8 success = false;
    
    while(timeout_in_seconds <= 0.f || os_convert_hardware_time(os_get_hardware_time() - start, Seconds) < timeout_in_seconds) {
        if(read_packet(connection)) {
            update_virtual_connection_information_for_packet(connection, &connection->incoming_packet.header);

            if(connection->incoming_packet.header.packet_type == PACKET_Connection_Established) {
                connection->info.client_id = connection->incoming_packet.header.sender_client_id;
                success = true;
                break;
            }
        }
    }

    return success;
}



/* ------------------------------------------ Lower Level Utilities ------------------------------------------ */

b8 remote_sockets_equal(Remote_Socket lhs, Remote_Socket rhs) {
#if FOUNDATION_WIN32
    return win32_remote_sockets_equal((Win32_Remote_Socket *) lhs, (Win32_Remote_Socket *) rhs);
#elif FOUNDATION_LINUX
    return linux_remote_sockets_equal((Linux_Remote_Socket *) lhs, (Linux_Remote_Socket *) rhs);
#endif
}
