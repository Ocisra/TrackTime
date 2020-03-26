#!/bin/bash

#This have to be executed to install yotta

# Check if all the necessary tools are installed
# Cmake
if command -v cmake
then
  echo -e "\e[31mCmake is not installed, please install it\e[0m"
fi
# Make
if command -v make
then
  echo -e "\e[31mMake is not installed, please install it\e[0m"
fi

# Create the directory where yotta stores the data
mkdir /var/lib/yotta

# copy the service config file under the right directory
cp ./yotta.service /usr/lib/systemd/system/yotta.service


# Compile
cmake ./CMakeLists.txt
make

# Place executables under /bin
cp ./bin/* /bin
