//      __________  ____     _____ ____  ______   _       ______  ___    ____  ____  __________
//     / ____/ __ \/ __ \   / ___// __ \/_  __/  | |     / / __ \/   |  / __ \/ __ \/ ____/ __ \
//    / /   / /_/ / /_/ /   \__ \/ /_/ / / /     | | /| / / /_/ / /| | / /_/ / /_/ / __/ / /_/ /
//   / /___/ ____/ ____/   ___/ / _, _/ / /      | |/ |/ / _, _/ ___ |/ ____/ ____/ /___/ _, _/
//   \____/_/   /_/       /____/_/ |_| /_/       |__/|__/_/ |_/_/  |_/_/   /_/   /_____/_/ |_|
//
// Created by Anders Cedronius on 2019-04-21.
//

#include "SRTNet.h"
#include "SRTNetInternal.h"

SRTNet::SRTNet() {
    mServerActive = false;
    mServerListenThreadActive = false;
    mClientActive = false;
    mClientThreadActive = false;
    SRT_LOGGER(true, LOGG_NOTIFY, "SRTNet constructed");
}

SRTNet::~SRTNet() {
    SRT_LOGGER(true, LOGG_NOTIFY, "SRTNet destruct");
}

void SRTNet::closeAllClientSockets() {
    mClientListMtx.lock();
    int lResult = SRT_ERROR;
    for (auto &rClient: mClientList) {
        SRTSOCKET lSocket = rClient.first;
        lResult = srt_close(lSocket);
        if(clientDisconnected) {
            clientDisconnected(rClient.second, lSocket);
        }
        if (lResult == SRT_ERROR) {
            SRT_LOGGER(true, LOGG_ERROR, "srt_close failed: " << srt_getlasterror_str());
        }
    }
    mClientList.clear();
    mClientListMtx.unlock();
}

bool SRTNet::isIPv4(const std::string &rStr) {
    struct sockaddr_in lSa = {0};
    return inet_pton(AF_INET, rStr.c_str(), &(lSa.sin_addr)) != 0;
}

bool SRTNet::isIPv6(const std::string &rStr) {
    struct sockaddr_in6 sa = {0};
    return inet_pton(AF_INET6, rStr.c_str(), &(sa.sin6_addr)) != 0;
}

