#ifndef YOTTA_TIMETRACKING_H
#define YOTTA_TIMETRACKING_H

#include <map>
#include <string>
#include <vector>

void signalHandler (int signum);
float getSystemUptime ();
bool containsNumber(std::string& str);
bool isNumber (std::string& str);
std::map <int, std::pair<std::string, int>> initProcessBuffer ();
void updateProcessBuffer(std::map<int, std::pair<std::string, int>>& processBuffer, std::vector<int>& pidList, std::vector<int>& newPidList, int& offset);
std::map<std::string, float> initUptimeBuffer (std::map<int, std::pair<std::string, int>>& processBuffer);
std::vector <int> getNewPidList ();
void save (std::map <int, std::pair<std::string, int>>& processBuffer, std::map <std::string, float>& uptimeBuffer, const int& CLK_TCK);
void timeTracking(std::map <std::string, float>& uptimeBuffer);

#endif //YOTTA_TIMETRACKING_H
