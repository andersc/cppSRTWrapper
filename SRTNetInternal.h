//
// Created by Anders Cedronius on 2019-04-22.
//

#ifndef CPPSRTWRAPPER_SRTGLOBALHANDLER_H
#define CPPSRTWRAPPER_SRTGLOBALHANDLER_H

#include <iostream>
#include <sstream>

// GLobal Logger -- Start
#define LOGG_NOTIFY 1
#define LOGG_WARN 2
#define LOGG_ERROR 4
#define LOGG_FATAL 8
#define LOGG_MASK  LOGG_NOTIFY | LOGG_WARN | LOGG_ERROR | LOGG_FATAL //What to logg?

#ifdef DEBUG
#define SRT_LOGGER(l,g,f) \
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
}
#else
#define SRT_LOGGER(l,g,f)
#endif
// GLobal Logger -- End

#endif //CPPSRTWRAPPER_SRTGLOBALHANDLER_H
