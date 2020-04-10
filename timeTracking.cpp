#include <csignal>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <unistd.h>
#include <utility>
#include <vector>

#include "log.h"
#include "util.hpp"

/// Dircetory where datas are stored while yotta is not running
const std::string DATA_DIR = "/var/lib/yotta/";

/// Time to sleep before checking for new/deleted processes
const int TT_PRECISION = 2;



/**
 * Get the time the system was up since last boot
 *
 * System uptime is the first word stored in '/proc/uptime'
 *
 * @return system uptime in seconds
 */
float getSystemUptime () {
    float systemUptime;
    std::ifstream uptime("/proc/uptime");
    if(!uptime.is_open())
        error("File not found or permission denied : /proc/uptime", FATAL);
    uptime >> systemUptime;
    return systemUptime;
}

/**
 * Check if a string contains at least one digit
 *
 * @param str : the string to check
 *
 * @return true  : if str contains at least a digit
 *         false : if str does not contain any digit
 */
bool containsNumber(std::string& str){
    return (str.find_first_of("0123456789") != std::string::npos);
}

/**
 * Initiate the process buffer
 *
 * Look for all directories named '/proc/XXXX', each one represent one process
 * In these directories, read './stat', in brackets is the process name and 22th word is process start time since boot
 *
 * @return process buffer with PID, name and start time
 *
 */
std::map <int, std::pair<std::string, int>> initProcessBuffer () {
    std::map <int, std::pair<std::string, int>> processBuffer;
    std::string path;
    int pid;
    int wordCount;
    int processStartTime;
    char c;

    for (auto& p: std::filesystem::directory_iterator("/proc")) {
        if (p.is_directory()) {
            path = p.path().string();
            if (containsNumber(path)) {
                pid = std::stoi(path.substr(6));
                std::ifstream pidStat(path + "/stat");
                if (pidStat.is_open()) {
                    wordCount = 1;
                    std::string processName;
                    while (wordCount != 22) {
                        pidStat.get(c);
                        if (c == '(') {
                            int depth = 0;
                            while (c != ')' || depth != 0) {
                                if (c == '(')
                                    depth += 1;
                                pidStat.get(c);
                                processName += c;
                                if (c == ')')
                                    depth -= 1;
                            }
                            processName.pop_back();
                        }
                        if (c == ' ')
                            wordCount += 1;
                    }
                    pidStat >> processStartTime;
                    processBuffer.insert({pid, std::make_pair(processName, processStartTime)});
                } else { // the process certainly ended between the beginning and the end of the function
                    std::string errmsg = "File not found or permission denied : " + path + "/stat";
                    error(errmsg.c_str(), INFO);
                }
            }
        }
    }
    return processBuffer;
}

/**
 * Update the process buffer to match the running processes
 *
 * Processes that were killed had already been deleted from the process buffer. We only need to add the new ones
 * We add processes from after the last 'old' process to the end of newPidList
 * The process to do it is the same than in initProcessBuffer
 * 
 * @param processBuffer : the process buffer to update
 * @param pidList : all PIDs present in the process buffer
 * @param newPidList : all PIDs actually running
 * @param offset : difference between the place of the last process in pidList and his place in newPidList
 */
void updateProcessBuffer(std::map<int, std::pair<std::string, int>>& processBuffer, std::vector<int>& pidList,
                         std::vector<int>& newPidList, int& offset) {
    // All of the old processes fits in new processes minus offset. If there are more processes than those in newPL,
    // I have to add them to the buffer
    if (pidList.size() - offset < newPidList.size()) {
        std::string path;
        int wordCount;
        char c;
        int processStartTime;

        // I start from the last old process and add all following process to the buffer
        for (auto i = pidList.size() - offset; i < newPidList.size(); i++) {
            path = "/proc/" + std::to_string(newPidList[i]) + "/stat";
            std::ifstream pidStat(path);
            if (pidStat.is_open()) {
                wordCount = 1;
                std::string processName;
                while (wordCount != 22) {
                    pidStat.get(c);
                    if (c == '(') {
                        int depth = 0;
                        while (c != ')' || depth != 0) {
                            if (c == '(')
                                depth += 1;
                            pidStat.get(c);
                            processName += c;
                            if (c == ')')
                                depth -= 1;
                        }
                        processName.pop_back();
                    }
                    if (c == ' ')
                        wordCount += 1;
                }
                pidStat >> processStartTime;
                processBuffer.insert({newPidList[i], std::make_pair(processName, processStartTime)});
            } else { // the process has certainly finished between getNewPidList and now
                std::string errmsg = "File not found or permission denied : " + path;
                error(errmsg.c_str(), INFO);
            }
        }
    }
}

/**
 * Initiate the process uptime buffer
 *
 * Copy all names of processes in processBuffer to timeBuffer
 * Uptime is 0 at the initialisation
 *
 * @param processBuffer : all actually running processes, with their names
 *
 * @return uptime buffer with name and uptime since last boot
 */
std::map<std::string, float> initUptimeBuffer (std::map<int, std::pair<std::string, int>>& processBuffer) {
    std::map <std::string, float> uptimeBuffer;
    for (auto& s : processBuffer) {
        uptimeBuffer.insert({s.second.first, 0});
    }
    return uptimeBuffer;

}

