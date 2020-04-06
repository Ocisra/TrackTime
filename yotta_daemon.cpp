#include <csignal>
#include <filesystem>
#include <fstream>
#include <map>
#include <thread>

#include "socket.hpp"
#include "timeTracking.hpp"


/// Path to temporary directory
const char* const TMP_DIR = "/run/yotta";

/// Dircetory where datas are stored while yotta is not running
const char* const DATA_DIR = "/var/lib/yotta";

/// Path to the log file
const char* const LOG_FILE = "/var/log/yotta.log";

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

/**
 * Main
 *
 * @return
 */
int main () {
    std::freopen(LOG_FILE, "a", stderr); // redirect stderr to the log file

    createNecessaryFiles();

    std::signal(SIGTERM, signalHandler);
    //todo sigint

    std::map<std::string, float> uptimeBuffer;
    std::map<int, std::pair<std::string, int>> processBuffer;

    std::thread thSocket(ySocket, std::ref(uptimeBuffer), std::ref(processBuffer), std::ref(gSignalStatus));

    timeTracking(uptimeBuffer, processBuffer, gSignalStatus);

    thSocket.join();

    std::fclose(stderr); // end the redirection of stderr

    return 0;
}
