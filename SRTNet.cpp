//      __________  ____     _____ ____  ______   _       ______  ___    ____  ____  __________
//     / ____/ __ \/ __ \   / ___// __ \/_  __/  | |     / / __ \/   |  / __ \/ __ \/ ____/ __ \
//    / /   / /_/ / /_/ /   \__ \/ /_/ / / /     | | /| / / /_/ / /| | / /_/ / /_/ / __/ / /_/ /
//   / /___/ ____/ ____/   ___/ / _, _/ / /      | |/ |/ / _, _/ ___ |/ ____/ ____/ /___/ _, _/
//   \____/_/   /_/       /____/_/ |_| /_/       |__/|__/_/ |_/_/  |_/_/   /_/   /_____/_/ |_|
//
// Created by Anders Cedronius on 2019-04-21.
//

#include "SRTNet.h"

#include <optional>

#include "SRTNetInternal.h"

namespace {

/// Wrapper around sockaddr_in
class SocketAddress {
public:
    SocketAddress(const std::string& ip, uint16_t port)
        : mIP(ip)
        , mPort(port) {
    }

    ///
    /// @return True if this SocketAddress is an IPv4 address
    bool isIPv4() {
        sockaddr_in sa{};
        return inet_pton(AF_INET, mIP.c_str(), &sa.sin_addr) != 0;
    }

    ///
    /// @return True if this SocketAddress is an IPv6 address
    bool isIPv6() {
        sockaddr_in6 sa{};
        return inet_pton(AF_INET6, mIP.c_str(), &sa.sin6_addr) != 0;
    }

    ///
    /// @return Get this address as an IPv4 sockaddr_in, nullopt if not a valid IPv4 address
    [[nodiscard]] std::optional<sockaddr_in> getIPv4() const {
        sockaddr_in socketAddressV4 = {0};
        socketAddressV4.sin_family = AF_INET;
        socketAddressV4.sin_port = htons(mPort);
        if (inet_pton(AF_INET, mIP.c_str(), &socketAddressV4.sin_addr) != 1) {
            return std::nullopt;
        }
        return socketAddressV4;
    }

    ///
    /// @return Get this address as an IPv6 sockaddr_in6, nullopt if not a valid IPv6 address
    [[nodiscard]] std::optional<sockaddr_in6> getIPv6() const {
        sockaddr_in6 socketAddressV6 = {0};
        socketAddressV6.sin6_family = AF_INET6;
        socketAddressV6.sin6_port = htons(mPort);
        if (inet_pton(AF_INET6, mIP.c_str(), &socketAddressV6.sin6_addr) != 1) {
            return std::nullopt;
        }
        return socketAddressV6;
    }

private:
    std::string mIP;
    uint16_t mPort;
};

} // namespace

SRTNet::SRTNet() {
    SRT_LOGGER(true, LOGG_NOTIFY, "SRTNet constructed");
}

SRTNet::~SRTNet() {
    stop();
    SRT_LOGGER(true, LOGG_NOTIFY, "SRTNet destruct")
}

void SRTNet::closeAllClientSockets() {
    std::lock_guard<std::mutex> lock(mClientListMtx);
    for (auto& client : mClientList) {
        SRTSOCKET socket = client.first;
        int result = srt_close(socket);
        if (clientDisconnected) {
            clientDisconnected(client.second, socket);
        }
        if (result == SRT_ERROR) {
            SRT_LOGGER(true, LOGG_ERROR, "srt_close failed: " << srt_getlasterror_str());
        }
    }
    mClientList.clear();
}

