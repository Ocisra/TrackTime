#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <vector>

#include <pwd.h>
#include <signal.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/un.h>

#include "log.h"
#include "util.hpp"
#include "config.hpp"

/// Dircetory where datas are stored while yotta is not running
const std::string DATA_DIR = "/var/lib/yotta/";

/// Path where the socket file is located
const char* const SOCKET_PATH = "/run/yotta/yotta_socket";

/// Version
const std::string VERSION = "0.2.3\n";

/// Message displayed when -h, --help option is provided
const std::string HELP_MSG = "Usage: yotta [options] [<process> ...]\n\n"
                             "Show uptimes of processes collected by the yotta-daemon systemd service\n\n"
                             "Options:\n"
                             "  -v, --version\t\t\t\tDisplay the version and exit\n"
                             "  -h, --help\t\t\t\tDipslay this help and exit\n"
                             "  -b, --boot\t\t\t\tDisplay only the informations since the last boot\n"
                             "  -B, --all-but-boot\t\t\tDisplay all the informations except those of the last boot\n"
                             "  -d, --day\t\t\t\tDisplay the uptime in days\n"
                             "  -H, --hour\t\t\t\tDisplay the uptime in hours\n"
                             "  -m, --minute\t\t\t\tDisplay the uptime in minutes\n"
                             "  -s, --second\t\t\t\tDisplay the uptime in seconds\n"
                             "  -j, --clock-tick\t\t\tDisplay the uptime in clock ticks\n"
                             "  -f, --default-time-format\t\tDisplay the uptime in default format\n"
                             "  -g, --greater-uptime-than <time>\tDisplay only the processes with a greater uptime than <time>\n"
                             "  -l, --lower-uptime-than <time>\tDisplay only the processes with a lower uptime than <time>\n"
                             "\n"
                             "Root only:\n"
                             "  -r, --reload                        Reload the config file\n"
                             "                                      Changing the config can lead to inaccuracies, use carefully\n"
                             "      --save                          Save the processes that have already finished\n"
                             "                                      Yotta will now consider them as part of previous boot\n"
                             "  -k, --kill                          Force kill the daemon\n"
                             "                                      This will cause all data of this boot to be lost, try to --save before\n";


float isProcessRoot () {
    struct passwd *pws;
    pws = getpwuid(geteuid());
    return (strcmp(pws->pw_name, "root") == 0);
}

float parseTime (std::string& str) {
    float time(0);
    float d(0), h(0), m(0), s(0), j(0);
    std::vector<std::string> buf;

    if (isFloat(str))
        return std::stof(str);

    while (!str.empty()) {
        buf.push_back(str.substr(0, str.find_first_of("dhmsj")+1));
        str.erase(0, str.find_first_of("dhmsj")+1);
    }

    for (auto& i : buf) {
        if (i[i.length()-1] == 'd')
            d += std::stof(i.substr(0, i.size()-1));
        else if (i[i.length()-1] == 'h')
            h += std::stof(i.substr(0, i.size()-1));
        else if (i[i.length()-1] == 'm')
            m += std::stof(i.substr(0, i.size()-1));
        else if (i[i.length()-1] == 's')
            s += std::stof(i.substr(0, i.size()-1));
        else if (i[i.length()-1] == 'j')
            j += std::stof(i.substr(0, i.size()-1));
    }

    time += d*24*60*60;
    time += h*60*60;
    time += m*60;
    time += s;
    time += j/sysconf(_SC_CLK_TCK);
    return time;
}

bool isTime (std::string& str) {
    int depth = 0;
    const std::string accepted_chars = "dhmsj";
    for (auto& c : str) {
        if (!isdigit(c) && accepted_chars.find(c) == std::string::npos && c != '.')
            return false;
        if (c == '.')
            depth += 1;
        if (depth == 2)
            return false;
        if (accepted_chars.find(c) != std::string::npos)
            depth = 0;
    }
    return true;
}

/**
 * Get the uptimes stored in the data file
 *
 * Read the data file line by line, if the line matches a process the user requested, add the uptime to the buffer
 *
 * @param toDisplay : buffer of what will be displayed
 * @param requestedProcesses : processes the user asked for in the command
 */
