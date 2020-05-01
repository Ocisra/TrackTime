#include <csignal>
#include <fstream>
#include <iomanip>
#include <iostream>

#include "log.h"

const char* logName[] = {"FATAL", "ERROR", "WARN", "INFO", "DEBUG", "TRACE"};

/**
 * Log the error and eventually kill the program
 *
 * @param msg : the message to write
 * @param isFatal : whether or not the error is fatal
 */
void error (const char* msg, unsigned int level) {
    std::string logmsg = logName[level-1]; // because table start at 0
    std::time_t t = std::time(nullptr);
    std::cerr << logmsg << ' ' << std::put_time(gmtime(&t), "%F %T") << ' ' << msg << '\n';

    if (level == FATAL)
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