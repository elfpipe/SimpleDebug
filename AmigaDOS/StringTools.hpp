#ifndef STROGNTOOLS_HPP
#define STRIGNTOOLS_HPP

#include <stdarg.h>

#include <vector>
#include <string>

using namespace std;

#define MAX_CHAR_BUFFER_SIZE 4096

vector<string> splitString(const string& str, const string& delim);
string printStringFormat (const char *format, ...);
string printStringFormat_helper (const char *format, va_list arg);

#endif