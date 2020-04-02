#ifndef PIPE_HPP
#define PIPE_HPP

#include <proto/dos.h>

#include <string>
#include <stdint.h>
#include "Strings.hpp"

using namespace std;

class Pipe {
private:
    const int CHUNK_SIZE = 4096;

    BPTR fd[2];
    char *buffer = 0;
    int size = 0;
    int bytes = 0;

    bool isReady = false;

public:
    Pipe();
    ~Pipe();

    void init();
    void cleanup();

    bool ready() {return isReady; }

    BPTR getWrite() { return fd[1]; }
    BPTR getRead() { return fd[0]; }

    vector<string> emptyPipe();
    bool bytesToRead();

    int read();
    int write(string text);
};
#endif