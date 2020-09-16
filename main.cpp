//
// Created by Anders Cedronius on 2019-04-21.
//
//

#include <iostream>
#include <thread>
#include <utility>
#include "SRTNet.h"

SRTNet gSRTNetServer;
SRTNet gSRTNetClient1;
SRTNet gSRTNetClient2;

std::atomic_bool gCloseConnection1 = {false};

//This is my class managed by the network connection.
class MyClass {
public:
    MyClass(SRTSOCKET srtSocket) {
        mIsKnown = false;
        mClientSocket = srtSocket;
    };

    ~MyClass() {
        if (mClientSocket)
            std::cout << "Client -> " << mClientSocket << " disconnected. (From Object)" << std::endl;
    };
    int mTest = 0;
    int mCounter = 0;
    std::atomic_bool mIsKnown;
    SRTSOCKET mClientSocket;
};

//This is a class used to test the optional connection context in the validateConnection callback.
class ConnectionClass {
public:
    int mSomeNumber = 111;
};

//**********************************
//Server part
//**********************************

//Return a connection object. (Return nullptr if you don't want to connect to that client)
std::shared_ptr<NetworkConnection>
validateConnection(struct sockaddr &rSin, SRTSOCKET lNewSocket, std::shared_ptr<NetworkConnection> &rpCtx) {

    if (rpCtx != nullptr) {
        auto lConnCls = std::any_cast<std::shared_ptr<ConnectionClass> &>(rpCtx->mObject);
        if (lConnCls->mSomeNumber != 111) {
            return nullptr;
        }
    } else {
        return nullptr;
    }

    char lAddrIPv6[INET6_ADDRSTRLEN];

    if (rSin.sa_family == AF_INET) {
        struct sockaddr_in *lpInConnectionV4 = (struct sockaddr_in *) &rSin;
        auto *lIp = (unsigned char *) &lpInConnectionV4->sin_addr.s_addr;
        std::cout << "Connecting IPv4: " << unsigned(lIp[0]) << "." << unsigned(lIp[1]) << "." << unsigned(lIp[2]) << "."
                  << unsigned(lIp[3]) << std::endl;

        //Do we want to accept this connection?
        //return nullptr;


    } else if (rSin.sa_family == AF_INET6) {
        struct sockaddr_in6 *pInConnectionV6 = (struct sockaddr_in6 *) &rSin;
        inet_ntop(AF_INET6, &pInConnectionV6->sin6_addr, lAddrIPv6, INET6_ADDRSTRLEN);
        printf("Connecting IPv6: %s\n", lAddrIPv6);

        //Do we want to accept this connection?
        //return nullptr;

    } else {
        //Not IPv4 and not IPv6. That's weird. don't connect.
        return nullptr;
    }

    auto lNetConn = std::make_shared<NetworkConnection>();
    lNetConn->mObject = std::make_shared<MyClass>(lNewSocket);
    return lNetConn;
}

//Data callback.
bool
handleData(std::unique_ptr<std::vector<uint8_t>> &rContent, SRT_MSGCTRL &rMsgCtrl, std::shared_ptr<NetworkConnection>& rpCtx,
           SRTSOCKET lClientHandle) {

    //Try catch?
    auto lpMyClass = std::any_cast<std::shared_ptr<MyClass> &>(rpCtx->mObject);

    if (!lpMyClass->mIsKnown) { //just a example/test. This connection is unknown. See what connection it is and set the test-parameter accordingly
        if (rContent->data()[0] == 1) {
            lpMyClass->mIsKnown = true;
            lpMyClass->mTest = 1;
        }

        if (rContent->data()[0] == 2) {
            lpMyClass->mIsKnown = true;
            lpMyClass->mTest = 2;
        }
    }

    if (lpMyClass->mIsKnown) {
        if (lpMyClass->mCounter++ == 100) { //every 100 packet you got respond back to the client using the same data.
            lpMyClass->mCounter = 0;
            SRT_MSGCTRL thisMSGCTRL = srt_msgctrl_default;
            gSRTNetServer.sendData(rContent->data(), rContent->size(), &thisMSGCTRL, lClientHandle);
        }
    }

    // std::cout << "Got ->" << content->size() << " " << std::endl;

    return true;
};

