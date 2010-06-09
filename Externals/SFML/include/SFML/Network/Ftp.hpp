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

#ifndef SFML_FTP_HPP
#define SFML_FTP_HPP

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/System/NonCopyable.hpp>
#include <SFML/Network/SocketTCP.hpp>
#include <string>
#include <vector>


namespace sf
{
class IPAddress;

////////////////////////////////////////////////////////////
/// This class provides methods for manipulating the FTP
/// protocol (described in RFC 959).
/// It provides easy access and transfers to remote
/// directories and files on a FTP server
////////////////////////////////////////////////////////////
class SFML_API Ftp : NonCopyable
{
public :

    ////////////////////////////////////////////////////////////
    /// Enumeration of transfer modes
    ////////////////////////////////////////////////////////////
    enum TransferMode
    {
        Binary, ///< Binary mode (file is transfered as a sequence of bytes)
        Ascii,  ///< Text mode using ASCII encoding
        Ebcdic  ///< Text mode using EBCDIC encoding
    };

    ////////////////////////////////////////////////////////////
    /// This class wraps a FTP response, which is basically :
    /// - a status code
    /// - a message
    ////////////////////////////////////////////////////////////
    class SFML_API Response
    {
    public :

        ////////////////////////////////////////////////////////////
        /// Enumerate all the valid status codes returned in
        /// a FTP response
        ////////////////////////////////////////////////////////////
        enum Status
        {
            // 1xx: the requested action is being initiated,
            // expect another reply before proceeding with a new command
            RestartMarkerReply          = 110, ///< Restart marker reply
            ServiceReadySoon            = 120, ///< Service ready in N minutes
            DataConnectionAlreadyOpened = 125, ///< Data connection already opened, transfer starting
            OpeningDataConnection       = 150, ///< File status ok, about to open data connection

            // 2xx: the requested action has been successfully completed
            Ok                    = 200, ///< Command ok
            PointlessCommand      = 202, ///< Command not implemented
            SystemStatus          = 211, ///< System status, or system help reply
            DirectoryStatus       = 212, ///< Directory status
            FileStatus            = 213, ///< File status
            HelpMessage           = 214, ///< Help message
            SystemType            = 215, ///< NAME system type, where NAME is an official system name from the list in the Assigned Numbers document
            ServiceReady          = 220, ///< Service ready for new user
            ClosingConnection     = 221, ///< Service closing control connection
            DataConnectionOpened  = 225, ///< Data connection open, no transfer in progress
            ClosingDataConnection = 226, ///< Closing data connection, requested file action successful
            EnteringPassiveMode   = 227, ///< Entering passive mode
            LoggedIn              = 230, ///< User logged in, proceed. Logged out if appropriate
            FileActionOk          = 250, ///< Requested file action ok
            DirectoryOk           = 257, ///< PATHNAME created

            // 3xx: the command has been accepted, but the requested action
            // is dormant, pending receipt of further information
            NeedPassword       = 331, ///< User name ok, need password
            NeedAccountToLogIn = 332, ///< Need account for login
            NeedInformation    = 350, ///< Requested file action pending further information

            // 4xx: the command was not accepted and the requested action did not take place,
            // but the error condition is temporary and the action may be requested again
            ServiceUnavailable        = 421, ///< Service not available, closing control connection
            DataConnectionUnavailable = 425, ///< Can't open data connection
            TransferAborted           = 426, ///< Connection closed, transfer aborted
            FileActionAborted         = 450, ///< Requested file action not taken
            LocalError                = 451, ///< Requested action aborted, local error in processing
            InsufficientStorageSpace  = 452, ///< Requested action not taken; insufficient storage space in system, file unavailable

            // 5xx: the command was not accepted and
            // the requested action did not take place
            CommandUnknown          = 500, ///< Syntax error, command unrecognized
            ParametersUnknown       = 501, ///< Syntax error in parameters or arguments
            CommandNotImplemented   = 502, ///< Command not implemented
            BadCommandSequence      = 503, ///< Bad sequence of commands
            ParameterNotImplemented = 504, ///< Command not implemented for that parameter
            NotLoggedIn             = 530, ///< Not logged in
            NeedAccountToStore      = 532, ///< Need account for storing files
            FileUnavailable         = 550, ///< Requested action not taken, file unavailable
            PageTypeUnknown         = 551, ///< Requested action aborted, page type unknown
            NotEnoughMemory         = 552, ///< Requested file action aborted, exceeded storage allocation
            FilenameNotAllowed      = 553, ///< Requested action not taken, file name not allowed

