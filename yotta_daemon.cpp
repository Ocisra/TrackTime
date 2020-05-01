#include <csignal>
#include <filesystem>
#include <fstream>
#include <map>
#include <thread>
#include <iostream>

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

/// Paths to possible config file
const char* const CONFIG_FILE[4] = {"/etc/yotta", "/etc/yotta.conf", "/etc/yotta/yotta", "/etc/yotta/yotta.conf"};

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


void loadConfig () {
    std::ifstream configFile;
    int i = 0;
    while (!configFile.is_open() && i != 4) {
        configFile.open(CONFIG_FILE[i]);
        ++i;
    }

    if (!configFile.is_open() || configFile.peek() == std::ifstream::traits_type::eof())
        return;

    std::string line;
    while (getline(configFile, line)) {
        std::string optionName = line.substr(0, line.find_last_of(':')); // because string index start at 0, +1-1=0
        trim(optionName);
        std::string value = line.substr(line.find_last_of(':') + 1,line.length() - line.find_last_of(':'));
        trim(value);

        if (optionName == "precision") {
            if (isFloat(value))
                config::precision = std::stof(value);
        } else if (optionName == "track_parallel_processes") {
            if (value == "true" || value == "True" || value == "t" || value == "T")
                config::track_parallel_processes = true;
            else if (value == "false" || value == "False" || value == "f" || value == "F")
                config::track_parallel_processes = false;
        }
    }
}

/**
 * Main
 *
 * @return
 */
int main () {
    std::freopen(LOG_FILE, "a", stderr); // redirect stderr to the log file

    std::signal(SIGTERM, signalHandler);

    createNecessaryFiles();

    loadConfig();

    std::map<std::string, float> uptimeBuffer;
    std::map<int, std::pair<std::string, int>> processBuffer;
    std::map<std::string, std::vector<std::pair<int, int>>> parallelTracking;

    std::thread thSocket(ySocket, std::ref(uptimeBuffer), std::ref(processBuffer), std::ref(gSignalStatus), std::ref(parallelTracking));

    timeTracking(uptimeBuffer, processBuffer, gSignalStatus, parallelTracking);

    thSocket.join();

    std::fclose(stderr); // end the redirection of stderr

    return 0;
}