bool SRTNet::startServer(const std::string& ip,
                         uint16_t port,
                         int reorder,
                         int32_t latency,
                         int overhead,
                         int mtu,
                         const std::string& psk,
                         bool singleSender,
                         std::shared_ptr<NetworkConnection> ctx) {
    std::lock_guard<std::mutex> lock(mNetMtx);

    SocketAddress socketAddress(ip, port);
    if (!socketAddress.isIPv4() && !socketAddress.isIPv6()) {
        SRT_LOGGER(true, LOGG_ERROR, "Failed to parse socket address");
        return false;
    }

    if (mCurrentMode != Mode::unknown) {
        SRT_LOGGER(true, LOGG_ERROR,
                   " "
                       << "SRTNet mode is already set");
        return false;
    }

    if (!clientConnected) {
        SRT_LOGGER(true, LOGG_FATAL, "waitForSRTClient needs clientConnected callback method terminating server!");
        return false;
    }

    mConnectionContext = ctx; // retain the optional context

    mContext = srt_create_socket();
    if (mContext == SRT_ERROR) {
        SRT_LOGGER(true, LOGG_FATAL, "srt_socket: " << srt_getlasterror_str());
        return false;
    }

    int32_t yes = 1;
    int result = srt_setsockflag(mContext, SRTO_RCVSYN, &yes, sizeof(yes));
    if (result == SRT_ERROR) {
        SRT_LOGGER(true, LOGG_FATAL, "srt_setsockflag SRTO_RCVSYN: " << srt_getlasterror_str());
        return false;
    }

    result = srt_setsockflag(mContext, SRTO_LATENCY, &latency, sizeof(latency));
    if (result == SRT_ERROR) {
        SRT_LOGGER(true, LOGG_FATAL, "srt_setsockflag SRTO_LATENCY: " << srt_getlasterror_str());
        return false;
    }

    result = srt_setsockflag(mContext, SRTO_LOSSMAXTTL, &reorder, sizeof(reorder));
    if (result == SRT_ERROR) {
        SRT_LOGGER(true, LOGG_FATAL, "srt_setsockflag SRTO_LOSSMAXTTL: " << srt_getlasterror_str());
        return false;
    }

    result = srt_setsockflag(mContext, SRTO_OHEADBW, &overhead, sizeof(overhead));
    if (result == SRT_ERROR) {
        SRT_LOGGER(true, LOGG_FATAL, "srt_setsockflag SRTO_OHEADBW: " << srt_getlasterror_str());
        return false;
    }

    result = srt_setsockflag(mContext, SRTO_PAYLOADSIZE, &mtu, sizeof(mtu));
    if (result == SRT_ERROR) {
        SRT_LOGGER(true, LOGG_FATAL, "srt_setsockflag SRTO_PAYLOADSIZE: " << srt_getlasterror_str());
        return false;
    }

    if (psk.length()) {
        int32_t aes128 = 16;
        result = srt_setsockflag(mContext, SRTO_PBKEYLEN, &aes128, sizeof(aes128));
        if (result == SRT_ERROR) {
            SRT_LOGGER(true, LOGG_FATAL, "srt_setsockflag SRTO_PBKEYLEN: " << srt_getlasterror_str());
            return false;
        }

        result = srt_setsockflag(mContext, SRTO_PASSPHRASE, psk.c_str(), psk.length());
        if (result == SRT_ERROR) {
            SRT_LOGGER(true, LOGG_FATAL, "srt_setsockflag SRTO_PASSPHRASE: " << srt_getlasterror_str());
            return false;
        }
    }

    std::optional<sockaddr_in> ipv4Address = socketAddress.getIPv4();
    if (ipv4Address.has_value()) {
        result = srt_bind(mContext, reinterpret_cast<sockaddr*>(&ipv4Address.value()), sizeof(ipv4Address.value()));
        if (result == SRT_ERROR) {
            SRT_LOGGER(true, LOGG_FATAL, "srt_bind: " << srt_getlasterror_str());
            srt_close(mContext);
            return false;
        }
    }

    std::optional<sockaddr_in6> ipv6Address = socketAddress.getIPv6();
    if (ipv6Address.has_value()) {
        result = srt_bind(mContext, reinterpret_cast<sockaddr*>(&ipv6Address.value()), sizeof(ipv6Address.value()));
        if (result == SRT_ERROR) {
            SRT_LOGGER(true, LOGG_FATAL, "srt_bind: " << srt_getlasterror_str());
            srt_close(mContext);
            return false;
        }
    }

    result = srt_listen(mContext, 2);
    if (result == SRT_ERROR) {
        SRT_LOGGER(true, LOGG_FATAL, "srt_listen: " << srt_getlasterror_str());
        srt_close(mContext);
        return false;
    }
    mServerActive = true;
    mCurrentMode = Mode::server;
    mWorkerThread = std::thread(&SRTNet::waitForSRTClient, this, singleSender);
    return true;
}

