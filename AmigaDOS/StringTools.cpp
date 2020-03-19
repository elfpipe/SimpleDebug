#include <stdarg.h>

#include <string>
#include <vector>
#include <string.h>
#include <iostream>
#include <sstream>

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

string printStringFormat (const char *format, ...)
{
	va_list argptr;
	va_start(argptr, format);
	string result = printStringFormat_helper (format, argptr);
	va_end(argptr);

	return result;
}

string printStringFormat_helper (const char *format, va_list argptr)
{
	char result[MAX_CHAR_BUFFER_SIZE];
	vsprintf(result, format, argptr);
	string resultString(result);
	
	return resultString;
}

string getStringUntil (char *input, char stopChar)
{
	stringstream iss(input);
	string result;
	getline (iss, result, stopChar);
	return result;
}
// --------------------------------------------------------------------

char *skip_in_string (char *source, const char *pattern)
{
	char *ptr = source;
	int patlen = strlen(pattern);

	while (*ptr != '\0')
	{
		if (*ptr == pattern[0])
		{
			int j = 0;
			while (pattern[j] == *ptr)
			{
				j++; ptr++;
				if (j==patlen)
					return ptr;
			}
		}
		else
			ptr++;
	}
	return ptr; //return a pointer to terminating \0
}

char *skip_numbers (char *strptr)
{
	char *ret = strptr;
	while(*ret >= '0' && *ret <= '9')
		ret++;
	return ret;
}