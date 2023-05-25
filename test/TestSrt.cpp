#include <condition_variable>
#include <thread>

#include <gtest/gtest.h>

#include "SRTNet.h"

std::string kValidPsk = "Th1$_is_4n_0pt10N4L_P$k";
std::string kInvalidPsk = "Th1$_is_4_F4k3_P$k";

size_t kMaxMessageSize = SRT_LIVE_MAX_PLSIZE;

namespace {
///
/// @brief Get the bind IP address and port of an SRT socket
/// @param socket The SRT socket to get the bind IP and Port from
/// @return The bind IP address and port of the SRT socket
std::pair<std::string, uint16_t> getBindIpAndPortFromSRTSocket(SRTSOCKET socket) {
    sockaddr_storage address{};
    int32_t addressSize = sizeof(decltype(address));
    EXPECT_EQ(srt_getsockname(socket, reinterpret_cast<sockaddr*>(&address), &addressSize), 0);
    if (address.ss_family == AF_INET) {
        const sockaddr_in* socketAddressV4 = reinterpret_cast<const sockaddr_in*>(&address);
        char ipv4Address[INET_ADDRSTRLEN];
        EXPECT_NE(inet_ntop(AF_INET, &(socketAddressV4->sin_addr), ipv4Address, INET_ADDRSTRLEN), nullptr);
        return {ipv4Address, ntohs(socketAddressV4->sin_port)};
    } else if (address.ss_family == AF_INET6) {
        const sockaddr_in6* socketAddressV6 = reinterpret_cast<const sockaddr_in6*>(&address);
        char ipv6Address[INET6_ADDRSTRLEN];
        EXPECT_NE(inet_ntop(AF_INET, &(socketAddressV6->sin6_addr), ipv6Address, INET6_ADDRSTRLEN), nullptr);
        return {ipv6Address, ntohs(socketAddressV6->sin6_port)};
    }
    return {"Unsupported", 0};
}

///
/// @brief Get the remote peer IP address and port of an SRT socket
/// @param socket The SRT socket to get the peer IP and Port from
/// @return The peer IP address and port of the SRT socket
std::pair<std::string, uint16_t> getPeerIpAndPortFromSRTSocket(SRTSOCKET socket) {
    sockaddr_storage address{};
    int32_t addressSize = sizeof(decltype(address));
    EXPECT_EQ(srt_getpeername(socket, reinterpret_cast<sockaddr*>(&address), &addressSize), 0);
    if (address.ss_family == AF_INET) {
        const sockaddr_in* socketAddressV4 = reinterpret_cast<const sockaddr_in*>(&address);
        char ipv4Address[INET_ADDRSTRLEN];
        EXPECT_NE(inet_ntop(AF_INET, &(socketAddressV4->sin_addr), ipv4Address, INET_ADDRSTRLEN), nullptr);
        return {ipv4Address, ntohs(socketAddressV4->sin_port)};
    } else if (address.ss_family == AF_INET6) {
        const sockaddr_in6* socketAddressV6 = reinterpret_cast<const sockaddr_in6*>(&address);
        char ipv6Address[INET6_ADDRSTRLEN];
        EXPECT_NE(inet_ntop(AF_INET, &(socketAddressV6->sin6_addr), ipv6Address, INET6_ADDRSTRLEN), nullptr);
        return {ipv6Address, ntohs(socketAddressV6->sin6_port)};
    }
    return {"Unsupported", 0};
}
} // namespace

