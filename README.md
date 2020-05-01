# Yotta  

This is a time tracking app and also the first project I post on Github.  
Bugs, questions, comments, requests : open an issue.  
Yotta has only be tested under ArchLinux but I guess it works under all the other Linux distributions.  
The installation will delete any executable named `yotta_daemon` or `yotta`. Use at your own risk.  
***

## Installation  
First of all, make sure you have the necessary packages installed : `cmake`, `make`, `git` and that you can take advantage of root privileges.  
Then, clone this repository :  
```
git clone https://github.com/ocisra/yotta.git
cd yotta
```  
Make the installation script executable and execute it :  
```
chmod +x ./install.sh
sudo ./install.sh
```
Finally, enable the daemon
```
sudo systemctl enable --now yotta
```
  
## Upgrade  
Move to the folder in which you cloned the repository :  
```
cd /path/to/yotta
```  
Update your copy of the repository :  
```
git pull
```  
Execute the installation script :  
```
sudo ./install.sh
```  

## Uninstall  
Move to the folder in which you cloned the repository :  
```
cd /path/to/yotta
```  
Execute the uninstallation script :  
```
sudo ./uninstall.sh
```  
Delete the directory : 
```
cd ..
rm -rf ./yotta 
```  

## Usage  
Show the uptimes off all the processes running and finished
```shell script
yotta
```
Display only the uptime the process mentionned
```shell script
yotta <process name>
```
Show the help message for more options
```shell script
yotta -h
```  

#### Help  
```
Usage: yotta [options] [<process> ...]

Options:
  -v, --version                       Display the version and exit
  -h, --help                          Dipslay this help and exit
  -b, --boot                          Display only the informations since the last boot
  -B, --all-but-boot                  Display all the informations except those of the last boot
  -d, --day                           Display the uptime in days
  -H, --hour                          Display the uptime in hours
  -m, --minute                        Display the uptime in minutes
  -s, --second                        Display the uptime in seconds
  -j, --clock-tick                    Display the uptime in clock ticks
  -f, --default-time-format           Display the uptime in default format
  -g, --greater-uptime-than <time>    Display only the processes with a greater uptime than <time> seconds
  -l, --lower-uptime-than <time>      Display only the processes with a lower uptime than <time> seconds
```