//Client disconnect callback.
void clientDisconnect(std::shared_ptr<NetworkConnection>& pCtx, SRTSOCKET lClientHandle) {
    std::cout << "Client -> " << lClientHandle << " disconnected. (From callback)" << std::endl;
}


//**********************************
//Client part
//**********************************

//Server sent back data callback.
void handleDataClient(std::unique_ptr<std::vector<uint8_t>> &rContent, SRT_MSGCTRL &rMsgCtrl,
                      std::shared_ptr<NetworkConnection> &rpCtx, SRTSOCKET lServerHandle) {

    std::cout << "Got data ->" << rContent->size() << std::endl;

    //Try catch?
    auto lpMyClass = std::any_cast<std::shared_ptr<MyClass> &>(rpCtx->mObject);

    int lPacketSize = rContent->size();
    if (lpMyClass->mTest == 1) {
        std::cout << "Got client1 data ->" << lPacketSize << std::endl;
        lpMyClass->mCounter++;
        if (lpMyClass->mCounter == 1) { //kill this connection after 1 packet from server
            gCloseConnection1 = true;
        }
        return;
    }
    if (lpMyClass->mTest == 2) {
        std::cout << "Got client2 data ->" << lPacketSize << std::endl;
        return;
    }
    return;
};

int main(int argc, const char *argv[]) {
    std::cout << "SRT wrapper start." << std::endl;

    bool lRunOnce = true;
    //Register the server callbacks
    gSRTNetServer.clientConnected = std::bind(&validateConnection, std::placeholders::_1, std::placeholders::_2,
                                              std::placeholders::_3);
    gSRTNetServer.receivedData = std::bind(&handleData, std::placeholders::_1, std::placeholders::_2,
                                           std::placeholders::_3, std::placeholders::_4);

    //The disconnect callback is optional you can use the destructor in your embedded object to detect disconnects also
    gSRTNetServer.clientDisconnected = std::bind(&clientDisconnect, std::placeholders::_1, std::placeholders::_2);

    //Create a optional connection context
    auto lConn1 = std::make_shared<NetworkConnection>();
    lConn1->mObject = std::make_shared<ConnectionClass>();

    /*Start the server
     * ip: bind to this ip (can be IPv4 or IPv6)
     * port: bind to this port
     * reorder: Number of packets in the reorder window
     * latency: the max latency in milliseconds before dropping the data
     * overhead: The % overhead tolerated for retransmits relative the original data stream.
     * mtu: max 1456
     */
    if (!gSRTNetServer.startServer("0.0.0.0", 8000, 16, 1000, 100, 1456, "Th1$_is_4_0pt10N4L_P$k", lConn1)) {
        std::cout << "SRT Server failed to start." << std::endl;
        return EXIT_FAILURE;
    }

    //The SRT connection is bidirectional and you are able to set different parameters for a particular direction
    //The parameters have the same meaning as for the above server but on the client side.
    auto lClient1Connection = std::make_shared<NetworkConnection>();
    std::shared_ptr<MyClass> lpMyClass1 = std::make_shared<MyClass>(0);
    lpMyClass1->mTest = 1;
    lClient1Connection->mObject = std::move(lpMyClass1);

    gSRTNetClient1.receivedData = std::bind(&handleDataClient, std::placeholders::_1, std::placeholders::_2,
                                            std::placeholders::_3, std::placeholders::_4);
    if (!gSRTNetClient1.startClient("127.0.0.1", 8000, 16, 1000, 100, lClient1Connection, 1456,
                                    "Th1$_is_4_0pt10N4L_P$k")) {
        std::cout << "SRT client1 failed starting." << std::endl;
        return EXIT_FAILURE;
    }

    auto lClient2Connection = std::make_shared<NetworkConnection>();
    std::shared_ptr<MyClass> lpMyClass2 = std::make_shared<MyClass>(0);
    lpMyClass2->mTest = 2;
    lClient2Connection->mObject = std::move(lpMyClass2);
    gSRTNetClient2.receivedData = std::bind(&handleDataClient, std::placeholders::_1, std::placeholders::_2,
                                            std::placeholders::_3, std::placeholders::_4);
    if (!gSRTNetClient2.startClient("127.0.0.1", 8000, 16, 1000, 100, lClient2Connection, 1456,
                                    "Th1$_is_4_0pt10N4L_P$k")) {
        std::cout << "SRT client2 failed starting." << std::endl;
        return EXIT_FAILURE;
    }

    std::this_thread::sleep_for(std::chrono::seconds(2));

    // auto clients  = mySRTNetServer.getActiveClients();
    //  std::cout << "The server got " << clients->mClientList->size() << " clients." << std::endl;
    //  clients = nullptr;

    gSRTNetServer.getActiveClients([](std::map<SRTSOCKET, std::shared_ptr<NetworkConnection>> &clientList) {
                                       std::cout << "The server got " << clientList.size() << " client(s)." << std::endl;
                                   }
    );

    //Send 300 packets with 10 milliseconds spacing. Packets are 1000 bytes long
    int lTimes = 0;

    std::vector<uint8_t> lBuffer1(1000);
    std::fill(lBuffer1.begin(), lBuffer1.end(), 1);
    std::vector<uint8_t> lBuffer2(1000);
    std::fill(lBuffer2.begin(), lBuffer2.end(), 2);

    std::cout << "SRT Start send." << std::endl;

    bool lStillSendClient1Data = true;

    while (true) {
        SRT_MSGCTRL lThisMSGCTRL1 = srt_msgctrl_default;
        if (lStillSendClient1Data) {
            lStillSendClient1Data = gSRTNetClient1.sendData(lBuffer1.data(), lBuffer1.size(), &lThisMSGCTRL1);
        }

        if (gCloseConnection1 && lRunOnce) {
            lRunOnce = false;
            gSRTNetClient1.stop();
        }

        SRT_MSGCTRL lThisMSGCTRL2 = srt_msgctrl_default;
        gSRTNetClient2.sendData(lBuffer2.data(), lBuffer2.size(), &lThisMSGCTRL2);

        std::this_thread::sleep_for(std::chrono::microseconds(10000));
        if (lTimes++ == 300) {
            break;
        }
    }

    std::cout << "Done sending" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(4));
    std::cout << "Get statistics." << std::endl;

    //Get statistics like this ->
    /*
     * SRTNetClearStats::no == Do not reset statistics after being read SRTNetClearStats::yes == clear statistics after being read
     * instantaneous == 1 if the statistics should be the instant data, not moving averages
     */
    SRT_TRACEBSTATS lCurrentClientStats1 = {0};
    gSRTNetClient1.getStatistics(&lCurrentClientStats1, SRTNetClearStats::yes, SRTNetInstant::no);

    SRT_TRACEBSTATS lCurrentClientStats2 = {0};
    gSRTNetClient2.getStatistics(&lCurrentClientStats2, SRTNetClearStats::yes, SRTNetInstant::no);

    //SRT_TRACEBSTATS lCurrentServerStats = {0};
    //mySRTNetServer.getStatistics(&currentServerStats,SRTNetClearStats::yes,SRTNetInstant::no);

    gSRTNetServer.getActiveClients([](std::map<SRTSOCKET, std::shared_ptr<NetworkConnection>> &rClientList) {
                                       std::cout << "The server got " << rClientList.size() << " clients." << std::endl;
                                   }
    );

    std::cout << "SRT garbage collect" << std::endl;
    gSRTNetServer.stop();
    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::cout << "stopClient 1" << std::endl;
    gSRTNetClient1.stop();
    std::cout << "stopClient 2" << std::endl;
    gSRTNetClient2.stop();

    gSRTNetServer.getActiveClients([](std::map<SRTSOCKET, std::shared_ptr<NetworkConnection>> &clientList) {
                                       std::cout << "The server got " << clientList.size() << " clients." << std::endl;
                                   }
    );

    std::cout << "SRT wrapper did end." << std::endl;
    return EXIT_SUCCESS;
}
