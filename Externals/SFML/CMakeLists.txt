set(SRC_NETWORK
	SFML/src/SFML/Network/Http.cpp
	SFML/src/SFML/Network/IpAddress.cpp
	SFML/src/SFML/Network/Packet.cpp
	SFML/src/SFML/Network/Socket.cpp
	SFML/src/SFML/Network/SocketSelector.cpp
	SFML/src/SFML/Network/TcpListener.cpp
	SFML/src/SFML/Network/TcpSocket.cpp
	SFML/src/SFML/Network/UdpSocket.cpp
)

if(WIN32)
	list(APPEND SRC_NETWORK SFML/src/SFML/Network/Win32/SocketImpl.cpp)
else()
	list(APPEND SRC_NETWORK SFML/src/SFML/Network/Unix/SocketImpl.cpp)
endif()

set(SRC_SYSTEM
	SFML/src/SFML/System/Err.cpp
	SFML/include/SFML/System/String.hpp
	SFML/src/SFML/System/String.cpp
	SFML/src/SFML/System/Utils.cpp
)

add_library(sfml-network STATIC ${SRC_NETWORK})
add_library(sfml-system STATIC ${SRC_SYSTEM})
target_compile_features(sfml-network PUBLIC cxx_std_17)
target_compile_features(sfml-system PUBLIC cxx_std_17)
target_compile_definitions(sfml-system PUBLIC SFML_STATIC)
target_include_directories(sfml-system PUBLIC SFML/include PRIVATE SFML/src)
target_include_directories(sfml-network PUBLIC SFML/include PRIVATE SFML/src)
target_link_libraries(sfml-network PUBLIC sfml-system)
dolphin_disable_warnings(sfml-network)
dolphin_disable_warnings(sfml-system)