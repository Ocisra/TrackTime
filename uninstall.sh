#!/bin/bash

# this has to be executed to uninstall yotta

# disable the daemon
if ! systemctl is-enabled yotta
then
  systemctl disable --now yotta
fi
# disactive the daemon
if ! systemctl is-active yotta
then
  systemctl stop yotta
fi

# remove the directory where yotta stores the datas
rm -rf /var/lib/yotta

# remove the unit file
rm -f /usr/lib/systemd/system/yotta.service

# remove the binaries
rm -f /bin/yotta /bin/yotta_daemon

# remove the log file
rm -f /var/log/yotta.log

echo -e "done"
