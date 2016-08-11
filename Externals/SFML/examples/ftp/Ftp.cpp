
////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Network.hpp>
#include <fstream>
#include <iostream>


////////////////////////////////////////////////////////////
/// Print a FTP response into a standard output stream
///
////////////////////////////////////////////////////////////
std::ostream& operator <<(std::ostream& stream, const sf::Ftp::Response& response)
{
    return stream << response.getStatus() << response.getMessage();
}


////////////////////////////////////////////////////////////
/// Entry point of application
///
/// \return Application exit code
///
////////////////////////////////////////////////////////////
int main()
{
    // Choose the server address
    sf::IpAddress address;
    do
    {
        std::cout << "Enter the FTP server address: ";
        std::cin  >> address;
    }
    while (address == sf::IpAddress::None);

    // Connect to the server
    sf::Ftp server;
    sf::Ftp::Response connectResponse = server.connect(address);
    std::cout << connectResponse << std::endl;
    if (!connectResponse.isOk())
        return EXIT_FAILURE;

    // Ask for user name and password
    std::string user, password;
    std::cout << "User name: ";
    std::cin  >> user;
    std::cout << "Password: ";
    std::cin  >> password;

    // Login to the server
    sf::Ftp::Response loginResponse = server.login(user, password);
    std::cout << loginResponse << std::endl;
    if (!loginResponse.isOk())
        return EXIT_FAILURE;

    // Main menu
    int choice = 0;
    do
    {
        // Main FTP menu
        std::cout << std::endl;
        std::cout << "Choose an action:"                      << std::endl;
        std::cout << "1. Print working directory"             << std::endl;
        std::cout << "2. Print contents of working directory" << std::endl;
        std::cout << "3. Change directory"                    << std::endl;
        std::cout << "4. Create directory"                    << std::endl;
        std::cout << "5. Delete directory"                    << std::endl;
        std::cout << "6. Rename file"                         << std::endl;
        std::cout << "7. Remove file"                         << std::endl;
        std::cout << "8. Download file"                       << std::endl;
        std::cout << "9. Upload file"                         << std::endl;
        std::cout << "0. Disconnect"                          << std::endl;
        std::cout << std::endl;

        std::cout << "Your choice: ";
        std::cin  >> choice;
        std::cout << std::endl;

        switch (choice)
        {
            default:
            {
                // Wrong choice
                std::cout << "Invalid choice!" << std::endl;
                std::cin.clear();
                std::cin.ignore(10000, '\n');
                break;
            }

            case 1:
            {
                // Print the current server directory
                sf::Ftp::DirectoryResponse response = server.getWorkingDirectory();
                std::cout << response << std::endl;
                std::cout << "Current directory is " << response.getDirectory() << std::endl;
                break;
            }

            case 2:
            {
                // Print the contents of the current server directory
                sf::Ftp::ListingResponse response = server.getDirectoryListing();
                std::cout << response << std::endl;
                const std::vector<std::string>& names = response.getListing();
                for (std::vector<std::string>::const_iterator it = names.begin(); it != names.end(); ++it)
                    std::cout << *it << std::endl;
                break;
            }

            case 3:
            {
                // Change the current directory
                std::string directory;
                std::cout << "Choose a directory: ";
                std::cin  >> directory;
                std::cout << server.changeDirectory(directory) << std::endl;
                break;
            }

            case 4:
            {
                // Create a new directory
                std::string directory;
                std::cout << "Name of the directory to create: ";
                std::cin  >> directory;
                std::cout << server.createDirectory(directory) << std::endl;
                break;
            }

            case 5:
            {
                // Remove an existing directory
                std::string directory;
                std::cout << "Name of the directory to remove: ";
                std::cin  >> directory;
                std::cout << server.deleteDirectory(directory) << std::endl;
                break;
            }

            case 6:
            {
                // Rename a file
                std::string source, destination;
                std::cout << "Name of the file to rename: ";
                std::cin  >> source;
                std::cout << "New name: ";
                std::cin  >> destination;
                std::cout << server.renameFile(source, destination) << std::endl;
                break;
            }

            case 7:
            {
                // Remove an existing directory
                std::string filename;
                std::cout << "Name of the file to remove: ";
                std::cin  >> filename;
                std::cout << server.deleteFile(filename) << std::endl;
                break;
            }

            case 8:
            {
                // Download a file from server
                std::string filename, directory;
                std::cout << "Filename of the file to download (relative to current directory): ";
                std::cin  >> filename;
                std::cout << "Directory to download the file to: ";
                std::cin  >> directory;
                std::cout << server.download(filename, directory) << std::endl;
                break;
            }

            case 9:
            {
                // Upload a file to server
                std::string filename, directory;
                std::cout << "Path of the file to upload (absolute or relative to working directory): ";
                std::cin  >> filename;
                std::cout << "Directory to upload the file to (relative to current directory): ";
                std::cin  >> directory;
                std::cout << server.upload(filename, directory) << std::endl;
                break;
            }

            case 0:
            {
                // Disconnect
                break;
            }
        }

    } while (choice != 0);

    // Disconnect from the server
    std::cout << "Disconnecting from server..." << std::endl;
    std::cout << server.disconnect() << std::endl;

    // Wait until the user presses 'enter' key
    std::cout << "Press enter to exit..." << std::endl;
    std::cin.ignore(10000, '\n');
    std::cin.ignore(10000, '\n');

    return EXIT_SUCCESS;
}