void SRTNet::serverEventHandler() {
    SRT_EPOLL_EVENT ready[MAX_WORKERS];
    while (mServerActive) {
        int ret = srt_epoll_uwait(mPollID, &ready[0], 5, 1000);
        if (ret == MAX_WORKERS + 1) {
            ret--;
        }

        if (ret > 0) {
            for (size_t i = 0; i < ret; i++) {
                uint8_t msg[2048];
                SRT_MSGCTRL thisMSGCTRL = srt_msgctrl_default;
                SRTSOCKET thisSocket = ready[i].fd;
                int result = srt_recvmsg2(thisSocket, reinterpret_cast<char*>(msg), sizeof(msg), &thisMSGCTRL);

                std::lock_guard<std::mutex> lock(mClientListMtx);
                auto iterator = mClientList.find(thisSocket);
                if (result == SRT_ERROR) {
                    SRT_LOGGER(true, LOGG_ERROR, "srt_recvmsg error: " << result << " " << srt_getlasterror_str());
                    if (iterator == mClientList.end()) {
                        continue; // This client has already been removed by closeAllClientSockets()
                    }
                    auto ctx = iterator->second;
                    mClientList.erase(iterator->first);
                    srt_epoll_remove_usock(mPollID, thisSocket);
                    srt_close(thisSocket);
                    if (clientDisconnected) {
                        clientDisconnected(ctx, thisSocket);
                    }
                } else if (result > 0 && receivedData) {
                    auto pointer = std::make_unique<std::vector<uint8_t>>(msg, msg + result);
                    receivedData(pointer, thisMSGCTRL, iterator->second, thisSocket);
                } else if (result > 0 && receivedDataNoCopy) {
                    receivedDataNoCopy(msg, result, thisMSGCTRL, iterator->second, thisSocket);
                }
            }
        } else if (ret == -1) {
            SRT_LOGGER(true, LOGG_ERROR, "epoll error: " << srt_getlasterror_str());
        }
    }
    SRT_LOGGER(true, LOGG_NOTIFY, "serverEventHandler exit");

    srt_epoll_release(mPollID);
}

void SRTNet::waitForSRTClient(bool singleSender) {
    int result = SRT_ERROR;
    mPollID = srt_epoll_create();
    srt_epoll_set(mPollID, SRT_EPOLL_ENABLE_EMPTY);
    mEventThread = std::thread(&SRTNet::serverEventHandler, this);

    closeAllClientSockets();

    while (mServerActive) {
        struct sockaddr_storage theirAddr = {0};
        SRT_LOGGER(true, LOGG_NOTIFY, "SRT Server wait for client");
        int addrSize = sizeof(theirAddr);
        SRTSOCKET newSocketCandidate = srt_accept(mContext, reinterpret_cast<sockaddr*>(&theirAddr), &addrSize);
        if (newSocketCandidate == -1) {
            continue;
        }
        SRT_LOGGER(true, LOGG_NOTIFY, "Client connected: " << newSocketCandidate);
        auto ctx = clientConnected(*reinterpret_cast<sockaddr*>(&theirAddr), newSocketCandidate, mConnectionContext);

        if (ctx) {
            const int events = SRT_EPOLL_IN | SRT_EPOLL_ERR;
            std::lock_guard<std::mutex> lock(mClientListMtx);
            mClientList[newSocketCandidate] = ctx;
            result = srt_epoll_add_usock(mPollID, newSocketCandidate, &events);
            if (result == SRT_ERROR) {
                SRT_LOGGER(true, LOGG_FATAL, "srt_epoll_add_usock error: " << srt_getlasterror_str());
            }

            if (singleSender) {
                int result = srt_close(mContext);
                if (result == SRT_ERROR) {
                    SRT_LOGGER(true, LOGG_ERROR, "srt_close failed: " << srt_getlasterror_str());
                }
                break;
            }
        } else {
            close(newSocketCandidate);
        }
    }
}

