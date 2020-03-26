#include <cstdio>
#include <sys/socket.h>
#include <cstdlib>
#include <iostream>
#include <sys/un.h>
#include <unistd.h>
#include <vector>
#include <iomanip>
#include <fstream>

#include "timeTracking.h"

const std::string DATA_DIR = "/var/lib/yotta/";

const std::string HELP_MSG = "This is the help message"; //todo

void error(const char *);

std::string getVersion () {
    return ("This is the version"); //todo
}

void getDataFile (std::map<std::string, std::pair<int, float>>& askedProcesses) {
    std::string uptimeDataFile = DATA_DIR + "uptime";
    std::ifstream uptimeDataFileR (uptimeDataFile);
    if (!uptimeDataFileR)
        error("Data file nonexistent");
    std::string line;
    std::string processName;
    float processUptime;

    while (getline(uptimeDataFileR, line)) {
        processName = line.substr(0, line.find_last_of(':')); // because string index start at 0, +1-1=0
        processUptime = std::stof(line.substr(line.find_last_of(' ') + 1)); // because string index start at 0
        if (askedProcesses.contains(processName))
            askedProcesses[processName].second += processUptime;
        else
            askedProcesses[processName] = std::make_pair(0, processUptime);
    }
}

bool isShortArg (char*& arg) {
    if (strlen(arg) < 2)
        return false;
    std::string firstChar;
    firstChar += arg[0];
    std::string secondChar;
    secondChar += arg[1];
    return (firstChar == "-" && secondChar != "-");
}

void getProcessBuffer (std::map<std::string, std::pair<int, float>>& askedProcesses) {
    const char* const socket_path = "/run/yotta/yotta_socket";

    int sockfd, servlen, n;
    struct sockaddr_un serv_addr{};
    char buffer[82];

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sun_family = AF_UNIX;
    strcpy(serv_addr.sun_path, socket_path);
    servlen = strlen(serv_addr.sun_path) + sizeof(serv_addr.sun_family);
    if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
        error("Creating socket");

    if (connect(sockfd, (struct sockaddr *) &serv_addr, servlen) < 0)
        error("Connecting");
    bzero(buffer, 82);


    write(sockfd, "processBuffer", 13);

    read(sockfd, buffer, 80);
    write(sockfd, "1", 1); // send "ok, received"
    n = std::stoi(buffer);

    for (int i = 0; i < n; ++i) {
        bzero(buffer, 82);
        read(sockfd, buffer, 80);
        std::string buf;
        buf += buffer;
        size_t pos1 = buf.find('\1');
        size_t pos2 = buf.find('\2');
        size_t end = buf.find('\n');
        size_t length1 = pos2 - (pos1 + 1);
        size_t length2 = end - pos2;
        std::string processName = buf.substr(pos1 + 1, length1);
        std::string uptime = buf.substr(pos2 + 1, length2);
        int pid = stoi(buf.substr(0, pos1));
        float processUptime = std::stof(uptime);
        if (askedProcesses.contains(processName)) {
            askedProcesses[processName].first = pid;
            askedProcesses[processName].second += processUptime;
        } else
            askedProcesses[processName] = std::make_pair(pid, processUptime);
        write(sockfd, "1", 1); // to say "ok, received"
    }
    close(sockfd);
}

void getUptimeBuffer (std::map<std::string, std::pair<int, float>>& askedProcesses) {
    const char* const socket_path = "/run/yotta/yotta_socket";

    int sockfd, servlen, n;
    struct sockaddr_un serv_addr{};
    char buffer[82];

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sun_family = AF_UNIX;
    strcpy(serv_addr.sun_path, socket_path);
    servlen = strlen(serv_addr.sun_path) + sizeof(serv_addr.sun_family);
    if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
        error("Creating socket");

    if (connect(sockfd, (struct sockaddr *) &serv_addr, servlen) < 0)
        error("Connecting");
    bzero(buffer, 82);


    write(sockfd, "uptimeBuffer", 13);

    read(sockfd, buffer, 80);
    write(sockfd, "1", 1); // send "ok, received"
    n = std::stoi(buffer);
    for (int i = 0; i < n; ++i) {
        bzero(buffer, 82);
        read(sockfd, buffer, 80);
        std::string buf;
        buf += buffer;
        size_t pos1 = buf.find('\1');
        size_t end = buf.find('\n');
        size_t length = end - pos1;
        std::string uptime = buf.substr(pos1 + 1, length);
        std::string processName = buf.substr(0, pos1);
        float processUptime = std::stof(uptime);
        askedProcesses[processName].second = processUptime;
        write(sockfd, "1", 1); // to say "ok, received"
    }
    close(sockfd);
}

