#ifndef INTERPRETER_HPP
#define INTERPRETER_HPP

#include "Definitions.hpp"
#include "ElfHandle.hpp"
#include "symtabs.h"

#include <string>

using namespace std;

StabsDefinition::Token interpretNumberToken(char *strptr);

struct StabsObject;
struct StabsFunction : public Function {
	StabsFunction() : Function(string(), 0x0, 0, 0) {}
	StabsFunction(string name, uint32_t address, int size, StabsObject *object)
	:	Function(name, address, size, (Object *)object)
	{}
	SymtabEntry *interpret (SymtabEntry *sym, char *stabstr, StabsObject *object, Header **header, uint32_t endOfSymtab);
};

struct StabsModule;
struct StabsObject : public Object {
	SymtabEntry *stabsOffset;
	bool wasInterpreted;

	StabsObject(string name, StabsModule *module, uint32_t startOffset, SymtabEntry *stabsOffset)
	:	Object(name, (Module *)module, startOffset)
	{
		this->stabsOffset = stabsOffset;
		wasInterpreted = false;
	}

	StabsDefinition *getTypeFromToken (StabsDefinition::Token token);

	Structure *interpretStructOrUnion (char *strptr);
	Enumerable *interpretEnum (char *strptr);
	StabsDefinition::Type interpretRange (char *strptr);
	StabsDefinition *getInterpretType (char *strptr);

	SymtabEntry *interpret (char *stabstr, SymtabEntry *sym, uint32_t stabsize);

	string printable();
};

struct StabsModule : public Module {
	ElfHandle *elfHandle;

	char *stabstr;
	uint32_t *stab;
	uint32_t stabsize;
	
	OSSymbols symbols;

	bool globalsLoaded;

public:
	StabsModule (string name, ElfHandle *handle); // : Module(name) { elfHandle = handle; }
	~StabsModule() { delete elfHandle; }

	bool initialPass (bool loadEverything);
	bool interpretGlobals ();
	void readNativeSymbols();

	uint32_t symbolValue(string symbol) { return symbols.valueOf(symbol); }

	string symPrintable(unsigned char code);
	string symtabsPrintable();
	string printable();
};

class StabsInterpreter
{
private:
	list <StabsModule *> modules;

private:
	void clear();
	
public:
	StabsInterpreter() {}
	~StabsInterpreter() { clear(); }

	bool loadModule (ElfHandle *handle);
	bool loadModule (uint32_t randomAddress);

	StabsModule *moduleFromName (string moduleName);
	StabsModule *moduleFromAddress (uint32_t address);
	StabsObject *objectFromAddress (uint32_t address);

	list<StabsModule *> &getModules() { return modules; }

	string printable();
};

int atoi (const char * str);

#endif

