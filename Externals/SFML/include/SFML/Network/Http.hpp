////////////////////////////////////////////////////////////
//
// SFML - Simple and Fast Multimedia Library
// Copyright (C) 2007-2013 Laurent Gomila (laurent.gom@gmail.com)
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
#include <SFML/Network/Export.hpp>
#include <SFML/Network/IPAddress.hpp>
#include <SFML/Network/TcpSocket.hpp>
#include <SFML/System/NonCopyable.hpp>
#include <SFML/System/Time.hpp>
#include <map>
#include <string>


namespace sf
{
////////////////////////////////////////////////////////////
/// \brief A HTTP client
///
////////////////////////////////////////////////////////////
class SFML_NETWORK_API Http : NonCopyable
{
public :

    ////////////////////////////////////////////////////////////
    /// \brief Define a HTTP request
    ///
    ////////////////////////////////////////////////////////////
    class SFML_NETWORK_API Request
    {
    public :

        ////////////////////////////////////////////////////////////
        /// \brief Enumerate the available HTTP methods for a request
        ///
        ////////////////////////////////////////////////////////////
        enum Method
        {
            Get,  ///< Request in get mode, standard method to retrieve a page
            Post, ///< Request in post mode, usually to send data to a page
            Head  ///< Request a page's header only
        };

        ////////////////////////////////////////////////////////////
        /// \brief Default constructor
        ///
        /// This constructor creates a GET request, with the root
        /// URI ("/") and an empty body.
        ///
        /// \param uri    Target URI
        /// \param method Method to use for the request
        /// \param body   Content of the request's body
        ///
        ////////////////////////////////////////////////////////////
        Request(const std::string& uri = "/", Method method = Get, const std::string& body = "");

        ////////////////////////////////////////////////////////////
        /// \brief Set the value of a field
        ///
        /// The field is created if it doesn't exist. The name of
        /// the field is case insensitive.
        /// By default, a request doesn't contain any field (but the
        /// mandatory fields are added later by the HTTP client when
        /// sending the request).
        ///
        /// \param field Name of the field to set
        /// \param value Value of the field
        ///
        ////////////////////////////////////////////////////////////
        void setField(const std::string& field, const std::string& value);

        ////////////////////////////////////////////////////////////
        /// \brief Set the request method
        ///
        /// See the Method enumeration for a complete list of all
        /// the availale methods.
        /// The method is Http::Request::Get by default.
        ///
        /// \param method Method to use for the request
        ///
        ////////////////////////////////////////////////////////////
        void setMethod(Method method);

        ////////////////////////////////////////////////////////////
        /// \brief Set the requested URI
        ///
        /// The URI is the resource (usually a web page or a file)
        /// that you want to get or post.
        /// The URI is "/" (the root page) by default.
        ///
        /// \param uri URI to request, relative to the host
        ///
        ////////////////////////////////////////////////////////////
        void setUri(const std::string& uri);

        ////////////////////////////////////////////////////////////
        /// \brief Set the HTTP version for the request
        ///
        /// The HTTP version is 1.0 by default.
        ///
        /// \param major Major HTTP version number
        /// \param minor Minor HTTP version number
        ///
        ////////////////////////////////////////////////////////////
        void setHttpVersion(unsigned int major, unsigned int minor);

        ////////////////////////////////////////////////////////////
        /// \brief Set the body of the request
        ///
        /// The body of a request is optional and only makes sense
        /// for POST requests. It is ignored for all other methods.
        /// The body is empty by default.
        ///
        /// \param body Content of the body
        ///
        ////////////////////////////////////////////////////////////
        void setBody(const std::string& body);

    private :

        friend class Http;

        ////////////////////////////////////////////////////////////
        /// \brief Prepare the final request to send to the server
        ///
        /// This is used internally by Http before sending the
        /// request to the web server.
        ///
        /// \return String containing the request, ready to be sent
        ///
        ////////////////////////////////////////////////////////////
        std::string prepare() const;

        ////////////////////////////////////////////////////////////
        /// \brief Check if the request defines a field
        ///
        /// This function uses case-insensitive comparisons.
        ///
        /// \param field Name of the field to test
        ///
        /// \return True if the field exists, false otherwise
        ///
        ////////////////////////////////////////////////////////////
        bool hasField(const std::string& field) const;

        ////////////////////////////////////////////////////////////
        // Types
        ////////////////////////////////////////////////////////////
        typedef std::map<std::string, std::string> FieldTable;

        ////////////////////////////////////////////////////////////
        // Member data
        ////////////////////////////////////////////////////////////
        FieldTable   m_fields;       ///< Fields of the header associated to their value
        Method       m_method;       ///< Method to use for the request
        std::string  m_uri;          ///< Target URI of the request
        unsigned int m_majorVersion; ///< Major HTTP version
        unsigned int m_minorVersion; ///< Minor HTTP version
        std::string  m_body;         ///< Body of the request
    };

