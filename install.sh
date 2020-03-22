#!/bin/bash

#This have to be executed to install yotta


# Compile yotta and yotta_daemon, place them in /bin
# TODO
# TODO

# Create the directory where yotta stores the data
mkdir /var/lib/yotta

# copy the service config file under the right directory
cp ./yotta.service /usr/lib/systemd/system/yotta.service



