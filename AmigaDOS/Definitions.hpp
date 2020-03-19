#ifndef DEFINITIONS_HPP
#define DEFINITIONS_HPP

#include "OSSymbols.hpp"

#include <string>
#include <list>
#include <vector>

using namespace std;

// -----------------------------------------------

struct SymtabEntry;

struct Structure;
struct Enumerable;

struct Definition {	
	enum Type {
		T_UNKNOWN = 0,
		T_POINTER,
		T_ARRAY,
		T_STRUCT,
		T_UNION,
		T_ENUM,
		T_CONFORMANT_ARRAY,
		T_VOID,
		T_BOOL,
		T_U8,
		T_8,
		T_U16,
		T_16,
		T_U32,
		T_32,
		T_U64,
		T_64,
		T_FLOAT32,
		T_FLOAT64,
		T_FLOAT128
	};
	string name;
	Type type;

	unsigned int blockSize;
	
	Definition *pointsTo;
	Structure *structure;
	Enumerable *enumerable;

	Definition (string name, Type type, int blockSSize = 0, Definition *pointsTo = 0, Structure *structure = 0, Enumerable *enumerable = 0)	{
        this->name = name;
		this->type = type;
		this->blockSize = blockSize;
		this->pointsTo = pointsTo;
		this->structure = structure;
		this->enumerable = enumerable;
	}
};

struct StabsDefinition : public Definition {
	struct Token {
		int file;
		int type;
	
		Token (int file, int type) { this->file = file; this->type = type; }
		bool operator==(Token &other) { return file == other.file && type == other.type; }
	};
    Token token;

	StabsDefinition (string name, Token _token, Type type, int blockSize = 0, Definition *pointsTo = 0, Structure *structure = 0, Enumerable *enumerable = 0)
	:	Definition(name, type, blockSize, pointsTo, structure, enumerable),
		token(_token)
	{}
};

struct Variable {
	typedef enum {
		NOTYPE = 0,
		POINTER = 1,
		COMBINED = 2, //struct or union
		NUMERICAL = 3
	} Kind;
	
	typedef enum {
		L_REGISTER,
		L_STACK,
		L_ABSOLUTE,
		L_POINTER
	} Location;

	string name;

	Definition *type;
	Kind kind;
	Location location;
	uint32_t offset; //address, register or offset TODO: better class handling of addresses (32/64 bit)

	Variable *pointer;

	Variable (string name, Definition *type, Location location, uint32_t offset, Variable *pointer = 0) {
		this->name = name;
		this->type = type;
		this->kind = kind;
		this->location = location;
		this->offset = offset;
		this->pointer = pointer;
	}

	list <Variable *> children();
	string valuePrintable(uint32_t stackPointer);
};

struct Structure {
	struct StructEntry {
		string name;
		int bitOffset, bitSize;
		StabsDefinition *type;
        StructEntry(string name, int bitOffset, int bitSize, StabsDefinition *type) {
            this->name = name;
            this->bitOffset = bitOffset;
            this->bitSize = bitSize;
            this->type = type;
        }
	};

public:
	list<StructEntry *> entries;
	
public:
	Structure() {}
	~Structure() { clear(); }

    void clear();

	void addEntry(string name, int bitOffset, int bitSize, StabsDefinition *type) {
        entries.push_back (new StructEntry(name, bitOffset, bitSize, type));
    }
	list<Variable *> children(Variable *parent);
};


struct Enumerable {
	struct EnumEntry {
		string name;
		int value; //should this be a double int?
		EnumEntry(string name, int value) { this->name = name; this->value = value; }
	};

	list<struct EnumEntry *> entries;
		
	~Enumerable() { clear(); }

	void clear();

	void addEntry (string name, int value) { entries.push_back (new EnumEntry(name, value)); }
	string valuePrintable (int value32);
};

struct Object;
struct Header {
	string name;
	Object *object;
	Header (string name, Object *object) { this->name = name; this->object = object; }
};
	
struct Function {
	struct Line {
		enum Type {
			LINE_NORMAL,
			LINE_LOOP
		};
		Type type;
	
		int line;
		uint32_t offset;      //offset relative to function's beginning address
		Function *function;
		
		Header *header;
	
		Line(Type type, int line, uint32_t offset, Function *function, Header *header) {
			this->type = type;
			this->line = line;
			this->offset = offset;
			this->function = function;
			this->header = header;
		}
	};

	vector<Line *> lines;
	
	list <Variable *> variables;
	list <Variable *> parameters;

	string name;	
	uint32_t address; // TODO: Better handling of address space (32/64 bit)
	uint32_t size;
	Object *object;
		
	Function(string name, uint32_t address, uint32_t size, Object *object) {
		this->name = name;
		this->address = address;
		this->size = size;
		this->object = object;
	}
	~Function() { clear(); }

	void clear();
	
	void addLine(Line::Type type, uint32_t offset, int line, Function *function, Header *header) {
		lines.push_back(new Line(type, offset, line, function, header));
	}
		
	bool addressInFunction(uint32_t address) { return address >= this->address && address < this->address + size; }
	Line *lineFromAddress(uint32_t address);
	uint32_t addressFromLine(int line);
	bool isSourceLine(int line);
	bool isSourceLine (int lineNumber, uint32_t address);
	string fileFromAddress(uint32_t address);

//	SourcecodeReference currentSourcecodePosition (uint32_t stackpointer) { printf("ARG!\n"); return SourcecodeReference(); }
};

struct Module;
struct Object {
	string name;
	Module *module;

	uint32_t startOffset;
	uint32_t endOffset;

	list<Definition *> types;
	list<Definition *> unknownTypes;
	list<Header *> headers;
	list<Function *> functions;

	// !!!!!
	//list < TEXTFILE what is that?> _files;
	//struct stabs_textfile **files;
		//////
	
	Object(string name, Module *module, uint32_t startOffset) {
		this->name = name;
		this->module = module;
		this->startOffset = startOffset;
	}
	
	Header *addHeader(string name) { Header *header = new Header(name, this); headers.push_back(header); return header; }

	Function *functionFromName (string name);
	Function *functionFromAddress (uint32_t address);
	bool addressInsideObject (uint32_t address) { return address >= startOffset && address < endOffset; }
	bool isSourceLine (string fileName, int line);
};

struct Module {
	string name;
	list<Object *> objects;
	list<Variable *> globals;

	uint32_t addressBegin, addressEnd;

	Module(string name) { this->name = name; }
	Object *objectFromName(string name);
	Object *objectFromAddress(uint32_t address);
};

///////////////////////////
struct SymtabEntry {
	unsigned long n_strx;
	unsigned char n_type;
	unsigned char n_other;
	unsigned short n_desc;
	unsigned int n_value;
};

#endif