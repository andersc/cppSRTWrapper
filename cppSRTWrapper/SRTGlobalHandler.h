//
// Created by Anders Cedronius on 2019-04-22.
//

#ifndef CPPSRTWRAPPER_SRTGLOBALHANDLER_H
#define CPPSRTWRAPPER_SRTGLOBALHANDLER_H

#include <iostream>

#include "../srt/srtcore/srt.h"

class SRTGlobalHandler {
public:
    static SRTGlobalHandler& GetInstance();
private:
    SRTGlobalHandler(){
        std::cout << __FILE__ << " " << __LINE__ << ": SRTGlobalHandler constructed:" << std::endl;
        int result=srt_startup();
        if (result) {
            std::cout << __FILE__ << " " << __LINE__ << ": srt_startup failed." << std::endl;
        }
    };
    SRTGlobalHandler(const SRTGlobalHandler&){};
    SRTGlobalHandler& operator=(const SRTGlobalHandler&){};
    ~SRTGlobalHandler(){
        std::cout << __FILE__ << " " << __LINE__ << ": SRTGlobalHandler destruct:" << std::endl;
        int result=srt_cleanup();
        if (result) {
            std::cout << __FILE__ << " " << __LINE__ << ": srt_cleanup failed." << std::endl;
        }
    };
};

#endif //CPPSRTWRAPPER_SRTGLOBALHANDLER_H