bool SRTNet::startServer(std::string lIp, uint16_t lPort, int lReorder, int32_t lLatency, int lOverhead, int lMtu,
                         std::string lPsk, std::shared_ptr<NetworkConnection> pCtx) {



    struct sockaddr_in lSaV4 = {0};
    struct sockaddr_in6 lSaV6 = {0};

    int lIpType = AF_INET;
    if (isIPv4(lIp)) {
        lIpType = AF_INET;
    } else if (isIPv6(lIp)) {
        lIpType = AF_INET6;
    } else {
        SRT_LOGGER(true, LOGG_ERROR, " " << "Provided IP-Address not valid.");
    }

    std::lock_guard<std::mutex> lLock(mNetMtx);
    int lResult = 0;

    int32_t lYes = 1;
    if (mCurrentMode != Mode::unknown) {
        SRT_LOGGER(true, LOGG_ERROR, " " << "SRTNet mode is already set");
        return false;
    }

    if (!clientConnected) {
        SRT_LOGGER(true, LOGG_FATAL, "waitForSRTClient needs clientConnected callback method terminating server!");
        return false;
    }

    mConnectionContext = pCtx; //retain the optional context

    srt_startup();
    mContext = srt_create_socket();
    if (mContext == SRT_ERROR) {
        SRT_LOGGER(true, LOGG_FATAL, "srt_socket: " << srt_getlasterror_str());
        return false;
    }

    if (lIpType == AF_INET) {
        lSaV4.sin_family = AF_INET;
        lSaV4.sin_port = htons(lPort);
        if (inet_pton(AF_INET, lIp.c_str(), &lSaV4.sin_addr) != 1) {
            SRT_LOGGER(true, LOGG_FATAL, "inet_pton failed ");
            srt_close(mContext);
            return false;
        }
    }

    if (lIpType == AF_INET6) {
        lSaV6.sin6_family = AF_INET6;
        lSaV6.sin6_port = htons(lPort);
        if (inet_pton(AF_INET6, lIp.c_str(), &lSaV6.sin6_addr) != 1) {
            SRT_LOGGER(true, LOGG_FATAL, "inet_pton failed ");
            srt_close(mContext);
            return false;
        }
    }

    lResult = srt_setsockflag(mContext, SRTO_RCVSYN, &lYes, sizeof lYes);
    if (lResult == SRT_ERROR) {
        SRT_LOGGER(true, LOGG_FATAL, "srt_setsockflag SRTO_RCVSYN: " << srt_getlasterror_str());
        return false;
    }

    lResult = srt_setsockflag(mContext, SRTO_LATENCY, &lLatency, sizeof lLatency);
    if (lResult == SRT_ERROR) {
        SRT_LOGGER(true, LOGG_FATAL, "srt_setsockflag SRTO_LATENCY: " << srt_getlasterror_str());
        return false;
    }

    lResult = srt_setsockflag(mContext, SRTO_LOSSMAXTTL, &lReorder, sizeof lReorder);
    if (lResult == SRT_ERROR) {
        SRT_LOGGER(true, LOGG_FATAL, "srt_setsockflag SRTO_LOSSMAXTTL: " << srt_getlasterror_str());
        return false;
    }

    lResult = srt_setsockflag(mContext, SRTO_OHEADBW, &lOverhead, sizeof lOverhead);
    if (lResult == SRT_ERROR) {
        SRT_LOGGER(true, LOGG_FATAL, "srt_setsockflag SRTO_OHEADBW: " << srt_getlasterror_str());
        return false;
    }

    lResult = srt_setsockflag(mContext, SRTO_PAYLOADSIZE, &lMtu, sizeof lMtu);
    if (lResult == SRT_ERROR) {
        SRT_LOGGER(true, LOGG_FATAL, "srt_setsockflag SRTO_PAYLOADSIZE: " << srt_getlasterror_str());
        return false;
    }

    if (lPsk.length()) {
        int32_t lAes128 = 16;
        lResult = srt_setsockflag(mContext, SRTO_PBKEYLEN, &lAes128, sizeof lAes128);
        if (lResult == SRT_ERROR) {
            SRT_LOGGER(true, LOGG_FATAL, "srt_setsockflag SRTO_PBKEYLEN: " << srt_getlasterror_str());
            return false;
        }

        lResult = srt_setsockflag(mContext, SRTO_PASSPHRASE, lPsk.c_str(), lPsk.length());
        if (lResult == SRT_ERROR) {
            SRT_LOGGER(true, LOGG_FATAL, "srt_setsockflag SRTO_PASSPHRASE: " << srt_getlasterror_str());
            return false;
        }
    }

    if (lIpType == AF_INET) {
        lResult = srt_bind(mContext, (struct sockaddr *) &lSaV4, sizeof lSaV4);
        if (lResult == SRT_ERROR) {
            SRT_LOGGER(true, LOGG_FATAL, "srt_bind: " << srt_getlasterror_str());
            srt_close(mContext);
            return false;
        }
    }

    if (lIpType == AF_INET6) {
        lResult = srt_bind(mContext, (struct sockaddr *) &lSaV6, sizeof lSaV6);
        if (lResult == SRT_ERROR) {
            SRT_LOGGER(true, LOGG_FATAL, "srt_bind: " << srt_getlasterror_str());
            srt_close(mContext);
            return false;
        }
    }

    lResult = srt_listen(mContext, 2);
    if (lResult == SRT_ERROR) {
        SRT_LOGGER(true, LOGG_FATAL, "srt_listen: " << srt_getlasterror_str());
        srt_close(mContext);
        return false;
    }
    mServerActive = true;
    mCurrentMode = Mode::server;
    std::thread(std::bind(&SRTNet::waitForSRTClient, this)).detach();
    return true;
}