void SRTNet::getActiveClients(
    const std::function<void(std::map<SRTSOCKET, std::shared_ptr<NetworkConnection>>&)>& function) {
    std::lock_guard<std::mutex> lock(mClientListMtx);
    function(mClientList);
}

bool SRTNet::startClient(const std::string& host,
                         uint16_t port,
                         int reorder,
                         int32_t latency,
                         int overhead,
                         std::shared_ptr<NetworkConnection>& ctx,
                         int mtu,
                         const std::string& psk) {
    return startClient(host, port, "0.0.0.0", 0, reorder, latency, overhead, ctx, mtu, psk);
}

// Host can provide a IP or name meaning any IPv4 or IPv6 address or name type www.google.com
// There is no IP-Version preference if a name is given. the first IP-version found will be used
bool SRTNet::startClient(const std::string& host,
                         uint16_t port,
                         const std::string& localHost,
                         uint16_t localPort,
                         int reorder,
                         int32_t latency,
                         int overhead,
                         std::shared_ptr<NetworkConnection>& ctx,
                         int mtu,
                         const std::string& psk) {
    std::lock_guard<std::mutex> lock(mNetMtx);
    if (mCurrentMode != Mode::unknown) {
        SRT_LOGGER(true, LOGG_ERROR,
                   " "
                       << "SRTNet mode is already set");
        return false;
    }

    mClientContext = ctx;

    int result = 0;
    int32_t yes = 1;
    SRT_LOGGER(true, LOGG_NOTIFY, "SRT client startup");

    mContext = srt_create_socket();
    if (mContext == SRT_ERROR) {
        SRT_LOGGER(true, LOGG_FATAL, "srt_socket: " << srt_getlasterror_str());
        return false;
    }

    result = srt_setsockflag(mContext, SRTO_SENDER, &yes, sizeof(yes));
    if (result == SRT_ERROR) {
        SRT_LOGGER(true, LOGG_FATAL, "srt_setsockflag SRTO_SENDER: " << srt_getlasterror_str());
        return false;
    }

    result = srt_setsockflag(mContext, SRTO_LATENCY, &latency, sizeof(latency));
    if (result == SRT_ERROR) {
        SRT_LOGGER(true, LOGG_FATAL, "srt_setsockflag SRTO_LATENCY: " << srt_getlasterror_str());
        return false;
    }

    result = srt_setsockflag(mContext, SRTO_LOSSMAXTTL, &reorder, sizeof(reorder));
    if (result == SRT_ERROR) {
        SRT_LOGGER(true, LOGG_FATAL, "srt_setsockflag SRTO_LOSSMAXTTL: " << srt_getlasterror_str());
        return false;
    }

    result = srt_setsockflag(mContext, SRTO_OHEADBW, &overhead, sizeof(overhead));
    if (result == SRT_ERROR) {
        SRT_LOGGER(true, LOGG_FATAL, "srt_setsockflag SRTO_OHEADBW: " << srt_getlasterror_str());
        return false;
    }

    result = srt_setsockflag(mContext, SRTO_PAYLOADSIZE, &mtu, sizeof(mtu));
    if (result == SRT_ERROR) {
        SRT_LOGGER(true, LOGG_FATAL, "srt_setsockflag SRTO_PAYLOADSIZE: " << srt_getlasterror_str());
        return false;
    }

    if (psk.length()) {
        int32_t aes128 = 16;
        result = srt_setsockflag(mContext, SRTO_PBKEYLEN, &aes128, sizeof(aes128));
        if (result == SRT_ERROR) {
            SRT_LOGGER(true, LOGG_FATAL, "srt_setsockflag SRTO_PBKEYLEN: " << srt_getlasterror_str());
            return false;
        }

        result = srt_setsockflag(mContext, SRTO_PASSPHRASE, psk.c_str(), psk.length());
        if (result == SRT_ERROR) {
            SRT_LOGGER(true, LOGG_FATAL, "srt_setsockflag SRTO_PASSPHRASE: " << srt_getlasterror_str());
            return false;
        }
    }

    // Set local interface to bind to
    SocketAddress localSocketAddress(localHost, localPort);

    std::optional<sockaddr_in> localIPv4Address = localSocketAddress.getIPv4();
    if (localIPv4Address.has_value()) {
        result = srt_bind(mContext, reinterpret_cast<sockaddr*>(&localIPv4Address.value()),
                          sizeof(localIPv4Address.value()));
        if (result == SRT_ERROR) {
            SRT_LOGGER(true, LOGG_FATAL, "srt_bind: " << srt_getlasterror_str());
            srt_close(mContext);
            return false;
        }
    }

    std::optional<sockaddr_in6> localIPv6Address = localSocketAddress.getIPv6();
    if (localIPv6Address.has_value()) {
        result = srt_bind(mContext, reinterpret_cast<sockaddr*>(&localIPv6Address.value()),
                          sizeof(localIPv6Address.value()));
        if (result == SRT_ERROR) {
            SRT_LOGGER(true, LOGG_FATAL, "srt_bind: " << srt_getlasterror_str());
            srt_close(mContext);
            return false;
        }
    }

    if (!localIPv4Address.has_value() && !localIPv6Address.has_value()) {
        SRT_LOGGER(true, LOGG_FATAL, "Failed to parse local socket address");
        srt_close(mContext);
        return false;
    }

    // Get all remote addresses for connection
    struct addrinfo hints = {0};
    struct addrinfo* svr;
    struct addrinfo* hld;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    hints.ai_family = AF_UNSPEC;
    std::stringstream portAsString;
    portAsString << port;
    result = getaddrinfo(host.c_str(), portAsString.str().c_str(), &hints, &svr);
    if (result) {
        SRT_LOGGER(true, LOGG_FATAL,
                   "Failed getting the IP target for > " << host << ":" << port << " Errno: " << result);
        return false;
    }

    SRT_LOGGER(true, LOGG_NOTIFY, "SRT connect");
    for (hld = svr; hld; hld = hld->ai_next) {
        result = srt_connect(mContext, reinterpret_cast<sockaddr*>(hld->ai_addr), hld->ai_addrlen);
        if (result != SRT_ERROR) {
            SRT_LOGGER(true, LOGG_NOTIFY, "Connected to SRT Server " << std::endl)
            break;
        }
    }
    if (result == SRT_ERROR) {
        srt_close(mContext);
        freeaddrinfo(svr);
        SRT_LOGGER(true, LOGG_FATAL, "srt_connect failed " << std::endl);
        return false;
    }
    freeaddrinfo(svr);
    mCurrentMode = Mode::client;
    mClientActive = true;
    mWorkerThread = std::thread(&SRTNet::clientWorker, this);
    return true;
}

