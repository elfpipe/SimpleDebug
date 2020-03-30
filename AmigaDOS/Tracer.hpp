#ifndef TRACER_HPP
#define TRACER_HPP

#include <proto/dos.h>
#include "Breaks.hpp"
class Tracer {
private:
    Breaks breaks;
    Process *process;
    ExceptionContext *context;
private:
    uint32_t branchAddress();

    void setTraceBit();
    void unsetTraceBit();
    static bool hasTraceBit();
public:
    Tracer(Process *process, ExceptionContext *context);

    void activate(bool branching = true);
    void suspend();
};
#endif