            // 10xx: SFML custom codes
            InvalidResponse  = 1000, ///< Response is not a valid FTP one
            ConnectionFailed = 1001, ///< Connection with server failed
            ConnectionClosed = 1002, ///< Connection with server closed
            InvalidFile      = 1003  ///< Invalid file to upload / download
        };

        ////////////////////////////////////////////////////////////
        /// Default constructor
        ///
        /// \param Code :    Response status code (InvalidResponse by default)
        /// \param Message : Response message (empty by default)
        ///
        ////////////////////////////////////////////////////////////
        Response(Status Code = InvalidResponse, const std::string& Message = "");

        ////////////////////////////////////////////////////////////
        /// Convenience function to check if the response status code
        /// means a success
        ///
        /// \return True if status is success (code < 400)
        ///
        ////////////////////////////////////////////////////////////
        bool IsOk() const;

        ////////////////////////////////////////////////////////////
        /// Get the response status code
        ///
        /// \return Status code
        ///
        ////////////////////////////////////////////////////////////
        Status GetStatus() const;

        ////////////////////////////////////////////////////////////
        /// Get the full message contained in the response
        ///
        /// \return The response message
        ///
        ////////////////////////////////////////////////////////////
        const std::string& GetMessage() const;

    private :

        ////////////////////////////////////////////////////////////
        // Member data
        ////////////////////////////////////////////////////////////
        Status      myStatus;  ///< Status code returned from the server
        std::string myMessage; ///< Last message received from the server
    };

    ////////////////////////////////////////////////////////////
    /// Specialization of FTP response returning a directory
    ////////////////////////////////////////////////////////////
    class SFML_API DirectoryResponse : public Response
    {
    public :

        ////////////////////////////////////////////////////////////
        /// Default constructor
        ///
        /// \param Resp : Source response
        ///
        ////////////////////////////////////////////////////////////
        DirectoryResponse(Response Resp);

        ////////////////////////////////////////////////////////////
        /// Get the directory returned in the response
        ///
        /// \return Directory name
        ///
        ////////////////////////////////////////////////////////////
        const std::string& GetDirectory() const;

    private :

        ////////////////////////////////////////////////////////////
        // Member data
        ////////////////////////////////////////////////////////////
        std::string myDirectory; ///< Directory extracted from the response message
    };


    ////////////////////////////////////////////////////////////
    /// Specialization of FTP response returning a filename lisiting
    ////////////////////////////////////////////////////////////
    class SFML_API ListingResponse : public Response
    {
    public :

        ////////////////////////////////////////////////////////////
        /// Default constructor
        ///
        /// \param Resp : Source response
        /// \param Data : Data containing the raw listing
        ///
        ////////////////////////////////////////////////////////////
        ListingResponse(Response Resp, const std::vector<char>& Data);

        ////////////////////////////////////////////////////////////
        /// Get the number of filenames in the listing
        ///
        /// \return Total number of filenames
        ///
        ////////////////////////////////////////////////////////////
        std::size_t GetCount() const;

        ////////////////////////////////////////////////////////////
        /// Get the Index-th filename in the directory
        ///
        /// \param Index : Index of the filename to get
        ///
        /// \return Index-th filename
        ///
        ////////////////////////////////////////////////////////////
        const std::string& GetFilename(std::size_t Index) const;

    private :

        ////////////////////////////////////////////////////////////
        // Member data
        ////////////////////////////////////////////////////////////
        std::vector<std::string> myFilenames; ///< Filenames extracted from the data
    };


    ////////////////////////////////////////////////////////////
    /// Destructor -- close the connection with the server
    ///
    ////////////////////////////////////////////////////////////
    ~Ftp();

    ////////////////////////////////////////////////////////////
    /// Connect to the specified FTP server
    ///
    /// \param Server :  FTP server to connect to
    /// \param Port :    Port used for connection (21 by default, standard FTP port)
    /// \param Timeout : Maximum time to wait, in seconds (0 by default, means no timeout)
    ///
    /// \return Server response to the request
    ///
    ////////////////////////////////////////////////////////////
    Response Connect(const IPAddress& Server, unsigned short Port = 21, float Timeout = 0.f);

    ////////////////////////////////////////////////////////////
    /// Log in using anonymous account
    ///
    /// \return Server response to the request
    ///
    ////////////////////////////////////////////////////////////
    Response Login();

    ////////////////////////////////////////////////////////////
    /// Log in using a username and a password
    ///
    /// \param UserName : User name
    /// \param Password : Password
    ///
    /// \return Server response to the request
    ///
    ////////////////////////////////////////////////////////////
    Response Login(const std::string& UserName, const std::string& Password);

