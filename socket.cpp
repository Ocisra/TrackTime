#include <csignal>
#include <filesystem>
#include <map>
#include <string>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <iostream>
#include <lcms2.h>

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
              volatile sig_atomic_t& gSignalStatus) {
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


        if(strcmp(buf, "uptimeBuffer\0") == 0) {
            std::map <std::string, float> uptimeBufBuf = uptimeBuffer; //to prevent the buffer to change while in communication with client
            std::string bufferSize = std::to_string(uptimeBufBuf.size());
            write(newsockfd, bufferSize.c_str(), bufferSize.size()); // send the number of lines that will be sent
            read(newsockfd, buf, 1);

            for (auto &s : uptimeBufBuf) {
                buf[0] = '\0';
                std::string toSend = s.first + '\1' + std::to_string(s.second) + "\n";
                write(newsockfd, toSend.c_str(), toSend.length());
                read(newsockfd, buf, 1); //to receive the "ok, received"
            }
        } else if (strcmp(buf, "processBuffer\0") == 0) {
            std::map<int, std::pair<std::string, int>> processBufBuf = processBuffer; //to prevent the buffer to change while in communication with client
            std::string bufferSize = std::to_string(processBufBuf.size());
            write(newsockfd, bufferSize.c_str(), bufferSize.size()); // send the number of lines that will be sent
            read(newsockfd, buf, 1);

            for (auto& s: processBufBuf) {
                buf[0] = '\0';
                std::string toSend = std::to_string(s.first) + '\1' + s.second.first + '\2' + std::to_string(s.second.second) + "\n";
                write(newsockfd, toSend.c_str(), toSend.length());
                read(newsockfd, buf, 1); //to receive the "ok, received"
            }
        }
        close(newsockfd);
    }
}