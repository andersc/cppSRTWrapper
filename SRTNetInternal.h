//
// Created by Anders Cedronius on 2019-04-22.
//

#ifndef CPPSRTWRAPPER_SRTGLOBALHANDLER_H
#define CPPSRTWRAPPER_SRTGLOBALHANDLER_H

#include <iostream>
#include <sstream>
#include <sys/syslog.h>

// Global Logger -- Start
#define LOGG_NOTIFY LOG_NOTICE
#define LOGG_WARN LOG_WARNING
#define LOGG_ERROR LOG_ERR
#define LOGG_FATAL LOG_CRIT
#define LOGG_LEVEL LOGG_NOTIFY //What to logg?

#ifdef DEBUG
#define SRT_LOGGER(l,g,f) \
{ \
  if (g <= LOGG_LEVEL) { \
    std::ostringstream a; \
    if (SRTNet::logHandler == SRTNet::defaultLogHandler) { \
      if (g == LOGG_NOTIFY) {a << "Notification: ";}             \
      else if (g == LOGG_WARN) {a << "Warning: ";} \
      else if (g == LOGG_ERROR) {a << "Error: ";} \
      else if (g == LOGG_FATAL) {a << "Fatal: ";} \
      if (l) {a << __FILE__ << " " << __LINE__ << " ";} \
    } \
    a << f; \
    SRTNet::logHandler(nullptr, g, __FILE__, __LINE__, nullptr, a.str().c_str()); \
  } \
}
#else
#define SRT_LOGGER(l,g,f)
#endif
// GLobal Logger -- End

#endif //CPPSRTWRAPPER_SRTGLOBALHANDLER_H
