#!/bin/bash

echo 'Building SRT'
cd srt
./configure
make
echo 'Building wrapper'
cd ../cppSRTWrapper/
cmake CMakeLists.txt
make

