#pragma once
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>
#include "Core/Core.h"
#include <ws2tcpip.h>
namespace SocketServer
{


// Need to link with Ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")
// #pragma comment (lib, "Mswsock.lib")


int __cdecl main(void);
}  // namespace SocketServer