void SRTNet::serverEventHandler() {
    SRT_EPOLL_EVENT lReady[MAX_WORKERS];
    while (mServerActive) {
        int lRet = srt_epoll_uwait(mPollID, &lReady[0], 5, 1000);
        if (lRet == MAX_WORKERS + 1) {
            lRet--;
        }

        if (lRet > 0) {
            for (int i = 0; i < lRet; i++) {
                uint8_t lMsg[2048];
                SRT_MSGCTRL lThisMSGCTRL = srt_msgctrl_default;
                SRTSOCKET lThisSocket = lReady[i].fd;
                int lResult = srt_recvmsg2(lThisSocket, (char *) lMsg, sizeof lMsg, &lThisMSGCTRL);
                if (lResult == SRT_ERROR) {
                    SRT_LOGGER(true, LOGG_ERROR, "srt_recvmsg error: " << lResult << " " << srt_getlasterror_str());
                    mClientListMtx.lock();
                    auto lCtx = mClientList.find(lThisSocket)->second;
                    mClientList.erase(mClientList.find(lThisSocket)->first);
                    srt_epoll_remove_usock(mPollID, lThisSocket);
                    srt_close(lThisSocket);
                    mClientListMtx.unlock();
                    if(clientDisconnected) {
                        clientDisconnected(lCtx, lThisSocket);
                    }
                } else if (lResult > 0 && receivedData) {
                    auto pointer = std::make_unique<std::vector<uint8_t>>(lMsg, lMsg + lResult);
                    receivedData(pointer, lThisMSGCTRL, mClientList.find(lThisSocket)->second, lThisSocket);
                } else if (lResult > 0 && receivedDataNoCopy) {
                    receivedDataNoCopy(lMsg, lResult, lThisMSGCTRL, mClientList.find(lThisSocket)->second, lThisSocket);
                }
            }
        } else if (lRet == -1) {
            SRT_LOGGER(true, LOGG_ERROR, "epoll error: " << srt_getlasterror_str());
        }

    }
    SRT_LOGGER(true, LOGG_NOTIFY, "serverEventHandler exit");
    srt_epoll_release(mPollID);
}

void SRTNet::waitForSRTClient() {
    int lResult = SRT_ERROR;
    mPollID = srt_epoll_create();
    srt_epoll_set(mPollID, SRT_EPOLL_ENABLE_EMPTY);
    std::thread(std::bind(&SRTNet::serverEventHandler, this)).detach();

    mServerListenThreadActive = true;

    closeAllClientSockets();

    while (mServerActive) {
        struct sockaddr_storage lTheir_addr = {0};
        SRT_LOGGER(true, LOGG_NOTIFY, "SRT Server wait for client");
        int lAddr_size = sizeof lTheir_addr;
        SRTSOCKET lNewSocketCandidate = srt_accept(mContext, (struct sockaddr *) &lTheir_addr, &lAddr_size);
        if (lNewSocketCandidate == -1) {
            continue;
        }
        SRT_LOGGER(true, LOGG_NOTIFY, "Client connected: " << lNewSocketCandidate);
        auto lCtx = clientConnected(*(struct sockaddr *) &lTheir_addr, lNewSocketCandidate, mConnectionContext);

        if (lCtx) {
            const int lEvents = SRT_EPOLL_IN | SRT_EPOLL_ERR;
            mClientListMtx.lock();
            mClientList[lNewSocketCandidate] = lCtx;
            mClientListMtx.unlock();
            lResult = srt_epoll_add_usock(mPollID, lNewSocketCandidate, &lEvents);
            if (lResult == SRT_ERROR) {
                SRT_LOGGER(true, LOGG_FATAL, "srt_epoll_add_usock error: " << srt_getlasterror_str());
            }
        } else {
            close(lNewSocketCandidate);
        }
    }
    mServerActive = false;
    mServerListenThreadActive = false;
}

void SRTNet::getActiveClients(const std::function<void(std::map<SRTSOCKET, std::shared_ptr<NetworkConnection>> &)>& rFunction) {
    mClientListMtx.lock();
    rFunction(mClientList);
    mClientListMtx.unlock();
}

