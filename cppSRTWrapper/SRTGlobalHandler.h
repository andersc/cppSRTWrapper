//
// Created by Anders Cedronius on 2019-04-22.
//

#ifndef CPPSRTWRAPPER_SRTGLOBALHANDLER_H
#define CPPSRTWRAPPER_SRTGLOBALHANDLER_H

#include <iostream>
#include <atomic>
#include <sstream>

#include "../srt/srtcore/srt.h"

// GLobal Logger -- Start


#define LOGG_NOTIFY 1
#define LOGG_WARN 2
#define LOGG_ERROR 4
#define LOGG_FATAL 8
#define LOGG_MASK  LOGG_NOTIFY | LOGG_WARN | LOGG_ERROR | LOGG_FATAL //What to logg?
#define DEBUG  //Turn logging on/off

#ifdef DEBUG
#define LOGGER(l,g,f) \
{ \
std::ostringstream a; \
if (g == (LOGG_NOTIFY & (LOGG_MASK))) {a << "Notification: ";} \
else if (g == (LOGG_WARN & (LOGG_MASK))) {a << "Warning: ";} \
else if (g == (LOGG_ERROR & (LOGG_MASK))) {a << "Error: ";} \
else if (g == (LOGG_FATAL & (LOGG_MASK))) {a << "Fatal: ";} \
if (a.str().length()) { \
if (l) {a << __FILE__ << " " << __LINE__ << " ";} \
a << f << std::endl; \
std::cout << a.str(); \
} \
}srt_bistats
#else
#define LOGGER(l,g,f)
#endif
// GLobal Logger -- End

class SRTGlobalHandler {
private:

    SRTGlobalHandler(){
        LOGGER(true, LOGG_NOTIFY, "SRTGlobalHandler constructed");
        intNumConnections=0;
        int result=srt_startup();
        if (result) {
            LOGGER(true, LOGG_FATAL, "srt_startup failed");
        }
    };
    ~SRTGlobalHandler(){
        LOGGER(true, LOGG_NOTIFY, "SRTGlobalHandler destruct");
        if (intNumConnections) {
            LOGGER(true, LOGG_WARN, "srt_cleanup when connections still active: " << intNumConnections);
        }
        int result=srt_cleanup();
        if (result) {
            LOGGER(true, LOGG_ERROR, "srt_cleanup failed");
        }
    };

    // delete copy and move constructors and assign operators
    SRTGlobalHandler(SRTGlobalHandler const&) = delete;             // Copy construct
    SRTGlobalHandler(SRTGlobalHandler&&) = delete;                  // Move construct
    SRTGlobalHandler& operator=(SRTGlobalHandler const&) = delete;  // Copy assign
    SRTGlobalHandler& operator=(SRTGlobalHandler &&) = delete;      // Move assign

public:
    static SRTGlobalHandler& GetInstance();
    std::atomic<int> intNumConnections; //for debugging connections
};

#endif //CPPSRTWRAPPER_SRTGLOBALHANDLER_H
