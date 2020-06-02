//
// Created by Anders Cedronius on 2019-04-21.
//

#ifndef CPPSRTWRAPPER_SRTNET_H
#define CPPSRTWRAPPER_SRTNET_H

#include <iostream>
#include <functional>
#include <atomic>
#include <thread>
#include <vector>
#include <utility>
#include <cstdlib>
#include <map>
#include <mutex>
#include <any>
#include "srt/srtcore/srt.h"

#ifdef WIN32
#include <Winsock2.h>
#define _WINSOCKAPI_
#include <ws2tcpip.h>
#include <io.h>
#else
#include <arpa/inet.h>
#endif

#define MAX_WORKERS 20 //Max number of connections to deal with each epoll

//Fill this class with all information you need for the duration of the connection both client and server
class NetworkConnection {
public:
    std::any object;
};

namespace  SRTNetClearStats {
    enum SRTNetClearStats : int {
        no,
        yes
    };
}

namespace  SRTNetInstant {
     enum SRTNetInstant : int {
        no,
        yes
    };
}


class SRTNet {
public:

    enum Mode {
        unknown,
        server,
        client
    };

    SRTNet();
    virtual ~SRTNet();

    bool startServer(std::string ip, uint16_t port, int reorder, int32_t latency, int overhead, int mtu, std::string psk="");
    bool startClient(std::string host, uint16_t port, int reorder, int32_t latency, int overhead, std::shared_ptr<NetworkConnection> &ctx, int mtu, std::string psk="");
    bool stop();
    bool sendData(uint8_t *data, size_t len, SRT_MSGCTRL *msgCtrl, SRTSOCKET targetSystem = 0);
    bool getStatistics(SRT_TRACEBSTATS *currentStats,int clear, int instantaneous, SRTSOCKET targetSystem = 0);
    void getActiveClients(std::function<void(std::map<SRTSOCKET, std::shared_ptr<NetworkConnection>> &)> function);

    std::function<std::shared_ptr<NetworkConnection>(struct sockaddr &sin, SRTSOCKET newSocket)> clientConnected = nullptr;
    std::function<void(std::unique_ptr <std::vector<uint8_t>> &data, SRT_MSGCTRL &msgCtrl, std::shared_ptr<NetworkConnection> &ctx, SRTSOCKET socket)> recievedData = nullptr;

    std::atomic<bool> serverActive;
    std::atomic<bool> clientActive;
    std::atomic<bool> serverListenThreadActive;
    std::atomic<bool> serverPollThreadActive;
    std::atomic<bool> clientThreadActive;

    // delete copy and move constructors and assign operators
    SRTNet(SRTNet const&) = delete;             // Copy construct
    SRTNet(SRTNet&&) = delete;                  // Move construct
    SRTNet& operator=(SRTNet const&) = delete;  // Copy assign
    SRTNet& operator=(SRTNet &&) = delete;      // Move assign


private:
    void waitForSRTClient();
    void serverEventHandler();
    void clientWorker();
    void closeAllClientSockets();
    bool isIPv4(const std::string& str);
    bool isIPv6(const std::string& str);
    SRTSOCKET context = 0;
    int poll_id = 0;
    std::mutex netMtx;
    Mode currentMode = Mode::unknown;
    std::map<SRTSOCKET, std::shared_ptr<NetworkConnection>> clientList = {};
    std::mutex clientListMtx;
    std::shared_ptr<NetworkConnection> clientContext = nullptr;
};


#endif //CPPSRTWRAPPER_SRTNET_H