TEST(TestSrt, StartStop) {
    SRTNet server;
    SRTNet client;

    auto serverCtx = std::make_shared<SRTNet::NetworkConnection>();
    EXPECT_FALSE(server.startServer("127.0.0.1", 8009, 16, 1000, 100, SRT_LIVE_MAX_PLSIZE, kValidPsk, false, serverCtx))
        << "Expect to fail without providing clientConnected callback";
    auto clientCtx = std::make_shared<SRTNet::NetworkConnection>();
    clientCtx->mObject = 42;
    EXPECT_FALSE(client.startClient("127.0.0.1", 8009, 16, 1000, 100, clientCtx, SRT_LIVE_MAX_PLSIZE, kValidPsk))
        << "Expect to fail with no server started";

    std::condition_variable connectedCondition;
    std::mutex connectedMutex;
    bool connected = false;

    // notice when client connects to server
    server.clientConnected = [&](struct sockaddr& sin, SRTSOCKET newSocket,
                                 std::shared_ptr<SRTNet::NetworkConnection>& ctx) {
        {
            std::lock_guard<std::mutex> lock(connectedMutex);
            connected = true;
        }
        connectedCondition.notify_one();
        auto connectionCtx = std::make_shared<SRTNet::NetworkConnection>();
        connectionCtx->mObject = 1111;
        return connectionCtx;
    };

    ASSERT_TRUE(server.startServer("127.0.0.1", 8009, 16, 1000, 100, SRT_LIVE_MAX_PLSIZE, kValidPsk, false, serverCtx));
    ASSERT_TRUE(client.startClient("127.0.0.1", 8009, 16, 1000, 100, clientCtx, SRT_LIVE_MAX_PLSIZE, kValidPsk));

    // check for client connecting
    {
        std::unique_lock<std::mutex> lock(connectedMutex);
        bool successfulWait = connectedCondition.wait_for(lock, std::chrono::seconds(2), [&]() { return connected; });
        ASSERT_TRUE(successfulWait) << "Timeout waiting for client to connect";
    }

    auto nClients = 0;
    server.getActiveClients([&](std::map<SRTSOCKET, std::shared_ptr<SRTNet::NetworkConnection>>& activeClients) {
        nClients = activeClients.size();
        for (const auto& socketNetworkConnectionPair : activeClients) {
            int32_t number = 0;
            EXPECT_NO_THROW(number = std::any_cast<int32_t>(socketNetworkConnectionPair.second->mObject));
            EXPECT_EQ(number, 1111);
        }
    });
    EXPECT_EQ(nClients, 1);

    auto [srtSocket, networkConnection] = client.getConnectedServer();
    EXPECT_NE(networkConnection, nullptr);
    int32_t number = 0;
    EXPECT_NO_THROW(number = std::any_cast<int32_t>(networkConnection->mObject));
    EXPECT_EQ(number, 42);

    // notice when client disconnects
    std::condition_variable disconnectCondition;
    std::mutex disconnectMutex;
    bool disconnected = false;
    server.clientDisconnected = [&](std::shared_ptr<SRTNet::NetworkConnection>& ctx, SRTSOCKET lSocket) {
        {
            std::lock_guard<std::mutex> lock(disconnectMutex);
            disconnected = true;
        }
        disconnectCondition.notify_one();
    };

    EXPECT_TRUE(client.stop());
    {
        std::unique_lock<std::mutex> lock(disconnectMutex);
        bool successfulWait =
            disconnectCondition.wait_for(lock, std::chrono::seconds(2), [&]() { return disconnected; });
        EXPECT_TRUE(successfulWait) << "Timeout waiting for client disconnect";
    }

    // start a new client and stop the server
    connected = false;
    SRTNet client2;
    ASSERT_TRUE(client2.startClient("127.0.0.1", 8009, 16, 1000, 100, clientCtx, SRT_LIVE_MAX_PLSIZE, kValidPsk));
    // check for client connecting
    {
        std::unique_lock<std::mutex> lock(connectedMutex);
        bool successfulWait = connectedCondition.wait_for(lock, std::chrono::seconds(2), [&]() { return connected; });
        ASSERT_TRUE(successfulWait) << "Timeout waiting for client2 to connect";
    }

    disconnected = false;
    EXPECT_TRUE(server.stop());
    {
        std::unique_lock<std::mutex> lock(disconnectMutex);
        bool successfulWait =
            disconnectCondition.wait_for(lock, std::chrono::seconds(2), [&]() { return disconnected; });
        EXPECT_TRUE(successfulWait) << "Timeout waiting for client disconnect";
    }
}

