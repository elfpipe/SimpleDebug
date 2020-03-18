#include <stdarg.h>

#include <string>
#include <vector>

#include "StringTools.hpp"

using namespace std;

vector<string> splitString(const string& str, const string& delim)
{
    vector<string> tokens;
    size_t prev = 0, pos = 0;
    do
    {
        pos = str.find(delim, prev);
        if (pos == string::npos) pos = str.length();
        string token = str.substr(prev, pos-prev);
        if (!token.empty()) tokens.push_back(token);
        prev = pos + delim.length();
    }
    while (pos < str.length() && prev < str.length());
    return tokens;
}

std::string printStringFormat (const char *format, ...)
{
	va_list argptr;
	va_start(argptr, format);
	string result = printStringFormat_helper (format, argptr);
	va_end(argptr);

	return result;
}

std::string printStringFormat_helper (const char *format, va_list argptr)
{
	char result[MAX_CHAR_BUFFER_SIZE];
	vsprintf(result, format, argptr);
	string resultString(result);
	
	return resultString;
}