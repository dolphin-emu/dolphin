////////////////////////////////////////////////////////////
//
// SFML - Simple and Fast Multimedia Library
// Copyright (C) 2007-2016 Laurent Gomila (laurent@sfml-dev.org)
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
#include <SFML/Network/Export.hpp>
#include <SFML/Network/TcpSocket.hpp>
#include <SFML/System/NonCopyable.hpp>
#include <SFML/System/Time.hpp>
#include <string>
#include <vector>


namespace sf
{
class IpAddress;

////////////////////////////////////////////////////////////
/// \brief A FTP client
///
////////////////////////////////////////////////////////////
class SFML_NETWORK_API Ftp : NonCopyable
{
public:

    ////////////////////////////////////////////////////////////
    /// \brief Enumeration of transfer modes
    ///
    ////////////////////////////////////////////////////////////
    enum TransferMode
    {
        Binary, ///< Binary mode (file is transfered as a sequence of bytes)
        Ascii,  ///< Text mode using ASCII encoding
        Ebcdic  ///< Text mode using EBCDIC encoding
    };

    ////////////////////////////////////////////////////////////
    /// \brief Define a FTP response
    ///
    ////////////////////////////////////////////////////////////
    class SFML_NETWORK_API Response
    {
    public:

        ////////////////////////////////////////////////////////////
        /// \brief Status codes possibly returned by a FTP response
        ///
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
            InvalidResponse  = 1000, ///< Not part of the FTP standard, generated by SFML when a received response cannot be parsed
            ConnectionFailed = 1001, ///< Not part of the FTP standard, generated by SFML when the low-level socket connection with the server fails
            ConnectionClosed = 1002, ///< Not part of the FTP standard, generated by SFML when the low-level socket connection is unexpectedly closed
            InvalidFile      = 1003  ///< Not part of the FTP standard, generated by SFML when a local file cannot be read or written
        };

        ////////////////////////////////////////////////////////////
        /// \brief Default constructor
        ///
        /// This constructor is used by the FTP client to build
        /// the response.
        ///
        /// \param code    Response status code
        /// \param message Response message
        ///
        ////////////////////////////////////////////////////////////
        explicit Response(Status code = InvalidResponse, const std::string& message = "");

        ////////////////////////////////////////////////////////////
        /// \brief Check if the status code means a success
        ///
        /// This function is defined for convenience, it is
        /// equivalent to testing if the status code is < 400.
        ///
        /// \return True if the status is a success, false if it is a failure
        ///
        ////////////////////////////////////////////////////////////
        bool isOk() const;

        ////////////////////////////////////////////////////////////
        /// \brief Get the status code of the response
        ///
        /// \return Status code
        ///
        ////////////////////////////////////////////////////////////
        Status getStatus() const;

        ////////////////////////////////////////////////////////////
        /// \brief Get the full message contained in the response
        ///
        /// \return The response message
        ///
        ////////////////////////////////////////////////////////////
        const std::string& getMessage() const;

    private:

        ////////////////////////////////////////////////////////////
        // Member data
        ////////////////////////////////////////////////////////////
        Status      m_status;  ///< Status code returned from the server
        std::string m_message; ///< Last message received from the server
    };

    ////////////////////////////////////////////////////////////
    /// \brief Specialization of FTP response returning a directory
    ///
    ////////////////////////////////////////////////////////////
    class SFML_NETWORK_API DirectoryResponse : public Response
    {
    public:

        ////////////////////////////////////////////////////////////
        /// \brief Default constructor
        ///
        /// \param response Source response
        ///
        ////////////////////////////////////////////////////////////
        DirectoryResponse(const Response& response);

        ////////////////////////////////////////////////////////////
        /// \brief Get the directory returned in the response
        ///
        /// \return Directory name
        ///
        ////////////////////////////////////////////////////////////
        const std::string& getDirectory() const;

    private:

        ////////////////////////////////////////////////////////////
        // Member data
        ////////////////////////////////////////////////////////////
        std::string m_directory; ///< Directory extracted from the response message
    };