TEST(TestSrt, TestPsk) {
    SRTNet server;
    SRTNet client;

    auto ctx = std::make_shared<SRTNet::NetworkConnection>();
    server.clientConnected = [&](struct sockaddr& sin, SRTSOCKET newSocket,
                                 std::shared_ptr<SRTNet::NetworkConnection>& ctx) { return ctx; };
    ASSERT_TRUE(server.startServer("127.0.0.1", 8009, 16, 1000, 100, SRT_LIVE_MAX_PLSIZE, kValidPsk, false, ctx));
    EXPECT_FALSE(client.startClient("127.0.0.1", 8009, 16, 1000, 100, ctx, SRT_LIVE_MAX_PLSIZE, kInvalidPsk))
        << "Expect to fail when using incorrect PSK";

    ASSERT_TRUE(server.stop());
    ASSERT_TRUE(server.startServer("127.0.0.1", 8009, 16, 1000, 100, SRT_LIVE_MAX_PLSIZE, kValidPsk, false, ctx));
    EXPECT_TRUE(client.startClient("127.0.0.1", 8009, 16, 1000, 100, ctx, SRT_LIVE_MAX_PLSIZE, kValidPsk));

    ASSERT_TRUE(server.stop());
    ASSERT_TRUE(client.stop());
    ASSERT_TRUE(server.startServer("127.0.0.1", 8009, 16, 1000, 100, SRT_LIVE_MAX_PLSIZE, "", false, ctx));
    EXPECT_TRUE(client.startClient("127.0.0.1", 8009, 16, 1000, 100, ctx, SRT_LIVE_MAX_PLSIZE));
}

TEST(TestSrt, SendReceive) {
    class ConnectionCtx {};

    SRTNet server;
    SRTNet client;
    auto serverCtx = std::make_shared<SRTNet::NetworkConnection>();
    serverCtx->mObject = std::make_shared<ConnectionCtx>();
    auto clientCtx = std::make_shared<SRTNet::NetworkConnection>();
    clientCtx->mObject = std::make_shared<ConnectionCtx>();

    server.clientConnected = [&](struct sockaddr& sin, SRTSOCKET newSocket,
                                 std::shared_ptr<SRTNet::NetworkConnection>& ctx) {
        EXPECT_EQ(ctx, serverCtx);
        return ctx;
    };

    server.clientDisconnected = [&](std::shared_ptr<SRTNet::NetworkConnection>& ctx, SRTSOCKET lSocket) {
        EXPECT_EQ(ctx, serverCtx);
    };

    // start server and client
    ASSERT_TRUE(server.startServer("127.0.0.1", 8009, 16, 1000, 100, SRT_LIVE_MAX_PLSIZE, kValidPsk, false, serverCtx));
    ASSERT_TRUE(client.startClient("127.0.0.1", 8009, 16, 1000, 100, clientCtx, SRT_LIVE_MAX_PLSIZE, kValidPsk));

    std::vector<uint8_t> sendBuffer(1000);
    std::condition_variable serverCondition;
    std::mutex serverMutex;
    bool serverGotData = false;
    SRTSOCKET clientSocket;
    server.receivedData = [&](std::unique_ptr<std::vector<uint8_t>>& data, SRT_MSGCTRL& msgCtrl,
                              std::shared_ptr<SRTNet::NetworkConnection>& ctx, SRTSOCKET socket) {
        EXPECT_EQ(ctx, serverCtx);
        EXPECT_EQ(*data, sendBuffer);
        clientSocket = socket;
        {
            std::lock_guard<std::mutex> lock(serverMutex);
            serverGotData = true;
        }
        SRT_MSGCTRL msgSendCtrl = srt_msgctrl_default;
        EXPECT_TRUE(server.sendData(data->data(), data->size(), &msgSendCtrl, socket));
        serverCondition.notify_one();
        return true;
    };

    std::condition_variable clientCondition;
    std::mutex clientMutex;
    bool clientGotData = false;
    client.receivedData = [&](std::unique_ptr<std::vector<uint8_t>>& data, SRT_MSGCTRL& msgCtrl,
                              std::shared_ptr<SRTNet::NetworkConnection>& ctx, SRTSOCKET socket) {
        EXPECT_EQ(ctx, clientCtx);
        EXPECT_EQ(*data, sendBuffer);
        {
            std::lock_guard<std::mutex> lock(clientMutex);
            clientGotData = true;
        }
        clientCondition.notify_one();
        return true;
    };

    std::fill(sendBuffer.begin(), sendBuffer.end(), 1);
    SRT_MSGCTRL msgCtrl = srt_msgctrl_default;
    EXPECT_TRUE(client.sendData(sendBuffer.data(), sendBuffer.size(), &msgCtrl));

    {
        std::unique_lock<std::mutex> lock(serverMutex);
        bool successfulWait = serverCondition.wait_for(lock, std::chrono::seconds(2), [&]() { return serverGotData; });
        EXPECT_TRUE(successfulWait) << "Timeout waiting for receiving data from client";
    }

    {
        std::unique_lock<std::mutex> lock(clientMutex);
        bool successfulWait = clientCondition.wait_for(lock, std::chrono::seconds(2), [&]() { return clientGotData; });
        EXPECT_TRUE(successfulWait) << "Timeout waiting for receiving data from server";
    }

    SRT_TRACEBSTATS clientStats;
    client.getStatistics(&clientStats, SRTNetClearStats::no, SRTNetInstant::yes);
    SRT_TRACEBSTATS serverStats;
    server.getStatistics(&serverStats, SRTNetClearStats::no, SRTNetInstant::yes, clientSocket);
    EXPECT_EQ(clientStats.pktSentTotal, 1);
    EXPECT_EQ(clientStats.pktRecvTotal, 1);
    EXPECT_EQ(clientStats.pktSentTotal, serverStats.pktRecvTotal);
    EXPECT_EQ(clientStats.pktRecvTotal, serverStats.pktSentTotal);

    // verify that sending to a stopped client fails
    ASSERT_TRUE(client.stop());
    std::this_thread::sleep_for(std::chrono::seconds(1));
    SRT_MSGCTRL msgSendCtrl = srt_msgctrl_default;
    EXPECT_FALSE(server.sendData(sendBuffer.data(), sendBuffer.size(), &msgSendCtrl, clientSocket));
}

