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

add_library(sfml-network STATIC ${SRC_NETWORK})
add_library(sfml-system STATIC ${SRC_SYSTEM})
target_compile_definitions(sfml-system PUBLIC SFML_STATIC)
target_include_directories(sfml-system PUBLIC include PRIVATE src)
target_include_directories(sfml-network PUBLIC include PRIVATE src)
target_link_libraries(sfml-network PUBLIC sfml-system)
dolphin_disable_warnings(sfml-network)
dolphin_disable_warnings(sfml-system)