    ////////////////////////////////////////////////////////////
    /// \brief Define a HTTP response
    ///
    ////////////////////////////////////////////////////////////
    class SFML_NETWORK_API Response
    {
    public :

        ////////////////////////////////////////////////////////////
        /// \brief Enumerate all the valid status codes for a response
        ///
        ////////////////////////////////////////////////////////////
        enum Status
        {
            // 2xx: success
            Ok             = 200, ///< Most common code returned when operation was successful
            Created        = 201, ///< The resource has successfully been created
            Accepted       = 202, ///< The request has been accepted, but will be processed later by the server
            NoContent      = 204, ///< The server didn't send any data in return
            ResetContent   = 205, ///< The server informs the client that it should clear the view (form) that caused the request to be sent
            PartialContent = 206, ///< The server has sent a part of the resource, as a response to a partial GET request

            // 3xx: redirection
            MultipleChoices  = 300, ///< The requested page can be accessed from several locations
            MovedPermanently = 301, ///< The requested page has permanently moved to a new location
            MovedTemporarily = 302, ///< The requested page has temporarily moved to a new location
            NotModified      = 304, ///< For conditionnal requests, means the requested page hasn't changed and doesn't need to be refreshed

            // 4xx: client error
            BadRequest          = 400, ///< The server couldn't understand the request (syntax error)
            Unauthorized        = 401, ///< The requested page needs an authentification to be accessed
            Forbidden           = 403, ///< The requested page cannot be accessed at all, even with authentification
            NotFound            = 404, ///< The requested page doesn't exist
            RangeNotSatisfiable = 407, ///< The server can't satisfy the partial GET request (with a "Range" header field)

            // 5xx: server error
            InternalServerError = 500, ///< The server encountered an unexpected error
            NotImplemented      = 501, ///< The server doesn't implement a requested feature
            BadGateway          = 502, ///< The gateway server has received an error from the source server
            ServiceNotAvailable = 503, ///< The server is temporarily unavailable (overloaded, in maintenance, ...)
            GatewayTimeout      = 504, ///< The gateway server couldn't receive a response from the source server
            VersionNotSupported = 505, ///< The server doesn't support the requested HTTP version

            // 10xx: SFML custom codes
            InvalidResponse  = 1000, ///< Response is not a valid HTTP one
            ConnectionFailed = 1001  ///< Connection with server failed
        };

        ////////////////////////////////////////////////////////////
        /// \brief Default constructor
        ///
        /// Constructs an empty response.
        ///
        ////////////////////////////////////////////////////////////
        Response();

        ////////////////////////////////////////////////////////////
        /// \brief Get the value of a field
        ///
        /// If the field \a field is not found in the response header,
        /// the empty string is returned. This function uses
        /// case-insensitive comparisons.
        ///
        /// \param field Name of the field to get
        ///
        /// \return Value of the field, or empty string if not found
        ///
        ////////////////////////////////////////////////////////////
        const std::string& getField(const std::string& field) const;

        ////////////////////////////////////////////////////////////
        /// \brief Get the response status code
        ///
        /// The status code should be the first thing to be checked
        /// after receiving a response, it defines whether it is a
        /// success, a failure or anything else (see the Status
        /// enumeration).
        ///
        /// \return Status code of the response
        ///
        ////////////////////////////////////////////////////////////
        Status getStatus() const;

        ////////////////////////////////////////////////////////////
        /// \brief Get the major HTTP version number of the response
        ///
        /// \return Major HTTP version number
        ///
        /// \see getMinorHttpVersion
        ///
        ////////////////////////////////////////////////////////////
        unsigned int getMajorHttpVersion() const;

        ////////////////////////////////////////////////////////////
        /// \brief Get the minor HTTP version number of the response
        ///
        /// \return Minor HTTP version number
        ///
        /// \see getMajorHttpVersion
        ///
        ////////////////////////////////////////////////////////////
        unsigned int getMinorHttpVersion() const;

        ////////////////////////////////////////////////////////////
        /// \brief Get the body of the response
        ///
        /// The body of a response may contain:
        /// \li the requested page (for GET requests)
        /// \li a response from the server (for POST requests)
        /// \li nothing (for HEAD requests)
        /// \li an error message (in case of an error)
        ///
        /// \return The response body
        ///
        ////////////////////////////////////////////////////////////
        const std::string& getBody() const;

    private :

        friend class Http;

        ////////////////////////////////////////////////////////////
        /// \brief Construct the header from a response string
        ///
        /// This function is used by Http to build the response
        /// of a request.
        ///
        /// \param data Content of the response to parse
        ///
        ////////////////////////////////////////////////////////////
        void parse(const std::string& data);

