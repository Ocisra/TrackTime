#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <unistd.h>
#include <utility>
#include <vector>

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
 * Initiate the process buffer
 *
 * Look for all directories named '/proc/XXXX', each one represent one process
 * In these directories, read './stat', in brackets is the process name and 22th word is process start time since boot
 *
 * @return process buffer
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
                        while (c != ')') {
                            pidStat.get(c);
                            processName += c;
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
 * TODO
 *
 * TODO
 * TODO
 * TODO
 * 
 * @param processBuffer : the process buffer to update
 * @param pidList : all PIDs present in the process buffer
 * @param newPidList : all PIDs actually running
 * @param offset : TODO
 */
void updateProcessBuffer(std::map<int, std::pair<std::string, int>>& processBuffer, std::vector<int>& pidList, std::vector<int>& newPidList, int& offset) {
    // All of the old processes fits in new processes minus offset. If there are more processes than those in newPL, I have to add them to the buffer
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
                    while (c != ')') {
                        pidStat.get(c);
                        processName += c;
                    }
                    processName.pop_back();
                }
                if (c == ' ')
                    wordCount += 1;
            }
            pidStat >> processStartTime;
            std::cout << processName << " added\n";
            processBuffer.insert({newPidList[i], std::make_pair(processName, processStartTime)});
        }
    }
}

std::map<std::string, float> initTimeBuffer (std::map<int, std::pair<std::string, int>>& processBuffer) {
    std::map <std::string, float> timeBuffer;
    for (auto& s : processBuffer) {
        timeBuffer.insert({s.second.first, 0});
    }
    return timeBuffer;

}

std::vector <int> initPidList (std::map<int, std::pair<std::string, int>>& processBuffer){
    std::vector <int> pidList;
    for (auto& s : processBuffer) {
        pidList.push_back(s.first);
    }
    return pidList;
}

void updateTimeBuffer (std::map<std::string, float>& timeBuffer, std::string& processName, float& processUptime) {
    timeBuffer[processName] += processUptime;
}

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

int main() {
    const int CLK_TCK = sysconf(_SC_CLK_TCK);

    std::map<int, std::pair<std::string, int>> processBuffer = initProcessBuffer();
    std::map <std::string, float> timeBuffer = initTimeBuffer(processBuffer);
    std::vector<int> pidList = initPidList(processBuffer);

    while (true) {
        std::vector<int> newPidList = getNewPidList();
        while (newPidList == pidList) {
            newPidList = getNewPidList();
            sleep(2);
            std::cout << "actualised" << std::endl;
        }

        int offset = 0;
        for (auto i = 0; i + offset < pidList.size(); i++) {
            while ((i >= newPidList.size() - 1 || pidList[i + offset] != newPidList[i]) && i + offset < pidList.size()) {
                std::string processName = processBuffer.find(pidList[i + offset])->second.first;
                int processStartTime = processBuffer.find(pidList[i + offset])->second.second;
                float systemUptime = getSystemUptime();
                float processUptime = systemUptime - (processStartTime / CLK_TCK); // in seconds
                std::cout << pidList[i + offset] << " not found\n";
                std::cout << processName << " " << processStartTime << " " << processUptime << std::endl;
                processBuffer.erase(processBuffer.find(pidList[i + offset]));
                updateTimeBuffer(timeBuffer, processName, processUptime);
                offset++;
            }
        }
        updateProcessBuffer(processBuffer, pidList, newPidList, offset);

        pidList = newPidList;
        std::cout << "a new one is out there\n";
    }
}


/// TODO
/// separer en plusieurs fichiers
/// doc propre