    ////////////////////////////////////////////////////////////
    /// \brief Specialization of FTP response returning a
    ///        filename listing
    ////////////////////////////////////////////////////////////
    class SFML_NETWORK_API ListingResponse : public Response
    {
    public:

        ////////////////////////////////////////////////////////////
        /// \brief Default constructor
        ///
        /// \param response  Source response
        /// \param data      Data containing the raw listing
        ///
        ////////////////////////////////////////////////////////////
        ListingResponse(const Response& response, const std::string& data);

        ////////////////////////////////////////////////////////////
        /// \brief Return the array of directory/file names
        ///
        /// \return Array containing the requested listing
        ///
        ////////////////////////////////////////////////////////////
        const std::vector<std::string>& getListing() const;

    private:

        ////////////////////////////////////////////////////////////
        // Member data
        ////////////////////////////////////////////////////////////
        std::vector<std::string> m_listing; ///< Directory/file names extracted from the data
    };


    ////////////////////////////////////////////////////////////
    /// \brief Destructor
    ///
    /// Automatically closes the connection with the server if
    /// it is still opened.
    ///
    ////////////////////////////////////////////////////////////
    ~Ftp();

    ////////////////////////////////////////////////////////////
    /// \brief Connect to the specified FTP server
    ///
    /// The port has a default value of 21, which is the standard
    /// port used by the FTP protocol. You shouldn't use a different
    /// value, unless you really know what you do.
    /// This function tries to connect to the server so it may take
    /// a while to complete, especially if the server is not
    /// reachable. To avoid blocking your application for too long,
    /// you can use a timeout. The default value, Time::Zero, means that the
    /// system timeout will be used (which is usually pretty long).
    ///
    /// \param server  Name or address of the FTP server to connect to
    /// \param port    Port used for the connection
    /// \param timeout Maximum time to wait
    ///
    /// \return Server response to the request
    ///
    /// \see disconnect
    ///
    ////////////////////////////////////////////////////////////
    Response connect(const IpAddress& server, unsigned short port = 21, Time timeout = Time::Zero);

    ////////////////////////////////////////////////////////////
    /// \brief Close the connection with the server
    ///
    /// \return Server response to the request
    ///
    /// \see connect
    ///
    ////////////////////////////////////////////////////////////
    Response disconnect();

    ////////////////////////////////////////////////////////////
    /// \brief Log in using an anonymous account
    ///
    /// Logging in is mandatory after connecting to the server.
    /// Users that are not logged in cannot perform any operation.
    ///
    /// \return Server response to the request
    ///
    ////////////////////////////////////////////////////////////
    Response login();

    ////////////////////////////////////////////////////////////
    /// \brief Log in using a username and a password
    ///
    /// Logging in is mandatory after connecting to the server.
    /// Users that are not logged in cannot perform any operation.
    ///
    /// \param name     User name
    /// \param password Password
    ///
    /// \return Server response to the request
    ///
    ////////////////////////////////////////////////////////////
    Response login(const std::string& name, const std::string& password);

    ////////////////////////////////////////////////////////////
    /// \brief Send a null command to keep the connection alive
    ///
    /// This command is useful because the server may close the
    /// connection automatically if no command is sent.
    ///
    /// \return Server response to the request
    ///
    ////////////////////////////////////////////////////////////
    Response keepAlive();

    ////////////////////////////////////////////////////////////
    /// \brief Get the current working directory
    ///
    /// The working directory is the root path for subsequent
    /// operations involving directories and/or filenames.
    ///
    /// \return Server response to the request
    ///
    /// \see getDirectoryListing, changeDirectory, parentDirectory
    ///
    ////////////////////////////////////////////////////////////
    DirectoryResponse getWorkingDirectory();

