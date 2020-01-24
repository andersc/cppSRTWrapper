//
// Created by Anders Cedronius on 2019-04-21.
//
//

// Simple SRT C++ wrapper
// The code in this sample project tests the wrapper and shows how to use it
// The code is NOT optimized for performance.. For performance you need to write your own wrapper
// and not use smart pointers and callbacks in the dataplane.

#include <iostream>
#include <thread>
#include <utility>

#include "SRTNet.h"

SRTNet mySRTNetServer;
SRTNet mySRTNetClient1;
SRTNet mySRTNetClient2;

std::atomic_bool closeConnection1;
std::atomic_bool runOnce;

//This is my class managed by the network connection.
class MyClass {
public:
  MyClass() {
    isKnown = false;
  };
  int test = 0;
  int counter = 0;
  std::atomic_bool isKnown;
};

//**********************************
//Server part
//**********************************

//Return a connection object. (Return nullptr if you don't want to connect to that client)
std::shared_ptr<NetworkConnection> validateConnection(struct sockaddr_in* sin) {
    auto* ip = (unsigned char*)&sin->sin_addr.s_addr;
    std::cout << "Connecting IP: " << unsigned(ip[0]) << "." << unsigned(ip[1]) << "." << unsigned(ip[2]) << "." << unsigned(ip[3]) << std::endl;
    auto a1 = std::make_shared<NetworkConnection>();
    a1->object = std::make_shared<MyClass>();
    return a1;
}

//Data callback.
bool handleData(std::unique_ptr <std::vector<uint8_t>> &content, SRT_MSGCTRL &msgCtrl, std::shared_ptr<NetworkConnection> ctx, SRTSOCKET clientHandle) {

    //Try catch?
    auto v = std::any_cast<std::shared_ptr<MyClass>&>(ctx -> object);

    if (!v->isKnown) { //just a example/test. This connection is unknown. See what connection it is and set the test-parameter accordingly
        if (content->data()[0] == 1) {
            v->isKnown=true;
            v->test = 1;
        }

        if (content->data()[0] == 2) {
            v->isKnown=true;
            v->test = 2;
        }
    }

    if (v->isKnown) {
        if (v->counter++ == 100) { //every 100 packet you got respond back to the client using the same data.
            v->counter = 0;
            SRT_MSGCTRL thisMSGCTRL = srt_msgctrl_default;
            mySRTNetServer.sendData(content->data(), content->size(), &thisMSGCTRL,clientHandle);
        }
    }

   // std::cout << "Got ->" << content->size() << " " << std::endl;

    return true;
};

//**********************************
//Client part
//**********************************

//Server sent back data callback.
void handleDataClient(std::unique_ptr <std::vector<uint8_t>> &content, SRT_MSGCTRL &msgCtrl, std::shared_ptr<NetworkConnection> &ctx, SRTSOCKET serverHandle){

  std::cout << "Got data ->" << content->size() << std::endl;

    //Try catch?
    auto v = std::any_cast<std::shared_ptr<MyClass>&>(ctx -> object);

    int packetSize=content->size();
    if (v->test == 1) {
        std::cout << "Got client1 data ->" << packetSize << std::endl;
        v->counter++;
        if (v->counter == 1) { //kill this connection after 1 packet from server
            closeConnection1 = true;
        }
        return;
    }
    if (v->test == 2) {
        std::cout << "Got client2 data ->" << packetSize << std::endl;
        return;
    }
    return;
};

