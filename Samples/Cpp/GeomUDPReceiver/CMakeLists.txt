cmake_minimum_required(VERSION 3.5)

project(GeomUDPReceiver LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES
    ../../Common/Cpp/DatagramSocket/DatagramSocket.cpp
    main.cpp
)
set(HEADERS
    ../../Common/Cpp/DatagramSocket/DatagramSocket.h
)

add_executable(GeomUDPReceiver ${SOURCES} ${HEADERS})
target_include_directories(GeomUDPReceiver PRIVATE "../../Common/Cpp/")

