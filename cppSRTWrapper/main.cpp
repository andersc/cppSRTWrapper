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

//Return true if the connection should be accepted by the server.
bool validateConnection(struct sockaddr_in* sin) {
    auto* ip = (unsigned char*)&sin->sin_addr.s_addr;
    std::cout << "Connecting IP: " << unsigned(ip[0]) << "." << unsigned(ip[1]) << "." << unsigned(ip[2]) << "." << unsigned(ip[3]) << std::endl;
    return true;
}

//Data callback.
bool handleData(std::unique_ptr <std::vector<uint8_t>> &data, SRT_MSGCTRL &msgCtrl){
    std::cout << "Got ->" << data->size() << " " << msgCtrl.pktseq << std::endl;
    return true;
};

//**********************************
//Client part
//**********************************

//Server sent back data callback.
bool handleDataClient(std::unique_ptr <std::vector<uint8_t>> &data, SRT_MSGCTRL &msgCtrl){
    std::cout << "Got client data ->" << data->size() << std::endl;
    return true;
};

int main(int argc, const char * argv[]) {

    SRTNet mySRTNetServer;
    SRTNet mySRTNetClient;

    std::cout << "SRT wrapper start." << std::endl;

    //Register the server callbacks
    mySRTNetServer.clientConnected=std::bind(&validateConnection, std::placeholders::_1);
    mySRTNetServer.recievedData=std::bind(&handleData, std::placeholders::_1, std::placeholders::_2);
    /*Start the server
     * ip: bind to this ip
     * port: bind to this port
     * reorder: Number of packets in the reorder window
     * latency: the max latency in milliseconds before dropping the data
     * overhead: The % overhead tolerated for retransmits relative the original data stream.
     */
    if (!mySRTNetServer.startServer("0.0.0.0","8000",16,1000,100)) {
        std::cout << "SRT Server failed to start." << std::endl;
        return EXIT_FAILURE;
    }

    //The SRT connection is bidirectional and you are able to set different parameters for a particular direction
    //The parameters have the same meaning as for the above server but on the client side.
    mySRTNetClient.recievedData=std::bind(&handleDataClient, std::placeholders::_1, std::placeholders::_2);
    if (!mySRTNetClient.startClient("127.0.0.1","8000",16,1000,100)) {
        std::cout << "SRT client failed start." << std::endl;
        return EXIT_FAILURE;
    }

    //Send 300 packets with 10 milliseconds spacing. Packets are 1000 bytes long
    int times=0;
    uint8_t data[2048];
    std::cout << "SRT Start send." << std::endl;
    while (true) {
        SRT_MSGCTRL thisMSGCTRL = srt_msgctrl_default;
        mySRTNetClient.sendData(&data[0], 1000, &thisMSGCTRL);
        usleep(10000); //wait 10 milli
        if (times++ == 300) {
            break;
        }
    }

    //Get statistics like this ->
    /*
     * Clear 0 == Do not reset statistics after being read 1 == clear statistics after being read
     * instantaneous == 1 if the statistics should use instant data, not moving averages
     */
    SRT_TRACEBSTATS currentClientStats = {0};
    mySRTNetClient.getStatistics(&currentClientStats,1,0);

    SRT_TRACEBSTATS currentServerStats = {0};
    mySRTNetServer.getStatistics(&currentServerStats,1,0);

    std::cout << "SRT End send." << std::endl;
    mySRTNetClient.stopClient();
    mySRTNetServer.stopServer();
    std::cout << "SRT wrapper did end." << std::endl;
    return EXIT_SUCCESS;
}