TEST(TestSrt, LargeMessage) {
    class ConnectionCtx {};

    SRTNet server;
    SRTNet client;
    auto serverCtx = std::make_shared<SRTNet::NetworkConnection>();
    serverCtx->mObject = std::make_shared<ConnectionCtx>();
    auto clientCtx = std::make_shared<SRTNet::NetworkConnection>();
    clientCtx->mObject = std::make_shared<ConnectionCtx>();

    server.clientConnected = [&](struct sockaddr& sin, SRTSOCKET newSocket,
                                 std::shared_ptr<SRTNet::NetworkConnection>& ctx) {
        EXPECT_EQ(ctx, serverCtx);
        return ctx;
    };

    // start server and client
    ASSERT_TRUE(server.startServer("127.0.0.1", 8009, 16, 1000, 100, SRT_LIVE_MAX_PLSIZE, kValidPsk, false, serverCtx));
    ASSERT_TRUE(client.startClient("127.0.0.1", 8009, 16, 1000, 100, clientCtx, SRT_LIVE_MAX_PLSIZE, kValidPsk));

    std::vector<uint8_t> sendBuffer(kMaxMessageSize + 1);
    std::fill(sendBuffer.begin(), sendBuffer.end(), 1);
    SRT_MSGCTRL msgCtrl = srt_msgctrl_default;
    EXPECT_FALSE(client.sendData(sendBuffer.data(), sendBuffer.size(), &msgCtrl));
}

