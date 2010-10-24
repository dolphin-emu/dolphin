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
#include <SFML/Network/Ftp.hpp>
#include <SFML/Network/IPAddress.hpp>
#include <algorithm>
#include <fstream>
#include <iterator>
#include <sstream>


namespace sf
{
////////////////////////////////////////////////////////////
// Utility class for exchanging stuff with the server
// on the data channel
////////////////////////////////////////////////////////////
class Ftp::DataChannel : NonCopyable
{
public :

    ////////////////////////////////////////////////////////////
    // Constructor
    ////////////////////////////////////////////////////////////
    DataChannel(Ftp& Owner);

    ////////////////////////////////////////////////////////////
    // Destructor
    ////////////////////////////////////////////////////////////
    ~DataChannel();

    ////////////////////////////////////////////////////////////
    // Open the data channel using the specified mode and port
    ////////////////////////////////////////////////////////////
    Ftp::Response Open(Ftp::TransferMode Mode);

    ////////////////////////////////////////////////////////////
    // Send data on the data channel
    ////////////////////////////////////////////////////////////
    void Send(const std::vector<char>& Data);

    ////////////////////////////////////////////////////////////
    // Receive data on the data channel until it is closed
    ////////////////////////////////////////////////////////////
    void Receive(std::vector<char>& Data);

private :

    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    Ftp&      myFtp;        ///< Reference to the owner Ftp instance
    SocketTCP myDataSocket; ///< Socket used for data transfers
};


////////////////////////////////////////////////////////////
/// Default constructor
////////////////////////////////////////////////////////////
Ftp::Response::Response(Status Code, const std::string& Message) :
myStatus (Code),
myMessage(Message)
{

}


////////////////////////////////////////////////////////////
/// Convenience function to check if the response status code
/// means a success
////////////////////////////////////////////////////////////
bool Ftp::Response::IsOk() const
{
    return myStatus < 400;
}


////////////////////////////////////////////////////////////
/// Get the response status code
////////////////////////////////////////////////////////////
Ftp::Response::Status Ftp::Response::GetStatus() const
{
    return myStatus;
}


////////////////////////////////////////////////////////////
/// Get the full message contained in the response
////////////////////////////////////////////////////////////
const std::string& Ftp::Response::GetMessage() const
{
    return myMessage;
}


////////////////////////////////////////////////////////////
/// Default constructor
////////////////////////////////////////////////////////////
Ftp::DirectoryResponse::DirectoryResponse(Ftp::Response Resp) :
Ftp::Response(Resp)
{
    if (IsOk())
    {
        // Extract the directory from the server response
        std::string::size_type Begin = Resp.GetMessage().find('"', 0);
        std::string::size_type End   = Resp.GetMessage().find('"', Begin + 1);
        myDirectory = Resp.GetMessage().substr(Begin + 1, End - Begin - 1);
    }
}


////////////////////////////////////////////////////////////
/// Get the directory returned in the response
////////////////////////////////////////////////////////////
const std::string& Ftp::DirectoryResponse::GetDirectory() const
{
    return myDirectory;
}


////////////////////////////////////////////////////////////
/// Default constructor
////////////////////////////////////////////////////////////
Ftp::ListingResponse::ListingResponse(Ftp::Response Resp, const std::vector<char>& Data) :
Ftp::Response(Resp)
{
    if (IsOk())
    {
        // Fill the array of strings
        std::string Paths(Data.begin(), Data.end());
        std::string::size_type LastPos = 0;
        for (std::string::size_type Pos = Paths.find("\r\n"); Pos != std::string::npos; Pos = Paths.find("\r\n", LastPos))
        {
            myFilenames.push_back(Paths.substr(LastPos, Pos - LastPos));
            LastPos = Pos + 2;
        }
    }
}


////////////////////////////////////////////////////////////
/// Get the number of filenames in the listing
////////////////////////////////////////////////////////////
std::size_t Ftp::ListingResponse::GetCount() const
{
    return myFilenames.size();
}


////////////////////////////////////////////////////////////
/// Get the Index-th filename in the directory
////////////////////////////////////////////////////////////
const std::string& Ftp::ListingResponse::GetFilename(std::size_t Index) const
{
    return myFilenames[Index];
}


////////////////////////////////////////////////////////////
/// Destructor -- close the connection with the server
////////////////////////////////////////////////////////////
Ftp::~Ftp()
{
    Disconnect();
}


////////////////////////////////////////////////////////////
/// Connect to the specified FTP server
////////////////////////////////////////////////////////////
Ftp::Response Ftp::Connect(const IPAddress& Server, unsigned short Port, float Timeout)
{
    // Connect to the server
    if (myCommandSocket.Connect(Port, Server, Timeout) != Socket::Done)
        return Response(Response::ConnectionFailed);

    // Get the response to the connection
    return GetResponse();
}


////////////////////////////////////////////////////////////
/// Log in using anonymous account
////////////////////////////////////////////////////////////
Ftp::Response Ftp::Login()
{
    return Login("anonymous", "user@sfml-dev.org");
}


////////////////////////////////////////////////////////////
/// Log in using a username and a password
////////////////////////////////////////////////////////////
Ftp::Response Ftp::Login(const std::string& UserName, const std::string& Password)
{
    Response Resp = SendCommand("USER", UserName);
    if (Resp.IsOk())
        Resp = SendCommand("PASS", Password);

    return Resp;
}


////////////////////////////////////////////////////////////
/// Close the connection with FTP server
////////////////////////////////////////////////////////////
Ftp::Response Ftp::Disconnect()
{
    // Send the exit command
    Response Resp = SendCommand("QUIT");
    if (Resp.IsOk())
        myCommandSocket.Close();

    return Resp;
}


////////////////////////////////////////////////////////////
/// Send a null command just to prevent from being disconnected
////////////////////////////////////////////////////////////
Ftp::Response Ftp::KeepAlive()
{
    return SendCommand("NOOP");
}


////////////////////////////////////////////////////////////
/// Get the current working directory
////////////////////////////////////////////////////////////
Ftp::DirectoryResponse Ftp::GetWorkingDirectory()
{
    return DirectoryResponse(SendCommand("PWD"));
}


////////////////////////////////////////////////////////////
/// Get the contents of the given directory
/// (subdirectories and files)
////////////////////////////////////////////////////////////
Ftp::ListingResponse Ftp::GetDirectoryListing(const std::string& Directory)
{
    // Open a data channel on default port (20) using ASCII transfer mode
    std::vector<char> DirData;
    DataChannel Data(*this);
    Response Resp = Data.Open(Ascii);
    if (Resp.IsOk())
    {
        // Tell the server to send us the listing
        Resp = SendCommand("NLST", Directory);
        if (Resp.IsOk())
        {
            // Receive the listing
            Data.Receive(DirData);

            // Get the response from the server
            Resp = GetResponse();
        }
    }

    return ListingResponse(Resp, DirData);
}


////////////////////////////////////////////////////////////
/// Change the current working directory
////////////////////////////////////////////////////////////
Ftp::Response Ftp::ChangeDirectory(const std::string& Directory)
{
    return SendCommand("CWD", Directory);
}


////////////////////////////////////////////////////////////
/// Go to the parent directory of the current one
////////////////////////////////////////////////////////////
Ftp::Response Ftp::ParentDirectory()
{
    return SendCommand("CDUP");
}


////////////////////////////////////////////////////////////
/// Create a new directory
////////////////////////////////////////////////////////////
Ftp::Response Ftp::MakeDirectory(const std::string& Name)
{
    return SendCommand("MKD", Name);
}


////////////////////////////////////////////////////////////
/// Remove an existing directory
////////////////////////////////////////////////////////////
Ftp::Response Ftp::DeleteDirectory(const std::string& Name)
{
    return SendCommand("RMD", Name);
}


////////////////////////////////////////////////////////////
/// Rename a file
////////////////////////////////////////////////////////////
Ftp::Response Ftp::RenameFile(const std::string& File, const std::string& NewName)
{
    Response Resp = SendCommand("RNFR", File);
    if (Resp.IsOk())
       Resp = SendCommand("RNTO", NewName);

    return Resp;
}


////////////////////////////////////////////////////////////
/// Remove an existing file
////////////////////////////////////////////////////////////
Ftp::Response Ftp::DeleteFile(const std::string& Name)
{
    return SendCommand("DELE", Name);
}


////////////////////////////////////////////////////////////
/// Download a file from the server
////////////////////////////////////////////////////////////
Ftp::Response Ftp::Download(const std::string& DistantFile, const std::string& DestPath, TransferMode Mode)
{
    // Open a data channel using the given transfer mode
    DataChannel Data(*this);
    Response Resp = Data.Open(Mode);
    if (Resp.IsOk())
    {
        // Tell the server to start the transfer
        Resp = SendCommand("RETR", DistantFile);
        if (Resp.IsOk())
        {
            // Receive the file data
            std::vector<char> FileData;
            Data.Receive(FileData);

            // Get the response from the server
            Resp = GetResponse();
            if (Resp.IsOk())
            {
                // Extract the filename from the file path
                std::string Filename = DistantFile;
                std::string::size_type Pos = Filename.find_last_of("/\\");
                if (Pos != std::string::npos)
                    Filename = Filename.substr(Pos + 1);

                // Make sure the destination path ends with a slash
                std::string Path = DestPath;
                if (!Path.empty() && (Path[Path.size() - 1] != '\\') && (Path[Path.size() - 1] != '/'))
                    Path += "/";

                // Create the file and copy the received data into it
                std::ofstream File((Path + Filename).c_str(), std::ios_base::binary);
                if (!File)
                    return Response(Response::InvalidFile);
                if (!FileData.empty())
                    File.write(&FileData[0], static_cast<std::streamsize>(FileData.size()));
            }
        }
    }

    return Resp;
}


////////////////////////////////////////////////////////////
/// Upload a file to the server
////////////////////////////////////////////////////////////
Ftp::Response Ftp::Upload(const std::string& LocalFile, const std::string& DestPath, TransferMode Mode)
{
    // Get the contents of the file to send
    std::ifstream File(LocalFile.c_str(), std::ios_base::binary);
    if (!File)
        return Response(Response::InvalidFile);
    File.seekg(0, std::ios::end);
    std::size_t Length = File.tellg();
    File.seekg(0, std::ios::beg);
    std::vector<char> FileData(Length);
    if (Length > 0)
        File.read(&FileData[0], static_cast<std::streamsize>(Length));

    // Extract the filename from the file path
    std::string Filename = LocalFile;
    std::string::size_type Pos = Filename.find_last_of("/\\");
    if (Pos != std::string::npos)
        Filename = Filename.substr(Pos + 1);

    // Make sure the destination path ends with a slash
    std::string Path = DestPath;
    if (!Path.empty() && (Path[Path.size() - 1] != '\\') && (Path[Path.size() - 1] != '/'))
        Path += "/";

    // Open a data channel using the given transfer mode
    DataChannel Data(*this);
    Response Resp = Data.Open(Mode);
    if (Resp.IsOk())
    {
        // Tell the server to start the transfer
        Resp = SendCommand("STOR", Path + Filename);
        if (Resp.IsOk())
        {
            // Send the file data
            Data.Send(FileData);

            // Get the response from the server
            Resp = GetResponse();
        }
    }

    return Resp;
}


////////////////////////////////////////////////////////////
/// Send a command to the FTP server
////////////////////////////////////////////////////////////
Ftp::Response Ftp::SendCommand(const std::string& Command, const std::string& Parameter)
{
    // Build the command string
    std::string CommandStr;
    if (Parameter != "")
        CommandStr = Command + " " + Parameter + "\r\n";
    else
        CommandStr = Command + "\r\n";

    // Send it to the server
    if (myCommandSocket.Send(CommandStr.c_str(), CommandStr.length()) != sf::Socket::Done)
        return Response(Response::ConnectionClosed);

    // Get the response
    return GetResponse();
}


////////////////////////////////////////////////////////////
/// Receive a response from the server
/// (usually after a command has been sent)
////////////////////////////////////////////////////////////
Ftp::Response Ftp::GetResponse()
{
    // We'll use a variable to keep track of the last valid code.
    // It is useful in case of multi-lines responses, because the end of such a response
    // will start by the same code
    unsigned int LastCode  = 0;
    bool IsInsideMultiline = false;
    std::string Message;

    for (;;)
    {
        // Receive the response from the server
        char Buffer[1024];
        std::size_t Length;
        if (myCommandSocket.Receive(Buffer, sizeof(Buffer), Length) != sf::Socket::Done)
            return Response(Response::ConnectionClosed);

        // There can be several lines inside the received buffer, extract them all
        std::istringstream In(std::string(Buffer, Length), std::ios_base::binary);
        while (In)
        {
            // Try to extract the code
            unsigned int Code;
            if (In >> Code)
            {
                // Extract the separator
                char Sep = 0;
                In.get(Sep);

                // The '-' character means a multiline response
                if ((Sep == '-') && !IsInsideMultiline)
                {
                    // Set the multiline flag
                    IsInsideMultiline = true;

                    // Keep track of the code
                    if (LastCode == 0)
                        LastCode = Code;

                    // Extract the line
                    std::getline(In, Message);

                    // Remove the ending '\r' (all lines are terminated by "\r\n")
                    Message.erase(Message.length() - 1);
                    Message = Sep + Message + "\n";
                }
                else
                {
                    // We must make sure that the code is the same, otherwise it means
                    // we haven't reached the end of the multiline response
                    if ((Sep != '-') && ((Code == LastCode) || (LastCode == 0)))
                    {
                        // Clear the multiline flag
                        IsInsideMultiline = false;

                        // Extract the line
                        std::string Line;
                        std::getline(In, Line);

                        // Remove the ending '\r' (all lines are terminated by "\r\n")
                        Line.erase(Line.length() - 1);

                        // Append it to the message
                        if (Code == LastCode)
                        {
                            std::ostringstream Out;
                            Out << Code << Sep << Line;
                            Message += Out.str();
                        }
                        else
                        {
                            Message = Sep + Line;
                        }

                        // Return the response code and message
                        return Response(static_cast<Response::Status>(Code), Message);
                    }
                    else
                    {
                        // The line we just read was actually not a response,
                        // only a new part of the current multiline response

                        // Extract the line
                        std::string Line;
                        std::getline(In, Line);

                        if (!Line.empty())
                        {
                            // Remove the ending '\r' (all lines are terminated by "\r\n")
                            Line.erase(Line.length() - 1);

                            // Append it to the current message
                            std::ostringstream Out;
                            Out << Code << Sep << Line << "\n";
                            Message += Out.str();
                        }
                    }
                }
            }
            else if (LastCode != 0)
            {
                // It seems we are in the middle of a multiline response

                // Clear the error bits of the stream
                In.clear();

                // Extract the line
                std::string Line;
                std::getline(In, Line);

                if (!Line.empty())
                {
                    // Remove the ending '\r' (all lines are terminated by "\r\n")
                    Line.erase(Line.length() - 1);

                    // Append it to the current message
                    Message += Line + "\n";
                }
            }
            else
            {
                // Error : cannot extract the code, and we are not in a multiline response
                return Response(Response::InvalidResponse);
            }
        }
    }

    // We never reach there
}


////////////////////////////////////////////////////////////
/// Constructor
////////////////////////////////////////////////////////////
Ftp::DataChannel::DataChannel(Ftp& Owner) :
myFtp(Owner)
{

}


////////////////////////////////////////////////////////////
/// Destructor
////////////////////////////////////////////////////////////
Ftp::DataChannel::~DataChannel()
{
    // Close the data socket
    myDataSocket.Close();
}


////////////////////////////////////////////////////////////
/// Open the data channel using the specified mode and port
////////////////////////////////////////////////////////////
Ftp::Response Ftp::DataChannel::Open(Ftp::TransferMode Mode)
{
    // Open a data connection in active mode (we connect to the server)
    Ftp::Response Resp = myFtp.SendCommand("PASV");
    if (Resp.IsOk())
    {
        // Extract the connection address and port from the response
        std::string::size_type begin = Resp.GetMessage().find_first_of("0123456789");
        if (begin != std::string::npos)
        {
            sf::Uint8 Data[6] = {0, 0, 0, 0, 0, 0};
            std::string Str = Resp.GetMessage().substr(begin);
            std::size_t Index = 0;
            for (int i = 0; i < 6; ++i)
            {
                // Extract the current number
                while (isdigit(Str[Index]))
                {
                    Data[i] = Data[i] * 10 + (Str[Index] - '0');
                    Index++;
                }

                // Skip separator
                Index++;
            }

            // Reconstruct connection port and address
            unsigned short Port = Data[4] * 256 + Data[5];
            sf::IPAddress Address(static_cast<sf::Uint8>(Data[0]),
                                  static_cast<sf::Uint8>(Data[1]),
                                  static_cast<sf::Uint8>(Data[2]),
                                  static_cast<sf::Uint8>(Data[3]));

            // Connect the data channel to the server
            if (myDataSocket.Connect(Port, Address) == Socket::Done)
            {
                // Translate the transfer mode to the corresponding FTP parameter
                std::string ModeStr;
                switch (Mode)
                {
                    case Ftp::Binary : ModeStr = "I"; break;
                    case Ftp::Ascii :  ModeStr = "A"; break;
                    case Ftp::Ebcdic : ModeStr = "E"; break;
                }

                // Set the transfer mode
                Resp = myFtp.SendCommand("TYPE", ModeStr);
            }
            else
            {
                // Failed to connect to the server
                Resp = Ftp::Response(Ftp::Response::ConnectionFailed);
            }
        }
    }

    return Resp;
}


////////////////////////////////////////////////////////////
/// Receive data on the data channel until it is closed
////////////////////////////////////////////////////////////
void Ftp::DataChannel::Receive(std::vector<char>& Data)
{
    // Receive data
    Data.clear();
    char Buffer[1024];
    std::size_t Received;
    while (myDataSocket.Receive(Buffer, sizeof(Buffer), Received) == sf::Socket::Done)
    {
        std::copy(Buffer, Buffer + Received, std::back_inserter(Data));
    }

    // Close the data socket
    myDataSocket.Close();
}


////////////////////////////////////////////////////////////
/// Send data on the data channel
////////////////////////////////////////////////////////////
void Ftp::DataChannel::Send(const std::vector<char>& Data)
{
    // Send data
    if (!Data.empty())
        myDataSocket.Send(&Data[0], Data.size());

    // Close the data socket
    myDataSocket.Close();
}

} // namespace sf
