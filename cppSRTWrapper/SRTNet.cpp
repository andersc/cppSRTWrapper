//
// Created by Anders Cedronius on 2019-04-21.
//

#include "SRTNet.h"

SRTNet::SRTNet() {
    std::cout << __FILE__ << " " << __LINE__ << ": SRTNet constructed" << std::endl;
}

SRTNet::SRTNet(const SRTNet& orig) {
    std::cout << __FILE__ << " " << __LINE__ << ": SRTNet copy constructor called" << std::endl;
}

SRTNet::~SRTNet() {
    srt_cleanup();
    std::cout << __FILE__ << " " << __LINE__ << ": SRTNet destruct" << std::endl;
}

void SRTNet::init() {
    srt_startup();
}
