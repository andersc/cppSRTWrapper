//
// Created by Anders Cedronius on 2019-04-21.
//

#include "SRTNet.h"
#include "SRTNetInternal.h"

SRTNet::SRTNet() {
  serverActive = false;
  serverListenThreadActive = false;
  serverPollThreadActive = false;
  clientActive = false;
  clientThreadActive = false;
  LOGGER(true, LOGG_NOTIFY, "SRTNet constructed");
}

SRTNet::~SRTNet() {
  LOGGER(true, LOGG_NOTIFY, "SRTNet destruct");
}

void SRTNet::closeAllClientSockets() {
  clientListMtx.lock();
  int result = SRT_ERROR;
  for (auto &x: clientList) {
    SRTSOCKET g = x.first;
    result = srt_close(g);
    if (result == SRT_ERROR) {
      LOGGER(true, LOGG_ERROR, "srt_close failed: " << srt_getlasterror_str());
    }
  }
  clientList.clear();
  clientListMtx.unlock();
}

bool SRTNet::isIPv4(const std::string &str) {
  struct sockaddr_in sa;
  return inet_pton(AF_INET, str.c_str(), &(sa.sin_addr)) != 0;
}

bool SRTNet::isIPv6(const std::string &str) {
  struct sockaddr_in6 sa;
  return inet_pton(AF_INET6, str.c_str(), &(sa.sin6_addr)) != 0;
}

bool SRTNet::startServer(std::string ip, uint16_t port, int reorder, int32_t latency, int overhead, int mtu) {

  struct sockaddr_in saV4 = {0};
  struct sockaddr_in6 saV6 = {0};

  int ipType = AF_INET;
  if (isIPv4(ip)) {
    ipType = AF_INET;
  } else if (isIPv6(ip)) {
    ipType = AF_INET6;
  } else {
    LOGGER(true, LOGG_ERROR, " " << "Provided IP-Address not valid.");
  }

  std::lock_guard<std::mutex> lock(netMtx);
  int result = 0;

  int32_t yes = 1;
  if (currentMode != Mode::unknown) {
    LOGGER(true, LOGG_ERROR, " " << "SRTNet mode is already set");
    return false;
  }

  if (!clientConnected) {
    LOGGER(true, LOGG_FATAL, "waitForSRTClient needs clientConnected callback method terminating server!");
    return false;
  }

  context = srt_create_socket();
  if (context == SRT_ERROR) {
    LOGGER(true, LOGG_FATAL, "srt_socket: " << srt_getlasterror_str());
    return false;
  }

  if (ipType == AF_INET) {
    saV4.sin_family = AF_INET;
    saV4.sin_port = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &saV4.sin_addr) != 1) {
      LOGGER(true, LOGG_FATAL, "inet_pton failed ");
      srt_close(context);
      return false;
    }
  }

  if (ipType == AF_INET6) {
    saV6.sin6_family = AF_INET6;
    saV6.sin6_port = htons(port);
    if (inet_pton(AF_INET6, ip.c_str(), &saV6.sin6_addr) != 1) {
      LOGGER(true, LOGG_FATAL, "inet_pton failed ");
      srt_close(context);
      return false;
    }
  }

  result = srt_setsockflag(context, SRTO_RCVSYN, &yes, sizeof yes);
  if (result == SRT_ERROR) {
    LOGGER(true, LOGG_FATAL, "srt_setsockflag SRTO_RCVSYN: " << srt_getlasterror_str());
    return false;
  }

  result = srt_setsockflag(context, SRTO_LATENCY, &latency, sizeof latency);
  if (result == SRT_ERROR) {
    LOGGER(true, LOGG_FATAL, "srt_setsockflag SRTO_LATENCY: " << srt_getlasterror_str());
    return false;
  }

  result = srt_setsockflag(context, SRTO_LOSSMAXTTL, &reorder, sizeof reorder);
  if (result == SRT_ERROR) {
    LOGGER(true, LOGG_FATAL, "srt_setsockflag SRTO_LOSSMAXTTL: " << srt_getlasterror_str());
    return false;
  }

  result = srt_setsockflag(context, SRTO_OHEADBW, &overhead, sizeof overhead);
  if (result == SRT_ERROR) {
    LOGGER(true, LOGG_FATAL, "srt_setsockflag SRTO_OHEADBW: " << srt_getlasterror_str());
    return false;
  }

  result = srt_setsockflag(context, SRTO_PAYLOADSIZE, &mtu, sizeof mtu);
  if (result == SRT_ERROR) {
    LOGGER(true, LOGG_FATAL, "srt_setsockflag SRTO_PAYLOADSIZE: " << srt_getlasterror_str());
    return false;
  }

  if (ipType == AF_INET) {
    result = srt_bind(context, (struct sockaddr *) &saV4, sizeof saV4);
    if (result == SRT_ERROR) {
      LOGGER(true, LOGG_FATAL, "srt_bind: " << srt_getlasterror_str());
      srt_close(context);
      return false;
    }
  }

  if (ipType == AF_INET6) {
    result = srt_bind(context, (struct sockaddr *) &saV6, sizeof saV6);
    if (result == SRT_ERROR) {
      LOGGER(true, LOGG_FATAL, "srt_bind: " << srt_getlasterror_str());
      srt_close(context);
      return false;
    }
  }

  result = srt_listen(context, 2);
  if (result == SRT_ERROR) {
    LOGGER(true, LOGG_FATAL, "srt_listen: " << srt_getlasterror_str());
    srt_close(context);
    return false;
  }
  serverActive = true;
  currentMode = Mode::server;
  std::thread(std::bind(&SRTNet::waitForSRTClient, this)).detach();
  return true;
}