// TODO Enable test when STAR-238 is fixed
TEST(TestSrt, DISABLED_RejectConnection) {
    SRTNet server;
    SRTNet client;

    std::condition_variable connectedCondition;
    std::mutex connectedMutex;
    bool connected = false;

    server.clientConnected = [&](struct sockaddr& sin, SRTSOCKET newSocket,
                                 std::shared_ptr<SRTNet::NetworkConnection>& ctx) {
        {
            std::lock_guard<std::mutex> lock(connectedMutex);
            connected = true;
        }
        connectedCondition.notify_one();
        return nullptr;
    };

    auto ctx = std::make_shared<SRTNet::NetworkConnection>();
    EXPECT_TRUE(server.startServer("127.0.0.1", 8009, 16, 1000, 100, SRT_LIVE_MAX_PLSIZE, kValidPsk, false, ctx));
    EXPECT_FALSE(client.startClient("127.0.0.1", 8009, 16, 1000, 100, ctx, SRT_LIVE_MAX_PLSIZE, kValidPsk))
        << "Expected client connection rejected";

    {
        std::unique_lock<std::mutex> lock(connectedMutex);
        bool successfulWait = connectedCondition.wait_for(lock, std::chrono::seconds(2), [&]() { return connected; });
        EXPECT_TRUE(successfulWait) << "Timeout waiting for client to connect";
    }

    auto nClients = 0;
    server.getActiveClients([&](std::map<SRTSOCKET, std::shared_ptr<SRTNet::NetworkConnection>>& activeClients) {
        nClients = activeClients.size();
    });
    EXPECT_EQ(nClients, 0);

    auto [srtSocket, networkConnection] = client.getConnectedServer();
    EXPECT_EQ(networkConnection, nullptr);

    std::condition_variable receiveCondition;
    std::mutex receiveMutex;
    size_t receivedBytes = 0;

    server.receivedData = [&](std::unique_ptr<std::vector<uint8_t>>& data, SRT_MSGCTRL& msgCtrl,
                              std::shared_ptr<SRTNet::NetworkConnection>& ctx, SRTSOCKET socket) {
        {
            std::lock_guard<std::mutex> lock(receiveMutex);
            receivedBytes = data->size();
        }
        receiveCondition.notify_one();
        return true;
    };

    std::vector<uint8_t> sendBuffer(1000);
    std::fill(sendBuffer.begin(), sendBuffer.end(), 1);
    SRT_MSGCTRL msgCtrl = srt_msgctrl_default;
    EXPECT_FALSE(client.sendData(sendBuffer.data(), sendBuffer.size(), &msgCtrl))
        << "Expect to fail sending data from unconnected client";

    {
        std::unique_lock<std::mutex> lock(receiveMutex);
        bool successfulWait =
            receiveCondition.wait_for(lock, std::chrono::seconds(2), [&]() { return receivedBytes == 1000; });
        EXPECT_FALSE(successfulWait) << "Did not expect to receive data from unconnected client";
    }
}