/**
 * Get the PID list of running processes
 *
 * Get the numbers of each subdirectories of /proc with numbers in their path
 * The only subdirectories that has numbers in their path are processes
 *
 * @return the PID list
 */
std::vector <int> getNewPidList () {
    std::vector <int> newPidList;
    std::string path;
    int pid;
    for (auto& p: std::filesystem::directory_iterator("/proc")) {
        if (p.is_directory()) {
            path = p.path().string();
            // get the pid (process) but not the tid (thread)
            // ./task is only contained in pid directories
            if (containsNumber(path) && std::filesystem::exists(p.path().string() + "/task")) {
                pid = std::stoi(path.substr(6));
                newPidList.push_back(pid);
            }
        }
    }
    return newPidList;
}

/**
 * Save the buffers when the program is asked to finish
 *
 * Do as if all active processes had just been closed
 * Add uptimes of current boot to those in the database
 * Rewrite the whole database file with the new values
 *
 * @param processBuffer : buffer of still active processes
 * @param uptimeBuffer : buffer of uptimes of already closed program of the actual boot
 * @param CLK_TCK : number of jiffies in a second
 * @param gSignalStatus : signal received by the program
 */
void save(std::map<int, std::pair<std::string, int>> &processBuffer, std::map<std::string, float> &uptimeBuffer,
          const int &CLK_TCK, volatile sig_atomic_t& gSignalStatus) {

    float systemUptime = getSystemUptime();
    std::string processName;
    float processUptime;
    for (auto& s : processBuffer) {
        processName = s.second.first;
        float processStartTime = s.second.second;
        processUptime = systemUptime - (processStartTime / CLK_TCK); // in seconds
        uptimeBuffer[processName] += processUptime;
    }
    std::string uptimeDataFile = DATA_DIR + "uptime";
    std::ifstream uptimeDataFileR (uptimeDataFile);
    if (!uptimeDataFileR.is_open()) {
        std::ofstream openingDataFile (uptimeDataFile);
        openingDataFile.close();
        uptimeDataFileR.open(uptimeDataFile);
        if (!uptimeDataFileR.is_open()) {
            std::string errmsg = "File not found or permission denied : " + uptimeDataFile;
            error(errmsg.c_str(), FATAL);
        }
    }


    float previousUptime;
    while (uptimeDataFileR >> processName) {
        std::string buf;
        uptimeDataFileR >> buf;
        while (!isFloat(buf)) {
            processName += " " + buf;
            uptimeDataFileR >> buf;
        }
        previousUptime = std::stof(buf);
        processName.pop_back(); // removing the closing parenthesis
        float uptimeSinceLastBoot = uptimeBuffer[processName];
        processUptime = previousUptime + uptimeSinceLastBoot;
        uptimeBuffer[processName] += processUptime;
    }
    uptimeDataFileR.close();
    std::ofstream uptimeDataFileW (uptimeDataFile, std::ios::out | std::ios::trunc);
    for (auto& s : uptimeBuffer) {
        uptimeDataFileW << s.first << ": " << s.second << "\n";
    }
    uptimeDataFileW.close();
}

/**
 * Main of the thread that count processes uptimes
 *
 * Compare the processes actually running to those that were TT_PRECISION before
 * If one has ended, count the uptime
 * If one is new, add it to the processes running
 * Repeat
 *
 * @param processBuffer : buffer of still active processes
 * @param uptimeBuffer : buffer of uptimes of already closed program of the actual boot
 * @param gSignalStatus : signal received by the program
 */
void timeTracking(std::map<std::string, float> &uptimeBuffer, std::map<int, std::pair<std::string, int>> &processBuffer,
                  volatile sig_atomic_t& gSignalStatus) {

    const int CLK_TCK = sysconf(_SC_CLK_TCK);

    processBuffer = initProcessBuffer();
    uptimeBuffer = initUptimeBuffer(processBuffer);
    std::vector <int> pidList = getNewPidList(); //initiate the pidList

    while (true) {
        if (gSignalStatus == SIGTERM) { // if sigterm received, save and exit
            save(processBuffer, uptimeBuffer, CLK_TCK, gSignalStatus);
            return;
        }

        std::vector<int> newPidList = getNewPidList();
        while (newPidList == pidList) { //check every TT_PRECISION seconds if there was a change in the list of processes
            sleep(TT_PRECISION);
            newPidList = getNewPidList();
        }

        int offset = 0;
        for (auto i = 0; i + offset < pidList.size(); i++) {
            while ((i >= newPidList.size() - 1 || pidList[i + offset] != newPidList[i]) && i + offset < pidList.size()) {
                //if the process has ended (i.e. the process is in pidList but not in newPidList), add it to uptimeBuffer
                std::string processName = processBuffer.find(pidList[i + offset])->second.first;
                float processStartTime = processBuffer.find(pidList[i + offset])->second.second;
                float systemUptime = getSystemUptime();
                float processUptime = systemUptime - (processStartTime / CLK_TCK) - 1; // in seconds, -1 to average because sleep is 2 seconds
                if (processUptime < 0)
                    processUptime = 0;
                processBuffer.erase(processBuffer.find(pidList[i + offset]));
                uptimeBuffer[processName] += processUptime;
                offset++;
            }
        }
        updateProcessBuffer(processBuffer, pidList, newPidList, offset);

        pidList = newPidList;
    }
}