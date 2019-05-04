include_directories(BEFORE include src)

set(SRC_NETWORK
	src/SFML/Network/Http.cpp
	src/SFML/Network/IPAddress.cpp
	src/SFML/Network/Packet.cpp
	src/SFML/Network/Socket.cpp
	src/SFML/Network/SocketSelector.cpp
	src/SFML/Network/TcpListener.cpp
	src/SFML/Network/TcpSocket.cpp
	src/SFML/Network/UdpSocket.cpp
)

if(WIN32)
	list(APPEND SRC_NETWORK src/SFML/Network/Win32/SocketImpl.cpp)
else()
	list(APPEND SRC_NETWORK src/SFML/Network/Unix/SocketImpl.cpp)
endif()

set(SRC_SYSTEM
	src/SFML/System/Err.cpp
	src/SFML/System/String.cpp
	src/SFML/System/Time.cpp
)

add_library(sfml-network ${SRC_NETWORK})
add_library(sfml-system ${SRC_SYSTEM})

set_target_properties(sfml-network PROPERTIES FOLDER Externals)
set_target_properties(sfml-system PROPERTIES FOLDER Externals)
disable_all_warnings(sfml-network)
disable_all_warnings(sfml-system)
