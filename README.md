# cppSRTWrapper

*Simple C++ wrapper of the [SRT](https://github.com/Haivision/srt) protocol*.

Current build status:

![Ubuntu 18.04](https://github.com/andersc/cppSRTWrapper/workflows/Ubuntu%2018.04/badge.svg)

## Building

Requires cmake version >= **3.10** and **C++17**

**Release:**

```sh
cmake -DCMAKE_BUILD_TYPE=Release .
make
```

***Debug:***

```sh
cmake -DCMAKE_BUILD_TYPE=Debug .
make
```

Output: 

**libsrtnet.a**

The static SRT C++ wrapper library 
 
**cppSRTWrapper**

*cppSRTWrapper* (executable) runs trough the unit tests and returns EXIT_SUCESS if all unit tests pass.

See the source code for examples on how to use cppSRTWrapper.
