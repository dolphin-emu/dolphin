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

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Network/Http.hpp>
#include <ctype.h>
#include <algorithm>
#include <iterator>
#include <sstream>


namespace
{
    ////////////////////////////////////////////////////////////
    // Convenience function to convert a string to lower case
    ////////////////////////////////////////////////////////////
    std::string ToLower(const std::string& Str)
    {
        std::string Ret = Str;
        for (std::string::iterator i = Ret.begin(); i != Ret.end(); ++i)
            *i = static_cast<char>(tolower(*i));

        return Ret;
    }
}


namespace sf
{
////////////////////////////////////////////////////////////
/// Default constructor
////////////////////////////////////////////////////////////
Http::Request::Request(Method RequestMethod, const std::string& URI, const std::string& Body)
{
    SetMethod(RequestMethod);
    SetURI(URI);
    SetHttpVersion(1, 0);
    SetBody(Body);
}


////////////////////////////////////////////////////////////
/// Set the value of a field; the field is added if it doesn't exist
////////////////////////////////////////////////////////////
void Http::Request::SetField(const std::string& Field, const std::string& Value)
{
    myFields[ToLower(Field)] = Value;
}


////////////////////////////////////////////////////////////
/// Set the request method.
/// This parameter is Get by default
////////////////////////////////////////////////////////////
void Http::Request::SetMethod(Http::Request::Method RequestMethod)
{
    myMethod = RequestMethod;
}


////////////////////////////////////////////////////////////
/// Set the target URI of the request.
/// This parameter is "/" by default
////////////////////////////////////////////////////////////
void Http::Request::SetURI(const std::string& URI)
{
    myURI = URI;

    // Make sure it starts with a '/'
    if (myURI.empty() || (myURI[0] != '/'))
        myURI.insert(0, "/");
}


////////////////////////////////////////////////////////////
/// Set the HTTP version of the request.
/// This parameter is 1.0 by default
////////////////////////////////////////////////////////////
void Http::Request::SetHttpVersion(unsigned int Major, unsigned int Minor)
{
    myMajorVersion = Major;
    myMinorVersion = Minor;
}


////////////////////////////////////////////////////////////
/// Set the body of the request. This parameter is optional and
/// makes sense only for POST requests.
/// This parameter is empty by default
////////////////////////////////////////////////////////////
void Http::Request::SetBody(const std::string& Body)
{
    myBody = Body;
}


////////////////////////////////////////////////////////////
/// Get the string representation of a request header
////////////////////////////////////////////////////////////
std::string Http::Request::ToString() const
{
    std::ostringstream Out;

    // Convert the method to its string representation
    std::string RequestMethod;
    switch (myMethod)
    {
        default :
        case Get :  RequestMethod = "GET";  break;
        case Post : RequestMethod = "POST"; break;
        case Head : RequestMethod = "HEAD"; break;
    }

    // Write the first line containing the request type
    Out << RequestMethod << " " << myURI << " ";
    Out << "HTTP/" << myMajorVersion << "." << myMinorVersion << "\r\n";

    // Write fields
    for (FieldTable::const_iterator i = myFields.begin(); i != myFields.end(); ++i)
    {
        Out << i->first << ": " << i->second << "\r\n";
    }

    // Use an extra \r\n to separate the header from the body
    Out << "\r\n";

    // Add the body
    Out << myBody;

    return Out.str();
}


////////////////////////////////////////////////////////////
/// Check if the given field has been defined
////////////////////////////////////////////////////////////
bool Http::Request::HasField(const std::string& Field) const
{
    return myFields.find(Field) != myFields.end();
}


////////////////////////////////////////////////////////////
/// Default constructor
////////////////////////////////////////////////////////////
Http::Response::Response() :
myStatus      (ConnectionFailed),
myMajorVersion(0),
myMinorVersion(0)
{

}


////////////////////////////////////////////////////////////
/// Get the value of a field
////////////////////////////////////////////////////////////
const std::string& Http::Response::GetField(const std::string& Field) const
{
    FieldTable::const_iterator It = myFields.find(ToLower(Field));
    if (It != myFields.end())
    {
        return It->second;
    }
    else
    {
        static const std::string Empty = "";
        return Empty;
    }
}


////////////////////////////////////////////////////////////
/// Get the header's status code
////////////////////////////////////////////////////////////
Http::Response::Status Http::Response::GetStatus() const
{
    return myStatus;
}


////////////////////////////////////////////////////////////
/// Get the major HTTP version number of the response
////////////////////////////////////////////////////////////
unsigned int Http::Response::GetMajorHttpVersion() const
{
    return myMajorVersion;
}


////////////////////////////////////////////////////////////
/// Get the major HTTP version number of the response
////////////////////////////////////////////////////////////
unsigned int Http::Response::GetMinorHttpVersion() const
{
    return myMinorVersion;
}


////////////////////////////////////////////////////////////
/// Get the body of the response. The body can contain :
/// - the requested page (for GET requests)
/// - a response from the server (for POST requests)
/// - nothing (for HEAD requests)
/// - an error message (in case of an error)
////////////////////////////////////////////////////////////
const std::string& Http::Response::GetBody() const
{
    return myBody;
}


////////////////////////////////////////////////////////////
/// Construct the header from a response string
////////////////////////////////////////////////////////////
void Http::Response::FromString(const std::string& Data)
{
    std::istringstream In(Data);

    // Extract the HTTP version from the first line
    std::string Version;
    if (In >> Version)
    {
        if ((Version.size() >= 8) && (Version[6] == '.') &&
            (ToLower(Version.substr(0, 5)) == "http/")   &&
             isdigit(Version[5]) && isdigit(Version[7]))
        {
            myMajorVersion = Version[5] - '0';
            myMinorVersion = Version[7] - '0';
        }
        else
        {
            // Invalid HTTP version
            myStatus = InvalidResponse;
            return;
        }
    }

    // Extract the status code from the first line
    int StatusCode;
    if (In >> StatusCode)
    {
        myStatus = static_cast<Status>(StatusCode);
    }
    else
    {
        // Invalid status code
        myStatus = InvalidResponse;
        return;
    }

    // Ignore the end of the first line
    In.ignore(10000, '\n');

    // Parse the other lines, which contain fields, one by one
    std::string Line;
    while (std::getline(In, Line) && (Line.size() > 2))
    {
        std::string::size_type Pos = Line.find(": ");
        if (Pos != std::string::npos)
        {
            // Extract the field name and its value
            std::string Field = Line.substr(0, Pos);
            std::string Value = Line.substr(Pos + 2);

            // Remove any trailing \r
            if (!Value.empty() && (*Value.rbegin() == '\r'))
                Value.erase(Value.size() - 1);

            // Add the field
            myFields[ToLower(Field)] = Value;
        }
    }

    // Finally extract the body
    myBody.clear();
    std::copy(std::istreambuf_iterator<char>(In), std::istreambuf_iterator<char>(), std::back_inserter(myBody));
}


////////////////////////////////////////////////////////////
/// Default constructor
////////////////////////////////////////////////////////////
Http::Http() :
myHost(),
myPort(0)
{

}


////////////////////////////////////////////////////////////
/// Construct the Http instance with the target host
////////////////////////////////////////////////////////////
Http::Http(const std::string& Host, unsigned short Port)
{
    SetHost(Host, Port);
}


////////////////////////////////////////////////////////////
/// Set the target host
////////////////////////////////////////////////////////////
void Http::SetHost(const std::string& Host, unsigned short Port)
{
    // Detect the protocol used
    std::string Protocol = ToLower(Host.substr(0, 8));
    if (Protocol.substr(0, 7) == "http://")
    {
        // HTTP protocol
        myHostName = Host.substr(7);
        myPort     = (Port != 0 ? Port : 80);
    }
    else if (Protocol == "https://")
    {
        // HTTPS protocol
        myHostName = Host.substr(8);
        myPort     = (Port != 0 ? Port : 443);
    }
    else
    {
        // Undefined protocol - use HTTP
        myHostName = Host;
        myPort     = (Port != 0 ? Port : 80);
    }

    // Remove any trailing '/' from the host name
    if (!myHostName.empty() && (*myHostName.rbegin() == '/'))
        myHostName.erase(myHostName.size() - 1);

    myHost = sf::IPAddress(myHostName);
}


////////////////////////////////////////////////////////////
/// Send a HTTP request and return the server's response.
/// You must be connected to a host before sending requests.
/// Any missing mandatory header field will be added with an appropriate value.
/// Warning : this function waits for the server's response and may
/// not return instantly; use a thread if you don't want to block your
/// application.
////////////////////////////////////////////////////////////
Http::Response Http::SendRequest(const Http::Request& Req, float Timeout)
{
    // First make sure the request is valid -- add missing mandatory fields
    Request ToSend(Req);
    if (!ToSend.HasField("From"))
    {
        ToSend.SetField("From", "user@sfml-dev.org");
    }
    if (!ToSend.HasField("User-Agent"))
    {
        ToSend.SetField("User-Agent", "libsfml-network/1.x");
    }
    if (!ToSend.HasField("Host"))
    {
        ToSend.SetField("Host", myHostName);
    }
    if (!ToSend.HasField("Content-Length"))
    {
        std::ostringstream Out;
        Out << ToSend.myBody.size();
        ToSend.SetField("Content-Length", Out.str());
    }
    if ((ToSend.myMethod == Request::Post) && !ToSend.HasField("Content-Type"))
    {
        ToSend.SetField("Content-Type", "application/x-www-form-urlencoded");
    }
    if ((ToSend.myMajorVersion * 10 + ToSend.myMinorVersion >= 11) && !ToSend.HasField("Connection"))
    {
        ToSend.SetField("Connection", "close");
    }

    // Prepare the response
    Response Received;

    // Connect the socket to the host
    if (myConnection.Connect(myPort, myHost, Timeout) == Socket::Done)
    {
        // Convert the request to string and send it through the connected socket
        std::string RequestStr = ToSend.ToString();

        if (!RequestStr.empty())
        {
            // Send it through the socket
            if (myConnection.Send(RequestStr.c_str(), RequestStr.size()) == sf::Socket::Done)
            {
                // Wait for the server's response
                std::string ReceivedStr;
                std::size_t Size = 0;
                char Buffer[1024];
                while (myConnection.Receive(Buffer, sizeof(Buffer), Size) == sf::Socket::Done)
                {
                    ReceivedStr.append(Buffer, Buffer + Size);
                }

                // Build the Response object from the received data
                Received.FromString(ReceivedStr);
            }
        }

        // Close the connection
        myConnection.Close();
    }

    return Received;
}

} // namespace sf