void SRTNet::clientWorker() {
    while (mClientActive) {
        uint8_t msg[2048];
        SRT_MSGCTRL thisMSGCTRL = srt_msgctrl_default;
        int result = srt_recvmsg2(mContext, reinterpret_cast<char*>(msg), sizeof(msg), &thisMSGCTRL);
        if (result == SRT_ERROR) {
            if (mClientActive) {
                SRT_LOGGER(true, LOGG_ERROR, "srt_recvmsg error: " << srt_getlasterror_str());
            }
            if (clientDisconnected) {
                clientDisconnected(mClientContext, mContext);
            }
            break;
        } else if (result > 0 && receivedData) {
            auto data = std::make_unique<std::vector<uint8_t>>(msg, msg + result);
            receivedData(data, thisMSGCTRL, mClientContext, mContext);
        } else if (result > 0 && receivedDataNoCopy) {
            receivedDataNoCopy(msg, result, thisMSGCTRL, mClientContext, mContext);
        }
    }
    mClientActive = false;
}

std::pair<SRTSOCKET, std::shared_ptr<SRTNet::NetworkConnection>> SRTNet::getConnectedServer() {
    if (mCurrentMode == Mode::client) {
        return {mContext, mClientContext};
    }
    return {0, nullptr};
}

SRTSOCKET SRTNet::getBoundSocket() const {
    return mContext;
}

