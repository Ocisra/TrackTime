#ifndef YOTTA_SOCKET_H
#define YOTTA_SOCKET_H

void ySocket (std::map <std::string, float>& uptimeBuffer, std::map<int, std::pair<std::string, int>>& processBuffer,
              volatile sig_atomic_t& gSignalStatus);

#endif //YOTTA_SOCKET_H
