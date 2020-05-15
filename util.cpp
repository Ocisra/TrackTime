#include <csignal>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>

#include "config.hpp"
#include "log.h"

const char* logName[] = {"FATAL", "ERROR", "WARN", "INFO", "DEBUG", "TRACE"};

/// Paths to possible config file
const char* const CONFIG_FILE[4] = {"/etc/yotta", "/etc/yotta.conf", "/etc/yotta/config", "/etc/yotta/yotta.conf"};

/// Dircetory where datas are stored while yotta is not running
const std::string DATA_DIR = "/var/lib/yotta/";



/**
 * Log the error and eventually kill the program
 *
 * @param msg : the message to write
 * @param isFatal : whether or not the error is fatal
 */
void error (const char* msg, unsigned int level) {
    std::string loglvlmsg = logName[level-1]; // because table start at 0
    std::time_t t = std::time(nullptr);
    std::cerr << loglvlmsg << ' ' << std::put_time(gmtime(&t), "%F %T") << ' ' << msg << '\n';

    if (level == FATAL) //todo raise sigkill after some time if it does not exit
        std::raise(SIGTERM);
}

/**
 * Prevent a thread to receive signal
 *
 * I can choose which thread will receive the signals
 */
void mask_sig()
{
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGRTMIN+3);

    pthread_sigmask(SIG_BLOCK, &mask, nullptr);

}

/**
 * Check if the string is a floating point number
 *
 * Check if each character is either a number or a dot
 * Count the number of dots and limit it to 1
 *
 * @param str : the string to test
 * @return true  : if the string is a float
 *         false : if the string is not a float
 */
bool isFloat (std::string& str) {
    bool containsOnlyNumber = true;
    int dotCount = 0;
    for (auto& s : str) {
        if (!isdigit(s) && (s != '.' || dotCount > 0)) {
            containsOnlyNumber = false;
            break;
        }
        if (s == '.')
            dotCount += 1;
    }
    return containsOnlyNumber;
}


void trim(std::string& s) {
    int start = 0;
    int end = s.length() - 1;

    while (start != end && std::isspace(s[start])) {
        start++;
    }
    s.erase(0, start);

    end = s.length() - 1;
    while (end != 0 && std::isspace(s[end])) {
        end--;
    }
    s.erase(end + 1, s.length() - 1);
}

/**
 * Get the time the system was up since last boot
 *
 * System uptime is the first word stored in '/proc/uptime'
 *
 * @return system uptime in seconds
 */
float getSystemUptime () {
    float systemUptime;
    std::ifstream uptime("/proc/uptime");
    if(!uptime.is_open())
        error("File not found or permission denied : /proc/uptime", FATAL);
    uptime >> systemUptime;
    return systemUptime;
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
    configFile.close();
}

void reloadConfig () {
    //reset to default
    config::precision = 2;
    config::track_parallel_processes = true;
    //load
    loadConfig();
}

/**
 * Save the buffers in the data file
 *
 * Add uptimes of current boot to those in the database
 * Rewrite the whole database file with the new values
 *
 * @param processBuffer : buffer of still active processes
 * @param uptimeBuffer : buffer of uptimes of already closed program of the actual boot
 * @param CLK_TCK : number of clock ticks in a second
 * @param parallelTracking : buffer of start/end time of each processes
 */
void saveData(std::map<std::string, float>& uptimeBuffer) {
    std::string processName;
    std::string uptimeDataFile = DATA_DIR + "uptime";
    std::ifstream uptimeDataFileR (uptimeDataFile);

    // Create the file if it was deleted
    if (!uptimeDataFileR.is_open()) {
        std::ofstream openingDataFile (uptimeDataFile);
        openingDataFile.close();
        uptimeDataFileR.open(uptimeDataFile);
        if (!uptimeDataFileR.is_open()) {
            std::string errmsg = "File not found or permission denied : " + uptimeDataFile;
            error(errmsg.c_str(), FATAL);
        }
    }

    // Uptimes are stored this way : 'process name: uptime \n process name: uptime ...'
    float previousUptime;
    while (uptimeDataFileR >> processName) {
        std::string buf;
        uptimeDataFileR >> buf;
        while (!isFloat(buf)) {
            processName += " " + buf;
            uptimeDataFileR >> buf;
        }
        previousUptime = std::stof(buf);
        processName.pop_back(); // removing the colon
        uptimeBuffer[processName] += previousUptime;
    }
    uptimeDataFileR.close();
    std::ofstream uptimeDataFileW (uptimeDataFile, std::ios::out | std::ios::trunc);

    for (auto& s : uptimeBuffer) {
        uptimeDataFileW << s.first << ": " << std::fixed << s.second << std::defaultfloat << "\n";
    }
    uptimeBuffer.clear();
    uptimeDataFileW.close();
}