TEST(TestSrt, SingleSender) {
    SRTNet server;
    SRTNet client;

    auto serverCtx = std::make_shared<SRTNet::NetworkConnection>();
    auto clientCtx = std::make_shared<SRTNet::NetworkConnection>();
    clientCtx->mObject = 42;

    std::condition_variable connectedCondition;
    std::mutex connectedMutex;
    bool connected = false;

    // notice when client connects to server
    server.clientConnected = [&](struct sockaddr& sin, SRTSOCKET newSocket,
                                 std::shared_ptr<SRTNet::NetworkConnection>& ctx) {
        {
            std::lock_guard<std::mutex> lock(connectedMutex);
            connected = true;
        }
        connectedCondition.notify_one();
        auto connectionCtx = std::make_shared<SRTNet::NetworkConnection>();
        connectionCtx->mObject = 1111;
        return connectionCtx;
    };

    ASSERT_TRUE(server.startServer("127.0.0.1", 8009, 16, 1000, 100, SRT_LIVE_MAX_PLSIZE, kValidPsk, true, serverCtx));
    ASSERT_TRUE(client.startClient("127.0.0.1", 8009, 16, 1000, 100, clientCtx, SRT_LIVE_MAX_PLSIZE, kValidPsk));

    // check for client connecting
    {
        std::unique_lock<std::mutex> lock(connectedMutex);
        bool successfulWait = connectedCondition.wait_for(lock, std::chrono::seconds(2), [&]() { return connected; });
        ASSERT_TRUE(successfulWait) << "Timeout waiting for client to connect";
    }

    auto nClients = 0;
    server.getActiveClients([&](std::map<SRTSOCKET, std::shared_ptr<SRTNet::NetworkConnection>>& activeClients) {
        nClients = activeClients.size();
        for (const auto& socketNetworkConnectionPair : activeClients) {
            int32_t number = 0;
            EXPECT_NO_THROW(number = std::any_cast<int32_t>(socketNetworkConnectionPair.second->mObject));
            EXPECT_EQ(number, 1111);
        }
    });
    EXPECT_EQ(nClients, 1);

    auto [srtSocket, networkConnection] = client.getConnectedServer();
    EXPECT_NE(networkConnection, nullptr);
    int32_t number = 0;
    EXPECT_NO_THROW(number = std::any_cast<int32_t>(networkConnection->mObject));
    EXPECT_EQ(number, 42);

    // start a new client, should fail since we only accept one single client
    connected = false;
    SRTNet client2;
    ASSERT_FALSE(client2.startClient("127.0.0.1", 8009, 16, 1000, 100, clientCtx, SRT_LIVE_MAX_PLSIZE, kValidPsk));

    server.getActiveClients([&](std::map<SRTSOCKET, std::shared_ptr<SRTNet::NetworkConnection>>& activeClients) {
        nClients = activeClients.size();
        for (const auto& socketNetworkConnectionPair : activeClients) {
            int32_t number = 0;
            EXPECT_NO_THROW(number = std::any_cast<int32_t>(socketNetworkConnectionPair.second->mObject));
            EXPECT_EQ(number, 1111);
        }
    });
    EXPECT_EQ(nClients, 1);

    EXPECT_TRUE(server.stop());
}

TEST(TestSrt, BindAddressForCaller) {
    SRTNet server;
    SRTNet client;

    auto serverCtx = std::make_shared<SRTNet::NetworkConnection>();
    auto clientCtx = std::make_shared<SRTNet::NetworkConnection>();
    clientCtx->mObject = 42;

    std::condition_variable connectedCondition;
    std::mutex connectedMutex;
    bool connected = false;

    // notice when client connects to server
    server.clientConnected = [&](struct sockaddr& sin, SRTSOCKET newSocket,
                                 std::shared_ptr<SRTNet::NetworkConnection>& ctx) {
        {
            std::lock_guard<std::mutex> lock(connectedMutex);
            connected = true;
        }
        connectedCondition.notify_one();
        auto connectionCtx = std::make_shared<SRTNet::NetworkConnection>();
        connectionCtx->mObject = 1111;
        return connectionCtx;
    };

    ASSERT_TRUE(server.startServer("127.0.0.1", 8010, 16, 1000, 100, SRT_LIVE_MAX_PLSIZE, kValidPsk, false, serverCtx));
    ASSERT_TRUE(client.startClient("127.0.0.1", 8010, "0.0.0.0", 8011, 16, 1000, 100, clientCtx, SRT_LIVE_MAX_PLSIZE,
                                   kValidPsk));

    // check for client connecting
    {
        std::unique_lock<std::mutex> lock(connectedMutex);
        bool successfulWait = connectedCondition.wait_for(lock, std::chrono::seconds(2), [&]() { return connected; });
        ASSERT_TRUE(successfulWait) << "Timeout waiting for client to connect";
    }

    size_t nClients = 0;
    server.getActiveClients([&](std::map<SRTSOCKET, std::shared_ptr<SRTNet::NetworkConnection>>& activeClients) {
        nClients = activeClients.size();
        for (const auto& socketNetworkConnectionPair : activeClients) {
            std::pair<std::string, uint16_t> peerIPAndPort =
                getPeerIpAndPortFromSRTSocket(socketNetworkConnectionPair.first);
            EXPECT_EQ(peerIPAndPort.first, "127.0.0.1");
            EXPECT_EQ(peerIPAndPort.second, 8011);

            std::pair<std::string, uint16_t> ipAndPort =
                getBindIpAndPortFromSRTSocket(socketNetworkConnectionPair.first);
            EXPECT_EQ(ipAndPort.first, "127.0.0.1");
            EXPECT_EQ(ipAndPort.second, 8010);
        }
    });
    EXPECT_EQ(nClients, 1);

    std::pair<std::string, uint16_t> serverIPAndPort = getBindIpAndPortFromSRTSocket(server.getBoundSocket());
    EXPECT_EQ(serverIPAndPort.first, "127.0.0.1");
    EXPECT_EQ(serverIPAndPort.second, 8010);

    std::pair<std::string, uint16_t> clientIPAndPort = getBindIpAndPortFromSRTSocket(client.getBoundSocket());
    EXPECT_EQ(clientIPAndPort.first, "127.0.0.1");
    EXPECT_EQ(clientIPAndPort.second, 8011);
}

