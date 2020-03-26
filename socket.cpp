#include <map>
#include <string>
#include <unistd.h>
#include <filesystem>
#include <sys/socket.h>
#include <sys/un.h>
#include <csignal>
#include <iostream>
#include <fstream>

#include "socket.h"
#include "yotta_daemon.h"


void createTmpPath (const std::string& socketPath) {
    std::string tmpPath = socketPath.substr(0, 10);
    if(!std::filesystem::is_directory(tmpPath));
        std::filesystem::create_directory(tmpPath);
}

void ySocket (std::map <std::string, float>& uptimeBuffer, std::map<int, std::pair<std::string, int>>& processBuffer,
              volatile sig_atomic_t& gSignalStatus) {

    const char* const socket_path = "/run/yotta/yotta_socket";
    createTmpPath(socket_path);

    mask_sig();
    int sockfd, newsockfd, servlen;
    socklen_t clilen;
    struct sockaddr_un cli_addr{}, serv_addr{};
    char buf[80];

    if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
        perror("error creating the socket");
    int enable = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
        perror("setsockopt(SO_REUSEADDR) failed");

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sun_family = AF_UNIX;
    strcpy(serv_addr.sun_path, socket_path);
    servlen = strlen(serv_addr.sun_path) + sizeof(serv_addr.sun_family);

    if (std::filesystem::is_socket(socket_path))
        unlink(socket_path);

    if (bind(sockfd, (struct sockaddr *) &serv_addr, servlen) < 0)
        perror("error binding socket");

    listen(sockfd, 5);
    clilen = sizeof(cli_addr);

    while (true) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(sockfd, &read_fds);

        struct timeval timeout{};
        timeout.tv_sec = 1;  // 1s timeout
        timeout.tv_usec = 0;

        int select_status;
        while (true) {
            select_status = select(sockfd+1, &read_fds, nullptr, nullptr, &timeout);
            if (select_status == -1) {
                perror("error selecting");
            } else if (select_status > 0) {
                break;  // we have data, we can accept now
            }
            // otherwise check signals
            if (gSignalStatus == SIGTERM) {
                close(sockfd);
                unlink(socket_path);
                return;
            }
        }
        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);

        if (newsockfd < 0)
            perror("error accepting");

        bzero(buf, 80);
        read(newsockfd, buf, 80);

        if(strcmp(buf, "uptimeBuffer\0") == 0) {
            std::map <std::string, float> uptimeBufBuf = uptimeBuffer; //to prevent the buffer to change while in communication with client
            std::string bufferSize = std::to_string(uptimeBufBuf.size());
            write(newsockfd, bufferSize.c_str(), bufferSize.size());
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
            write(newsockfd, bufferSize.c_str(), bufferSize.size());
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