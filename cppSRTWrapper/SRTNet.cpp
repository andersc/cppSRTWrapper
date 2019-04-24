//
// Created by Anders Cedronius on 2019-04-21.
//

#include "SRTNet.h"

SRTNet::SRTNet() {
    serverActive = false;
    LOGGER(true, LOGG_NOTIFY, "SRTNet constructed")
}

SRTNet::SRTNet(const SRTNet& orig) {
    LOGGER(true, LOGG_NOTIFY, "SRTNet copy constructor called")
}

SRTNet::~SRTNet() {
    if (context) {
        srt_close(context);
    }
    if (their_fd) {
        srt_close(their_fd);
    }
    LOGGER(true, LOGG_NOTIFY, "SRTNet destruct")
}

bool SRTNet::startServer(std::string ip, std::string port) {

    int result = 0;
    struct sockaddr_in sa;
    int yes = 1;

    if (currentMode != Mode::Unknown) {
        LOGGER(true, LOGG_ERROR, " " << "SRTNet mode is already set")
        return false;
    }

    context = srt_create_socket();
    if (context == SRT_ERROR) {
        LOGGER(true, LOGG_FATAL, "srt_socket: " << srt_getlasterror_str())
        return false;
    }
    pSRTHandler.intNumConnections++;

    sa.sin_family = AF_INET;
    sa.sin_port = htons(atoi(port.c_str()));
    if (inet_pton(AF_INET, ip.c_str(), &sa.sin_addr) != 1) {
        LOGGER(true, LOGG_FATAL, "inet_pton failed ")
        pSRTHandler.intNumConnections--;
        srt_close(context);
        return false;
    }

    srt_setsockflag(context, SRTO_RCVSYN, &yes, sizeof yes);

    result = srt_bind(context, (struct sockaddr*) &sa, sizeof sa);
    if (result == SRT_ERROR) {
        LOGGER(true, LOGG_FATAL, "srt_bind: " << srt_getlasterror_str())
        pSRTHandler.intNumConnections--;
        srt_close(context);
        return false;
    }

    result = srt_listen(context, 2);
    if (result == SRT_ERROR) {
        LOGGER(true, LOGG_FATAL, "srt_listen: " << srt_getlasterror_str())
        pSRTHandler.intNumConnections--;
        srt_close(context);
        return false;
    }
    std::thread(std::bind(&SRTNet::waitForSRTClient, this)).detach();
    currentMode = Mode::Server;
    serverActive = true;
    return true;
}

void SRTNet::waitForSRTClient() {
    int result = 0;
    serverThreadActive = true;
    while (serverActive) {
        struct sockaddr_storage their_addr;
        LOGGER(true, LOGG_NOTIFY, "SRT Server wait for client");
        int addr_size = sizeof their_addr;
        their_fd = srt_accept(context, (struct sockaddr *) &their_addr, &addr_size);
        pSRTHandler.intNumConnections++;
        LOGGER(true, LOGG_NOTIFY, "Client connected");
        if (!clientConnected) {
            LOGGER(true, LOGG_FATAL, "waitForSRTClient needs clientConnected");
            if (their_fd) {
                pSRTHandler.intNumConnections--;
                srt_close(their_fd);
            }
            if (context) {
                pSRTHandler.intNumConnections--;
                srt_close(context);
            }
            return;
        }
        if (clientConnected(their_addr)) {
            while (serverActive) {
                uint8_t msg[2048];
                SRT_MSGCTRL thisMSGCTRL;
                result = srt_recvmsg2(their_fd, (char*)msg, sizeof msg, &thisMSGCTRL);
                if (result == SRT_ERROR) {
                    if (serverActive) {
                        LOGGER(true, LOGG_ERROR, "srt_recvmsg error: " << srt_getlasterror_str());
                    }
                    break;
                } else if (result>0 && recievedData) {
                    recievedData(&msg[0],result);
                }
            }
        }
    }
    serverActive = false;
    serverThreadActive = false;
}

bool SRTNet::startClient(std::string host, std::string port) {

    if (currentMode != Mode::Unknown) {
        LOGGER(true, LOGG_ERROR, " " << "SRTNet mode is already set")
        return false;
    }

    int result = 0;
    int yes = 1;
    struct sockaddr_in sa;

    LOGGER(true, LOGG_NOTIFY, "SRT client startup")

    context = srt_create_socket();
    if (context == SRT_ERROR) {
        LOGGER(true, LOGG_FATAL, "srt_socket: " << srt_getlasterror_str())
        return false;
    }
    pSRTHandler.intNumConnections++;
    sa.sin_family = AF_INET;
    sa.sin_port = htons(atoi(port.c_str()));
    if (inet_pton(AF_INET, host.c_str(), &sa.sin_addr) != 1) {
        pSRTHandler.intNumConnections--;
        srt_close(context);
        LOGGER(true, LOGG_FATAL, "inet_pton failed ")
        return false;
    }

    srt_setsockflag(context, SRTO_SENDER, &yes, sizeof yes);

    LOGGER(true, LOGG_NOTIFY, "SRT connect");
    result = srt_connect(context, (struct sockaddr*) &sa, sizeof sa);
    if (result == SRT_ERROR) {
        pSRTHandler.intNumConnections--;
        srt_close(context);
        LOGGER(true, LOGG_FATAL, "srt_connect: " << srt_getlasterror_str())
        return false;
    }

    currentMode = Mode::Client;
    return true;
}

bool SRTNet::sendData(uint8_t* data, size_t len) {
    if (currentMode != Mode::Client) {
        LOGGER(true, LOGG_ERROR, "SRTNet mode is not client")
        return false;
    }
    int result = srt_sendmsg2(context, (const char*)data, len, NULL);
    if (result == SRT_ERROR)
    {
        LOGGER(true, LOGG_ERROR, "srt_sendmsg2 failed")
        return false;
    }
    if (result != len) {
        LOGGER(true, LOGG_ERROR, "Failed sending all data")
        return false;
    }
    return true;
}

bool SRTNet::stopServer() {
    if (currentMode != Mode::Server) {
        LOGGER(true, LOGG_ERROR, "SRTNet mode is not server")
        return false;
    }
    serverActive = false;
    if (context) {
        pSRTHandler.intNumConnections--;
        srt_close(context);
    }
    if (their_fd) {
        pSRTHandler.intNumConnections--;
        srt_close(their_fd);
    }

    int threadRunning=1000;
    while (serverThreadActive) {
        usleep(1000);
        if (!--threadRunning) {
            LOGGER(true, LOGG_FATAL, "SRTNet server thread is still running after 1 second. Crash and burn baby!!")
            break;
        }
    }

    currentMode = Mode::Unknown;
    return true;
}

bool SRTNet::stopClient() {
    if (currentMode != Mode::Client) {
        LOGGER(true, LOGG_ERROR, "SRTNet mode is not client")
        return false;
    }
    if (context) {
        pSRTHandler.intNumConnections--;
        srt_close(context);
    }
    currentMode = Mode::Unknown;
    return true;
}
