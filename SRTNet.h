//      __________  ____     _____ ____  ______   _       ______  ___    ____  ____  __________
//     / ____/ __ \/ __ \   / ___// __ \/_  __/  | |     / / __ \/   |  / __ \/ __ \/ ____/ __ \
//    / /   / /_/ / /_/ /   \__ \/ /_/ / / /     | | /| / / /_/ / /| | / /_/ / /_/ / __/ / /_/ /
//   / /___/ ____/ ____/   ___/ / _, _/ / /      | |/ |/ / _, _/ ___ |/ ____/ ____/ /___/ _, _/
//   \____/_/   /_/       /____/_/ |_| /_/       |__/|__/_/ |_/_/  |_/_/   /_/   /_____/_/ |_|
//
// Created by Anders Cedronius on 2019-04-21.
//

#pragma once

#include <any>
#include <atomic>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

#include "srt/srtcore/srt.h"

#ifdef WIN32
#include <Winsock2.h>
#define _WINSOCKAPI_
#include <io.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else

#include <arpa/inet.h>

#endif

#define MAX_WORKERS 5 // Max number of connections to deal with each epoll

namespace SRTNetClearStats {
enum SRTNetClearStats : int { no, yes };
}

namespace SRTNetInstant {
enum SRTNetInstant : int { no, yes };
}

class SRTNet {
public:

    enum class Mode {
        unknown,
        server,
        client
    };

    // Fill this class with all information you need for the duration of the connection both client and server
    class NetworkConnection {
    public:
        std::any mObject;
    };

    SRTNet();

    /**
     *
     * @brief Constructor that also sets a log handler
     * @param handler The log handler to be used
     * @param loglevel The log level to use
     *
     **/
    SRTNet(SRT_LOG_HANDLER_FN* handler, int loglevel);

    virtual ~SRTNet();

    /**
     *
     * Starts an SRT Server
     *
     * @param localIP Listen IP
     * @param localPort Listen Port
     * @param reorder number of packets in re-order window
     * @param latency Max re-send window (ms) / also the delay of transmission
     * @param overhead % extra of the BW that will be allowed for re-transmission packets
     * @param mtu sets the MTU
     * @param peerIdleTimeout Optional Connection considered broken if no packet received before this timeout.
     * Defaults to 5 seconds.
     * @param psk Optional Pre Shared Key (AES-128)
     * @param singleSender set to true to accept just one sender to connect to the server, otherwise the server will
     * keep waiting and accepting more incoming sender connections
     * @param ctx optional context used only in the clientConnected callback
     * @return true if server was able to start
     */
    bool startServer(const std::string& localIP,
                     uint16_t localPort,
                     int reorder,
                     int32_t latency,
                     int overhead,
                     int mtu,
                     int32_t peerIdleTimeout = 5000,
                     const std::string& psk = "",
                     bool singleSender = false,
                     std::shared_ptr<NetworkConnection> ctx = {});

    /**
     *
     * Starts an SRT Client
     *
     * @param host Remote host IP or hostname
     * @param port Remote host Port
     * @param reorder number of packets in re-order window
     * @param latency Max re-send window (ms) / also the delay of transmission
     * @param overhead % extra of the BW that will be allowed for re-transmission packets
     * @param ctx the context used in the receivedData and receivedDataNoCopy callback
     * @param mtu sets the MTU
     * @param peerIdleTimeout Optional Connection considered broken if no packet received before this timeout.
     * Defaults to 5 seconds.
     * @param psk Optional Pre Shared Key (AES-128)
     * @return true if client was able to connect to the server
     */
    bool startClient(const std::string& host,
                     uint16_t port,
                     int reorder,
                     int32_t latency,
                     int overhead,
                     std::shared_ptr<NetworkConnection>& ctx,
                     int mtu,
                     int32_t peerIdleTimeout = 5000,
                     const std::string& psk = "");

    /**
     *
     * Starts an SRT Client with a specified local address to bind to
     *
     * @param host Remote host IP or hostname to connect to
     * @param port Remote host port to connect to
     * @param localHost Local host IP to bind to
     * @param localPort Local port to bind to, use 0 to automatically pick an unused port
     * @param reorder number of packets in re-order window
     * @param latency Max re-send window (ms) / also the delay of transmission
     * @param overhead % extra of the BW that will be allowed for re-transmission packets
     * @param ctx the context used in the receivedData and receivedDataNoCopy callback
     * @param mtu sets the MTU
     * @param peerIdleTimeout Optional Connection considered broken if no packet received before this timeout.
     * Defaults to 5 seconds.
     * @param psk Optional Pre Shared Key (AES-128)
     * @return true if configuration was ok and remote IP port could be resolved, false otherwise
     */
    bool startClient(const std::string& host,
                     uint16_t port,
                     const std::string& localHost,
                     uint16_t localPort,
                     int reorder,
                     int32_t latency,
                     int overhead,
                     std::shared_ptr<NetworkConnection>& ctx,
                     int mtu,
                     int32_t peerIdleTimeout = 5000,
                     const std::string& psk = "");

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
     * @param size size of the data
     * @param msgCtrl pointer to a SRT_MSGCTRL struct.
     * @param targetSystem the target sending the data to (used in server mode only)
     * @return true if sendData was able to send the data to the target.
     */
    bool sendData(const uint8_t* data, size_t size, SRT_MSGCTRL* msgCtrl, SRTSOCKET targetSystem = 0);