int main (int argc, char* argv[]) {

    bool boot_opt(false), allButBoot_opt(false), day_opt(false), hour_opt(false), minute_opt(false), second_opt(false), 
         jiffy_opt(false), defaultTimeFormat_opt(false);
    int greaterUptime_opt(0), lowerUptime_opt(0);

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
            std::cout << getVersion();
            exit(0);
        } else if (arg == "-h" || arg == "--help") {
            std::cout << HELP_MSG;
            exit(0);
        } else if (arg == "-b" || arg == "--boot")
            boot_opt = true;
        else if (arg == "-B" || arg == "--all-but-boot")
            allButBoot_opt = true;
        else if (arg == "-d" || arg == "-day")
            day_opt = true;
        else if (arg == "-H" || arg == "-hour")
            hour_opt = true;
        else if (arg == "-m" || arg == "-minute")
            minute_opt = true;
        else if (arg == "-s" || arg == "--second")
            second_opt = true;
        else if (arg == "-j" || arg == "--jiffy")
            jiffy_opt = true;
        else if (arg == "-f" || arg == "--default-time-format")
            defaultTimeFormat_opt = true;
        else if (arg == "-g" || arg == "--greater-uptime-than") {
            if (isFloat(argsBuffer[1]) && stoi(argsBuffer[1]) > 0)
                greaterUptime_opt = stoi(argsBuffer[1]);
            else {
                std::cout << "Provided value '" + arg + "' to argument '-g | --greater-uptime-than' is not a positive number\n"
                             "Usage: 'yotta -g <number>' to show all the processes with a greater uptime than <number> seconds";
                exit(0);
            }
            argsBuffer.erase(argsBuffer.begin()+1);
        } else if (arg == "-l" || arg == "--lower-uptime-than") {
            if (isFloat(argsBuffer[1]) && stoi(argsBuffer[1]) > 0)
                lowerUptime_opt = stoi(argsBuffer[1]);
            else {
                std::cout << "Provided value '" + arg + "' to argument '-l | --lower-uptime-than' is not a positive number\n"
                             "Usage: 'yotta -l <number>' to show all the processes with a lower uptime than <number> seconds";
                exit(0);
            }
            argsBuffer.erase(argsBuffer.begin()+1);
        } else {
            std::cout << "Provided argument '" + arg + "' is not an argument\nUse 'yotta -h' for help";
            exit(0);
        }
        argsBuffer.erase(argsBuffer.begin());
    }


    if (boot_opt && allButBoot_opt) {
        std::cout << "Using '-b | -boot' and '-B | --all-but-boot' in the same command result is impossible\n"
                     "Use 'yotta -h' for help";
        exit(0);
    }

    if (greaterUptime_opt >= lowerUptime_opt && greaterUptime_opt != 0 && lowerUptime_opt !=0) {
        std::cout << "The interval given by '" << lowerUptime_opt << "' and '" << greaterUptime_opt << "', "
                     "parameters of '-l | --lower-uptime-than' and '-g | --greater-uptime-than' is invalid\n"
                     "The parameter of '-l | --lower-uptime-than' has to be greater or equal to the parameter of '-g | --greater-uptime-than'";
        exit(0);
    }

    std::map<std::string, std::pair<int, float>> askedProcesses;


    if (!allButBoot_opt) {
        if (system("pidof yotta_daemon") == 0) { //TODO print the pid on screen 
            getUptimeBuffer(askedProcesses);
            getProcessBuffer(askedProcesses);
        } else {
            error("daemon is not running");
        }
    }

    if (!boot_opt) {
        getDataFile(askedProcesses);
    }

    std::cout << std::setw(40) << "Name" << std::setw(6) << "PID";
    if (defaultTimeFormat_opt || (!day_opt && !hour_opt && !minute_opt && !second_opt && !jiffy_opt))
        std::cout << std::setw(19) << "Uptime";
    if (day_opt)
        std::cout << std::setw(9) << "Days";
    if (hour_opt)
        std::cout << std::setw(12) << "Hours";
    if (minute_opt)
        std::cout << std::setw(16) << "Minutes";
    if (second_opt)
        std::cout << std::setw(20) << "Seconds";
    if (jiffy_opt)
        std::cout << std::setw(20) << "Jiffies";
    for (auto& s : askedProcesses) {
        if (!((greaterUptime_opt && s.second.second <= greaterUptime_opt) || (lowerUptime_opt && s.second.second >= lowerUptime_opt))) { // check uptime conditions
            std::cout << "\n" << std::setw(40) << s.first;
            if (s.second.first != 0)
                std::cout << std::setw(6) << s.second.first;
            else
                std::cout << std::setw(6) << "    ";

            if (defaultTimeFormat_opt || (!day_opt && !hour_opt && !minute_opt && !second_opt && !jiffy_opt)) {
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
            if (jiffy_opt) {
                int jiffies = s.second.second * sysconf(_SC_CLK_TCK);
                std::cout << std::setw(20) << jiffies;
            }
        }
    }

    if (jiffy_opt) {
        int jps = sysconf(_SC_CLK_TCK);
        std::cout << "\n1 jiffy = " << (float)1/jps << " second   |   " << "1 second = " << jps << " jiffies";
    }
    exit(0);
}

void error(const char *msg)
{
    perror(msg);
    exit(0);
}