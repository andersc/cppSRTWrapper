//
// Created by Anders Cedronius on 2019-04-22.
//

#include "SRTGlobalHandler.h"

SRTGlobalHandler& SRTGlobalHandler::GetInstance()
{
    static SRTGlobalHandler instance;
    return instance;
}