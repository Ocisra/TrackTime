#!/bin/bash

# This has to be executed to install yotta

# Make the uninstallation script executable
chmod +x ./uninstall.sh

# Check if all the necessary tools are installed
# Cmake
if ! command -v cmake
then
  echo -e "\e[31mCmake is not installed, please install it\e[0m"
  exit 1
fi
# Make
if  ! command -v make
then
  echo -e "\e[31mMake is not installed, please install it\e[0m"
  exit 1
fi

# Create the directory where yotta stores the datas
mkdir -p /var/lib/yotta

# Clean and copy the service config file under the right directory
rm -f /usr/lib/systemd/system/yotta.service
cp ./yotta.service /usr/lib/systemd/system/yotta.service


# Compile
cmake ./CMakeLists.txt
make

# Clean and place executables under /bin
rm -f /bin/yotta /bin/yotta_daemon
cp ./bin/* /bin

echo -e "Done"
