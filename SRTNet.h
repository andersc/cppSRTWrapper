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
#pragma comment( lib, "ws2_32.lib")
#else

#include <arpa/inet.h>

#endif

#define MAX_WORKERS 20 //Max number of connections to deal with each epoll

//Fill this class with all information you need for the duration of the connection both client and server
class NetworkConnection {
public:
    std::any object;
};

namespace SRTNetClearStats {
    enum SRTNetClearStats : int {
        no,
        yes
    };
}

namespace SRTNetInstant {
    enum SRTNetInstant : int {
        no,
        yes
    };
}


class SRTNet {
public:

    SRTNet();

    virtual ~SRTNet();

    /**
     *
     * Starts a SRT Server
     *
     * @param ip Listen IP
     * @param port Listen Port
     * @param reorder number of packets in re-order window
     * @param latency Max re-send window (ms) / also the delay of transmission
     * @param overhead % extra of the BW that will be allowed for re-transmission packets
     * @param mtu sets the MTU
     * @param psk Optional Pre Shared Key (AES-128)
     * @return true if server was able to start
    */
    bool startServer(std::string ip, uint16_t port, int reorder, int32_t latency, int overhead, int mtu,
                     std::string psk = "");

    /**
     *
     * Starts a SRT Client
     *
     * @param ip Host IP
     * @param port Host Port
     * @param reorder number of packets in re-order window
     * @param latency Max re-send window (ms) / also the delay of transmission
     * @param overhead % extra of the BW that will be allowed for re-transmission packets
     * @param mtu sets the MTU
     * @param psk Optional Pre Shared Key (AES-128)
     * @return true if client was able to connect to the server
    */
    bool startClient(std::string host, uint16_t port, int reorder, int32_t latency, int overhead,
                     std::shared_ptr<NetworkConnection> &ctx, int mtu, std::string psk = "");

    /**
     *
     * Stops the service
     *
     * @return true if the service stopped successfully.
    */
    bool stop();

    /**
     *
     * Send data
     *
     * @param data pointer to the data
     * @param len size of the data
     * @param msgCtrl pointer to a SRT_MSGCTRL struct.
     * @param targetSystem the target sending the data to.
     * @return true if sendData was able to send the data to the target.
    */
    bool sendData(uint8_t *data, size_t len, SRT_MSGCTRL *msgCtrl, SRTSOCKET targetSystem = 0);

    /**
     *
     * Get connection statistics
     *
     * @param currentStats pointer to the statistics struct
     * @param clear Clears the data after reading SRTNetClearStats::yes or no
     * @param instantaneous Get the parameters now SRTNetInstant::yes or filtered values SRTNetInstant::no
     * @param targetSystem The target connection to get statistics about.
     * @return true if statistics was populated.
    */
    bool getStatistics(SRT_TRACEBSTATS *currentStats, int clear, int instantaneous, SRTSOCKET targetSystem = 0);

    /**
     *
     * Get active clients (A server method)
     *
     * @param function. pass a function getting the map containing the list of active connections
     * Where the map contains the SRTSocketHandle (SRTSOCKET) and it's associated NetworkConnection you provided.
    */
    void
    getActiveClients(const std::function<void(std::map<SRTSOCKET, std::shared_ptr<NetworkConnection>> &)> &function);

    ///Callback handling connecting clients
    std::function<std::shared_ptr<NetworkConnection>(struct sockaddr &sin,
                                                     SRTSOCKET newSocket)> clientConnected = nullptr;
    ///Callback receiving data type vector
    std::function<void(std::unique_ptr<std::vector<uint8_t>> &data, SRT_MSGCTRL &msgCtrl,
                       std::shared_ptr<NetworkConnection> &ctx, SRTSOCKET socket)> receivedData = nullptr;

    ///Callback receiving data no copy
    std::function<void(const uint8_t *data, size_t len, SRT_MSGCTRL &msgCtrl,
                       std::shared_ptr<NetworkConnection> &ctx, SRTSOCKET socket)> receivedDataNoCopy = nullptr;

    ///Callback handling disconnecting clients
    std::function<void(std::shared_ptr<NetworkConnection> &ctx,
                                                     SRTSOCKET socket)> clientDisconnected = nullptr;

    // delete copy and move constructors and assign operators
    SRTNet(SRTNet const &) = delete;             // Copy construct
    SRTNet(SRTNet &&) = delete;                  // Move construct
    SRTNet &operator=(SRTNet const &) = delete;  // Copy assign
    SRTNet &operator=(SRTNet &&) = delete;      // Move assign


private:

    //Internal variables and methods

    enum Mode {
        unknown,
        server,
        client
    };

    void waitForSRTClient();
    void serverEventHandler();
    void clientWorker();
    void closeAllClientSockets();
    bool isIPv4(const std::string &str);
    bool isIPv6(const std::string &str);

    //Server avtive? true == yes
    std::atomic<bool> serverActive;
    //Client avtive? true == yes
    std::atomic<bool> clientActive;
    //Server thread active? true == yes
    std::atomic<bool> serverListenThreadActive;
    //Client thread active? true == yes
    std::atomic<bool> clientThreadActive;

    SRTSOCKET context = 0;
    int poll_id = 0;
    std::mutex netMtx;
    Mode currentMode = Mode::unknown;
    std::map<SRTSOCKET, std::shared_ptr<NetworkConnection>> clientList = {};
    std::mutex clientListMtx;
    std::shared_ptr<NetworkConnection> clientContext = nullptr;
};


#endif //CPPSRTWRAPPER_SRTNET_H
