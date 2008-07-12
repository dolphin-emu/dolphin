// Under MIT licence from http://www.mindcontrol.org/~hplus/http-get.html

#if !defined( sock_port_h )
#define sock_port_h

// There are differences between Linux/Berkeley sockets and WinSock2
// This file wraps some of the more common ones (as used by this library).

#include <stdarg.h>

#if defined( WIN32 )

// Windows features go here
#include <Winsock2.h>
#include <stdio.h>

#if !defined( NEED_SHORT_TYPES )
#define NEED_SHORT_TYPES 1
#endif
#if !defined( NEED_WINDOWS_POLL )
#define NEED_WINDOWS_POLL 1
#endif
#if !defined( NEED_GETTIMEOFDAY )
#define NEED_GETTIMEOFDAY 1
#endif
#if !defined( NEED_SNPRINTF )
#define NEED_SNPRINTF 1
#endif
#if !defined( NEED_HSTRERROR )
#define NEED_HSTRERROR 1
#endif
#if !defined( NEED_GETLASTERROR )
#define NEED_GETLASTERROR 1
#endif
#if !defined( NEED_FIREWALL_ENABLE )
#define NEED_FIREWALL_ENABLE 1
#endif

typedef int socklen_t;
#define MSG_NOSIGNAL 0
#define MAKE_SOCKET_NONBLOCKING(x,r) \
	do { u_long _x = 1; (r) = ioctlsocket( (x), FIONBIO, &_x ); } while(0)
#define NONBLOCK_MSG_SEND 0
#define INIT_SOCKET_LIBRARY() \
	do { WSADATA wsaData; WSAStartup( MAKEWORD(2,2), &wsaData ); } while(0)

#pragma warning( disable: 4250 )

#define SIN_ADDR_UINT(x) \
	((uint&)(x).S_un.S_addr)
#define BAD_SOCKET_FD 0xffffffffU

#else

// Linux features go here
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/time.h>
#include <sys/poll.h>

#if !defined( NEED_CLOSESOCKET )
#define NEED_CLOSESOCKET 1
#endif
#if !defined( NEED_IOCTLSOCKET )
#define NEED_IOCTLSOCKET 1
#endif
#if !defined( NEED_SHORT_TYPES )
#define NEED_SHORT_TYPES 1
#endif
#if !defined( NEED_ERRNO )
#define NEED_ERRNO 1
#endif

extern int h_errno;
// hack -- I don't make it non-blocking; instead, I pass 
// NONBLOCK_MSG_SEND for each call to sendto()
#define MAKE_SOCKET_NONBLOCKING(x,r) do { (r) = 0; } while(0)
#define NONBLOCK_MSG_SEND MSG_DONTWAIT
#define INIT_SOCKET_LIBRARY() do {} while(0)

#if !defined( SOCKET )
#define SOCKET int
#endif
#define SIN_ADDR_UINT(x) \
	((uint&)(x).s_addr)
#define BAD_SOCKET_FD -1

#endif

#if NEED_CLOSESOCKET
#define closesocket close
#endif

#if NEED_PROTOENT
struct protoent {
	int p_proto;
};
#endif
#if NEED_GETPROTOBYNAME
inline protoent * getprotobyname( char const * s ) {
	static protoent p;
	if( !strcmp( s, "udp" ) ) {
		p.p_proto = IPPROTO_UDP;
		return &p;
	}
	if( !strcmp( s, "tcp" ) ) {
		p.p_proto = IPPROTO_TCP;
		return &p;
	}
	return 0;
}
#endif

#if NEED_SHORT_TYPES
typedef unsigned long ulong;
typedef unsigned int uint;
typedef unsigned short ushort;
typedef unsigned char uchar;
#endif

#if NEED_WINDOWS_POLL
struct pollfd {
	SOCKET fd;
	unsigned short events;
	unsigned short revents;
};
#define POLLIN 0x1
#define POLLPRI 0x2
#define POLLOUT 0x4
#define POLLERR 0x100
#define POLLHUP 0x200
#define POLLNVAL 0x8000
int poll( pollfd * iofds, size_t count, int ms );
#endif

#if NEED_GETTIMEOFDAY
// this is not thread safe!
void gettimeofday( timeval * otv, int );
#endif

#if NEED_SNPRINTF
inline int vsnprintf( char * buf, int size, char const * fmt, va_list vl ) {
	int r = _vsnprintf( buf, size, fmt, vl );
	if( r < 0 ) {
		buf[size-1] = 0;
		r = size-1;
	}
	return r;
}
inline int snprintf( char * buf, int size, char const * fmt, ... ) {
	va_list vl;
	va_start( vl, fmt );
	int r = vsnprintf( buf, size, fmt, vl );
	va_end( vl );
	return r;
}
#endif

#if NEED_HSTRERROR
// NOT thread safe!
inline char const * hstrerror( ulong ec ) {
	static char err[ 128 ];
	snprintf( err, 128, "host error 0x%lx", ec );
	return err;
}
#endif

#if NEED_GETLASTERROR
#define SOCKET_ERRNO WSAGetLastError()
inline bool SOCKET_WOULDBLOCK_ERROR( int e ) { return e == WSAEWOULDBLOCK; }
inline bool SOCKET_NEED_REOPEN( int e ) { return e == WSAECONNRESET; }
#endif

#if NEED_ERRNO
#include <errno.h>
#define SOCKET_ERRNO errno
inline bool SOCKET_WOULDBLOCK_ERROR( int e ) { return e == EWOULDBLOCK; }
inline bool SOCKET_NEED_REOPEN( int e ) { return false; }
#endif

#if NEED_FIREWALL_ENABLE
extern bool ENABLE_FIREWALL();
#else
#define ENABLE_FIREWALL() true
#endif

#endif  //  sock_port_h

