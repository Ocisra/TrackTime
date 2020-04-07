#!/bin/bash

# This has to be executed to uninstall yotta

# Disable the daemon
if systemctl is-enabled yotta_daemon
then
  systemctl disable --now yotta_daemon
fi
# Disactive the daemon
if systemctl is-active yotta_daemon
then
  systemctl stop yotta_daemon
fi

# Remove the directory where yotta stores the datas
rm -rf /var/lib/yotta

# Remove the unit file
rm -f /usr/lib/systemd/system/yotta.service

# Remove the binaries
rm -f /bin/yotta /bin/yotta_daemon

echo -e "Done"
