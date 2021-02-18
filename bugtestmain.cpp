//
// Created by tomas on 2021-02-18.
//


#include <iostream>
#include <thread>
#include "SRTNet.h"


//This is my class managed by the network connection.
class MyClass {
};

//This is a class used to test the optional connection context in the validateConnection callback.
class ConnectionClass {
};

//**********************************
//Server part
//**********************************

//Return a connection object. (Return nullptr if you don't want to connect to that client)
std::shared_ptr<SRTNet::NetworkConnection>
validateConnection(struct sockaddr &rSin, SRTSOCKET lNewSocket, std::shared_ptr<SRTNet::NetworkConnection> &rpCtx) {
    auto lNetConn = std::make_shared<SRTNet::NetworkConnection>();
    lNetConn->mObject = std::make_shared<MyClass>();
    return lNetConn;
}

//Data callback.
bool
handleData(std::unique_ptr<std::vector<uint8_t>> &rContent, SRT_MSGCTRL &rMsgCtrl, std::shared_ptr<SRTNet::NetworkConnection>& rpCtx,
           SRTSOCKET lClientHandle) {
    return true;
};


int main() {
    std::cout << "Running SRT minimal bug test" << std::endl;
    {
        SRTNet srt;
        //Register the server callbacks
        srt.clientConnected = std::bind(&validateConnection, std::placeholders::_1, std::placeholders::_2,
                                                  std::placeholders::_3);
        srt.receivedData = std::bind(&handleData, std::placeholders::_1, std::placeholders::_2,
                                               std::placeholders::_3, std::placeholders::_4);

        auto lConn1 = std::make_shared<SRTNet::NetworkConnection>();
        lConn1->mObject = std::make_shared<ConnectionClass>();

        if (!srt.startServer("0.0.0.0", 8000, 16, 1000, 100, 1456, "Th1$_is_4_0pt10N4L_P$k", lConn1)) {
            std::cout << "SRT Server failed to start." << std::endl;
            return EXIT_FAILURE;
        }
        std::cout << "Server is up for 10 seconds" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(10));

        srt.stop();
    }
    std::cout << "Stopped and destroyed the server, now the port should be free." << std::endl;

    std::cout << "Sleeping for 20 seconds" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(20));
    std::cout << "Done" << std::endl;
}
