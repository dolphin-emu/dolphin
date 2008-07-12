// Under MIT licence from http://www.mindcontrol.org/~hplus/http-get.html

#if defined(WIN32)
#include <winsock2.h>
#include <windows.h>
#include <time.h>
#else
#include <unistd.h>
#include <fcntl.h>
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "HTTP.h"
#include "PortableSockets.h"

#if !defined(WIN32)
static int strnicmp( char const * a, char const * b, int n) {
	return strncasecmp( a, b, n );
}
#endif


namespace {

	struct Chunk {
		Chunk() {
			next_ = 0;
			size_ = 0;
		}
		Chunk * next_;
		size_t size_;
		char data_[ 30000 ];
	};

	class HTTPQuery : public I_HTTPRequest {
	public:
		HTTPQuery() {
			head_ = 0;
			curRd_ = 0;
			curWr_ = 0;
			curOffset_ = 0;
			toRead_ = 0;
			complete_ = false;
			gotLength_ = false;
			socket_ = BAD_SOCKET_FD;
		}
		~HTTPQuery() {
			if (socket_ != BAD_SOCKET_FD) {
				::closesocket( socket_ );
			}
			Chunk * ch = head_;
			while( ch != 0) {
				Chunk * d = ch;
				ch = ch->next_;
				::free( d );
			}
		}

		void setQuery( char const * host, unsigned short port, char const * url) {
			if (strlen( url ) > 1536 || strlen( host ) > 256) {
				return;
			}
			struct hostent * hent = gethostbyname( host );
			if (hent == 0) {
				complete_ = true;
				return;
			}
			addr_.sin_family = AF_INET;
			addr_.sin_addr = *(in_addr *)hent->h_addr_list[0];
			addr_.sin_port = htons( port );
			socket_ = ::socket( AF_INET, SOCK_STREAM, getprotobyname("tcp")->p_proto );
			if (socket_ == BAD_SOCKET_FD) {
				complete_ = true;
				return;
			}
			int r;
			r = ::connect( socket_, (sockaddr *)&addr_, sizeof( addr_ ) );
			if (r < 0) {
				complete_ = true;
				return;
			}
			MAKE_SOCKET_NONBLOCKING( socket_, r );
			if (r < 0) {
				complete_ = true;
				return;
			}
			char buf[2048];
			sprintf( buf, "GET %s HTTP/1.0\r\nUser-Agent: Dolphin2.0\r\nAccept: */*\r\nHost: %s\r\nConnection: close\r\n\r\n",
				url, host );
			r = ::send( socket_, buf, int(strlen( buf )), NONBLOCK_MSG_SEND );
			if (r != (int)strlen( buf )) {
				complete_ = true;
				return;
			}
		}

