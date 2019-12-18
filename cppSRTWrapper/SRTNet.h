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
#include <cstdlib>
#include <map>
#include <mutex>
#include <any>
#include "../srt/srtcore/srt.h"

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

    bool startServer(std::string ip, uint16_t port, int reorder, int32_t latency, int overhead, int mtu);
    bool startClient(std::string host, uint16_t port, int reorder, int32_t latency, int overhead, std::shared_ptr<NetworkConnection> &ctx, int mtu);
    bool stop();
    bool sendData(uint8_t *data, size_t len, SRT_MSGCTRL *msgCtrl, SRTSOCKET targetSystem = 0);
    bool getStatistics(SRT_TRACEBSTATS *currentStats,int clear, int instantaneous, SRTSOCKET targetSystem = 0);

    std::function<std::shared_ptr<NetworkConnection>(struct sockaddr_in* sin)> clientConnected = nullptr;
    std::function<void(std::unique_ptr <std::vector<uint8_t>> &data, SRT_MSGCTRL &msgCtrl, std::shared_ptr<NetworkConnection> &ctx, SRTSOCKET)> recievedData = nullptr;
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
    SRTSOCKET context = 0;
    int poll_id = 0;
    std::mutex netMtx;
    std::mutex clientListMtx;
    std::map<SRTSOCKET, std::shared_ptr<NetworkConnection>> clientList;
    Mode currentMode = Mode::unknown;
    std::shared_ptr<NetworkConnection> clientContext = nullptr;
};

#endif //CPPSRTWRAPPER_SRTNET_H
