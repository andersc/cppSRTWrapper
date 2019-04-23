//
// Created by Anders Cedronius on 2019-04-21.
//

#include <iostream>
#include <thread>

#include "SRTNet.h"

//**********************************
//Server part
//**********************************
bool validateConnection(struct sockaddr_storage& connectingClient) {
    struct sockaddr_in *sin = (struct sockaddr_in *)&connectingClient;
    unsigned char *ip = (unsigned char *)&sin->sin_addr.s_addr;
    printf("%d %d %d %d\n", ip[0], ip[1], ip[2], ip[3]);
    return true;
};

bool handleData(uint8_t* data, size_t len){
    std::cout << "Got ->" << len << std::endl;
    return true;
};


//**********************************
//Client part
//**********************************


int main(int argc, const char * argv[]) {

    SRTNet mySRTNetServer;
    SRTNet mySRTNetClient;

    std::cout << "SRT wrapper did start." << std::endl;

    mySRTNetServer.clientConnected=std::bind(&validateConnection, std::placeholders::_1);
    mySRTNetServer.recievedData=std::bind(&handleData, std::placeholders::_1, std::placeholders::_2);
    if (!mySRTNetServer.startServer("0.0.0.0","8000")) {
        std::cout << "SRT Server failed to start." << std::endl;
        return EXIT_FAILURE;
    }

    // std::thread(std::bind(&startServer, "127.0.0.1", "8000")).detach();

    if (!mySRTNetClient.startClient("127.0.0.1","8000")) {
        std::cout << "SRT client failed start." << std::endl;
        return EXIT_FAILURE;
    }

    int times=0;
    uint8_t data[2048];
    std::cout << "SRT Start send." << std::endl;
    while (true) {
        mySRTNetClient.sendData(&data[0], 1000);
        usleep(10000); //wait 10 mili
        if (times++ == 300) {
            break;
        }
    }
    std::cout << "SRT End send." << std::endl;

    mySRTNetClient.stopClient();
    mySRTNetServer.stopServer();
    usleep(1000); //Wait for buffers to flush
    std::cout << "SRT wrapper did end." << std::endl;
    return EXIT_SUCCESS;
}
