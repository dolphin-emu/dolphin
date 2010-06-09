////////////////////////////////////////////////////////////
//
// SFML - Simple and Fast Multimedia Library
// Copyright (C) 2007-2009 Laurent Gomila (laurent.gom@gmail.com)
//
// This software is provided 'as-is', without any express or implied warranty.
// In no event will the authors be held liable for any damages arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it freely,
// subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented;
//    you must not claim that you wrote the original software.
//    If you use this software in a product, an acknowledgment
//    in the product documentation would be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such,
//    and must not be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source distribution.
//
////////////////////////////////////////////////////////////

#ifndef SFML_HTTP_HPP
#define SFML_HTTP_HPP

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/System/NonCopyable.hpp>
#include <SFML/Network/IPAddress.hpp>
#include <SFML/Network/SocketTCP.hpp>
#include <map>
#include <string>


namespace sf
{
////////////////////////////////////////////////////////////
/// This class provides methods for manipulating the HTTP
/// protocol (described in RFC 1945).
/// It can connect to a website, get its files, send requests, etc.
////////////////////////////////////////////////////////////
class SFML_API Http : NonCopyable
{
public :

    ////////////////////////////////////////////////////////////
    /// This class wraps an HTTP request, which is basically :
    /// - a header with a method, a target URI, and a set of field/value pairs
    /// - an optional body (for POST requests)
    ////////////////////////////////////////////////////////////
    class SFML_API Request
    {
    public :

        ////////////////////////////////////////////////////////////
        /// Enumerate the available HTTP methods for a request
        ////////////////////////////////////////////////////////////
        enum Method
        {
            Get,  ///< Request in get mode, standard method to retrieve a page
            Post, ///< Request in post mode, usually to send data to a page
            Head  ///< Request a page's header only
        };

        ////////////////////////////////////////////////////////////
        /// Default constructor
        ///
        /// \param RequestMethod : Method to use for the request (Get by default)
        /// \param URI :           Target URI ("/" by default -- index page)
        /// \param Body :          Content of the request's body (empty by default)
        ///
        ////////////////////////////////////////////////////////////
        Request(Method RequestMethod = Get, const std::string& URI = "/", const std::string& Body = "");

        ////////////////////////////////////////////////////////////
        /// Set the value of a field; the field is added if it doesn't exist
        ///
        /// \param Field : Name of the field to set (case-insensitive)
        /// \param Value : Value of the field
        ///
        ////////////////////////////////////////////////////////////
        void SetField(const std::string& Field, const std::string& Value);

        ////////////////////////////////////////////////////////////
        /// Set the request method.
        /// This parameter is Http::Request::Get by default
        ///
        /// \param RequestMethod : Method to use for the request
        ///
        ////////////////////////////////////////////////////////////
        void SetMethod(Method RequestMethod);

        ////////////////////////////////////////////////////////////
        /// Set the target URI of the request.
        /// This parameter is "/" by default
        ///
        /// \param URI : URI to request, local to the host
        ///
        ////////////////////////////////////////////////////////////
        void SetURI(const std::string& URI);

        ////////////////////////////////////////////////////////////
        /// Set the HTTP version of the request.
        /// This parameter is 1.0 by default
        ///
        /// \param Major : Major version number
        /// \param Minor : Minor version number
        ///
        ////////////////////////////////////////////////////////////
        void SetHttpVersion(unsigned int Major, unsigned int Minor);

        ////////////////////////////////////////////////////////////
        /// Set the body of the request. This parameter is optional and
        /// makes sense only for POST requests.
        /// This parameter is empty by default
        ///
        /// \param Body : Content of the request body
        ///
        ////////////////////////////////////////////////////////////
        void SetBody(const std::string& Body);

    private :

        friend class Http;

        ////////////////////////////////////////////////////////////
        /// Get the string representation of the request header
        ///
        /// \return String containing the request
        ///
        ////////////////////////////////////////////////////////////
        std::string ToString() const;

        ////////////////////////////////////////////////////////////
        /// Check if the given field has been defined
        ///
        /// \param Field : Name of the field to check (case-insensitive)
        ///
        /// \return True if the field exists
        ///
        ////////////////////////////////////////////////////////////
        bool HasField(const std::string& Field) const;

        ////////////////////////////////////////////////////////////
        // Types
        ////////////////////////////////////////////////////////////
        typedef std::map<std::string, std::string> FieldTable;

        ////////////////////////////////////////////////////////////
        // Member data
        ////////////////////////////////////////////////////////////
        FieldTable   myFields;       ///< Fields of the header
        Method       myMethod;       ///< Method to use for the request
        std::string  myURI;          ///< Target URI of the request
        unsigned int myMajorVersion; ///< Major HTTP version
        unsigned int myMinorVersion; ///< Minor HTTP version
        std::string  myBody;         ///< Body of the request
    };

    ////////////////////////////////////////////////////////////
    /// This class wraps an HTTP response, which is basically :
    /// - a header with a status code and a set of field/value pairs
    /// - a body (the content of the requested resource)
    ////////////////////////////////////////////////////////////
    class SFML_API Response
    {
    public :

        ////////////////////////////////////////////////////////////
        /// Enumerate all the valid status codes returned in
        /// a HTTP response
        ////////////////////////////////////////////////////////////
        enum Status
        {
            // 2xx: success
            Ok        = 200, ///< Most common code returned when operation was successful
            Created   = 201, ///< The resource has successfully been created
            Accepted  = 202, ///< The request has been accepted, but will be processed later by the server
            NoContent = 204, ///< Sent when the server didn't send any data in return

