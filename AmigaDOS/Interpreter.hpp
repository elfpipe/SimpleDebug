#ifndef INTERPRETER_HPP
#define INTERPRETER_HPP

#include "Definitions.hpp"
#include "ElfHandle.hpp"
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
	bool loadInterpretObject (StabsObject *object);

	uint32_t symbolValue(string symbol) { return symbols.valueOf(symbol); }

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

	
enum __stab_debug_code
{
N_UNDF = 0x00,
N_GSYM = 0x20,
N_FNAME = 0x22,
N_FUN = 0x24,
N_STSYM = 0x26,
N_LCSYM = 0x28,
N_MAIN = 0x2a,
N_ROSYM = 0x2c,
N_PC = 0x30,
N_NSYMS = 0x32,
N_NOMAP = 0x34,
N_OBJ = 0x38,
N_OPT = 0x3c,
N_RSYM = 0x40,
N_M2C =  0x42,
N_SLINE = 0x44,
N_DSLINE = 0x46,
N_BSLINE = 0x48,
N_BROWS = 0x48,
N_DEFD = 0x4a,
N_FLINE = 0x4C,
N_EHDECL = 0x50,
N_MOD2 = 0x50,
N_CATCH = 0x54,
N_SSYM = 0x60,
N_ENDM = 0x62,
N_SO = 0x64,
N_ALIAS = 0x6c,
N_LSYM = 0x80,
N_BINCL = 0x82,
N_SOL = 0x84,
N_PSYM = 0xa0,
N_EINCL = 0xa2,
N_ENTRY = 0xa4,
N_LBRAC = 0xc0,
N_EXCL = 0xc2,
N_SCOPE = 0xc4,
N_PATCH = 0xd0,
N_RBRAC = 0xe0,
N_BCOMM = 0xe2,
N_ECOMM = 0xe4,
N_ECOML = 0xe8,
N_WITH = 0xea,
N_NBTEXT = 0xF0,
N_NBDATA = 0xF2,
N_NBBSS =  0xF4,
N_NBSTS =  0xF6,
N_NBLCS =  0xF8,
N_LENG = 0xfe,
LAST_UNUSED_STAB_CODE
};


/* Definitions of "desc" field for N_SO stabs in Solaris2.  */

#define	N_SO_AS		1
#define	N_SO_C		2
#define	N_SO_ANSI_C	3
#define	N_SO_CC		4	/* C++ */
#define	N_SO_FORTRAN	5
#define	N_SO_PASCAL	6

/* Solaris2: Floating point type values in basic types.  */

#define	NF_NONE		0
#define	NF_SINGLE	1	/* IEEE 32-bit */
#define	NF_DOUBLE	2	/* IEEE 64-bit */
#define	NF_COMPLEX	3	/* Fortran complex */
#define	NF_COMPLEX16	4	/* Fortran double complex */
#define	NF_COMPLEX32	5	/* Fortran complex*16 */
#define	NF_LDOUBLE	6	/* Long double (whatever that is) */

int atoi (const char * str);

#endif

