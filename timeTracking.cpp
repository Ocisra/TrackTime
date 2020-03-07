#include <csignal>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <unistd.h>
#include <utility>
#include <vector>

#include "timeTracking.h"

const std::string DATA_DIR = "/var/lib/yotta/";

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
        throw std::ios_base::failure("File not found");
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
 * Check if the st
 * @param str
 * @return
 */
bool isInt (std::string& str) {
    bool containsOnlyNumber = true;
    for (auto& s : str) {
        if (!isdigit(s)) {
            containsOnlyNumber = false;
            break;
        }
    }
    return containsOnlyNumber;
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
                std::ifstream pidStat (path + "/stat");
                if(!pidStat.is_open())
                    throw std::ios_base::failure("File not found");
                wordCount = 1;
                std::string processName;
                while (wordCount != 22) {
                    pidStat.get(c);
                    if (c == '('){
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
            }
        }
    }
    return processBuffer;
}

/**
 * Update the process buffer to match with actually running processes
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
            std::ifstream pidStat (path);
            if(!pidStat.is_open())
                throw std::ios_base::failure("File not found");
            wordCount = 1;
            std::string processName;
            while (wordCount != 22) {
                pidStat.get(c);
                if (c == '('){
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
            if (containsNumber(path)) {
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
    }
    if (!uptimeDataFileR.is_open())
        throw std::ios_base::failure("file not found");

    float previousUptime;
    while (uptimeDataFileR >> processName) {
        std::string buf;
        uptimeDataFileR >> buf;
        while (!isInt(buf)) {
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
        uptimeDataFileW << s.first << ": " << s.second << std::endl;
    }
    uptimeDataFileW.close();


    std::exit(gSignalStatus);
}

void timeTracking(std::map<std::string, float> &uptimeBuffer, std::map<int, std::pair<std::string, int>> &processBuffer,
                  volatile sig_atomic_t& gSignalStatus) {
    const int CLK_TCK = sysconf(_SC_CLK_TCK);

    processBuffer = initProcessBuffer();
    uptimeBuffer = initUptimeBuffer(processBuffer);
    std::vector <int> pidList = getNewPidList(); //initiate the pidList

    while (true) {
        if (gSignalStatus == SIGTERM)
            save(processBuffer, uptimeBuffer, CLK_TCK, gSignalStatus);
        std::vector<int> newPidList = getNewPidList();
        while (newPidList == pidList) {
            sleep(2);
            newPidList = getNewPidList();
        }

        int offset = 0;
        for (auto i = 0; i + offset < pidList.size(); i++) {
            while ((i >= newPidList.size() - 1 || pidList[i + offset] != newPidList[i]) && i + offset < pidList.size()) {
                std::string processName = processBuffer.find(pidList[i + offset])->second.first;
                float processStartTime = processBuffer.find(pidList[i + offset])->second.second;
                float systemUptime = getSystemUptime();
                float processUptime = systemUptime - (processStartTime / CLK_TCK); // in seconds
                processBuffer.erase(processBuffer.find(pidList[i + offset]));
                uptimeBuffer[processName] += processUptime;
                offset++;
            }
        }
        updateProcessBuffer(processBuffer, pidList, newPidList, offset);

        pidList = newPidList;
    }
}


/// TODO
/// multiple file is cleaner