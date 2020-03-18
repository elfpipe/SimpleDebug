#ifndef DB101_BREAKpointsHANDLER_hpp
#define DB101_BREAKpointsHANDLER_hpp
//
//
// breakpointshandler.hpp - handle break markings in Debug 101
//

#include <exec/types.h>

#include <list>

using namespace std;

class Breaks {
private:
	struct Break {
		uint32 address;
		uint32 buffer;
        Break(uint32 _address) : address(_address) { }
	};
	list <Break *> breaks;
	bool activated;

public:
	Breaks() : activated(false) { }
	~Breaks() { clear(); }

	//bool cpuHasTracebit () { return _cpuHasTracebit; }
	bool isBreak(uint32 address);

	void activate();
	void suspend();

	void insert(uint32 address);
	void remove(uint32 address);

	void clear ();

#ifdef __amigaos4__
	static int memory_insert_break_instruction (uint32 address, uint32 *buffer);
	static int memory_remove_break_instruction (uint32 address, uint32 *buffer);
#endif
};
#endif
