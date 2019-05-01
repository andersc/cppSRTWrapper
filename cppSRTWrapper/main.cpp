//
// Created by Anders Cedronius on 2019-04-21.
//
//

// Simple SRT C++ wrapper where the server only accept one client
// Use this for simple point to point SRT tests

#include <iostream>
#include <thread>

#include "SRTNet.h"

//**********************************
//Server part
//**********************************
bool validateConnection(struct sockaddr_in* sin) {
    unsigned char* ip = (unsigned char*)&sin->sin_addr.s_addr;
    std::cout << "Connecting IP: " << unsigned(ip[0]) << "." <<unsigned(ip[1]) << "." << unsigned(ip[2]) << "." << unsigned(ip[3]) << std::endl;
    return true;
};

bool handleData(std::unique_ptr <std::vector<uint8_t>> &data, SRT_MSGCTRL &msgCtrl){
    std::cout << "Got ->" << data->size() << std::endl;
    return true;
};

//**********************************
//Client part
//**********************************

bool handleDataClient(std::unique_ptr <std::vector<uint8_t>> &data, SRT_MSGCTRL &msgCtrl){
    std::cout << "Got client data ->" << data->size() << std::endl;
    return true;
};

int main(int argc, const char * argv[]) {

    SRTNet mySRTNetServer;
    SRTNet mySRTNetClient;

    std::cout << "SRT wrapper did start." << std::endl;

    mySRTNetServer.clientConnected=std::bind(&validateConnection, std::placeholders::_1);
    mySRTNetServer.recievedData=std::bind(&handleData, std::placeholders::_1, std::placeholders::_2);
    if (!mySRTNetServer.startServer("0.0.0.0","8000",16,1000,100)) {
        std::cout << "SRT Server failed to start." << std::endl;
        return EXIT_FAILURE;
    }

    mySRTNetClient.recievedData=std::bind(&handleDataClient, std::placeholders::_1, std::placeholders::_2);
    if (!mySRTNetClient.startClient("127.0.0.1","8000",16,1000,100)) {
        std::cout << "SRT client failed start." << std::endl;
        return EXIT_FAILURE;
    }

    int times=0;
    uint8_t data[2048];
    std::cout << "SRT Start send." << std::endl;
    while (true) {
        SRT_MSGCTRL thisMSGCTRL = srt_msgctrl_default;
        mySRTNetClient.sendData(&data[0], 1000, &thisMSGCTRL);
        usleep(10000); //wait 10 mili
        if (times++ == 300) {
            break;
        }
    }

    //Get statistics
    SRT_TRACEBSTATS currentClientStats = {0};
    mySRTNetClient.getStatistics(&currentClientStats,1,0);

    SRT_TRACEBSTATS currentServerStats = {0};
    mySRTNetServer.getStatistics(&currentServerStats,1,0);

    std::cout << "SRT End send." << std::endl;
    mySRTNetServer.stopServer();
    mySRTNetClient.stopClient();
    std::cout << "SRT wrapper did end." << std::endl;
    return EXIT_SUCCESS;
}
