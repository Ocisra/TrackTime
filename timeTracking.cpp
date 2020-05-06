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
#include "config.hpp"

/// Dircetory where datas are stored while yotta is not running
const std::string DATA_DIR = "/var/lib/yotta/";




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
                    std::string errmsg = "Init : File not found or permission denied : " + path + "/stat";
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
 * Copy all names of processes in processBuffer to uptimeBuffer
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
 * Do as if all active processes had just been closed
 *
 * @param processBuffer : buffer of still active processes
 * @param uptimeBuffer : buffer of uptimes of already closed program of the actual boot
 * @param CLK_TCK : number of clock ticks in a second
 * @param parallelTracking : buffer of start/end time of each processes
 */
void mergeProcesses(std::map<int, std::pair<std::string, int>>& processBuffer, std::map<std::string, float>& uptimeBuffer,
                    const int &CLK_TCK, std::map<std::string, std::vector<std::pair<int, int>>>& parallelTracking) {

    float systemUptime = getSystemUptime();
    std::string processName;
    float processUptime;

    for (auto& s : processBuffer) {
        processName = s.second.first;
        float processStartTime = s.second.second;

        if (!config::track_parallel_processes && parallelTracking.contains(processName)) {
            //if i don't want to track parallel running processes, i check if it is one
            for (auto t = parallelTracking[processName].begin(); t != parallelTracking[processName].end(); t++) {
                if (processStartTime >= t->first && processStartTime <= t->second) {
                    //if the processes started when another was running, change the time it started
                    processStartTime = t->second;
                } else if (processStartTime < t->first) {
                    //if the process started before another start (and obviously ended after), remove the included uptime
                    float includedUptime = t->second - t->first;
                    uptimeBuffer[processName] -= includedUptime / CLK_TCK;
                    parallelTracking[processName].erase(t--);
                }
            }
        }

        processUptime = systemUptime - (processStartTime / CLK_TCK) - (config::precision / 2); //to average
        if (processUptime < 0) // averaging a very short uptime may cause a negative uptime
            processUptime = 0;
        uptimeBuffer[processName] += processUptime;
        if (!config::track_parallel_processes)
            parallelTracking[processName].push_back(std::make_pair(processStartTime, systemUptime*CLK_TCK)); //we store in clock ticks
    }

}

/**
 * Save the buffers in the data file
 *
 * Add uptimes of current boot to those in the database
 * Rewrite the whole database file with the new values
 *
 * @param processBuffer : buffer of still active processes
 * @param uptimeBuffer : buffer of uptimes of already closed program of the actual boot
 * @param CLK_TCK : number of clock ticks in a second
 * @param gSignalStatus : signal received by the program
 * @param parallelTracking : buffer of start/end time of each processes
 */
void save(std::map<int, std::pair<std::string, int>>& processBuffer, std::map<std::string, float>& uptimeBuffer,
          const int &CLK_TCK, volatile sig_atomic_t& gSignalStatus, std::map<std::string, std::vector<std::pair<int, int>>>& parallelTracking) {

    mergeProcesses(processBuffer, uptimeBuffer, CLK_TCK, parallelTracking);

    std::string processName;
    std::string uptimeDataFile = DATA_DIR + "uptime";
    std::ifstream uptimeDataFileR (uptimeDataFile);

    // Create the file if it was deleted
    if (!uptimeDataFileR.is_open()) {
        std::ofstream openingDataFile (uptimeDataFile);
        openingDataFile.close();
        uptimeDataFileR.open(uptimeDataFile);
        if (!uptimeDataFileR.is_open()) {
            std::string errmsg = "File not found or permission denied : " + uptimeDataFile;
            error(errmsg.c_str(), FATAL);
        }
    }

    // Uptimes are stored this way : 'process name: uptime \n process name: uptime ...'
    float previousUptime;
    while (uptimeDataFileR >> processName) {
        std::string buf;
        uptimeDataFileR >> buf;
        while (!isFloat(buf)) {
            processName += " " + buf;
            uptimeDataFileR >> buf;
        }
        previousUptime = std::stof(buf);
        processName.pop_back(); // removing the colon
        uptimeBuffer[processName] += previousUptime;
    }
    uptimeDataFileR.close();
    std::ofstream uptimeDataFileW (uptimeDataFile, std::ios::out | std::ios::trunc);

    for (auto& s : uptimeBuffer) {
        uptimeDataFileW << s.first << ": " << std::fixed << s.second << std::defaultfloat << "\n";
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
 * @param parallelTracking : buffer of start/end time of each processes
 */
void timeTracking(std::map<std::string, float>& uptimeBuffer, std::map<int, std::pair<std::string, int>>& processBuffer,
                  volatile sig_atomic_t& gSignalStatus, std::map<std::string, std::vector<std::pair<int, int>>>& parallelTracking) {

    const int CLK_TCK = sysconf(_SC_CLK_TCK);

    processBuffer = initProcessBuffer();
    uptimeBuffer = initUptimeBuffer(processBuffer);
    std::vector <int> pidList = getNewPidList(); //initiate the pidList

    while (true) {
        if (gSignalStatus == SIGTERM) { // if sigterm received, save and exit
            save(processBuffer, uptimeBuffer, CLK_TCK, gSignalStatus, parallelTracking);
            return;
        }

        std::vector<int> newPidList = getNewPidList();
        while (newPidList == pidList) { //check every TT_PRECISION seconds if there was a change in the list of processes
            sleep(config::precision);
            newPidList = getNewPidList();
        }


        int offset = 0;
        for (auto i = 0; i + offset < pidList.size(); i++) {
            while ((i >= newPidList.size() - 1 || pidList[i + offset] != newPidList[i]) && i + offset < pidList.size()) {
                if (processBuffer.contains(pidList[i+offset])) {
                    //the process has ended (i.e. the process is in pidList but not in newPidList)
                    std::string processName = processBuffer.find(pidList[i + offset])->second.first;
                    float processStartTime = processBuffer.find(pidList[i + offset])->second.second;
                    float systemUptime = getSystemUptime();

                    if (!config::track_parallel_processes && parallelTracking.contains(processName)) {
                        //if i don't want to track parallel running processes, i check if it is one
                        for (auto s = parallelTracking[processName].begin(); s != parallelTracking[processName].end(); s++) {
                            if (processStartTime >= s->first && processStartTime <= s->second) {
                                //if the processes started when another was running, change the time it started
                                processStartTime = s->second;
                            } else if (processStartTime < s->first) {
                                //if the process started before another start (and obviously ended after), remove the included uptime
                                float includedUptime = s->second - s->first;
                                uptimeBuffer[processName] -= includedUptime / CLK_TCK;
                                parallelTracking[processName].erase(s--);
                            }
                        }
                    }

                    //then add the uptime to uptimeBuffer
                    float processUptime = systemUptime - (processStartTime / CLK_TCK);
                    processBuffer.erase(processBuffer.find(pidList[i + offset])); // delete the process that just finished
                    uptimeBuffer[processName] += processUptime; // add its uptime
                    if (!config::track_parallel_processes)
                        parallelTracking[processName].push_back(std::make_pair(processStartTime, systemUptime * CLK_TCK)); //we store in clock ticks
                }
                offset++;

            }
        }
        updateProcessBuffer(processBuffer, pidList, newPidList, offset);

        pidList = newPidList;
    }
}