SRTNet::Mode SRTNet::getCurrentMode() const {
    std::lock_guard<std::mutex> lock(mNetMtx);
    return mCurrentMode;
}

bool SRTNet::sendData(const uint8_t* data, size_t len, SRT_MSGCTRL* msgCtrl, SRTSOCKET targetSystem) {
    int result;

    if (mCurrentMode == Mode::client && mContext && mClientActive) {
        result = srt_sendmsg2(mContext, reinterpret_cast<const char*>(data), len, msgCtrl);
    } else if (mCurrentMode == Mode::server && targetSystem && mServerActive) {
        result = srt_sendmsg2(targetSystem, reinterpret_cast<const char*>(data), len, msgCtrl);
    } else {
        SRT_LOGGER(true, LOGG_WARN, "Can't send data, the client is not active.");
        return false;
    }

    if (result == SRT_ERROR) {
        SRT_LOGGER(true, LOGG_ERROR, "srt_sendmsg2 failed: " << srt_getlasterror_str());
        return false;
    }

    if (result != len) {
        SRT_LOGGER(true, LOGG_ERROR, "Failed sending all data");
        return false;
    }

    return true;
}

bool SRTNet::stop() {
    std::lock_guard<std::mutex> lock(mNetMtx);
    if (mCurrentMode == Mode::server) {
        mServerActive = false;
        if (mContext) {
            int result = srt_close(mContext);
            if (result == SRT_ERROR) {
                SRT_LOGGER(true, LOGG_ERROR, "srt_close failed: " << srt_getlasterror_str());
                return false;
            }
        }
        closeAllClientSockets();
        if (mWorkerThread.joinable()) {
            mWorkerThread.join();
        }
        if (mEventThread.joinable()) {
            mEventThread.join();
        }
        SRT_LOGGER(true, LOGG_NOTIFY, "Server stopped");
        mCurrentMode = Mode::unknown;
        return true;
    } else if (mCurrentMode == Mode::client) {
        mClientActive = false;
        if (mContext) {
            int result = srt_close(mContext);
            if (result == SRT_ERROR) {
                SRT_LOGGER(true, LOGG_ERROR, "srt_close failed: " << srt_getlasterror_str());
                return false;
            }
        }

        if (mWorkerThread.joinable()) {
            mWorkerThread.join();
        }
        SRT_LOGGER(true, LOGG_NOTIFY, "Client stopped");
        mCurrentMode = Mode::unknown;
        return true;
    }
    return true;
}

bool SRTNet::getStatistics(SRT_TRACEBSTATS* currentStats, int clear, int instantaneous, SRTSOCKET targetSystem) {
    std::lock_guard<std::mutex> lock(mNetMtx);
    if (mCurrentMode == Mode::client && mClientActive && mContext) {
        int result = srt_bistats(mContext, currentStats, clear, instantaneous);
        if (result == SRT_ERROR) {
            SRT_LOGGER(true, LOGG_ERROR, "srt_bistats failed: " << srt_getlasterror_str());
            return false;
        }
    } else if (mCurrentMode == Mode::server && mServerActive && targetSystem) {
        int result = srt_bistats(targetSystem, currentStats, clear, instantaneous);
        if (result == SRT_ERROR) {
            SRT_LOGGER(true, LOGG_ERROR, "srt_bistats failed: " << srt_getlasterror_str());
            return false;
        }
    } else {
        SRT_LOGGER(true, LOGG_ERROR, "Statistics not available");
        return false;
    }
    return true;
}
