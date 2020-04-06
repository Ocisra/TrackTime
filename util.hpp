#ifndef YOTTA_UTIL_HPP
#define YOTTA_UTIL_HPP

void error (const char * msg, bool isFatal = false);
void mask_sig ();
bool isFloat (std::string& str);

#endif //YOTTA_UTIL_HPP