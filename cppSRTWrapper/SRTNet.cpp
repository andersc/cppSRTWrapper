//
// Created by Anders Cedronius on 2019-04-21.
//

#include "SRTNet.h"

SRTNet::SRTNet() {
    serverActive = false;
    std::cout << __FILE__ << " " << __LINE__ << ": SRTNet constructed" << std::endl;
}

SRTNet::SRTNet(const SRTNet& orig) {
    std::cout << __FILE__ << " " << __LINE__ << ": SRTNet copy constructor called" << std::endl;
}

SRTNet::~SRTNet() {
    if (context) {
        srt_close(context);
    }
    if (their_fd) {
        srt_close(their_fd);
    }
    std::cout << __FILE__ << " " << __LINE__ << ": SRTNet destruct" << std::endl;
}

bool SRTNet::startServer(std::string ip, std::string port) {

    int result = 0;
    struct sockaddr_in sa;
    int yes = 1;

    if (currentMode != Mode::Unknown) {
        std::cout << __FILE__ << " " << __LINE__ << ": SRTNet mode is already set" << std::endl;
        return false;
    }

    std::cout << __FILE__ << " " << __LINE__ << ": srt socket" << std::endl;
    context = srt_create_socket();
    if (context == SRT_ERROR) {
        std::cout << __FILE__ << " " << __LINE__ << ": srt_socket: " << srt_getlasterror_str() << std::endl;
        return false;
    }

    std::cout << __FILE__ << " " << __LINE__ << ": srt bind address" << std::endl;
    sa.sin_family = AF_INET;
    sa.sin_port = htons(atoi(port.c_str()));
    if (inet_pton(AF_INET, ip.c_str(), &sa.sin_addr) != 1) {
        return false;
    }

    std::cout << __FILE__ << " " << __LINE__ << ": srt setsockflag" << std::endl;
    srt_setsockflag(context, SRTO_RCVSYN, &yes, sizeof yes);

    std::cout << __FILE__ << " " << __LINE__ << ": srt bind" << std::endl;
    result = srt_bind(context, (struct sockaddr*) &sa, sizeof sa);
    if (result == SRT_ERROR) {
        std::cout << __FILE__ << " " << __LINE__ << ": srt_bind: " << srt_getlasterror_str() << std::endl;
        return false;
    }

    std::cout << __FILE__ << " " << __LINE__ << ": srt listen" << std::endl;
    result = srt_listen(context, 2);
    if (result == SRT_ERROR) {
        std::cout << __FILE__ << " " << __LINE__ << ": srt_listen: " << srt_getlasterror_str() << std::endl;
        return false;
    }
    std::thread(std::bind(&SRTNet::waitForSRTClient, this)).detach();
    currentMode = Mode::Server;
    serverActive = true;
    return true;
}

void SRTNet::waitForSRTClient() {
    int result = 0;
    while (serverActive) {
        struct sockaddr_storage their_addr;
        std::cout << __FILE__ << " " << __LINE__ << ":SRT Server wait for client" << std::endl;
        int addr_size = sizeof their_addr;
        their_fd = srt_accept(context, (struct sockaddr *) &their_addr, &addr_size);
        std::cout << __FILE__ << " " << __LINE__ << ":Client connected" << std::endl;
        if (!clientConnected) {
            std::cout << __FILE__ << " " << __LINE__ << ":waitForSRTClient needs clientConnected" << std::endl;
            return;
        }
        if (clientConnected(their_addr)) {
            while (serverActive) {
                uint8_t msg[2048];
                SRT_MSGCTRL thisMSGCTRL;
                result = srt_recvmsg2(their_fd, (char*)msg, sizeof msg, &thisMSGCTRL);
                if (result == SRT_ERROR)
                {
                    std::cout << __FILE__ << " " << __LINE__ << ":SRT srt_recvmsg error" << std::endl;
                    break;
                }
                if (result>0 && recievedData) {
                    recievedData(&msg[0],result);

                }
            }
            if (their_fd) {
                srt_close(their_fd);
            }
        }
    }
    if (context) {
        srt_close(context);
    }
    serverActive = false;
}

bool SRTNet::startClient(std::string host, std::string port) {
    if (currentMode != Mode::Unknown) {
        std::cout << __FILE__ << " " << __LINE__ << ": SRTNet mode is already set" << std::endl;
        return false;
    }

    int result = 0;
    int yes = 1;
    struct sockaddr_in sa;

    std::cout << __FILE__ << " " << __LINE__ << ": SRT client startup" << std::endl;

    context = srt_create_socket();
    if (context == SRT_ERROR) {
        std::cout << __FILE__ << " " << __LINE__ << ": srt_socket: " << srt_getlasterror_str() << std::endl;
        return false;
    }

    std::cout << __FILE__ << " " << __LINE__ << ": srt remote address" << std::endl;
    sa.sin_family = AF_INET;
    sa.sin_port = htons(atoi(port.c_str()));
    if (inet_pton(AF_INET, host.c_str(), &sa.sin_addr) != 1) {
        srt_close(context);
        return false;
    }

    std::cout << __FILE__ << " " << __LINE__ << ": srt setsockflag" << std::endl;
    srt_setsockflag(context, SRTO_SENDER, &yes, sizeof yes);

    std::cout << __FILE__ << " " << __LINE__ << ": srt connect" << std::endl;
    result = srt_connect(context, (struct sockaddr*) &sa, sizeof sa);
    if (result == SRT_ERROR) {
        std::cout << __FILE__ << " " << __LINE__ << ": srt_connect: " << srt_getlasterror_str() << std::endl;
        srt_close(context);
        return false;
    }

    currentMode = Mode::Client;
    return true;
}

bool SRTNet::sendData(uint8_t* data, size_t len) {
    int result = 0;
    if (currentMode != Mode::Client) {
        std::cout << __FILE__ << " " << __LINE__ << ": SRTNet mode is not client" << std::endl;
        return false;
    }
    result = srt_sendmsg2(context, (const char*)data, len, NULL);
    if (result == SRT_ERROR)
    {
        std::cout << __FILE__ << " " << __LINE__ << ": srt_sendmsg2 failed" << std::endl;
        return false;
    }
    if (result != len) {
        std::cout << __FILE__ << " " << __LINE__ << ": Failed sending all data" << std::endl;
        return false;
    }
    return true;
}

bool SRTNet::stopServer() {
    if (currentMode != Mode::Server) {
        std::cout << __FILE__ << " " << __LINE__ << ": SRTNet mode is not server" << std::endl;
        return false;
    }
    serverActive = false;
    if (context) {
        srt_close(context);
    }
    if (their_fd) {
        srt_close(their_fd);
    }
    currentMode = Mode::Unknown;
    return true;
}

bool SRTNet::stopClient() {
    if (currentMode != Mode::Client) {
        std::cout << __FILE__ << " " << __LINE__ << ": SRTNet mode is not client" << std::endl;
        return false;
    }
    if (context) {
        srt_close(context);
    }
    currentMode = Mode::Unknown;
    return true;
}