void SRTNet::serverEventHandler() {
  SRT_EPOLL_EVENT ready[MAX_WORKERS];
  while (serverActive) {
    int ret = srt_epoll_uwait(poll_id, &ready[0], 5, 1000);
    if (ret == MAX_WORKERS + 1) {
      ret--;
    }

    if (ret > 0) {
      for (int i = 0; i < ret; i++) {
        uint8_t msg[2048];
        SRT_MSGCTRL thisMSGCTRL = srt_msgctrl_default;
        SRTSOCKET thisSocket = ready[i].fd;
        int result = srt_recvmsg2(thisSocket, (char *) msg, sizeof msg, &thisMSGCTRL);
        if (result == SRT_ERROR) {
          LOGGER(true, LOGG_ERROR, "srt_recvmsg error: " << result << " " << srt_getlasterror_str());
          clientListMtx.lock();
          clientList.erase(clientList.find(thisSocket)->first);
          srt_epoll_remove_usock(poll_id, thisSocket);
          srt_close(thisSocket);
          clientListMtx.unlock();
        } else if (result > 0 && recievedData) {
          auto pointer = std::make_unique<std::vector<uint8_t>>(msg, msg + result);
          recievedData(pointer, thisMSGCTRL, clientList.find(thisSocket)->second, thisSocket);
        }
      }
    } else if (ret == -1) {
      LOGGER(true, LOGG_ERROR, "epoll error: " << srt_getlasterror_str());
    }

  }
  LOGGER(true, LOGG_NOTIFY, "serverEventHandler exit");
  srt_epoll_release(poll_id);
}

void SRTNet::waitForSRTClient() {
  int result = SRT_ERROR;
  poll_id = srt_epoll_create();
  srt_epoll_set(poll_id, SRT_EPOLL_ENABLE_EMPTY);
  std::thread(std::bind(&SRTNet::serverEventHandler, this)).detach();

  serverListenThreadActive = true;

  closeAllClientSockets();

  while (serverActive) {
    struct sockaddr_storage their_addr;
    LOGGER(true, LOGG_NOTIFY, "SRT Server wait for client");
    int addr_size = sizeof their_addr;
    SRTSOCKET newSocketCandidate = srt_accept(context, (struct sockaddr *) &their_addr, &addr_size);
    if (newSocketCandidate == -1) {
      continue;
    }
    LOGGER(true, LOGG_NOTIFY, "Client connected: " << newSocketCandidate);
    auto ctx = clientConnected(*(struct sockaddr *) &their_addr, newSocketCandidate);

    if (ctx) {
      const int events = SRT_EPOLL_IN | SRT_EPOLL_ERR;
      clientListMtx.lock();
      clientList[newSocketCandidate] = ctx;
      clientListMtx.unlock();
      result = srt_epoll_add_usock(poll_id, newSocketCandidate, &events);
      if (result == SRT_ERROR) {
        LOGGER(true, LOGG_FATAL, "srt_epoll_add_usock error: " << srt_getlasterror_str());
      }
    } else {
      close(newSocketCandidate);
    }
  }
  serverActive = false;
  serverListenThreadActive = false;
}

