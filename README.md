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
sudo chmod +x ./install.sh
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