int main(int argc, const char * argv[]) {


    closeConnection1 = false;
    runOnce = true;

    std::cout << "SRT wrapper start." << std::endl;

    //Register the server callbacks
    mySRTNetServer.clientConnected=std::bind(&validateConnection, std::placeholders::_1);
    mySRTNetServer.recievedData=std::bind(&handleData, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);
    /*Start the server
     * ip: bind to this ip
     * port: bind to this port
     * reorder: Number of packets in the reorder window
     * latency: the max latency in milliseconds before dropping the data
     * overhead: The % overhead tolerated for retransmits relative the original data stream.
     * mtu: max 1456
     */
    if (!mySRTNetServer.startServer("0.0.0.0", 8000, 16, 1000, 100, 1456)) {
        std::cout << "SRT Server failed to start." << std::endl;
        return EXIT_FAILURE;
    }

    //The SRT connection is bidirectional and you are able to set different parameters for a particular direction
    //The parameters have the same meaning as for the above server but on the client side.
    auto client1Connection=std::make_shared<NetworkConnection>();
    std::shared_ptr<MyClass> a1 = std::make_shared<MyClass>();
    a1->test = 1;
    client1Connection->object = std::move(a1);

    mySRTNetClient1.recievedData=std::bind(&handleDataClient, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);
    if (!mySRTNetClient1.startClient("127.0.0.1", 8000, 16, 1000, 100,client1Connection, 1456)) {
        std::cout << "SRT client1 failed starting." << std::endl;
        return EXIT_FAILURE;
    }

    auto client2Connection=std::make_shared<NetworkConnection>();
    std::shared_ptr<MyClass> a2 = std::make_shared<MyClass>();
    a2->test = 2;
    client2Connection->object = std::move(a2);
    mySRTNetClient2.recievedData=std::bind(&handleDataClient, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);
    if (!mySRTNetClient2.startClient("127.0.0.1", 8000, 16, 1000, 100,client2Connection, 1456)) {
        std::cout << "SRT client2 failed starting." << std::endl;
        return EXIT_FAILURE;
    }

    sleep(2);

    //Send 300 packets with 10 milliseconds spacing. Packets are 1000 bytes long
    int times = 0;

    std::vector<uint8_t>buffer1(1000);
    std::fill (buffer1.begin(),buffer1.end(),1);
    std::vector<uint8_t>buffer2(1000);
    std::fill (buffer2.begin(),buffer2.end(),2);

    std::cout << "SRT Start send." << std::endl;

    bool stillSendClient1Data = true;

    while (true) {
        SRT_MSGCTRL thisMSGCTRL1 = srt_msgctrl_default;
        if (stillSendClient1Data) {
            stillSendClient1Data = mySRTNetClient1.sendData(buffer1.data(), buffer1.size(), &thisMSGCTRL1);
        }

        if (closeConnection1 && runOnce) {
            runOnce = false;
            mySRTNetClient1.stop();
        }

        SRT_MSGCTRL thisMSGCTRL2 = srt_msgctrl_default;
        mySRTNetClient2.sendData(buffer2.data(), buffer2.size(), &thisMSGCTRL2);

        usleep(10000); //wait 10 milli
        if (times++ == 300) {
            break;
        }
    }

    std::cout << "Done sending" << std::endl;
    sleep(4);
    std::cout << "Get statistics." << std::endl;

    //Get statistics like this ->
    /*
     * SRTNetClearStats::no == Do not reset statistics after being read SRTNetClearStats::yes == clear statistics after being read
     * instantaneous == 1 if the statistics should be the instant data, not moving averages
     */
    SRT_TRACEBSTATS currentClientStats1 = {0};
    mySRTNetClient1.getStatistics(&currentClientStats1,SRTNetClearStats::yes,SRTNetInstant::no);

    SRT_TRACEBSTATS currentClientStats2 = {0};
    mySRTNetClient2.getStatistics(&currentClientStats2,SRTNetClearStats::yes,SRTNetInstant::no);

    SRT_TRACEBSTATS currentServerStats = {0};
    //mySRTNetServer.getStatistics(&currentServerStats,SRTNetClearStats::yes,SRTNetInstant::no);

    std::cout << "SRT garbagecollect" << std::endl;
    mySRTNetServer.stop();
    sleep(2);
    std::cout << "stopClient 1" << std::endl;
    mySRTNetClient1.stop();
    std::cout << "stopClient 2" << std::endl;
    mySRTNetClient2.stop();


    std::cout << "SRT wrapper did end." << std::endl;
    return EXIT_SUCCESS;
}
