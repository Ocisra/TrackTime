#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <unistd.h>
#include <utility>
#include <vector>


float getSystemUptime () {
    float systemUptime;
    std::ifstream uptime("/proc/uptime");
    if(!uptime.is_open())
        throw std::ios_base::failure("File not found");
    uptime >> systemUptime;
    return systemUptime;
}

bool containsNumber(std::string& str){
    return (str.find_first_of("0123456789") != std::string::npos);
}

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

std::vector <int> initPidList(std::map<int, std::pair<std::string, int>>& processBuffer){
    std::vector <int> pidList;
    for (auto& s : processBuffer) {
        pidList.push_back(s.first);
    }
    return pidList;
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

/**
 * TODO
 * @param processBuffer
 * @param pidList
 * @param newPidList
 * @param offset
 */
void updateProcessBuffer(std::map<int, std::pair<std::string, int>>& processBuffer, std::vector<int>& pidList, std::vector<int>& newPidList, int& offset) {
    if (pidList.size() + offset < newPidList.size()) {
        std::string path;
        int wordCount;
        char c;
        int processStartTime;
        for (auto i = pidList.size()+offset; i < newPidList.size() - 1; i++) {
            path = &"/proc/" [ newPidList[i]];
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
            processBuffer.insert({newPidList[i], std::make_pair(processName, processStartTime)});
        }
    }
}

int main() {
    std::map<int, std::pair<std::string, int>> processBuffer = initProcessBuffer();
    for (auto& s : processBuffer) {
        std::cout << s.first << " " << s.second.first << " " << s.second.second << std::endl;
    }

    std::vector <int> pidList = initPidList(processBuffer);
    for (auto& s : pidList) {
        std::cout << s << std::endl;
    }

    while (true) {
        std::vector<int> newPidList = getNewPidList();
        while (newPidList == pidList) {
            newPidList = getNewPidList();
            sleep(2);
            std::cout << "actualised" << std::endl;
        }

        int offset = 0;
        for (auto i = 0; i+offset < pidList.size() - 1; i++) {
            while (pidList[i+offset] != newPidList[i] && i <= pidList.size() - 1) {
                std::cout << pidList[i+offset] << " not found\n";
                std::cout << processBuffer.find(pidList[i+offset])->second.first << " " << processBuffer.find(pidList[i+offset])->second.second << std::endl;
                processBuffer.erase(processBuffer.find(pidList[i+offset]));
                offset++;
            }
        }
        updateProcessBuffer(processBuffer, pidList, newPidList, offset);

        pidList = newPidList;
        std::cout << "a new one is out there\n";
    }
    return 0;


    std::cout << "Hello, World!" << std::endl;
    int pid; //proc qu'on etudie
    int wordCount; //compteur des mots dans le fichier
    char c; //caractere sur lequel est le curseur
    int nextPid = 1000; //pid de depart
    int pStartTime; //jiffies auquel le proc a demarre
    float processUptime; //uptime du proc (en s)
    const int CLK_TCK = sysconf(_SC_CLK_TCK); //nb de jiffies par seconde
    std::cout << "Clock tick per second : " << CLK_TCK << std::endl;
    float systemUptime = getSystemUptime(); // system uptime (en s)
    std::cout << "Current system uptime : " << systemUptime;
    while (nextPid < 20000) { // pid auquel j'arrete d'en chercher de nouveaux
        pid = nextPid;
        //std::cout << pid;
        std::ifstream pidStat("/proc/" + std::to_string(pid) + "/stat"); // ouvrir /proc/PID/stat
        if (pidStat.is_open()) {
            //std::cout << " opened" << std::endl;
            wordCount = 1;
            while (wordCount != 22) { // chercher le 22e mot = pStartTime
                pidStat.get(c);
                if (c == ' ')
                    wordCount += 1;
                if(wordCount == 2) // 2e mot = nom du proc
                    std::cout << c;
            }
            pidStat >> pStartTime; // on a trouve pStartTime
            //std::cout << pStartTime << std::endl;
            processUptime = (systemUptime - (pStartTime / CLK_TCK)) / 60; // calcul de l'uptime (en s)
            std::cout << processUptime << std::endl;
        }
        nextPid += 1; // pid suivant
    }
    return 0;
}



/// TODO
/// uniformiser le nommage
/// separer en plusieurs fichiers
/// doc propre
/// chercher le dernier pid
/// trouver quand un proc est cree/suppr