		void dispose() {
			delete this;
		}
		void step() {
			if (!complete_) {
				if (!curWr_ || (curWr_->size_ == sizeof( curWr_->data_ ))) {
					Chunk * c = new Chunk;
					if (!head_) {
						head_ = c;
						curWr_ = c;
					}
					else {
						curWr_->next_ = c;
						curWr_ = c;
					}
				}
				assert( curWr_ && (curWr_->size_ < sizeof( curWr_->data_ )) );
				int r = ::recv( socket_, &curWr_->data_[curWr_->size_], int(sizeof(curWr_->data_)-curWr_->size_), 
					NONBLOCK_MSG_SEND );
				if (r > 0) {
					curWr_->size_ += r;
					assert( curWr_->size_ <= sizeof( curWr_->data_ ) );
					if (gotLength_) {
						if (toRead_ <= size_t(r)) {
							toRead_ = 0;
							complete_ = true;
						}
						else {
							toRead_ -= r;
						}
					}
					if (!gotLength_) {
						char const * end = &head_->data_[head_->size_];
						char const * ptr = &head_->data_[1];
						while( ptr < end-1) {
							if (ptr[-1] == '\n') {
								if (!strnicmp( ptr, "content-length:", 15 )) {
									ptr += 15;
									toRead_ = strtol( ptr, (char **)&ptr, 10 );
									gotLength_ = true;
								}
								else if (ptr[0] == '\r' && ptr[1] == '\n') {
									size_t haveRead = end-ptr-2;
									if (haveRead > toRead_) {
										toRead_ = 0;
									}
									else {
										toRead_ -= haveRead;
									}
									if (toRead_ == 0) {
										complete_ = true;
									}
									break;
								}
							}
							++ptr;
						}
					}
				}
				else if (r < 0) {
					if (!SOCKET_WOULDBLOCK_ERROR( SOCKET_ERRNO )) {
						complete_ = true;
					}
				}
			}
		}
		bool complete() {
			step();
			return complete_;
		}
		void rewind() {
			curRd_ = head_;
			curOffset_ = 0;
		}
		size_t read( void * ptr, size_t size) {
			step();
			if (!head_) {
				return 0;
			}
			if (!curRd_) {
				curRd_ = head_;
				assert( curOffset_ == 0 );
			}
			size_t copied = 0;
			while( size > 0) {
				assert( curRd_->size_ <= sizeof( curRd_->data_ ) );
				size_t toCopy = curRd_->size_ - curOffset_;
				if (toCopy > size) {
					toCopy = size;
				}
				memcpy( ptr, &curRd_->data_[curOffset_], toCopy );
				curOffset_ += toCopy;
				assert( curOffset_ <= sizeof(curRd_->data_) );
				ptr = ((char *)ptr)+toCopy;
				size -= toCopy;
				copied += toCopy;
				if (curOffset_ == curRd_->size_) {
					if (curRd_->next_ != 0) {
						curRd_ = curRd_->next_;
						curOffset_ = 0;
					}
					else {
						break;
					}
				}
			}
			return copied;
		}

		Chunk * head_;
		Chunk * curRd_;
		Chunk * curWr_;
		size_t curOffset_;
		size_t toRead_;
		bool complete_;
		bool gotLength_;
		SOCKET socket_;
		sockaddr_in addr_;
	};

};

I_HTTPRequest * NewHTTPRequest( char const * url )
{
	static bool socketsInited;
	if (!socketsInited) {
		socketsInited = true;
		INIT_SOCKET_LIBRARY();
	}
	if (strncmp( url, "http://", 7 )) {
		return 0;
	}
	url += 7;
	char const * path = strchr( url, '/' );
	if (!path) {
		return 0;
	}
	char name[ 256 ];
	if (path-url > 255) {
		return 0;
	}
	strncpy( name, url, path-url );
	name[path-url] = 0;
	char * port = strrchr( name, ':' );
	unsigned short iport = 80;
	if (port) {
		*port = 0;
		iport = (unsigned short)( strtol( port+1, &port, 10 ) );
	}
	HTTPQuery * q = new HTTPQuery;
	q->setQuery( name, iport, path );
	return q;
}

// TODO(ector):
// Currently extremely bad implementation - busy waits!
std::string HTTPDownloadText(const char *url)
{
	I_HTTPRequest *r = NewHTTPRequest(url);
	int timeout = 10;
	std::string text = "";
	time_t t;
	t = 0;
	char buf[4096];
	while (true) 
	{
		r->step();
		size_t rd = r->read(buf, 4096);
		if (rd > 0) 
		{
			buf[rd] = 0;
			text += buf;
		}
		else {
			if (r->complete())
				goto next;

			if (!t) {
				time(&t);
			}
			else {
				time_t t2;
				time(&t2);
				if (t2 > t + timeout) {
					fprintf( stderr, "timeout\n");
					goto next;
				}
			}
		}
	}
next:
    r->dispose();
	return text;
}

void UnittestMyNetwork()
{
	I_HTTPRequest * r = NewHTTPRequest( "http://www.cisco.com/" );
	char buf[1024];
	while( !r->complete()) {
		r->step();
		while( r->read( buf, sizeof( buf ) ) )
			;
	}
	char buf2[100000];
	r->rewind();
	while( r->read( buf2, sizeof( buf2 ) ) )
		;
	r->dispose();
}