    ////////////////////////////////////////////////////////////
    /// \brief Get the contents of the given directory
    ///
    /// This function retrieves the sub-directories and files
    /// contained in the given directory. It is not recursive.
    /// The \a directory parameter is relative to the current
    /// working directory.
    ///
    /// \param directory Directory to list
    ///
    /// \return Server response to the request
    ///
    /// \see getWorkingDirectory, changeDirectory, parentDirectory
    ///
    ////////////////////////////////////////////////////////////
    ListingResponse getDirectoryListing(const std::string& directory = "");

    ////////////////////////////////////////////////////////////
    /// \brief Change the current working directory
    ///
    /// The new directory must be relative to the current one.
    ///
    /// \param directory New working directory
    ///
    /// \return Server response to the request
    ///
    /// \see getWorkingDirectory, getDirectoryListing, parentDirectory
    ///
    ////////////////////////////////////////////////////////////
    Response changeDirectory(const std::string& directory);

    ////////////////////////////////////////////////////////////
    /// \brief Go to the parent directory of the current one
    ///
    /// \return Server response to the request
    ///
    /// \see getWorkingDirectory, getDirectoryListing, changeDirectory
    ///
    ////////////////////////////////////////////////////////////
    Response parentDirectory();

    ////////////////////////////////////////////////////////////
    /// \brief Create a new directory
    ///
    /// The new directory is created as a child of the current
    /// working directory.
    ///
    /// \param name Name of the directory to create
    ///
    /// \return Server response to the request
    ///
    /// \see deleteDirectory
    ///
    ////////////////////////////////////////////////////////////
    Response createDirectory(const std::string& name);

    ////////////////////////////////////////////////////////////
    /// \brief Remove an existing directory
    ///
    /// The directory to remove must be relative to the
    /// current working directory.
    /// Use this function with caution, the directory will
    /// be removed permanently!
    ///
    /// \param name Name of the directory to remove
    ///
    /// \return Server response to the request
    ///
    /// \see createDirectory
    ///
    ////////////////////////////////////////////////////////////
    Response deleteDirectory(const std::string& name);

    ////////////////////////////////////////////////////////////
    /// \brief Rename an existing file
    ///
    /// The filenames must be relative to the current working
    /// directory.
    ///
    /// \param file    File to rename
    /// \param newName New name of the file
    ///
    /// \return Server response to the request
    ///
    /// \see deleteFile
    ///
    ////////////////////////////////////////////////////////////
    Response renameFile(const std::string& file, const std::string& newName);

    ////////////////////////////////////////////////////////////
    /// \brief Remove an existing file
    ///
    /// The file name must be relative to the current working
    /// directory.
    /// Use this function with caution, the file will be
    /// removed permanently!
    ///
    /// \param name File to remove
    ///
    /// \return Server response to the request
    ///
    /// \see renameFile
    ///
    ////////////////////////////////////////////////////////////
    Response deleteFile(const std::string& name);

    ////////////////////////////////////////////////////////////
    /// \brief Download a file from the server
    ///
    /// The filename of the distant file is relative to the
    /// current working directory of the server, and the local
    /// destination path is relative to the current directory
    /// of your application.
    /// If a file with the same filename as the distant file
    /// already exists in the local destination path, it will
    /// be overwritten.
    ///
    /// \param remoteFile Filename of the distant file to download
    /// \param localPath  The directory in which to put the file on the local computer
    /// \param mode       Transfer mode
    ///
    /// \return Server response to the request
    ///
    /// \see upload
    ///
    ////////////////////////////////////////////////////////////
    Response download(const std::string& remoteFile, const std::string& localPath, TransferMode mode = Binary);

    ////////////////////////////////////////////////////////////
    /// \brief Upload a file to the server
    ///
    /// The name of the local file is relative to the current
    /// working directory of your application, and the
    /// remote path is relative to the current directory of the
    /// FTP server.
    ///
    /// \param localFile  Path of the local file to upload
    /// \param remotePath The directory in which to put the file on the server
    /// \param mode       Transfer mode
    ///
    /// \return Server response to the request
    ///
    /// \see download
    ///
    ////////////////////////////////////////////////////////////
    Response upload(const std::string& localFile, const std::string& remotePath, TransferMode mode = Binary);

