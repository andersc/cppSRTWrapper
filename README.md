# cppSRTWrapper

*Simple C++ wrapper of the SRT protocol* 

_macOS_. Install Xcode + Command line tools (will install first time you open Xcode)

_Ubuntu 18.04_ in a terminal enter this -> 

sudo apt-get update

sudo apt install tcl cmake libssl-dev build-essential


Then for both _macOS_ and _Ubuntu 18.04_ inside the cppSRTWrapper directory


git submodule update --init --recursive

./buildALL.sh

cd cppSRTWrapper/

./cppSRTWrapper



Known errors . You might get this error ->
!!FATAL!!:SRT.c: CChannel reported ERROR DURING TRANSMISSION - IPE. INTERRUPTING worker anyway.
!!FATAL!!:SRT.c: CChannel reported ERROR DURING TRANSMISSION - IPE. INTERRUPTING worker anyway.

Fixed in SRT branch *dev-fix-misleading-close-error*)
