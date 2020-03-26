#ifndef YOTTA_TIMETRACKING_H
#define YOTTA_TIMETRACKING_H

#include <csignal>
#include <map>
#include <string>
#include <vector>

float getSystemUptime ();
bool containsNumber(std::string& str);
bool isFloat (std::string& str);
std::map <int, std::pair<std::string, int>> initProcessBuffer ();
void updateProcessBuffer(std::map<int, std::pair<std::string, int>>& processBuffer, std::vector<int>& pidList,
                         std::vector<int>& newPidList, int& offset);
std::map<std::string, float> initUptimeBuffer (std::map<int, std::pair<std::string, int>>& processBuffer);
std::vector <int> getNewPidList ();
void save(std::map<int, std::pair<std::string, int>> &processBuffer, std::map<std::string, float> &uptimeBuffer,
          const int &CLK_TCK, volatile sig_atomic_t& gSignalStatus);
void timeTracking(std::map<std::string, float> &uptimeBuffer, std::map<int, std::pair<std::string, int>> &processBuffer,
                  volatile sig_atomic_t& gSignalStatus);

#endif //YOTTA_TIMETRACKING_H