void getDataFile (std::map<std::string, std::pair<int, float>>& toDisplay, std::vector<std::string> requestedProcesses) {
    std::string uptimeDataFile = DATA_DIR + "uptime";
    std::ifstream uptimeDataFileR (uptimeDataFile);
    if (uptimeDataFileR) {
        if (uptimeDataFileR.peek() != std::ifstream::traits_type::eof()) {
            std::string line;
            std::string processName;
            float processUptime;

            while (getline(uptimeDataFileR, line)) {
                processName = line.substr(0, line.find_last_of(':')); // because string index start at 0, +1-1=0
                if (requestedProcesses.empty() ||
                    std::find(requestedProcesses.begin(), requestedProcesses.end(), processName) !=
                    requestedProcesses.end()) {
                    processUptime = std::stof(
                            line.substr(line.find_last_of(' ') + 1)); // because string index start at 0
                    if (toDisplay.contains(processName))
                        toDisplay[processName].second += processUptime;
                    else
                        toDisplay[processName] = std::make_pair(0, processUptime);
                }
            }
        } else
            error("No data on a previous boot\n", INFO);
    } else
        error("Data file nonexistant\n", WARN);
}

/**
 * Check if the argument is of the short form
 *
 * @param arg : argument to test
 * @return true : if it is a short arguments
 *         false : if it is not a short argument
 */
bool isShortArg (char*& arg) {
    if (strlen(arg) < 2)
        return false;
    std::string firstChar;
    firstChar += arg[0];
    std::string secondChar;
    secondChar += arg[1];
    return (firstChar == "-" && secondChar != "-");
}

/**
 * Get the buffer of processes that have already finished or are still running (their name and uptime) and add it to the buffer to display
 *
 * Connect to the socket, receive one line for each process, parse the line and add the datas to the buffer to display
 *
 * @param toDisplay : buffer of what will be displayed
 * @param requestedProcesses : processes the user asked for in the command
 */
void getUptimeBuffer (std::map<std::string, std::pair<int, float>>& toDisplay, std::vector<std::string> requestedProcesses) {
    int sockfd, servlen, n;
    struct sockaddr_un serv_addr{};
    char buffer[82];

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sun_family = AF_UNIX;
    strcpy(serv_addr.sun_path, SOCKET_PATH);
    servlen = strlen(serv_addr.sun_path) + sizeof(serv_addr.sun_family);
    if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
        error("Creating the socket\n", FATAL);

    if (connect(sockfd, (struct sockaddr *) &serv_addr, servlen) < 0)
        error("Connecting to the socket\n", FATAL);
    bzero(buffer, 82);


    write(sockfd, "uptimeBuffer", 13);

    read(sockfd, buffer, 80);
    write(sockfd, "1", 1); // send "ok, received"
    n = std::stoi(buffer);
    for (int i = 0; i < n; ++i) {
        // each process is sent through the socket in the form of "name\1uptime\n"
        bzero(buffer, 82);
        read(sockfd, buffer, 80);
        std::string buf;
        buf += buffer;
        size_t pos1 = buf.find('\1');
        std::string processName = buf.substr(0, pos1);
        if (requestedProcesses.empty() || std::find(requestedProcesses.begin(), requestedProcesses.end(), processName) != requestedProcesses.end()) {
            size_t end = buf.find('\n');
            size_t length = end - pos1;
            std::string uptime = buf.substr(pos1 + 1, length);
            float processUptime = std::stof(uptime);
            toDisplay[processName].second = processUptime;
        }
        write(sockfd, "1", 1); // to say "ok, received"
    }
    close(sockfd);
}

/**
 * Main
 *
 * Parse the arguments
 * Perform several tests over conflictive arguments
 * Get the necessary data
 * Display only what is needed
 *
 * @param argc : number of arguments
 * @param argv : arguments
 * @return 0 : if success
 *         non-zero : if error
 */
