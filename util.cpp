#include <csignal>
#include <iomanip>
#include <iostream>


/**
 * Log the error and eventually kill the program
 *
 * @param msg : the message to write
 * @param isFatal : whether or not the error is fatal
 */
void error (const char* msg, bool isFatal) {
    std::string fatalmsg;
    if (isFatal)
        fatalmsg = "FATAL";
    else
        fatalmsg = "ERROR";
    std::time_t t = std::time(nullptr);
    std::cerr << fatalmsg << ' ' << std::put_time(gmtime(&t), "%F %T") << ' ' << msg << '\n';

    if (isFatal)
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