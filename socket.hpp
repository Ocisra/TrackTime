#ifndef YOTTA_SOCKET_HPP
#define YOTTA_SOCKET_HPP

#include <vector>

void ySocket (std::map <std::string, float>& uptimeBuffer, std::map<int, std::pair<std::string, int>>& processBuffer,
              volatile sig_atomic_t& gSignalStatus, std::map<std::string, std::vector<std::pair<int, int>>>& parallelTracking);

#endif //YOTTA_SOCKET_HPP