            // 3xx: redirection
            MultipleChoices  = 300, ///< The requested page can be accessed from several locations
            MovedPermanently = 301, ///< The requested page has permanently moved to a new location
            MovedTemporarily = 302, ///< The requested page has temporarily moved to a new location
            NotModified      = 304, ///< For conditionnal requests, means the requested page hasn't changed and doesn't need to be refreshed

            // 4xx: client error
            BadRequest   = 400, ///< The server couldn't understand the request (syntax error)
            Unauthorized = 401, ///< The requested page needs an authentification to be accessed
            Forbidden    = 403, ///< The requested page cannot be accessed at all, even with authentification
            NotFound     = 404, ///< The requested page doesn't exist

            // 5xx: server error
            InternalServerError = 500, ///< The server encountered an unexpected error
            NotImplemented      = 501, ///< The server doesn't implement a requested feature
            BadGateway          = 502, ///< The gateway server has received an error from the source server
            ServiceNotAvailable = 503, ///< The server is temporarily unavailable (overloaded, in maintenance, ...)

            // 10xx: SFML custom codes
            InvalidResponse  = 1000, ///< Response is not a valid HTTP one
            ConnectionFailed = 1001  ///< Connection with server failed
        };

        ////////////////////////////////////////////////////////////
        /// Default constructor
        ///
        ////////////////////////////////////////////////////////////
        Response();

        ////////////////////////////////////////////////////////////
        /// Get the value of a field
        ///
        /// \param Field : Name of the field to get (case-insensitive)
        ///
        /// \return Value of the field, or empty string if not found
        ///
        ////////////////////////////////////////////////////////////
        const std::string& GetField(const std::string& Field) const;

        ////////////////////////////////////////////////////////////
        /// Get the header's status code
        ///
        /// \return Header's status code
        ///
        ////////////////////////////////////////////////////////////
        Status GetStatus() const;

        ////////////////////////////////////////////////////////////
        /// Get the major HTTP version number of the response
        ///
        /// \return Major version number
        ///
        ////////////////////////////////////////////////////////////
        unsigned int GetMajorHttpVersion() const;

        ////////////////////////////////////////////////////////////
        /// Get the major HTTP version number of the response
        ///
        /// \return Major version number
        ///
        ////////////////////////////////////////////////////////////
        unsigned int GetMinorHttpVersion() const;

        ////////////////////////////////////////////////////////////
        /// Get the body of the response. The body can contain :
        /// - the requested page (for GET requests)
        /// - a response from the server (for POST requests)
        /// - nothing (for HEAD requests)
        /// - an error message (in case of an error)
        ///
        /// \return The response body
        ///
        ////////////////////////////////////////////////////////////
        const std::string& GetBody() const;

    private :

        friend class Http;

        ////////////////////////////////////////////////////////////
        /// Construct the header from a response string
        ///
        /// \param Data : Content of the response's header to parse
        ///
        ////////////////////////////////////////////////////////////
        void FromString(const std::string& Data);

        ////////////////////////////////////////////////////////////
        // Types
        ////////////////////////////////////////////////////////////
        typedef std::map<std::string, std::string> FieldTable;

        ////////////////////////////////////////////////////////////
        // Member data
        ////////////////////////////////////////////////////////////
        FieldTable   myFields;       ///< Fields of the header
        Status       myStatus;       ///< Status code
        unsigned int myMajorVersion; ///< Major HTTP version
        unsigned int myMinorVersion; ///< Minor HTTP version
        std::string  myBody;         ///< Body of the response
    };

    ////////////////////////////////////////////////////////////
    /// Default constructor
    ///
    ////////////////////////////////////////////////////////////
    Http();

    ////////////////////////////////////////////////////////////
    /// Construct the Http instance with the target host
    ///
    /// \param Host : Web server to connect to
    /// \param Port : Port to use for connection (0 by default -- use the standard port of the protocol used)
    ///
    ////////////////////////////////////////////////////////////
    Http(const std::string& Host, unsigned short Port = 0);

    ////////////////////////////////////////////////////////////
    /// Set the target host
    ///
    /// \param Host : Web server to connect to
    /// \param Port : Port to use for connection (0 by default -- use the standard port of the protocol used)
    ///
    ////////////////////////////////////////////////////////////
    void SetHost(const std::string& Host, unsigned short Port = 0);

    ////////////////////////////////////////////////////////////
    /// Send a HTTP request and return the server's response.
    /// You must be connected to a host before sending requests.
    /// Any missing mandatory header field will be added with an appropriate value.
    /// Warning : this function waits for the server's response and may
    /// not return instantly; use a thread if you don't want to block your
    /// application.
    ///
    /// \param Req :     Request to send
    /// \param Timeout : Maximum time to wait, in seconds (0 by default, means no timeout)
    ///
    /// \return Server's response
    ///
    ////////////////////////////////////////////////////////////
    Response SendRequest(const Request& Req, float Timeout = 0.f);

private :

    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    SocketTCP      myConnection; ///< Connection to the host
    IPAddress      myHost;       ///< Web host address
    std::string    myHostName;   ///< Web host name
    unsigned short myPort;       ///< Port used for connection with host
};

} // namespace sf


#endif // SFML_HTTP_HPP