//Host can provide a IP or name meaning any IPv4 or IPv6 address or name type www.google.com
//There is no IP-Version preference if a name is given. the first IP-version found will be used
bool SRTNet::startClient(std::string lHost,
                         uint16_t lPort,
                         int lReorder,
                         int32_t lLatency,
                         int lOverhead,
                         std::shared_ptr<NetworkConnection> &rpCtx,
                         int lMtu,
                         std::string lPsk) {
    std::lock_guard<std::mutex> lLock(mNetMtx);
    if (mCurrentMode != Mode::unknown) {
        SRT_LOGGER(true, LOGG_ERROR, " " << "SRTNet mode is already set");
        return false;
    }

    mClientContext = rpCtx;

    int lResult = 0;
    int32_t lYes = 1;
    SRT_LOGGER(true, LOGG_NOTIFY, "SRT client startup");

    srt_startup();
    mContext = srt_create_socket();
    if (mContext == SRT_ERROR) {
        SRT_LOGGER(true, LOGG_FATAL, "srt_socket: " << srt_getlasterror_str());
        return false;
    }

    lResult = srt_setsockflag(mContext, SRTO_SENDER, &lYes, sizeof lYes);
    if (lResult == SRT_ERROR) {
        SRT_LOGGER(true, LOGG_FATAL, "srt_setsockflag SRTO_SENDER: " << srt_getlasterror_str());
        return false;
    }

    lResult = srt_setsockflag(mContext, SRTO_LATENCY, &lLatency, sizeof lLatency);
    if (lResult == SRT_ERROR) {
        SRT_LOGGER(true, LOGG_FATAL, "srt_setsockflag SRTO_LATENCY: " << srt_getlasterror_str());
        return false;
    }

    lResult = srt_setsockflag(mContext, SRTO_LOSSMAXTTL, &lReorder, sizeof lReorder);
    if (lResult == SRT_ERROR) {
        SRT_LOGGER(true, LOGG_FATAL, "srt_setsockflag SRTO_LOSSMAXTTL: " << srt_getlasterror_str());
        return false;
    }

    lResult = srt_setsockflag(mContext, SRTO_OHEADBW, &lOverhead, sizeof lOverhead);
    if (lResult == SRT_ERROR) {
        SRT_LOGGER(true, LOGG_FATAL, "srt_setsockflag SRTO_OHEADBW: " << srt_getlasterror_str());
        return false;
    }

    lResult = srt_setsockflag(mContext, SRTO_PAYLOADSIZE, &lMtu, sizeof lMtu);
    if (lResult == SRT_ERROR) {
        SRT_LOGGER(true, LOGG_FATAL, "srt_setsockflag SRTO_PAYLOADSIZE: " << srt_getlasterror_str());
        return false;
    }

    if (lPsk.length()) {
        int32_t aes128 = 16;
        lResult = srt_setsockflag(mContext, SRTO_PBKEYLEN, &aes128, sizeof aes128);
        if (lResult == SRT_ERROR) {
            SRT_LOGGER(true, LOGG_FATAL, "srt_setsockflag SRTO_PBKEYLEN: " << srt_getlasterror_str());
            return false;
        }

        lResult = srt_setsockflag(mContext, SRTO_PASSPHRASE, lPsk.c_str(), lPsk.length());
        if (lResult == SRT_ERROR) {
            SRT_LOGGER(true, LOGG_FATAL, "srt_setsockflag SRTO_PASSPHRASE: " << srt_getlasterror_str());
            return false;
        }
    }

    //get all addresses for connection
    struct addrinfo lHints = {0};
    struct addrinfo *pSvr;
    struct addrinfo *pHld;
    lHints.ai_socktype = SOCK_DGRAM;
    lHints.ai_protocol = IPPROTO_UDP;
    lHints.ai_family = AF_UNSPEC;
    std::stringstream portAsString;
    portAsString << lPort;
    lResult = getaddrinfo(lHost.c_str(), portAsString.str().c_str(), &lHints, &pSvr);
    if (lResult) {
        SRT_LOGGER(true, LOGG_FATAL,
                   "Failed getting the IP target for > " << lHost << ":" << unsigned(lPort) << " Errno: "
                                                         << unsigned(lResult));
        return false;
    }

    SRT_LOGGER(true, LOGG_NOTIFY, "SRT connect");
    for (pHld = pSvr; pHld; pHld = pHld->ai_next) {
        lResult = srt_connect(mContext, (struct sockaddr *) pHld->ai_addr, pHld->ai_addrlen);
        if (lResult != SRT_ERROR) {
            SRT_LOGGER(true, LOGG_NOTIFY, "Connected to SRT Server " << std::endl)
            break;
        }
    }
    if (lResult == SRT_ERROR) {
        srt_close(mContext);
        freeaddrinfo(pSvr);
        SRT_LOGGER(true, LOGG_FATAL, "srt_connect failed " << std::endl)
        return false;
    }
    freeaddrinfo(pSvr);
    mCurrentMode = Mode::client;
    mClientActive = true;
    std::thread(std::bind(&SRTNet::clientWorker, this)).detach();
    return true;
}

void SRTNet::clientWorker() {
    int lResult = 0;
    mClientThreadActive = true;
    while (mClientActive) {
        uint8_t lMsg[2048];
        SRT_MSGCTRL lThisMSGCTRL = srt_msgctrl_default;
        lResult = srt_recvmsg2(mContext, (char *) lMsg, sizeof lMsg, &lThisMSGCTRL);
        if (lResult == SRT_ERROR) {
            if (mClientActive) {
                SRT_LOGGER(true, LOGG_ERROR, "srt_recvmsg error: " << srt_getlasterror_str());
            }
            break;
        } else if (lResult > 0 && receivedData) {
            auto lpData = std::make_unique<std::vector<uint8_t>>(lMsg, lMsg + lResult);
            receivedData(lpData, lThisMSGCTRL, mClientContext, mContext);
        } else if (lResult > 0 && receivedDataNoCopy) {
            receivedDataNoCopy(lMsg, lResult, lThisMSGCTRL, mClientContext, mContext);
        }
    }
    mClientActive = false;
    mClientThreadActive = false;
}

