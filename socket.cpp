#include <csignal>
#include <filesystem>
#include <map>
#include <string>
#include <unistd.h>
#include <vector>

#include <sys/socket.h>
#include <sys/un.h>
#include <iostream>
#include <lcms2.h>

#include "config.hpp"
#include "log.h"
#include "util.hpp"

/// Path where the socket file is located
const char* const SOCKET_PATH = "/run/yotta/yotta_socket";

/**
 * Main of the socket thread
 *
 * Create the socket
 * Wait for a connection to be established
 * Accept and send requested datas
 *
 * @param uptimeBuffer : buffer of already finished processes
 * @param processBuffer : buffer of actually running processes
 * @param gSignalStatus : signal received
 */
void ySocket (std::map <std::string, float>& uptimeBuffer, std::map<int, std::pair<std::string, int>>& processBuffer,
              volatile sig_atomic_t& gSignalStatus, std::map<std::string, std::vector<std::pair<int, int>>>& parallelTracking) {
    mask_sig();
    int sockfd, newsockfd, servlen;
    socklen_t clilen;
    struct sockaddr_un cli_addr{}, serv_addr{};
    char buf[80];

    if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
        error("Creating the socket", FATAL);

    int enable = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
        error("setsockopt(SO_REUSEADDR)", FATAL);

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sun_family = AF_UNIX;
    strcpy(serv_addr.sun_path, SOCKET_PATH);
    servlen = strlen(serv_addr.sun_path) + sizeof(serv_addr.sun_family);

    if (std::filesystem::is_socket(SOCKET_PATH))
        unlink(SOCKET_PATH);

    if (bind(sockfd, (struct sockaddr *) &serv_addr, servlen) < 0)
        error("Binding the socket", FATAL);

    listen(sockfd, 5);
    clilen = sizeof(cli_addr);

    //change the permission of the socket so everyone can run yotta_cli
    std::filesystem::permissions(SOCKET_PATH, std::filesystem::perms::owner_all | std::filesystem::perms::group_all |
                                  std::filesystem::perms::others_all, std::filesystem::perm_options::add);

    while (true) {
        while (true) {
            int select_status;

            fd_set read_fds;
            FD_ZERO(&read_fds);
            FD_SET(sockfd, &read_fds);

            struct timeval timeout{};
            timeout.tv_sec = 1;  // 1s timeout
            timeout.tv_usec = 0;

            select_status = select(sockfd+1, &read_fds, nullptr, nullptr, &timeout); //check if there is a client
            if (select_status == -1) { //error
                error("Selecting the socket connection", ERROR);
            } else if (select_status > 0) { //at least one client
                break;  // we have data, we can accept now
            }
            // otherwise check signals
            if (gSignalStatus == SIGTERM) {
                close(sockfd);
                unlink(SOCKET_PATH);
                return;
            }
        }
        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);

        if (newsockfd < 0) {
                error("Accepting the connection", ERROR);
        }


        bzero(buf, 80);
        read(newsockfd, buf, 80);


        int CLK_TCK = sysconf(_SC_CLK_TCK);
        float systemUptime = getSystemUptime();
        std::string processName;
        float processUptime;

        // Store the data in buffers to prevent them from changing
        // and to be able to modify them without any repercussions on the time tracking part
        std::map <int, std::pair<std::string, int>> processBufBuf = processBuffer;
        std::map <std::string, float> uptimeBufBuf = uptimeBuffer;
        std::map <std::string, std::vector<std::pair<int, int>>> parallelTrackingBuf = parallelTracking;

        for (auto& s : processBufBuf) {
            processName = s.second.first;
            float processStartTime = s.second.second;

            if (!config::track_parallel_processes && parallelTrackingBuf.contains(processName)) {
                //if i don't want to track parallel running processes, i check if it is one
                for (auto t = parallelTrackingBuf[processName].begin(); t != parallelTrackingBuf[processName].end(); ++t) {
                    if (processStartTime >= t->first && processStartTime <= t->second) {
                        //if the processes started when another was running, change the time it started
                        processStartTime = t->second;
                    } else if (processStartTime < t->first) {
                        //if the process started before another start (and obviously ended after), remove the included uptime
                        float includedUptime = t->second - t->first;
                        uptimeBufBuf[processName] -= includedUptime / CLK_TCK;
                        parallelTrackingBuf[processName].erase(t--); //decrement the iterator because we erase an element
                                                                     //it is decremented after having been passed to erase
                    }
                }
            }

            processUptime = systemUptime - (processStartTime / CLK_TCK) - (config::precision / 2); //to average
            if (processUptime < 0) // averaging a very short uptime may cause a negative uptime
                processUptime = 0;
            uptimeBufBuf[processName] += processUptime;
            if (!config::track_parallel_processes)
                parallelTrackingBuf[processName].push_back(std::make_pair(processStartTime, systemUptime*CLK_TCK)); //we store in clock ticks
        }

        if(strcmp(buf, "uptimeBuffer\0") == 0) {
            std::string bufferSize = std::to_string(uptimeBufBuf.size());
            write(newsockfd, bufferSize.c_str(), bufferSize.size()); // send the number of lines that will be sent
            read(newsockfd, buf, 1);

            for (auto &s : uptimeBufBuf) {
                buf[0] = '\0';
                std::string toSend = s.first + '\1' + std::to_string(s.second) + "\n";
                write(newsockfd, toSend.c_str(), toSend.length());
                read(newsockfd, buf, 1); //to receive the "ok, received"
            }
        }
        close(newsockfd);
    }
}