// Under MIT licence from http://www.mindcontrol.org/~hplus/http-get.html

#if !defined( mynetwork_h )
#define mynetwork_h

#include <string>

//  I_HTTPRequest will run until it's received all available data 
//  from the query. You can rewind and read the data received just 
//  like a regular stream. Reading will return 0 bytes when at the 
//  end, even if the query isn't yet complete. Test for completeness 
//  with complete(). You need to step() the query every so often to 
//  retrieve more data. Calling complete() and rewind() may step 
//  the query.
class I_HTTPRequest {
public:
	virtual ~I_HTTPRequest() {}
	virtual void dispose() = 0;
	virtual void step() = 0;
	virtual bool complete() = 0;
	virtual void rewind() = 0;
	virtual size_t read(void * ptr, size_t data) = 0;
};

//  The format of "url" is "http://host:port/path". Name resolution 
//  will be done synchronously, which can be a problem.
//  This request will NOT deal with user names and passwords. You 
//  have been warned!
I_HTTPRequest * NewHTTPRequest(char const * url);

std::string HTTPDownloadText(const char *url);

#endif

