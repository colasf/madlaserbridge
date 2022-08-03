#pragma once

#include <string>

static std::string ipIntToStr(unsigned int ip) {
  return std::to_string((ip >> 24) & 0xFF) + '.' + std::to_string((ip >> 16) & 0xFF) + '.' +
          std::to_string((ip >> 8) & 0xFF) + '.' + std::to_string(ip & 0xFF);
}

struct GenericAddr
{
    short 			family = 0;
    unsigned int 	ip = 0;
    unsigned short 	port = 0;
    bool operator!=(const GenericAddr& other) {return other.family!=family || other.ip!=ip || other.port!=port;}
    bool operator==(const GenericAddr& other) {return !operator!=(other);}
    bool isValid() const {return ip!=0 && port!=0;}
};

/*********************************************************************************
  UNIX version
*********************************************************************************/

#if defined(__linux__) || defined (__APPLE__)

#ifndef UINT
    #define UINT unsigned int
#endif

#ifndef SOCKET
    #define SOCKET int
#endif

#ifndef SOCKADDR_IN
    #define SOCKADDR_IN sockaddr_in
#endif

#ifndef INVALID_SOCKET
    #define INVALID_SOCKET -1
#endif

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <netdb.h> // get host by name
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

class DatagramSocket
{
public:
    DatagramSocket(UINT interfaceIP, UINT port);
    ~DatagramSocket();

    bool joinMulticastGroup(UINT ip, UINT interfaceIP);
    bool leaveMulticastGroup(UINT ip, UINT interfaceIP);

    bool sendBroadcast(UINT port,void * buf,UINT buflen);

    bool sendTo(const GenericAddr & addr,const void *buf,UINT buflen);
    bool recvFrom(GenericAddr & addr,void * buf,UINT & buflen);

    bool isInitialized();

private:
    void closeSocket();

    int m_port=0;
    SOCKET m_socket=INVALID_SOCKET;
    bool m_isInitialized=false;
};

#endif

/*********************************************************************************
  WIN32 version
*********************************************************************************/

#if defined(_WIN32)

#include <memory>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")

class DatagramSocket
{
public:
    DatagramSocket(unsigned int interfaceIP, unsigned int port);

    ~DatagramSocket();

    bool joinMulticastGroup(unsigned int ip, unsigned int interfaceIP);
    bool leaveMulticastGroup(unsigned int ip, unsigned int interfaceIP);

    bool sendBroadcast(unsigned int port, void * buf, unsigned int buflen);

    bool sendTo(const GenericAddr & addr, const void *buf, unsigned int buflen);
    bool recvFrom(GenericAddr & addr, void * buf, unsigned int & buflen);

    bool isInitialized();

private:
    void closeSocket();

    int m_port = 0;
    SOCKET m_socket = INVALID_SOCKET;
    bool m_isInitialized = false;
};

#endif
