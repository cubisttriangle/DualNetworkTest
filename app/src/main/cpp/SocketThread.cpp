//
// Created by vincent on 4/27/20.
//

#include "SocketThread.h"
#include "native-lib.h"

#include <android/log.h>

#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sstream>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <thread>

#define TAG "SocketThread"

SocketThread::SocketThread() noexcept
  : mKeepRunning( false )
{

}

SocketThread::~SocketThread()
{
    Stop();
}

void SocketThread::Stop()
{
    mKeepRunning = false;
    if ( mThread.joinable() )
    {
        mThread.join();
    }
}

void SocketThread::Run( const std::vector<EndPoint>& endPoints )
{
    if ( mKeepRunning )
    {
        __android_log_print( ANDROID_LOG_DEBUG, TAG, "Already running." );
        return;
    }

    mKeepRunning = true;

    std::thread t( [=] ()
    {
        std::vector<Socket> unconnectedSockets, connectedSockets;

        // Bind each socket to their endpoint.
        for ( const auto& ep : endPoints )
        {
            Socket s;
            if ( SetupSocket( s, ep ) )
            {
                unconnectedSockets.push_back( s );
            }
        }

        // Display socket address on UI
        UpdateUiText( unconnectedSockets );

        // Setup epoll
        int epollFd = ::epoll_create( 10 );
        constexpr int maxEpollEvents = 1024;
        epoll_event epollEvents[maxEpollEvents];
        {
            if ( -1 == epollFd )
            {
                __android_log_print( ANDROID_LOG_ERROR, TAG, "Failed to setup epoll fd: %s", strerror( errno ) );
                CloseSockets( unconnectedSockets );
                mKeepRunning = false;
                return;
            }

            // Zero everything out.
            for ( int i = 0; i < maxEpollEvents; ++i )
            {
                memset( &epollEvents[i], 0, sizeof( epollEvents[i] ) );
            }
        }

        MonitorSockets( epollFd, unconnectedSockets );

        // Poll for events.
        constexpr int pollIntervalMs = 5000;
        while ( mKeepRunning )
        {
            int newFds = epoll_wait( epollFd, epollEvents, maxEpollEvents, pollIntervalMs );
            if ( -1 == newFds )
            {
                // Don't really care about our wait getting interrupted. We'll live to wait another day.
                if ( EINTR != errno )
                    __android_log_print( ANDROID_LOG_ERROR, TAG, "Error condition encountered while waiting for socket events: %s", strerror( errno ) );
            }
            else
            {
                // Should fit MTU.
                constexpr int bufSize = 2048;
                uint8_t buf[bufSize];
                bool handledSocket = false;

                // If we have new events, handle them.
                for ( int i = 0; i < newFds; ++i )
                {
                    const int fd = epollEvents[i].data.fd;

                    // Check if this is a new 'connection'.
                    for ( int i = 0; i<unconnectedSockets.size(); ++i )
                    {
                        auto s = unconnectedSockets[i];
                        if ( fd == s.fd )
                        {
                            __android_log_print( ANDROID_LOG_DEBUG, TAG, "New connection on fd: %d, %s:%d", fd, s.acquiredEndPoint.ip.c_str(), s.acquiredEndPoint.port );
                            handledSocket = true;

                            sockaddr_in addr;
                            socklen_t addrLen = sizeof( sockaddr_in );
                            int recvdBytes = ::recvfrom( fd, buf, bufSize, 0, (sockaddr*)&addr, &addrLen );

                            if ( EndPointFromAddr( addr, s.remoteEndPoint ) )
                            {
                                __android_log_print( ANDROID_LOG_DEBUG, TAG, "New connection is from remote: %s:%d", s.remoteEndPoint.ip.c_str(), s.remoteEndPoint.port );
                            }

                            if ( -1 == ::connect( s.fd, (sockaddr*)&addr, addrLen ) )
                            {
                                __android_log_print( ANDROID_LOG_ERROR, TAG, "Failed to connect to remote: %s:%d", s.remoteEndPoint.ip.c_str(), s.remoteEndPoint.port );
                            }
                            else
                            {
                                unconnectedSockets.erase( unconnectedSockets.begin() + i );
                                connectedSockets.push_back( s );
                                Socket replacement;
                                if ( !SetupSocket( replacement, s.acquiredEndPoint ) )
                                {
                                    __android_log_print( ANDROID_LOG_ERROR, TAG, "Failed to setup new socket on: %s:%d", s.acquiredEndPoint.ip.c_str(), s.acquiredEndPoint.port );
                                }
                                else
                                {
                                    unconnectedSockets.push_back( replacement );
                                }
                            }
                        }
                    } // End unconnected socket check

                    if ( handledSocket )
                        continue;

                    for ( auto& s : connectedSockets )
                    {
                        if ( fd == s.fd )
                        {
                            __android_log_print( ANDROID_LOG_DEBUG, TAG, "New message from connected client: %s:%d", s.remoteEndPoint.ip.c_str(), s.remoteEndPoint.port );
                            int recvdBytes = ::recv( fd, buf, bufSize, 0 );
                            break;
                        }
                    }
                } // End fd check loop
            }
        }

        // Cleanup
        {
            UnmonitorSockets( epollFd, unconnectedSockets );
            UnmonitorSockets( epollFd, connectedSockets );
            ::close( epollFd );
            CloseSockets( unconnectedSockets );
            CloseSockets( connectedSockets );
        }
    } );
    mThread = std::move( t );
}

