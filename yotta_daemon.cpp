#include <map>
#include <thread>
#include <sys/types.h>
#include <unistd.h>
#include <cstdlib>
#include <sys/socket.h>
#include <sys/un.h>
#include <cstdio>
#include <iostream>

#include "timeTracking.h"
#include "socket.h"




void error(const char *msg)
{
    perror(msg);
    exit(0);
}


int main () {
    std::map<std::string, float> uptimeBuffer;

    std::thread mythread(ySocket, std::ref(uptimeBuffer));

/*
 ********************
 * THIS TEST SOCKET *
 ********************
    void error(const char *);

    int sockfd, servlen, n;
    struct sockaddr_un serv_addr;
    char buffer[82];

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sun_family = AF_UNIX;
    strcpy(serv_addr.sun_path, "5687");
    servlen = strlen(serv_addr.sun_path) + sizeof(serv_addr.sun_family);
    if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
        error("Creating socket");
    sleep(5);
    if (connect(sockfd, (struct sockaddr *) &serv_addr, servlen) < 0)
        error("Connecting");
    printf("Please enter your message: ");
    bzero(buffer, 82);
    fgets(buffer, 80, stdin);

    write(sockfd, buffer, strlen(buffer));

    n = read(sockfd, buffer, 80);

    printf("The return message was\n");
    std::cout << buffer;

    write(1, buffer, n);

    mythread.join();

    close(sockfd);
*/
    timeTracking(uptimeBuffer);

    return 0;
}

//todo error handler
//todo send map through socket
