//
// nativesymbols.hpp - native symbol handling by elf handle for symbol table lookup
//

#ifndef DB101_NATIVESYMBOLS_hpp
#define DB101_NATIVESYMBOLS_hpp

#include <string>
#include <list>

#include <exec/types.h>

using namespace std;

class OSSymbols
{
public:
	struct OSSymbol {
		string name;
		uint32 value;
		
		OSSymbol (string _name, uint32 _value) : name(_name), value(_value) { }
	};

private:
	list <OSSymbol *> symbols;
	bool symbolsLoaded;

public:
	OSSymbols();
	~OSSymbols();

	bool hasSymbols() { return symbolsLoaded; }
	void clear();
	
	void readNativeSymbols(APTR handle);
	void addSymbol(string name, uint32 value);
	
	uint32 valueOf(string name);
	string nameFromValue(uint32 value);

    string printableList();
	void dummy (APTR handle);
};
#endif
