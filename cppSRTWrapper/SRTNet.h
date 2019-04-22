//
// Created by Anders Cedronius on 2019-04-21.
//

#ifndef CPPSRTWRAPPER_SRTNET_H
#define CPPSRTWRAPPER_SRTNET_H

#include <iostream>

#include "../srt/srtcore/srt.h"
#include "SRTGlobalHandler.h"


class SRTNet {
public:
    SRTNet();
    SRTNet(const SRTNet& orig);
    virtual ~SRTNet();

    void init();
private:

    SRTGlobalHandler& pSRTHandler = SRTGlobalHandler::GetInstance();

};


#endif //CPPSRTWRAPPER_SRTNET_H