bool SocketThread::SetupSocket( Socket &s, EndPoint ep )
{
    s.fd = ::socket( AF_INET, SOCK_DGRAM|SOCK_NONBLOCK|SOCK_CLOEXEC, 0 );

    if ( -1 == s.fd )
    {
        __android_log_print( ANDROID_LOG_ERROR, TAG, "Failed to create socket for ep: %s:%d - %s", ep.ip.c_str(), ep.port, strerror( errno ) );
        return false;
    }

    int optval = 1;
    if ( -1 == setsockopt( s.fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof( optval ) ) )
    {
        __android_log_print( ANDROID_LOG_ERROR, TAG, "Could reuse addr for socket: %s:%d - %s", ep.ip.c_str(), ep.port, strerror( errno ) );
    }

    if ( -1 == setsockopt( s.fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof( optval ) ) )
    {
        __android_log_print( ANDROID_LOG_ERROR, TAG, "Could reuse port for socket: %s:%d - %s", ep.ip.c_str(), ep.port, strerror( errno ) );
    }

    if ( !BindSocketToEndPoint( s, ep ) )
    {
        ::close( s.fd );
        return false;
    }

    // Get current address to which fd is bound (in case input endpoint wasn't specified)
    if ( !GetBoundEndPoint( s.fd, s.acquiredEndPoint ) )
    {
        __android_log_print( ANDROID_LOG_ERROR, TAG, "Failed to get local address from socket for ep: %s:%d - %s", ep.ip.c_str(), ep.port, strerror( errno ) );
        ::close( s.fd );
        return false;
    }
    __android_log_print( ANDROID_LOG_DEBUG, TAG, "Requested ep, %s:%d, acquired ep, %s:%d", ep.ip.c_str(), ep.port, s.acquiredEndPoint.ip.c_str(), s.acquiredEndPoint.port );

    return true;
}

void SocketThread::UnmonitorSockets( int epollFd, const std::vector<Socket>& sockets )
{
    for ( auto& s : sockets )
    {
        UnmonitorFd( epollFd, s.fd );
    }
}

void SocketThread::MonitorSockets( int epollFd, const std::vector<Socket>& sockets )
{
    for ( auto& s : sockets )
    {
        MonitorFd( epollFd, s.fd );
    }
}

void SocketThread::CloseSockets( std::vector<Socket>& sockets )
{
    for ( auto& s : sockets )
    {
        ::close( s.fd );
    }
}

bool SocketThread::UnmonitorFd( int epollFd, int sock )
{
    if ( -1 == epoll_ctl( epollFd, EPOLL_CTL_DEL, sock, nullptr ) )
    {
        __android_log_print( ANDROID_LOG_ERROR, TAG, "Epoll delete failed on fd: %d", sock );
        return false;
    }

    return true;
}

bool SocketThread::MonitorFd( int epollFd, int sock )
{
    struct epoll_event clientEvent;
    clientEvent.events  = EPOLLIN | EPOLLRDHUP;
    clientEvent.data.fd = sock;
    if ( -1 == epoll_ctl( epollFd, EPOLL_CTL_ADD, sock, &clientEvent ) )
    {
        __android_log_print( ANDROID_LOG_ERROR, TAG, "Epoll monitor failed on fd: %d", sock );
        return false;
    }
    return true;
}

bool SocketThread::BindSocketToEndPoint( Socket& sock, EndPoint ep )
{
    sock.requestedEndPoint = ep;

    sock.addr.sin_family         = AF_INET;
    sock.addr.sin_port           = htons( ep.port );
    sock.addr.sin_addr.s_addr    = inet_addr( ep.ip.c_str() );

    if ( -1 == inet_pton( AF_INET, ep.ip.c_str(), &(sock.addr.sin_addr) ) )
    {
        __android_log_print( ANDROID_LOG_ERROR, TAG, "Failed to convert ip for ep: %s:%d - %s", ep.ip.c_str(), ep.port, strerror( errno ) );
        return false;
    }

    socklen_t addrLen = sizeof( struct sockaddr );

    if ( -1 == ::bind( sock.fd, (struct sockaddr*)&sock.addr, addrLen ) )
    {
        __android_log_print( ANDROID_LOG_ERROR, TAG, "Failed to bind socket for ep: %s:%d - %s", ep.ip.c_str(), ep.port, strerror( errno ) );
        return false;
    }

    return true;
}

bool SocketThread::EndPointFromAddr( const sockaddr_in& addr, EndPoint& ep )
{
    char str[INET_ADDRSTRLEN];
    if ( nullptr == inet_ntop( AF_INET, &(addr.sin_addr), str, INET_ADDRSTRLEN ) )
    {
        __android_log_print( ANDROID_LOG_ERROR, TAG, "Failed to convert local ip to str for addr: %d - %s", addr.sin_addr.s_addr, strerror( errno ) );
        return false;
    }

    ep.ip = std::string( str );
    ep.port = ntohs( addr.sin_port );

    return true;
}

bool SocketThread::GetBoundAddr( int fd, sockaddr_in& addr)
{
    socklen_t sockLen = sizeof( addr );
    if ( -1 == ::getsockname( fd, (sockaddr*)&addr, &sockLen ) )
    {
        __android_log_print( ANDROID_LOG_ERROR, TAG, "Failed to get local address from socket fd %d: %s", fd, strerror( errno ) );
        return false;
    }

    return true;
}

bool SocketThread::GetBoundEndPoint( int fd, EndPoint& ep )
{
    sockaddr_in addr;
    return GetBoundAddr( fd, addr ) && EndPointFromAddr( addr, ep );
}

void SocketThread::UpdateUiText( const std::vector<Socket> &sockets )
{
    std::stringstream ss;

    for( const auto& s : sockets )
    {
       ss << s.acquiredEndPoint.ip << " : " << s.acquiredEndPoint.port << std::endl;
    }

    updateActivityText( ss.str() );
}