    /**
     *
     * Get connection statistics
     *
     * @param currentStats pointer to the statistics struct
     * @param clear Clears the data after reading SRTNetClearStats::yes or no
     * @param instantaneous Get the parameters now SRTNetInstant::yes or filtered values SRTNetInstant::no
     * @param targetSystem The target connection to get statistics about (used in server mode only)
     * @return true if statistics was populated.
     */
    bool getStatistics(SRT_TRACEBSTATS* currentStats, int clear, int instantaneous, SRTSOCKET targetSystem = 0);

    /**
     *
     * Get active clients (A server method)
     *
     * @param function. pass a function getting the map containing the list of active connections
     * Where the map contains the SRTSocketHandle (SRTSOCKET) and it's associated NetworkConnection you provided.
     */
    void
    getActiveClients(const std::function<void(std::map<SRTSOCKET, std::shared_ptr<NetworkConnection>>&)>& function);

    /**
     *
     * @brief Get the SRT socket and the network connection context object associated with the connected server. This
     * method only works when in client mode.
     * @returns The SRT socket and network connection context of the connected server in case this SRTNet is in client
     * mode and is connected to a SRT server. Returns {0, nullptr} if not in client mode or if in client mode and not
     * connected yet.
     *
     */
    std::pair<SRTSOCKET, std::shared_ptr<NetworkConnection>> getConnectedServer();

    /**
     * 
     * @brief Check if client is connected to remote end
     * @returns True is client is connected to the the remote end, false otherwise
    */
    bool isClientConnected() const;

    /**
     *
     * @brief Get the underlying, bound SRT socket. Works both in client and server mode.
     * @returns The bound socket in case there is one, otherwise 0.
     *
     */
    SRTSOCKET getBoundSocket() const;

    /**
     *
     * @brief Get the current operating mode.
     * @returns The operating mode.
     *
    */
    Mode getCurrentMode() const;

    /**
     *
     * @brief Default log handler which outputs the message to std::cout
     * @param opaque not used
     * @param level the log level of this message
     * @param file name of the file where this message is logged
     * @param line line number in the file
     * @param area not used
     * @param message the line to be logged
     *
     */
    static void defaultLogHandler(void* opaque, int level, const char* file, int line, const char* area, const char* message);

    /**
     *
     * @brief Set log handler
     * @param handler the new log handler to be used
     * @param loglevel the log level to use
     *
     */
    void setLogHandler(SRT_LOG_HANDLER_FN* handler, int loglevel);

    /// Callback handling connecting clients (only server mode)
    std::function<std::shared_ptr<NetworkConnection>(struct sockaddr& sin,
                                                     SRTSOCKET newSocket,
                                                     std::shared_ptr<NetworkConnection>& ctx)>
        clientConnected = nullptr;
    /// Callback receiving data type vector
    std::function<void(std::unique_ptr<std::vector<uint8_t>>& data,
                       SRT_MSGCTRL& msgCtrl,
                       std::shared_ptr<NetworkConnection>& ctx,
                       SRTSOCKET socket)>
        receivedData = nullptr;

    /// Callback receiving data no copy
    std::function<void(const uint8_t* data,
                       size_t size,
                       SRT_MSGCTRL& msgCtrl,
                       std::shared_ptr<NetworkConnection>& ctx,
                       SRTSOCKET socket)>
        receivedDataNoCopy = nullptr;

    /// Callback handling disconnecting clients (server and client mode)
    std::function<void(std::shared_ptr<NetworkConnection>& ctx, SRTSOCKET lSocket)> clientDisconnected = nullptr;

    // delete copy and move constructors and assign operators
    SRTNet(SRTNet const&) = delete;            // Copy construct
    SRTNet(SRTNet&&) = delete;                 // Move construct
    SRTNet& operator=(SRTNet const&) = delete; // Copy assign
    SRTNet& operator=(SRTNet&&) = delete;      // Move assign

private:
    // Internal variables and methods

    void waitForSRTClient(bool singleSender);

    void serverEventHandler();

    void clientWorker();

    void closeAllClientSockets();

    bool createClientSocket();

    void connectClient();

    SRT_LOG_HANDLER_FN* logHandler = defaultLogHandler;

    // Server active? true == yes
    std::atomic<bool> mServerActive = {false};
    // Client active? true == yes
    std::atomic<bool> mClientActive = {false};

    std::thread mWorkerThread;
    std::thread mEventThread;

    SRTSOCKET mContext = 0;
    int mPollID = 0;
    mutable std::mutex mNetMtx;
    Mode mCurrentMode = Mode::unknown;
    std::map<SRTSOCKET, std::shared_ptr<NetworkConnection>> mClientList = {};
    std::mutex mClientListMtx;
    std::shared_ptr<NetworkConnection> mClientContext = nullptr;
    std::shared_ptr<NetworkConnection> mConnectionContext = nullptr;
    std::atomic<bool> mClientConnected = false;
    std::string mCallerHost;
    uint16_t mCallerPort;

    std::string mCallerLocalHost;
    uint16_t mCallerLocalPort;
    int mCallerReorder;
    int32_t mCallerLatency;
    int mCallerOverhead;
    int mCallerMtu;
    int32_t mCallerPeerIdleTimeout;
    std::string mCallerPsk;
};
