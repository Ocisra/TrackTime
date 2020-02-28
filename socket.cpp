#include <map>
#include <string>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <csignal>

#include "socket.h"


/**
 * Declare gSignalStatus
 */
namespace {
    volatile std::sig_atomic_t gSignalStatus;
}

/**
 * Handle the signal sent to the program
 *
 * As it is impossible to pass customs arguments to a signal handler,
 * I can almost only change the value of a volatile std::sig_atomic_t variable
 *
 * @param signum : number corresponding to the signal received
 *                 SIGTERM = 15
 */
void signalHandler2 (int signum) {
    gSignalStatus = signum;
}

void ySocket (std::map <std::string, float>& uptimeBuffer) {
    std::signal(SIGTERM, signalHandler2);

    int sockfd, newsockfd, servlen, n;
    socklen_t clilen;
    struct sockaddr_un cli_addr, serv_addr;
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

        n = read(newsockfd, buf, 80);

        printf("A connection has been established\n");

        write(1, buf, n);

        write(newsockfd, "I got your message\n", 19);

        close(newsockfd);
    }
    close(sockfd);
}