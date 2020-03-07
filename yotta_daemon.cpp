#include <map>
#include <thread>
#include <sys/types.h>
#include <unistd.h>
#include <cstdlib>
#include <sys/socket.h>
#include <sys/un.h>
#include <cstdio>
#include <iostream>
#include <csignal>

#include "timeTracking.h"
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
void signalHandler (int signum) {
    gSignalStatus = signum;
}




int main () {
    std::signal(SIGTERM, signalHandler);

    std::map<std::string, float> uptimeBuffer;
    std::map<int, std::pair<std::string, int>> processBuffer;

    std::thread thSocket(ySocket, std::ref(uptimeBuffer), std::ref(processBuffer), std::ref(gSignalStatus));

    timeTracking(uptimeBuffer, processBuffer, gSignalStatus);

    return 0;
}

//todo error handler
//todo send map through socket