//Host can provide a IP or name meaning any IPv4 or IPv6 address or name type www.google.com
//There is no IP-Version preference if a name is given. the first IP-version found will be used
bool SRTNet::startClient(std::string host,
                         uint16_t port,
                         int reorder,
                         int32_t latency,
                         int overhead,
                         std::shared_ptr<NetworkConnection> &ctx,
                         int mtu) {
  std::lock_guard<std::mutex> lock(netMtx);
  if (currentMode != Mode::unknown) {
    LOGGER(true, LOGG_ERROR, " " << "SRTNet mode is already set");
    return false;
  }

  clientContext = ctx;

  int result = 0;
  int32_t yes = 1;
  LOGGER(true, LOGG_NOTIFY, "SRT client startup");

  context = srt_create_socket();
  if (context == SRT_ERROR) {
    LOGGER(true, LOGG_FATAL, "srt_socket: " << srt_getlasterror_str());
    return false;
  }

  result = srt_setsockflag(context, SRTO_SENDER, &yes, sizeof yes);
  if (result == SRT_ERROR) {
    LOGGER(true, LOGG_FATAL, "srt_setsockflag SRTO_SENDER: " << srt_getlasterror_str());
    return false;
  }

  result = srt_setsockflag(context, SRTO_LATENCY, &latency, sizeof latency);
  if (result == SRT_ERROR) {
    LOGGER(true, LOGG_FATAL, "srt_setsockflag SRTO_LATENCY: " << srt_getlasterror_str());
    return false;
  }

  result = srt_setsockflag(context, SRTO_LOSSMAXTTL, &reorder, sizeof reorder);
  if (result == SRT_ERROR) {
    LOGGER(true, LOGG_FATAL, "srt_setsockflag SRTO_LOSSMAXTTL: " << srt_getlasterror_str());
    return false;
  }

  result = srt_setsockflag(context, SRTO_OHEADBW, &overhead, sizeof overhead);
  if (result == SRT_ERROR) {
    LOGGER(true, LOGG_FATAL, "srt_setsockflag SRTO_OHEADBW: " << srt_getlasterror_str());
    return false;
  }

  result = srt_setsockflag(context, SRTO_PAYLOADSIZE, &mtu, sizeof mtu);
  if (result == SRT_ERROR) {
    LOGGER(true, LOGG_FATAL, "srt_setsockflag SRTO_PAYLOADSIZE: " << srt_getlasterror_str());
    return false;
  }

  //get all addresses for connection
  struct addrinfo hints = {0};
  struct addrinfo *svr;
  struct addrinfo *p;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_protocol = IPPROTO_UDP;
  hints.ai_family   = AF_UNSPEC;
  std::stringstream portAsString;
  portAsString << port;
  result = getaddrinfo(host.c_str(),portAsString.str().c_str(),&hints, &svr);
  if (result) {
    LOGGER(true, LOGG_FATAL, "Failed getting the IP target for > " << host << ":" << unsigned(port) << " Errno: " << unsigned(result));
    return false;
  }

  LOGGER(true, LOGG_NOTIFY, "SRT connect");
  for (p = svr; p; p = p->ai_next) {
    result = srt_connect(context, (struct sockaddr *) p->ai_addr,  p->ai_addrlen);
    if (result != SRT_ERROR) {
      LOGGER(true, LOGG_NOTIFY, "Connected to SRT Server " << std::endl)
      break;
    }
  }
  if (result == SRT_ERROR) {
    srt_close(context);
    freeaddrinfo(svr);
    LOGGER(true, LOGG_FATAL, "srt_connect failed " << std::endl)
    return false;
  }
  freeaddrinfo(svr);

  currentMode = Mode::client;
  clientActive = true;
  std::thread(std::bind(&SRTNet::clientWorker, this)).detach();
  return true;
}

