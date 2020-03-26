#!/bin/bash

# This has to be executed to uninstall yotta

# Remove the directory where yotta stores the datas
rm -rf /var/lib/yotta

# Remove the unit file
rm -f /usr/lib/systemd/system/yotta.service

# Remove the binaries
rm -f /bin/yotta /bin/yotta_daemon

echo -e "Done"
