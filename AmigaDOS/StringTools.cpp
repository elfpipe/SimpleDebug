#include <stdarg.h>
#include <string>
#include <sstream>

#include "StringTools.hpp"

using namespace std;

string printStringFormat_helper (const char *format, va_list argptr)
{
	char result[MAX_CHAR_BUFFER_SIZE];
	vsprintf(result, format, argptr);
	string resultString(result);
	
	return resultString;
}

string printStringFormat (const char *format, ...)
{
	va_list argptr;
	va_start(argptr, format);
	string result = printStringFormat_helper (format, argptr);
	va_end(argptr);

	return result;
}