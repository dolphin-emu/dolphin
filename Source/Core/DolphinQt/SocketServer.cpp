
#include "SocketServer.h"
#pragma once
#include "Core/HW/DVD/DVDInterface.h"
#include "Core/Core.h"
#include <curl/curl.h>
namespace SocketServer
{
#undef UNICODE

#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>
#include <string>
  #include <compare>
  
#include <ws2tcpip.h>

// Need to link with Ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

// #pragma comment (lib, "Mswsock.lib")

#define DEFAULT_BUFLEN 5120
#define DEFAULT_PORT 27019



  int __cdecl main(void)
  {
    CURL* curl = curl_easy_init();

    wchar_t str[DEFAULT_BUFLEN];
    WSADATA wsaData;
    int iResult;

    SOCKET ListenSocket = INVALID_SOCKET;
    SOCKET ClientSocket = INVALID_SOCKET;

    struct addrinfo* result = NULL;
    struct addrinfo hints;

    int iSendResult;
    char recvbuf[DEFAULT_BUFLEN];
    int recvbuflen = DEFAULT_BUFLEN;

    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0)
    {
      wsprintf(str, L"WSAStartup failed with error: %d\n", iResult);
      OutputDebugString(str);
      return 1;
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    sockaddr_in service;
    service.sin_family = AF_INET;
    service.sin_addr.s_addr = inet_addr("0.0.0.0");
    service.sin_port = htons(DEFAULT_PORT);

    // Create a SOCKET for connecting to server
    ListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ListenSocket == INVALID_SOCKET)
    {
      
      wsprintf(str, L"socket failed with error: %ld\n", WSAGetLastError());
      OutputDebugString(str);
      freeaddrinfo(result);
      WSACleanup();
      return 1;
    }

    // Setup the TCP listening socket
    iResult = bind(ListenSocket, (SOCKADDR*)&service, sizeof(service));
    if (iResult == SOCKET_ERROR)
    {
      wsprintf(str, L"bind failed with error: %d\n", WSAGetLastError());
      OutputDebugString(str);
      freeaddrinfo(result);
      closesocket(ListenSocket);
      WSACleanup();
      return 1;
    }

    freeaddrinfo(result);

