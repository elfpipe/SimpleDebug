#ifndef DB101_BREAKpointsHANDLER_hpp
#define DB101_BREAKpointsHANDLER_hpp
//
//
// breakpointshandler.hpp - handle break markings in Debug 101
//

#include <exec/types.h>

#include <list>
#include <stdint.h>

using namespace std;

class Breaks {
private:
	struct Break {
		uint32_t address;
		uint32_t buffer;
        Break(uint32_t address) { this->address = address; }
	};
	list <Break *> breaks;
	bool activated;

public:
	Breaks() : activated(false) { }
	~Breaks() { clear(); }

	bool isBreak(uint32_t address);

	void activate();
	void deactivate();

	void insert(uint32_t address);
	void remove(uint32_t address);

	void clear ();

#ifdef __amigaos4__
	static int memory_insert_break_instruction (uint32_t address, uint32_t *buffer);
	static int memory_remove_break_instruction (uint32_t address, uint32_t *buffer);
#endif
};
#endif
