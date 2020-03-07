#include <map>
#include <string>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <csignal>
#include <iostream>

#include "socket.h"



void ySocket (std::map <std::string, float>& uptimeBuffer, std::map<int, std::pair<std::string, int>>& processBuffer,
              volatile sig_atomic_t& gSignalStatus) {

    int sockfd, newsockfd, servlen, n;
    socklen_t clilen;
    struct sockaddr_un cli_addr{}, serv_addr{};
    char buf[80];
    if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
        perror("error creating the socket");
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sun_family = AF_UNIX;
    strcpy(serv_addr.sun_path, "5687");
    servlen = strlen(serv_addr.sun_path) + sizeof(serv_addr.sun_family);
    if (bind(sockfd, (struct sockaddr *) &serv_addr, servlen) < 0)
        perror("error binding socket");
    listen(sockfd, 5);
    clilen = sizeof(cli_addr);

    while (gSignalStatus != SIGTERM ) {
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
    close(sockfd);
}