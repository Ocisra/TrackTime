#include <csignal>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <thread>
#include <unistd.h>

#include "config.hpp"
#include "socket.hpp"
#include "timeTracking.hpp"
#include "util.hpp"


/// Path to temporary directory
const char* const TMP_DIR = "/run/yotta";

/// Dircetory where datas are stored while yotta is not running
const char* const DATA_DIR = "/var/lib/yotta";

/// Path to the log file
const char* const LOG_FILE = "/var/log/yotta.log";

/// Value of the signal received
volatile std::sig_atomic_t gSignalStatus = 0;


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

/**
 * Create the necessary files and directories
 */
void createNecessaryFiles () {
    if (!std::filesystem::is_directory(TMP_DIR))
        std::filesystem::create_directory(TMP_DIR);

    if (!std::filesystem::is_directory(DATA_DIR))
        std::filesystem::create_directory(DATA_DIR);

    std::ifstream logFile (LOG_FILE);
    if (!logFile.is_open()) {
        std::ofstream openingDataFile (LOG_FILE);
        openingDataFile.close();
    }
}

void testt () {
    while (true) {
        std::cout << config::track_parallel_processes << " " << config::precision << std::endl;
        sleep(1);
    }
}

/**
 * Main
 *
 * @return
 */
int main () {
    std::freopen(LOG_FILE, "a", stderr); // redirect stderr to the log file

    std::signal(SIGTERM, signalHandler); // termination request by systemd
    std::signal(SIGUSR1, signalHandler); // save request by the root user
    std::signal(SIGUSR2, signalHandler); // reload request by the root user

    createNecessaryFiles();

    loadConfig();

    std::map<std::string, float> uptimeBuffer;
    std::map<int, std::pair<std::string, int>> processBuffer;
    std::map<std::string, std::vector<std::pair<int, int>>> parallelTracking;

    std::thread test (testt);

    std::thread thSocket(ySocket, std::ref(uptimeBuffer), std::ref(processBuffer), std::ref(gSignalStatus), std::ref(parallelTracking));

    test.join();
    timeTracking(uptimeBuffer, processBuffer, gSignalStatus, parallelTracking);

    thSocket.join();

    std::fclose(stderr); // end the redirection of stderr

    return 0;
}
