//
// Created by Anders Cedronius on 2019-04-21.
//

#ifndef CPPSRTWRAPPER_SRTNET_H
#define CPPSRTWRAPPER_SRTNET_H

#include <iostream>
#include <functional>
#include <sstream>
#include <atomic>
#include <thread>
#include <stdlib.h>

#include "../srt/srtcore/srt.h"
#include "SRTGlobalHandler.h"


class SRTNet {
public:
    enum Mode {
        Unknown,
        Server,
        Client
    };

    SRTNet();
    SRTNet(const SRTNet& orig);
    virtual ~SRTNet();

    bool startServer(std::string ip, std::string port);
    bool startClient(std::string host, std::string port);
    bool stopServer();
    bool stopClient();
    bool sendData(uint8_t* data, size_t len);

    std::function<bool(struct sockaddr_storage& connectingClient)> clientConnected;
    std::function<bool(uint8_t*, size_t)> recievedData;
    std::atomic<bool> serverActive;

private:
    void waitForSRTClient();
    SRTGlobalHandler& pSRTHandler = SRTGlobalHandler::GetInstance();
    SRTSOCKET context;
    SRTSOCKET their_fd;
    Mode currentMode = Mode::Unknown;
};

#endif //CPPSRTWRAPPER_SRTNET_H
