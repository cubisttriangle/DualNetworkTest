//
// Created by vincent on 4/27/20.
//

#ifndef DUALNETWORKTEST_SOCKETTHREAD_H
#define DUALNETWORKTEST_SOCKETTHREAD_H

#include <netinet/in.h>

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

struct EndPoint
{
    EndPoint( const std::string& ipStr = "0.0.0.0", uint16_t portNo = 0 )
      : ip( ipStr )
      , port( portNo )
    {}

    EndPoint( const EndPoint& other )
      : ip( other.ip )
      , port( other.port )
    {}

    EndPoint& operator=( const EndPoint& other )
    {
        this->ip = other.ip;
        this->port = other.port;
        return *this;
    }

    std::string ip { "0.0.0.0" };
    uint16_t port { 0 };
};

struct Socket
{
    EndPoint requestedEndPoint;
    EndPoint acquiredEndPoint;
    EndPoint remoteEndPoint;
    int fd;
    sockaddr_in addr;
};

class SocketThread
{
public:
    SocketThread() noexcept;
    SocketThread( const SocketThread& other ) = delete;
    ~SocketThread();
    SocketThread& operator=( const SocketThread& other ) = delete;

    void Run( const std::vector<EndPoint>& endPoints );
    void Stop();

    static bool EndPointFromAddr( const sockaddr_in& addr, EndPoint& ep );
    static bool GetBoundAddr( int fd, sockaddr_in& addr );
    static bool GetBoundEndPoint( int fd, EndPoint& ep );
    static bool BindSocketToEndPoint( Socket& sock, EndPoint ep );
    static bool MonitorFd( int epollFd, int sock );
    static bool UnmonitorFd( int epollFd, int sock );
    static bool SetupSocket( Socket& sock, EndPoint ep );
    static void MonitorSockets( int epollFd, const std::vector<Socket>& sockets );
    static void UnmonitorSockets( int epollFd, const std::vector<Socket>& sockets );
    static void CloseSockets( std::vector<Socket>& sockets );
    static void UpdateUiText( const std::vector<Socket>& sockets );

private:

    std::atomic<bool> mKeepRunning;
    std::thread mThread;
};



#endif //DUALNETWORKTEST_SOCKETTHREAD_H