void SRTNet::clientWorker() {
  int result = 0;
  clientThreadActive = true;

  while (clientActive) {
    uint8_t msg[2048];
    SRT_MSGCTRL thisMSGCTRL = srt_msgctrl_default;
    result = srt_recvmsg2(context, (char *) msg, sizeof msg, &thisMSGCTRL);
    if (result == SRT_ERROR) {
      if (clientActive) {
        LOGGER(true, LOGG_ERROR, "srt_recvmsg error: " << srt_getlasterror_str());
      }
      break;
    } else if (result > 0 && recievedData) {
      auto pointer = std::make_unique<std::vector<uint8_t>>(msg, msg + result);
      recievedData(pointer, thisMSGCTRL, clientContext, context);
    }
  }

  clientActive = false;
  clientThreadActive = false;
}

bool SRTNet::sendData(uint8_t *data, size_t len, SRT_MSGCTRL *msgCtrl, SRTSOCKET targetSystem) {
  int result;
  if (currentMode == Mode::client && context && clientActive) {
    result = srt_sendmsg2(context, (const char *) data, len, msgCtrl);
  } else if (currentMode == Mode::server && targetSystem && serverActive) {
    result = srt_sendmsg2(targetSystem, (const char *) data, len, msgCtrl);
  } else {
    LOGGER(true, LOGG_ERROR, "Can't send data, the client is not active.");
    return false;
  }

  if (result == SRT_ERROR) {
    LOGGER(true, LOGG_ERROR, "srt_sendmsg2 failed: " << srt_getlasterror_str());
    return false;
  }
  if (result != len) {
    LOGGER(true, LOGG_ERROR, "Failed sending all data");
    return false;
  }
  return true;
}

bool SRTNet::stop() {
  int result = SRT_ERROR;
  std::lock_guard<std::mutex> lock(netMtx);
  if (currentMode == Mode::server) {
    serverActive = false;
    if (context) {
      result = srt_close(context);
      if (result == SRT_ERROR) {
        LOGGER(true, LOGG_ERROR, "srt_close failed: " << srt_getlasterror_str());
        return false;
      }
    }
    closeAllClientSockets();
    int threadRunning = 1000;
    while (serverListenThreadActive) {
      std::this_thread::sleep_for(std::chrono::microseconds(1000));
      if (!--threadRunning) {
        LOGGER(true, LOGG_FATAL, "SRTNet server thread is still running after 1 second. Crash and burn baby!!");
        break;
      }
    }
    LOGGER(true, LOGG_NOTIFY, "Server stopped");
    currentMode = Mode::unknown;
    return true;
  } else if (currentMode == Mode::client) {
    clientActive = false;
    if (context) {
      result = srt_close(context);
      if (result == SRT_ERROR) {
        LOGGER(true, LOGG_ERROR, "srt_close failed: " << srt_getlasterror_str());
        return false;
      }
    }

    int threadRunning = 1000; //Timeout after 1000ms
    while (clientThreadActive) {
      std::this_thread::sleep_for(std::chrono::microseconds(1000));
      if (!--threadRunning) {
        LOGGER(true, LOGG_FATAL, "SRTNet client thread is still running after 1 second. Crash and burn baby!!");
        break;
      }
    }
    LOGGER(true, LOGG_NOTIFY, "Client stopped");
    currentMode = Mode::unknown;
    return true;
  }
  LOGGER(true, LOGG_ERROR, "SRTNet nothing to stop");
  return true;
}

bool SRTNet::getStatistics(SRT_TRACEBSTATS *currentStats, int clear, int instantaneous, SRTSOCKET targetSystem) {
  std::lock_guard<std::mutex> lock(netMtx);
  int result;
  if (currentMode == Mode::client && clientActive && context) {
    result = srt_bistats(context, currentStats, clear, instantaneous);
    if (result == SRT_ERROR) {
      LOGGER(true, LOGG_ERROR, "srt_bistats failed: " << srt_getlasterror_str());
      return false;
    }
  } else if (currentMode == Mode::server && serverActive && targetSystem) {
    result = srt_bistats(targetSystem, currentStats, clear, instantaneous);
    if (result == SRT_ERROR) {
      LOGGER(true, LOGG_ERROR, "srt_bistats failed: " << srt_getlasterror_str());
      return false;
    }
  } else {
    LOGGER(true, LOGG_ERROR, "Statistics not available");
    return false;
  }
  return true;
}