int main (int argc, char* argv[]) {
    // Display options
    bool boot_opt(false), allButBoot_opt(false), day_opt(false), hour_opt(false), minute_opt(false), second_opt(false), 
         clockTick_opt(false), defaultTimeFormat_opt(false);
    float greaterUptime_opt(0), lowerUptime_opt(0);
    std::string greaterUptimeBuf, lowerUptimeBuf;
    std::vector<std::string> requestedProcesses(0); //processes the user mentioned in the command

    // Admin options
    bool kill_opt(false), save_opt(false), reload_opt(false);

    std::vector<std::string> argsBuffer;


    for (int i = 1; i < argc; ++i) { //take all args except the command
        if (isShortArg(argv[i])) {
            for (int j = 1; j < strlen(argv[i]); ++j) {
                std::string shortArg = "-";
                shortArg += argv[i][j];
                argsBuffer.push_back(shortArg);
            }
        } else
            argsBuffer.emplace_back(argv[i]);
    }

    while (!argsBuffer.empty()) {
        std::string arg = argsBuffer[0];
        if (arg == "-v" || arg == "--version") {
            std::cout << VERSION;
            exit(0);
        } else if (arg == "-h" || arg == "--help") {
            std::cout << HELP_MSG;
            exit(0);
        } else if (arg == "-b" || arg == "--boot")
            boot_opt = true;
        else if (arg == "-B" || arg == "--all-but-boot")
            allButBoot_opt = true;
        else if (arg == "-d" || arg == "--day")
            day_opt = true;
        else if (arg == "-H" || arg == "--hour")
            hour_opt = true;
        else if (arg == "-m" || arg == "--minute")
            minute_opt = true;
        else if (arg == "-s" || arg == "--second")
            second_opt = true;
        else if (arg == "-j" || arg == "--clock-tick")
            clockTick_opt = true;
        else if (arg == "-f" || arg == "--default-time-format")
            defaultTimeFormat_opt = true;
        else if (arg == "-g" || arg == "--greater-uptime-than") {
            if (isTime(argsBuffer[1]))
                greaterUptimeBuf = argsBuffer[1];
            else {
                std::cout << "Provided value '" + argsBuffer[1] + "' to argument '-g | --greater-uptime-than' is not a positive integer\n\n"
                             "Usage: 'yotta -g <time>' to show all the processes with a lower uptime than <time>\n"
                             "       <time> is the form <<number>[d | h | m | s | j] ...> where the letters correspond to day, hour, minute, second, jiffy\n"
                             "       Without a letter the time is counted in seconds\n";
                exit(1);
            }
            argsBuffer.erase(argsBuffer.begin()+1);
        } else if (arg == "-l" || arg == "--lower-uptime-than") {
            if (isTime(argsBuffer[1]))
                lowerUptimeBuf = argsBuffer[1];
            else {
                std::cout << "Provided value '" + argsBuffer[1] + "' to argument '-l | --lower-uptime-than' is not a positive integer\n\n"
                             "Usage: 'yotta -l <time>' to show all the processes with a lower uptime than <time>\n"
                             "       <time> is the form <<number>[d | h | m | s | j] ...> where the letters correspond to day, hour, minute, second, jiffy\n"
                             "       Without a letter the time is counted in seconds\n";
                exit(1);
            }
            argsBuffer.erase(argsBuffer.begin()+1);
        } else if (arg[0] != '-') {
            requestedProcesses.push_back(arg);
        } else if (arg == "-k" || arg == "--kill" || arg == "--save" || arg == "-r" || arg == "--reload"){ //root only options
            if (isProcessRoot()) {
                if (arg == "-k" || arg == "--kill") {
                    kill_opt = true;
                } else if (arg == "--save") {
                    save_opt = true;
                } else if (arg == "-r" || arg == "--reload") {
                    reload_opt = true;
                }
            } else {
                std::cout << "You must be root to execute '" + arg + "' argument";
                exit(1);
            }
        } else {
            std::cout << "Yotta : invalid option -- '" + arg + "'\n'yotta -h' for more information";
            exit(1);
        }
        argsBuffer.erase(argsBuffer.begin());
    }


    if (boot_opt && allButBoot_opt) {
        std::cout << "Using '-b | -boot' and '-B | --all-but-boot' in the same command result is impossible\n"
                     "Use 'yotta -h' for help\n";
        exit(1);
    }

    if (!greaterUptimeBuf.empty())
        greaterUptime_opt = parseTime(greaterUptimeBuf);
    if (!lowerUptimeBuf.empty())
        lowerUptime_opt = parseTime(lowerUptimeBuf);

    if (greaterUptime_opt >= lowerUptime_opt && greaterUptime_opt != 0 && lowerUptime_opt != 0) {
        std::cout << "The interval given by '" << lowerUptime_opt << "' and '" << greaterUptime_opt << "', "
                     "parameters of '-l | --lower-uptime-than' and '-g | --greater-uptime-than' is invalid\n"
                     "The parameter of '-l | --lower-uptime-than' has to be greater than the parameter of '-g | --greater-uptime-than'\n";
        exit(1);
    }
    if (kill_opt || save_opt || reload_opt) {
        //get pid of daemon
        char line[10]; //up to large enough pid imo
        FILE *cmd = popen("pidof -s yotta_daemon", "r");
        unsigned long int pid = 0;
        fgets(line, 10, cmd);
        pid = strtoul(line, NULL, 10);
        pclose(cmd);

        if (save_opt) {
            if (pid < 1) {
                error("The damon is not running\n", FATAL);
            }
            kill(pid, SIGUSR1);
            exit(0);
        }
        if (kill_opt) {
            if (pid < 1) {
                error("The damon is not running\n", FATAL);
            }
            kill(pid, SIGKILL);
            exit(0);
        }
        if (reload_opt) {
            if (pid < 1) {
                error("The damon is not running\n", FATAL);
            }
            kill(pid, SIGUSR2);
            exit(0);
        }
    }

    std::map<std::string, std::pair<int, float>> toDisplay; //everything in the map will be displayed

    if (!allButBoot_opt) {
        if (system("pidof yotta_daemon > /dev/null") == 0) {
            getUptimeBuffer(toDisplay, requestedProcesses);
        } else {
            error("The daemon is not running\n", WARN);
        }
    }
    if (!boot_opt) {
        getDataFile(toDisplay, requestedProcesses);
    }

    std::cout << std::setw(40) << "Name" << std::setw(6) << "PID";
    if (defaultTimeFormat_opt || (!day_opt && !hour_opt && !minute_opt && !second_opt && !clockTick_opt))
        std::cout << std::setw(19) << "Uptime";
    if (day_opt)
        std::cout << std::setw(9) << "Days";
    if (hour_opt)
        std::cout << std::setw(12) << "Hours";
    if (minute_opt)
        std::cout << std::setw(16) << "Minutes";
    if (second_opt)
        std::cout << std::setw(20) << "Seconds";
    if (clockTick_opt)
        std::cout << std::setw(20) << "Jiffies";
    for (auto& s : toDisplay) {
        if (!((greaterUptime_opt && s.second.second <= greaterUptime_opt) || (lowerUptime_opt && s.second.second >= lowerUptime_opt))) { // check uptime conditions
            std::cout << "\n" << std::setw(40) << s.first;
            if (s.second.first != 0)
                std::cout << std::setw(6) << s.second.first;
            else
                std::cout << std::setw(6) << "    ";

            if (defaultTimeFormat_opt || (!day_opt && !hour_opt && !minute_opt && !second_opt && !clockTick_opt)) {
                std::string uptime;

                /// calculate time
                int total = s.second.second; //in seconds
                int seconds = total % 60;
                int minutes = ((total - seconds) / 60) % 60;
                int hours = ((total - 60*minutes - seconds) / (60*60)) % 24;
                int days = (total - 24*60*hours - 60*minutes - seconds) / (24*60*60);

                /// display only what is needed
                if (days >= 1)
                    uptime = std::to_string(days) + "d " + std::to_string(hours) + 'h' + std::to_string(minutes)
                                     + 'm' + std::to_string(seconds) + 's';
                else if (hours >= 1)
                    uptime = std::to_string(hours) + 'h' + std::to_string(minutes) + 'm' + std::to_string(seconds) + 's';
                else if (minutes >= 1)
                    uptime = std::to_string(minutes) + 'm' + std::to_string(seconds) + 's';
                else
                    uptime = std::to_string(seconds) + 's';
                std::cout << std::setw(19) << uptime;
            }
            if (day_opt) {
                float days = s.second.second / (60*60*24);
                std::cout << std::setw(9) << std::fixed << std::setprecision(2) << days;
            }
            if (hour_opt) {
                float hours = s.second.second / (60*60);
                std::cout << std::setw(12) << std::fixed << std::setprecision(2) << hours;
            }
            if (minute_opt) {
                float minutes = s.second.second / 60;
                std::cout << std::setw(16) << std::fixed << std::setprecision(2) << minutes;
            }
            if (second_opt) {
                float seconds = s.second.second;
                std::cout << std::setw(20) << std::fixed << std::setprecision(2) << seconds;
            }
            if (clockTick_opt) {
                int jiffies = s.second.second * sysconf(_SC_CLK_TCK);
                std::cout << std::setw(20) << jiffies;
            }
        }
    }
    std::cout << '\n';
    if (clockTick_opt) {
        int jps = sysconf(_SC_CLK_TCK);
        std::cout << "1 clock tick = " << (float)1/jps << " second   |   " << "1 second = " << jps << " clock ticks\n";
    }
    exit(0);
}

//todo -h<opt> detailed help of the option