        ////////////////////////////////////////////////////////////
        // Types
        ////////////////////////////////////////////////////////////
        typedef std::map<std::string, std::string> FieldTable;

        ////////////////////////////////////////////////////////////
        // Member data
        ////////////////////////////////////////////////////////////
        FieldTable   m_fields;       ///< Fields of the header
        Status       m_status;       ///< Status code
        unsigned int m_majorVersion; ///< Major HTTP version
        unsigned int m_minorVersion; ///< Minor HTTP version
        std::string  m_body;         ///< Body of the response
    };

    ////////////////////////////////////////////////////////////
    /// \brief Default constructor
    ///
    ////////////////////////////////////////////////////////////
    Http();

    ////////////////////////////////////////////////////////////
    /// \brief Construct the HTTP client with the target host
    ///
    /// This is equivalent to calling setHost(host, port).
    /// The port has a default value of 0, which means that the
    /// HTTP client will use the right port according to the
    /// protocol used (80 for HTTP, 443 for HTTPS). You should
    /// leave it like this unless you really need a port other
    /// than the standard one, or use an unknown protocol.
    ///
    /// \param host Web server to connect to
    /// \param port Port to use for connection
    ///
    ////////////////////////////////////////////////////////////
    Http(const std::string& host, unsigned short port = 0);

    ////////////////////////////////////////////////////////////
    /// \brief Set the target host
    ///
    /// This function just stores the host address and port, it
    /// doesn't actually connect to it until you send a request.
    /// The port has a default value of 0, which means that the
    /// HTTP client will use the right port according to the
    /// protocol used (80 for HTTP, 443 for HTTPS). You should
    /// leave it like this unless you really need a port other
    /// than the standard one, or use an unknown protocol.
    ///
    /// \param host Web server to connect to
    /// \param port Port to use for connection
    ///
    ////////////////////////////////////////////////////////////
    void setHost(const std::string& host, unsigned short port = 0);

    ////////////////////////////////////////////////////////////
    /// \brief Send a HTTP request and return the server's response.
    ///
    /// You must have a valid host before sending a request (see setHost).
    /// Any missing mandatory header field in the request will be added
    /// with an appropriate value.
    /// Warning: this function waits for the server's response and may
    /// not return instantly; use a thread if you don't want to block your
    /// application, or use a timeout to limit the time to wait. A value
    /// of Time::Zero means that the client will use the system defaut timeout
    /// (which is usually pretty long).
    ///
    /// \param request Request to send
    /// \param timeout Maximum time to wait
    ///
    /// \return Server's response
    ///
    ////////////////////////////////////////////////////////////
    Response sendRequest(const Request& request, Time timeout = Time::Zero);

private :

    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    TcpSocket      m_connection; ///< Connection to the host
    IpAddress      m_host;       ///< Web host address
    std::string    m_hostName;   ///< Web host name
    unsigned short m_port;       ///< Port used for connection with host
};

} // namespace sf


#endif // SFML_HTTP_HPP


////////////////////////////////////////////////////////////
/// \class sf::Http
/// \ingroup network
///
/// sf::Http is a very simple HTTP client that allows you
/// to communicate with a web server. You can retrieve
/// web pages, send data to an interactive resource,
/// download a remote file, etc.
///
/// The HTTP client is split into 3 classes:
/// \li sf::Http::Request
/// \li sf::Http::Response
/// \li sf::Http
///
/// sf::Http::Request builds the request that will be
/// sent to the server. A request is made of:
/// \li a method (what you want to do)
/// \li a target URI (usually the name of the web page or file)
/// \li one or more header fields (options that you can pass to the server)
/// \li an optional body (for POST requests)
///
/// sf::Http::Response parse the response from the web server
/// and provides getters to read them. The response contains:
/// \li a status code
/// \li header fields (that may be answers to the ones that you requested)
/// \li a body, which contains the contents of the requested resource
///
/// sf::Http provides a simple function, SendRequest, to send a
/// sf::Http::Request and return the corresponding sf::Http::Response
/// from the server.
///
/// Usage example:
/// \code
/// // Create a new HTTP client
/// sf::Http http;
///
/// // We'll work on http://www.sfml-dev.org
/// http.setHost("http://www.sfml-dev.org");
///
/// // Prepare a request to get the 'features.php' page
/// sf::Http::Request request("features.php");
///
/// // Send the request
/// sf::Http::Response response = http.sendRequest(request);
///
/// // Check the status code and display the result
/// sf::Http::Response::Status status = response.getStatus();
/// if (status == sf::Http::Response::Ok)
/// {
///     std::cout << response.getBody() << std::endl;
/// }
/// else
/// {
///     std::cout << "Error " << status << std::endl;
/// }
/// \endcode
///
////////////////////////////////////////////////////////////
