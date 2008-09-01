// Under MIT licence from http://www.mindcontrol.org/~hplus/http-get.html
// We only use a subset of the definitions in the original source.

#if !defined( sock_port_h )
#define sock_port_h

// There are differences between Linux/Berkeley sockets and WinSock2
// This file wraps some of the more common ones (as used by this library).

#if defined(_WIN32)
// Windows features go here

#include <Winsock2.h>

typedef int socklen_t;
#define MAKE_SOCKET_NONBLOCKING(x,r) \
	do { u_long _x = 1; (r) = ioctlsocket( (x), FIONBIO, &_x ); } while(0)
#define NONBLOCK_MSG_SEND 0
#define INIT_SOCKET_LIBRARY() \
	do { WSADATA wsaData; WSAStartup( MAKEWORD(2,2), &wsaData ); } while(0)

#define BAD_SOCKET_FD 0xffffffffU

#define SOCKET_ERRNO WSAGetLastError()
inline bool SOCKET_WOULDBLOCK_ERROR(int e) { return e == WSAEWOULDBLOCK; }
inline bool SOCKET_NEED_REOPEN(int e) { return e == WSAECONNRESET; }

#else
// POSIX features go here

#include <sys/socket.h>
#include <netdb.h>
#include <cerrno>

#if !defined(SOCKET)
#define SOCKET int
#endif

// hack -- I don't make it non-blocking; instead, I pass
// NONBLOCK_MSG_SEND for each call to sendto()
#define MAKE_SOCKET_NONBLOCKING(x,r) do { (r) = 0; } while(0)
#define NONBLOCK_MSG_SEND MSG_DONTWAIT
#define INIT_SOCKET_LIBRARY() do {} while(0)

#define BAD_SOCKET_FD -1

#define closesocket close

#define SOCKET_ERRNO errno
inline bool SOCKET_WOULDBLOCK_ERROR(int e) { return e == EWOULDBLOCK; }
inline bool SOCKET_NEED_REOPEN(int e) { return false; }

#endif

#endif  //  sock_port_h