    iResult = listen(ListenSocket, SOMAXCONN);
    if (iResult == SOCKET_ERROR)
    {
      wsprintf(str, L"listen failed with error: %d\n", WSAGetLastError());
      OutputDebugString(str);
      closesocket(ListenSocket);
      WSACleanup();
      return 1;
    }
    do
    {
    // Accept a client socket
    ClientSocket = accept(ListenSocket, NULL, NULL);
    if (ClientSocket == INVALID_SOCKET)
    {
      wsprintf(str, L"accept failed with error: %d\n", WSAGetLastError());
      OutputDebugString(str);
      closesocket(ListenSocket);
      WSACleanup();
      return 1;
    }

    // No longer need server socket
    //closesocket(ListenSocket);

    // Receive until the peer shuts down the connection
    
      iResult = recv(ClientSocket, recvbuf, recvbuflen, 0);
      if (iResult > 0)
      {
       

        wsprintf(str, L"Bytes received: %d\n", iResult);
        OutputDebugString(str);

        std::string request_string(recvbuf);




        const WCHAR* out;
        int nChars = MultiByteToWideChar(CP_ACP, 0, recvbuf, -1, NULL, 0);
        // allocate it
        out = new WCHAR[nChars];
        MultiByteToWideChar(CP_ACP, 0, recvbuf, -1, (LPWSTR)out, nChars);

      OutputDebugString(out);

        // Echo the buffer back to the sender
      const char* unknown_response = "HTTP/1.1 200 OK\r\n"
                             "Date: Mon, 27 Jul 2009 12:28:53 GMT\r\n"
                             "Server: Apache / 2.2.14(Win32)\r\n"
                             "Last-Modified: Wed, 22 Jul 2009 19:15:56 GMT\r\n"
                             "Content-Length: 20\r\n"
                             "Content-Type: text/html\r\n"
                             "Connection: Closed\r\n\r\n"
                             "<h1>Hello World</h1>";

      size_t is_load_command = request_string.find("GET /load?");
      wsprintf(str, L"load_command %xi", is_load_command);
      OutputDebugString(str);

      size_t is_eject_command = request_string.find("GET /eject ");
      wsprintf(str, L"load_command %xi", is_eject_command);
      OutputDebugString(str);
      
      if (is_load_command != std::string::npos )
      {
        size_t end_game = request_string.find(" HTTP/1.1");
        std::string game = request_string.substr(10, end_game - 10);

        std::string game_cleaned(
            curl_easy_unescape(curl, game.c_str(), static_cast<int>(game.length()), NULL));

        try
        {
          if (Core::IsRunning())
            Core::RunAsCPUThread([&game_cleaned] { DVDInterface::ChangeDisc(game_cleaned); });
        }
        catch (int e)
        {
          wsprintf(str, L"Exception occurred. %i", e);

          OutputDebugString(str);
        }

        char response_html[DEFAULT_BUFLEN];
        sprintf(response_html, "<h1>Loaded: %s</h1>", game_cleaned.c_str());
        char response[DEFAULT_BUFLEN];
        sprintf(response,
                "HTTP/1.1 200 OK\r\n"
                "Server: Apache / 2.2.14(Win32)\r\n"
                "Content-Length: %zi\r\n\r\n%s",
                strlen(response_html), response_html);
        iSendResult = send(ClientSocket, response, static_cast<int>(strlen(response)), 0);
        
      }
      else if (is_eject_command != std::string::npos)
      {
        try
        {
          if (Core::IsRunning())
            Core::RunAsCPUThread([] { DVDInterface::EjectDisc(DVDInterface::EjectCause::User); });
        }
        catch (int e)
        {
          wsprintf(str, L"Exception occurred. %i", e);

          OutputDebugString(str);
        }

        char response_html[DEFAULT_BUFLEN];
        sprintf(response_html, "<h1>Ejected Disk</h1>");
        char response[DEFAULT_BUFLEN];
        sprintf(response,
                "HTTP/1.1 200 OK\r\n"
                "Server: Apache / 2.2.14(Win32)\r\n"
                "Content-Length: %zi\r\n\r\n%s",
                strlen(response_html), response_html);
        iSendResult = send(ClientSocket, response, static_cast<int>(strlen(response)), 0);

      }else
      {
        iSendResult =
            send(ClientSocket, unknown_response, static_cast<int>(strlen(unknown_response)), 0);
      }


        
        if (iSendResult == SOCKET_ERROR)
        {
          wsprintf(str, L"send failed with error: %d\n", WSAGetLastError());
          OutputDebugString(str);
          closesocket(ClientSocket);
          WSACleanup();
          return 1;
        }
        wsprintf(str, L"Bytes sent: %d\n", iSendResult);
        OutputDebugString(str);
      }
      else if (iResult == 0)
      {
        wsprintf(str, L"Connection closing...\n");
        OutputDebugString(str);
      }
      else
      {
        wsprintf(str, L"recv failed with error: %d\n", WSAGetLastError());
        OutputDebugString(str);
        closesocket(ClientSocket);
        WSACleanup();
        return 1;
      }
      closesocket(ClientSocket);
    } while (iResult > 0);

    // shutdown the connection since we're done
    iResult = shutdown(ClientSocket, SD_SEND);
    if (iResult == SOCKET_ERROR)
    {
      wsprintf(str, L"shutdown failed with error: %d\n", WSAGetLastError());
      OutputDebugString(str);
      closesocket(ClientSocket);
      WSACleanup();
      return 1;
    }

    // cleanup
    closesocket(ClientSocket);
    WSACleanup();

    return 0;
  }
};  // namespace SocketServer