    ////////////////////////////////////////////////////////////
    /// \brief Send a command to the FTP server
    ///
    /// While the most often used commands are provided as member
    /// functions in the sf::Ftp class, this method can be used
    /// to send any FTP command to the server. If the command
    /// requires one or more parameters, they can be specified
    /// in \a parameter. If the server returns information, you
    /// can extract it from the response using Response::getMessage().
    ///
    /// \param command   Command to send
    /// \param parameter Command parameter
    ///
    /// \return Server response to the request
    ///
    ////////////////////////////////////////////////////////////
    Response sendCommand(const std::string& command, const std::string& parameter = "");

private:

    ////////////////////////////////////////////////////////////
    /// \brief Receive a response from the server
    ///
    /// This function must be called after each call to
    /// sendCommand that expects a response.
    ///
    /// \return Server response to the request
    ///
    ////////////////////////////////////////////////////////////
    Response getResponse();

    ////////////////////////////////////////////////////////////
    /// \brief Utility class for exchanging datas with the server
    ///        on the data channel
    ///
    ////////////////////////////////////////////////////////////
    class DataChannel;

    friend class DataChannel;

    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    TcpSocket   m_commandSocket; ///< Socket holding the control connection with the server
    std::string m_receiveBuffer; ///< Received command data that is yet to be processed
};

} // namespace sf


#endif // SFML_FTP_HPP


////////////////////////////////////////////////////////////
/// \class sf::Ftp
/// \ingroup network
///
/// sf::Ftp is a very simple FTP client that allows you
/// to communicate with a FTP server. The FTP protocol allows
/// you to manipulate a remote file system (list files,
/// upload, download, create, remove, ...).
///
/// Using the FTP client consists of 4 parts:
/// \li Connecting to the FTP server
/// \li Logging in (either as a registered user or anonymously)
/// \li Sending commands to the server
/// \li Disconnecting (this part can be done implicitly by the destructor)
///
/// Every command returns a FTP response, which contains the
/// status code as well as a message from the server. Some
/// commands such as getWorkingDirectory() and getDirectoryListing()
/// return additional data, and use a class derived from
/// sf::Ftp::Response to provide this data. The most often used
/// commands are directly provided as member functions, but it is
/// also possible to use specific commands with the sendCommand() function.
///
/// Note that response statuses >= 1000 are not part of the FTP standard,
/// they are generated by SFML when an internal error occurs.
///
/// All commands, especially upload and download, may take some
/// time to complete. This is important to know if you don't want
/// to block your application while the server is completing
/// the task.
///
/// Usage example:
/// \code
/// // Create a new FTP client
/// sf::Ftp ftp;
///
/// // Connect to the server
/// sf::Ftp::Response response = ftp.connect("ftp://ftp.myserver.com");
/// if (response.isOk())
///     std::cout << "Connected" << std::endl;
///
/// // Log in
/// response = ftp.login("laurent", "dF6Zm89D");
/// if (response.isOk())
///     std::cout << "Logged in" << std::endl;
///
/// // Print the working directory
/// sf::Ftp::DirectoryResponse directory = ftp.getWorkingDirectory();
/// if (directory.isOk())
///     std::cout << "Working directory: " << directory.getDirectory() << std::endl;
///
/// // Create a new directory
/// response = ftp.createDirectory("files");
/// if (response.isOk())
///     std::cout << "Created new directory" << std::endl;
///
/// // Upload a file to this new directory
/// response = ftp.upload("local-path/file.txt", "files", sf::Ftp::Ascii);
/// if (response.isOk())
///     std::cout << "File uploaded" << std::endl;
///
/// // Send specific commands (here: FEAT to list supported FTP features)
/// response = ftp.sendCommand("FEAT");
/// if (response.isOk())
///     std::cout << "Feature list:\n" << response.getMessage() << std::endl;
///
/// // Disconnect from the server (optional)
/// ftp.disconnect();
/// \endcode
///
////////////////////////////////////////////////////////////
