//      __________  ____     _____ ____  ______   _       ______  ___    ____  ____  __________
//     / ____/ __ \/ __ \   / ___// __ \/_  __/  | |     / / __ \/   |  / __ \/ __ \/ ____/ __ \
//    / /   / /_/ / /_/ /   \__ \/ /_/ / / /     | | /| / / /_/ / /| | / /_/ / /_/ / __/ / /_/ /
//   / /___/ ____/ ____/   ___/ / _, _/ / /      | |/ |/ / _, _/ ___ |/ ____/ ____/ /___/ _, _/
//   \____/_/   /_/       /____/_/ |_| /_/       |__/|__/_/ |_/_/  |_/_/   /_/   /_____/_/ |_|
//
// Created by Anders Cedronius on 2019-04-21.
//

// Prefixes used
// m class member
// p pointer (*)
// r reference (&)
// h part of header
// l local scope

#pragma once

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
#include <memory>
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

    //Fill this class with all information you need for the duration of the connection both client and server
    class NetworkConnection {
    public:
        std::any mObject;
    };

    SRTNet();

    virtual ~SRTNet();

    /**
     *
     * Starts a SRT Server
     *
     * @param lIp Listen IP
     * @param lPort Listen Port
     * @param lReorder number of packets in re-order window
     * @param lLatency Max re-send window (ms) / also the delay of transmission
     * @param lOverhead % extra of the BW that will be allowed for re-transmission packets
     * @param lMtu sets the MTU
     * @param lPsk Optional Pre Shared Key (AES-128)
     * @param pCtx optional context used only in the clientConnected callback
     * @return true if server was able to start
    */
    bool startServer(const std::string& lIp, uint16_t lPort, int lReorder, int32_t lLatency, int lOverhead, int lMtu,
                     const std::string& lPsk = "", std::shared_ptr<NetworkConnection> pCtx = {});

    /**
     *
     * Starts a SRT Client
     *
     * @param lHost Host IP
     * @param lPort Host Port
     * @param lReorder number of packets in re-order window
     * @param lLatency Max re-send window (ms) / also the delay of transmission
     * @param lOverhead % extra of the BW that will be allowed for re-transmission packets
     * @param rpCtx the context used in the receivedData and receivedDataNoCopy callback
     * @param lMtu sets the MTU
     * @param lPsk Optional Pre Shared Key (AES-128)
     * @return true if client was able to connect to the server
    */
    bool startClient(const std::string& lHost, uint16_t lPort, int lReorder, int32_t lLatency, int lOverhead,
                     std::shared_ptr<NetworkConnection> &rpCtx, int lMtu, const std::string& lPsk = "");

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
     * @param pData pointer to the data
     * @param lSize size of the data
     * @param pMsgCtrl pointer to a SRT_MSGCTRL struct.
     * @param lTargetSystem the target sending the data to (used in server mode only)
     * @return true if sendData was able to send the data to the target.
    */
    bool sendData(const uint8_t *pData, size_t lSize, SRT_MSGCTRL *pMsgCtrl, SRTSOCKET lTargetSystem = 0);

    /**
     *
     * Get connection statistics
     *
     * @param pCurrentStats pointer to the statistics struct
     * @param lClear Clears the data after reading SRTNetClearStats::yes or no
     * @param lInstantaneous Get the parameters now SRTNetInstant::yes or filtered values SRTNetInstant::no
     * @param lTargetSystem The target connection to get statistics about (used in server mode only)
     * @return true if statistics was populated.
    */
    bool getStatistics(SRT_TRACEBSTATS *pCurrentStats, int lClear, int lInstantaneous, SRTSOCKET lTargetSystem = 0);

    /**
     *
     * Get active clients (A server method)
     *
     * @param rFunction. pass a function getting the map containing the list of active connections
     * Where the map contains the SRTSocketHandle (SRTSOCKET) and it's associated NetworkConnection you provided.
    */
    void
    getActiveClients(const std::function<void(std::map<SRTSOCKET, std::shared_ptr<NetworkConnection>> &)> &rFunction);

    ///Callback handling connecting clients (only server mode)
    std::function<std::shared_ptr<NetworkConnection>(struct sockaddr &rSin,
                                                     SRTSOCKET lNewSocket, std::shared_ptr<NetworkConnection> &rpCtx)> clientConnected = nullptr;
    ///Callback receiving data type vector
    std::function<void(std::unique_ptr<std::vector<uint8_t>> &rData, SRT_MSGCTRL &rMsgCtrl,
                       std::shared_ptr<NetworkConnection> &rpCtx, SRTSOCKET lSocket)> receivedData = nullptr;

    ///Callback receiving data no copy
    std::function<void(const uint8_t *pData, size_t lSize, SRT_MSGCTRL &rMsgCtrl,
                       std::shared_ptr<NetworkConnection> &rCtx, SRTSOCKET lSocket)> receivedDataNoCopy = nullptr;

    ///Callback handling disconnecting clients (server and client mode)
    std::function<void(std::shared_ptr<NetworkConnection> &rCtx,
                                                     SRTSOCKET lSocket)> clientDisconnected = nullptr;

    // delete copy and move constructors and assign operators
    SRTNet(SRTNet const &) = delete;             // Copy construct
    SRTNet(SRTNet &&) = delete;                  // Move construct
    SRTNet &operator=(SRTNet const &) = delete;  // Copy assign
    SRTNet &operator=(SRTNet &&) = delete;      // Move assign


private:

    //Internal variables and methods

    enum class Mode {
        unknown,
        server,
        client
    };

    void waitForSRTClient();
    void serverEventHandler();
    void clientWorker();
    void closeAllClientSockets();
    static bool isIPv4(const std::string &str);
    static bool isIPv6(const std::string &str);

    //Server avtive? true == yes
    std::atomic<bool> mServerActive = {false};
    //Client avtive? true == yes
    std::atomic<bool> mClientActive;
    //Server thread active? true == yes
    std::atomic<bool> mServerListenThreadActive;
    //Client thread active? true == yes
    std::atomic<bool> mClientThreadActive;

    SRTSOCKET mContext = 0;
    int mPollID = 0;
    std::mutex mNetMtx;
    Mode mCurrentMode = Mode::unknown;
    std::map<SRTSOCKET, std::shared_ptr<NetworkConnection>> mClientList = {};
    std::mutex mClientListMtx;
    std::shared_ptr<NetworkConnection> mClientContext = nullptr;
    std::shared_ptr<NetworkConnection> mConnectionContext = nullptr;
};