    ////////////////////////////////////////////////////////////
    /// Close the connection with FTP server
    ///
    /// \return Server response to the request
    ///
    ////////////////////////////////////////////////////////////
    Response Disconnect();

    ////////////////////////////////////////////////////////////
    /// Send a null command just to prevent from being disconnected
    ///
    /// \return Server response to the request
    ///
    ////////////////////////////////////////////////////////////
    Response KeepAlive();

    ////////////////////////////////////////////////////////////
    /// Get the current working directory
    ///
    /// \return Server response to the request
    ///
    ////////////////////////////////////////////////////////////
    DirectoryResponse GetWorkingDirectory();

    ////////////////////////////////////////////////////////////
    /// Get the contents of the given directory
    /// (subdirectories and files)
    ///
    /// \param Directory : Directory to list ("" by default, the current one)
    ///
    /// \return Server response to the request
    ///
    ////////////////////////////////////////////////////////////
    ListingResponse GetDirectoryListing(const std::string& Directory = "");

    ////////////////////////////////////////////////////////////
    /// Change the current working directory
    ///
    /// \param Directory : New directory, relative to the current one
    ///
    /// \return Server response to the request
    ///
    ////////////////////////////////////////////////////////////
    Response ChangeDirectory(const std::string& Directory);

    ////////////////////////////////////////////////////////////
    /// Go to the parent directory of the current one
    ///
    /// \return Server response to the request
    ///
    ////////////////////////////////////////////////////////////
    Response ParentDirectory();

    ////////////////////////////////////////////////////////////
    /// Create a new directory
    ///
    /// \param Name : Name of the directory to create
    ///
    /// \return Server response to the request
    ///
    ////////////////////////////////////////////////////////////
    Response MakeDirectory(const std::string& Name);

    ////////////////////////////////////////////////////////////
    /// Remove an existing directory
    ///
    /// \param Name : Name of the directory to remove
    ///
    /// \return Server response to the request
    ///
    ////////////////////////////////////////////////////////////
    Response DeleteDirectory(const std::string& Name);

    ////////////////////////////////////////////////////////////
    /// Rename a file
    ///
    /// \param File :    File to rename
    /// \param NewName : New name
    ///
    /// \return Server response to the request
    ///
    ////////////////////////////////////////////////////////////
    Response RenameFile(const std::string& File, const std::string& NewName);

    ////////////////////////////////////////////////////////////
    /// Remove an existing file
    ///
    /// \param Name : File to remove
    ///
    /// \return Server response to the request
    ///
    ////////////////////////////////////////////////////////////
    Response DeleteFile(const std::string& Name);

    ////////////////////////////////////////////////////////////
    /// Download a file from the server
    ///
    /// \param DistantFile : Path of the distant file to download
    /// \param DestPath :    Where to put to file on the local computer
    /// \param Mode :        Transfer mode (binary by default)
    ///
    /// \return Server response to the request
    ///
    ////////////////////////////////////////////////////////////
    Response Download(const std::string& DistantFile, const std::string& DestPath, TransferMode Mode = Binary);

    ////////////////////////////////////////////////////////////
    /// Upload a file to the server
    ///
    /// \param LocalFile : Path of the local file to upload
    /// \param DestPath :  Where to put to file on the server
    /// \param Mode :      Transfer mode (binary by default)
    ///
    /// \return Server response to the request
    ///
    ////////////////////////////////////////////////////////////
    Response Upload(const std::string& LocalFile, const std::string& DestPath, TransferMode Mode = Binary);

private :

    ////////////////////////////////////////////////////////////
    /// Send a command to the FTP server
    ///
    /// \param Command :   Command to send
    /// \param Parameter : Command parameter ("" by default)
    ///
    /// \return Server response to the request
    ///
    ////////////////////////////////////////////////////////////
    Response SendCommand(const std::string& Command, const std::string& Parameter = "");

    ////////////////////////////////////////////////////////////
    /// Receive a response from the server
    /// (usually after a command has been sent)
    ///
    /// \return Server response to the request
    ///
    ////////////////////////////////////////////////////////////
    Response GetResponse();

    ////////////////////////////////////////////////////////////
    /// Utility class for exchanging datas with the server
    /// on the data channel
    ////////////////////////////////////////////////////////////
    class DataChannel;

    friend class DataChannel;

    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    SocketTCP myCommandSocket; ///< Socket holding the control connection with the server
};

} // namespace sf


#endif // SFML_FTP_HPP
