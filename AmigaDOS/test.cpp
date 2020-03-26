#ifndef STROGNTOOLS_HPP
#define STRIGNTOOLS_HPP

#include <stdarg.h>

#include <vector>
#include <string>
#include <sstream>
#include <iostream>

using namespace std;

#define MAX_CHAR_BUFFER_SIZE 4096

vector<string> splitString(const string& str, const string& delim);
string printStringFormat (const char *format, ...);
string printStringFormat_helper (const char *format, va_list arg);
string getStringUntil (char *input, char stopChar);
char *skip_in_string (char *source, const char *pattern);
char *skip_numbers (char *strptr);
string to_string(unsigned int a);
string to_string(int a);
string to_string(bool b);

class astream {
private:
    stringstream ss;
public:
    astream(string s) { ss << s; }
    void setString(string s) { ss << s; }
    vector<string> split(char delim) {
        vector<string> result;
        string line;
        while(getline(ss, line, delim))
            result.push_back(line);
        return result;
    }
    string get(char delim) {
        string result;
        getline(ss, result, delim);
        return result;
    }
    string get() {
        string result;
        getline(ss, result);
        return result;
    }
    void skip(char delim) {
        get(delim);
    }
    void skip() {
        ss.get();
    }
    void skipNums() {
        while(!ss.eof()) {
            char c = ss.get();
            if(c < '0' || c > '9') {
                ss.unget();
                break;
            }
        }
    }
    int getInt() {
        int result = 0;
        char c = ss.peek();
        if(c >= '0' && c <= '9')
            ss >> result;
        return result;
    }
    char peek() {
        return ss.peek();
    }
    bool eof() {
        return ss.eof();
    }
    void reset() {
        ss.seekg(0);
    }
    bool endsWith(char c) {
        int pos = ss.tellg();
        ss.seekg(-1, ss.end);
        bool result = ss.get() == c;
        ss.seekg(pos);
        return result;
    }
};

#if 1
int main() {
    astream a("4321/1234:Hello#Hi#Howareyou");
    cout << a.endsWith('u') << "\n";
    a.skipNums();
    cout << a.peek() << "\n";
//    a.skip('/');
    cout << a.getInt() << "\n";
    a.skip(':');
    vector<string> r = a.split('#');
    cout << r.size() << "\n";
    cout << r[0] << " " << r[1] << " " << r[2] << "\n";

    astream b("Hello\nHow are you?\nWhat's your favorite color?\n");
    cout << "\n" << b.get() << "\n";
    cout << b.get() << "\n";
    cout << b.get() << "\n";
    cout << b.eof() << "\n";
    b.get();
    cout << b.eof() << "\n";

    return 0;
}
#endif // 0
#endif