TEST(TestSrt, AutomaticPortSelection) {
    SRTNet server;
    SRTNet client;

    auto serverCtx = std::make_shared<SRTNet::NetworkConnection>();
    auto clientCtx = std::make_shared<SRTNet::NetworkConnection>();
    clientCtx->mObject = 42;

    std::condition_variable connectedCondition;
    std::mutex connectedMutex;
    bool connected = false;

    // notice when client connects to server
    server.clientConnected = [&](struct sockaddr& sin, SRTSOCKET newSocket,
                                 std::shared_ptr<SRTNet::NetworkConnection>& ctx) {
        {
            std::lock_guard<std::mutex> lock(connectedMutex);
            connected = true;
        }
        connectedCondition.notify_one();
        auto connectionCtx = std::make_shared<SRTNet::NetworkConnection>();
        connectionCtx->mObject = 1111;
        return connectionCtx;
    };

    const uint16_t kAnyPort = 0;

    ASSERT_TRUE(server.startServer("0.0.0.0", kAnyPort, 16, 1000, 100, SRT_LIVE_MAX_PLSIZE, kValidPsk, false, serverCtx));

    std::pair<std::string, uint16_t> serverIPAndPort = getBindIpAndPortFromSRTSocket(server.getBoundSocket());
    EXPECT_EQ(serverIPAndPort.first, "0.0.0.0");
    EXPECT_GT(serverIPAndPort.second, 1024); // We expect it won't pick a privileged port

    ASSERT_TRUE(client.startClient("127.0.0.1", serverIPAndPort.second, "0.0.0.0", kAnyPort, 16, 1000, 100, clientCtx,
                                   SRT_LIVE_MAX_PLSIZE, kValidPsk));

    // check for client connecting
    {
        std::unique_lock<std::mutex> lock(connectedMutex);
        bool successfulWait = connectedCondition.wait_for(lock, std::chrono::seconds(2), [&]() { return connected; });
        ASSERT_TRUE(successfulWait) << "Timeout waiting for client to connect";
    }

    std::pair<std::string, uint16_t> clientIPAndPort = getBindIpAndPortFromSRTSocket(client.getBoundSocket());
    EXPECT_EQ(clientIPAndPort.first, "127.0.0.1");
    EXPECT_GT(clientIPAndPort.second, 1024); // We expect it won't pick a privileged port
    EXPECT_NE(clientIPAndPort.second, serverIPAndPort.second);

    size_t nClients = 0;
    server.getActiveClients([&](std::map<SRTSOCKET, std::shared_ptr<SRTNet::NetworkConnection>>& activeClients) {
        nClients = activeClients.size();
        for (const auto& socketNetworkConnectionPair : activeClients) {
            std::pair<std::string, uint16_t> peerIPAndPort =
                getPeerIpAndPortFromSRTSocket(socketNetworkConnectionPair.first);
            EXPECT_EQ(peerIPAndPort.first, "127.0.0.1");
            EXPECT_EQ(peerIPAndPort.second, clientIPAndPort.second);

            std::pair<std::string, uint16_t> ipAndPort =
                getBindIpAndPortFromSRTSocket(socketNetworkConnectionPair.first);
            EXPECT_EQ(ipAndPort.first, "127.0.0.1");
            EXPECT_EQ(ipAndPort.second, serverIPAndPort.second);
        }
    });
    EXPECT_EQ(nClients, 1);
}
