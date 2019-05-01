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
#include <stdlib.h>

#include "SRTGlobalHandler.h"

class SRTNet {
public:
    enum Mode {
        Unknown,
        Server,
        Client
    };

    SRTNet();
    virtual ~SRTNet();

    bool startServer(std::string ip, std::string port, int reorder, int32_t latency, int overhead);
    bool startClient(std::string host, std::string port, int reorder, int32_t latency, int overhead);
    bool stopServer();
    bool stopClient();
    bool sendData(uint8_t* data, size_t len, SRT_MSGCTRL *msgCtrl);
    bool getStatistics(SRT_TRACEBSTATS *currentStats,int clear, int instantaneous);

    std::function<bool(struct sockaddr_in* sin)> clientConnected;
    std::function<bool(std::unique_ptr <std::vector<uint8_t>> &data, SRT_MSGCTRL &msgCtrl)> recievedData;
    std::atomic<bool> serverActive;
    std::atomic<bool> clientActive;
    std::atomic<bool> serverThreadActive;
    std::atomic<bool> clientThreadActive;

private:
    // delete copy and move constructors and assign operators
    SRTNet(SRTNet const&) = delete;             // Copy construct
    SRTNet(SRTNet&&) = delete;                  // Move construct
    SRTNet& operator=(SRTNet const&) = delete;  // Copy assign
    SRTNet& operator=(SRTNet &&) = delete;      // Move assign

    void waitForSRTClient();
    void serverResponce();
    SRTGlobalHandler& pSRTHandler = SRTGlobalHandler::GetInstance();
    SRTSOCKET context;
    SRTSOCKET their_fd;
    Mode currentMode = Mode::Unknown;
};

#endif //CPPSRTWRAPPER_SRTNET_H
