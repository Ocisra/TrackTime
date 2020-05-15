#ifndef YOTTA_UTIL_HPP
#define YOTTA_UTIL_HPP

void error (const char * msg, unsigned int level);
void mask_sig ();
bool isFloat (std::string& str);
void trim (std::string& s);
float getSystemUptime ();
void loadConfig ();
void reloadConfig ();
void saveData (std::map<std::string, float>& uptimeBuffer);

#endif //YOTTA_UTIL_HPP