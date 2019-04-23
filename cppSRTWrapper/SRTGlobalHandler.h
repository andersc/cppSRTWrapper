//
// Created by Anders Cedronius on 2019-04-22.
//

#ifndef CPPSRTWRAPPER_SRTGLOBALHANDLER_H
#define CPPSRTWRAPPER_SRTGLOBALHANDLER_H

#include <iostream>
#include <atomic>

#include "../srt/srtcore/srt.h"

class SRTGlobalHandler {
private:
    std::atomic<int> intNumConnections; //Unused

    SRTGlobalHandler(){
        std::cout << __FILE__ << " " << __LINE__ << ": SRTGlobalHandler constructed:" << std::endl;
        intNumConnections=0;
        int result=srt_startup();
        if (result) {
            std::cout << __FILE__ << " " << __LINE__ << ": srt_startup failed." << std::endl;
        }
    };
    ~SRTGlobalHandler(){
        std::cout << __FILE__ << " " << __LINE__ << ": SRTGlobalHandler destruct:" << std::endl;
        if (intNumConnections) {
            std::cout << __FILE__ << " " << __LINE__ << ": SRT garbage collection when connections still active:" << std::endl;
        }
        int result=srt_cleanup();
        if (result) {
            std::cout << __FILE__ << " " << __LINE__ << ": srt_cleanup failed." << std::endl;
        }
    };

    // delete copy and move constructors and assign operators
    SRTGlobalHandler(SRTGlobalHandler const&) = delete;             // Copy construct
    SRTGlobalHandler(SRTGlobalHandler&&) = delete;                  // Move construct
    SRTGlobalHandler& operator=(SRTGlobalHandler const&) = delete;  // Copy assign
    SRTGlobalHandler& operator=(SRTGlobalHandler &&) = delete;      // Move assign

public:
    static SRTGlobalHandler& GetInstance();
    int getActiveConnections() {return intNumConnections;}
};

#endif //CPPSRTWRAPPER_SRTGLOBALHANDLER_H