bool SRTNet::sendData(uint8_t *pData, size_t len, SRT_MSGCTRL *pMsgCtrl, SRTSOCKET lTargetSystem) {
    int lResult;

    if (mCurrentMode == Mode::client && mContext && mClientActive) {
        lResult = srt_sendmsg2(mContext, (const char *) pData, len, pMsgCtrl);
    } else if (mCurrentMode == Mode::server && lTargetSystem && mServerActive) {
        lResult = srt_sendmsg2(lTargetSystem, (const char *) pData, len, pMsgCtrl);
    } else {
        SRT_LOGGER(true, LOGG_ERROR, "Can't send data, the client is not active.");
        return false;
    }

    if (lResult == SRT_ERROR) {
        SRT_LOGGER(true, LOGG_ERROR, "srt_sendmsg2 failed: " << srt_getlasterror_str());
        return false;
    }

    if (lResult != len) {
        SRT_LOGGER(true, LOGG_ERROR, "Failed sending all data");
        return false;
    }

    return true;
}

bool SRTNet::stop() {
    int lResult = SRT_ERROR;
    std::lock_guard<std::mutex> lLock(mNetMtx);
    if (mCurrentMode == Mode::server) {
        mServerActive = false;
        if (mContext) {
            lResult = srt_close(mContext);
            if (lResult == SRT_ERROR) {
                SRT_LOGGER(true, LOGG_ERROR, "srt_close failed: " << srt_getlasterror_str());
                return false;
            }
        }
        closeAllClientSockets();
        int lThreadRunning = 1000;
        while (mServerListenThreadActive) {
            std::this_thread::sleep_for(std::chrono::microseconds(1000));
            if (!--lThreadRunning) {
                SRT_LOGGER(true, LOGG_FATAL,
                           "SRTNet server thread is still running after 1 second. Crash and burn baby!!");
                break;
            }
        }
        srt_cleanup();
        SRT_LOGGER(true, LOGG_NOTIFY, "Server stopped");
        mCurrentMode = Mode::unknown;
        return true;
    } else if (mCurrentMode == Mode::client) {
        mClientActive = false;
        if (mContext) {
            lResult = srt_close(mContext);
            if (lResult == SRT_ERROR) {
                SRT_LOGGER(true, LOGG_ERROR, "srt_close failed: " << srt_getlasterror_str());
                return false;
            }
        }

        int lThreadRunning = 1000; //Timeout after 1000ms
        while (mClientThreadActive) {
            std::this_thread::sleep_for(std::chrono::microseconds(1000));
            if (!--lThreadRunning) {
                SRT_LOGGER(true, LOGG_FATAL,
                           "SRTNet client thread is still running after 1 second. Crash and burn baby!!");
                break;
            }
        }
        srt_cleanup();
        SRT_LOGGER(true, LOGG_NOTIFY, "Client stopped");
        mCurrentMode = Mode::unknown;
        return true;
    }
    SRT_LOGGER(true, LOGG_ERROR, "SRTNet nothing to stop");
    return true;
}

bool SRTNet::getStatistics(SRT_TRACEBSTATS *pCurrentStats, int lClear, int lInstantaneous, SRTSOCKET lTargetSystem) {
    std::lock_guard<std::mutex> lLock(mNetMtx);
    int lResult = SRT_ERROR;
    if (mCurrentMode == Mode::client && mClientActive && mContext) {
        lResult = srt_bistats(mContext, pCurrentStats, lClear, lInstantaneous);
        if (lResult == SRT_ERROR) {
            SRT_LOGGER(true, LOGG_ERROR, "srt_bistats failed: " << srt_getlasterror_str());
            return false;
        }
    } else if (mCurrentMode == Mode::server && mServerActive && lTargetSystem) {
        lResult = srt_bistats(lTargetSystem, pCurrentStats, lClear, lInstantaneous);
        if (lResult == SRT_ERROR) {
            SRT_LOGGER(true, LOGG_ERROR, "srt_bistats failed: " << srt_getlasterror_str());
            return false;
        }
    } else {
        SRT_LOGGER(true, LOGG_ERROR, "Statistics not available");
        return false;
    }
    return true;
}
