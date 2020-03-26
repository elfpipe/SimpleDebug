//
//
// stabsinterpreter.cpp - stabs interpretation module for runtime code tracking (Debug 101)
//
//

// -------------------------------------------------------------------------------------------

#include "Definitions.hpp"
#include "LowLevel.hpp"
#include "StringTools.hpp"

#include <list>

using namespace std;

#define MAX(a,b)	((a)>(b)?(a):(b))
#define MIN(a,b)	((a)<(b)?(a):(b))

// ---------------------------------------------------------------------------

string Variable::valuePrintable(uint32_t stackPointer)
{
    uint32_t *address = 0x0;

    switch (location) {
		case L_STACK:
			address = (uint32_t *)(stackPointer + offset); //_offset
			break;

		case L_ABSOLUTE:
			address = (uint32_t *)offset; //_address 
			break;

		case L_POINTER:
			//address = (uint32 *)get_pointer_value(s);
			break;

		case L_REGISTER:
			//return printRegisterValueString (_location);
			break;
    }
	
	if(!is_readable_address ((uint32)address)) {
		return string ("<No access>");
	}
	
	if (kind == NOTYPE) {
		return printStringFormat ("0x%x <no type>", *address);
	}
	
	switch (type->type) {
		case StabsDefinition::T_POINTER:
			return printStringFormat ("(*) 0x%x", *address);
            
		case StabsDefinition::T_VOID:
			return printStringFormat ("0x%x <void>", *address);
			
		case StabsDefinition::T_ENUM: {
			Enumerable *enumerable = type->enumerable;
			
			if (type->pointsTo)
				enumerable = type->pointsTo->enumerable;

            int32 value32 = *(int32*)address;
			
			return enumerable->valuePrintable(value32);
		}

		case StabsDefinition::T_STRUCT:
			return "-struct-";

		case StabsDefinition::T_UNION:
			return "-union-";

		case StabsDefinition::T_32: {
			int32 value32 = *(int32*)address;
			return printStringFormat ("%d", value32);
		}

		case StabsDefinition::T_U32: {
			uint32 valueu32 = *address;
			return printStringFormat ("0x%x", valueu32);
		}

		case StabsDefinition::T_16: {
			int16 value16 = *(int16*)address;
			int32 convert16 = value16;
			return printStringFormat ("%d", convert16);
		}

		case StabsDefinition::T_U16: {
			uint16 valueu16 = *(uint16*)address;
			uint32 convertu16 = valueu16;
			return printStringFormat ("%u", convertu16);
		}

		case StabsDefinition::T_8: {
			signed char value8 = *(signed char*)address;
			int32 convert8 = value8;
			return printStringFormat ("%d", convert8);
		}

		case StabsDefinition::T_U8: {
			char valueu8 = *(char*)address;
			uint32 convertu8 = valueu8;
			return printStringFormat ("%u (%c)", convertu8, valueu8);
		}

		case StabsDefinition::T_FLOAT32: {
			float valuef32 = *(float*)address;
			return printStringFormat ("%e", valuef32);
		}

		case StabsDefinition::T_FLOAT64: {
			double valuef64 = *(double*)address;
			return printStringFormat ("%f", valuef64);
		}

		default: {
			//this should not happen
			uint32 unknownu32 = *(uint32*)address;
			return printStringFormat ("0x%x (unknown)", unknownu32);
		}
	}
	return string();
}

string Variable::kindPrintable(Kind kind)
{
	switch(kind) {
		case NOTYPE:
			return "NOTYPE";
		case POINTER:
			return "POINTER";
		case COMBINED:
			return "COMBINED";
		case NUMERICAL:
			return "NUMERICAL";
	}
	return string();
}

string Variable::printable()
{
	return "VAR: " + name + "(" + type->printable() + ")" +  kindPrintable(kind) + " " + locationPrintable(location) + " " + patch::toString(offset) + (pointer ? pointer->printable() : "\n");
}

string Variable::locationPrintable(Location location)
{
	switch(location) {
		case L_REGISTER:
			return "L_REGISTER";
		case L_STACK:
			return "L_STACK";
		case L_ABSOLUTE:
			return "L_ABSOLUTE";
		case L_POINTER:
			return "L_POINTER";
	}
	return string();
}

string Definition::typePrintable(Type type)
{
	switch(type) {
		case T_UNKNOWN:
			return "T_UNKNOWN";
		case T_POINTER:
			return "T_POINTER";
		case T_ARRAY:
			return "T_ARRAY";
		case T_STRUCT:
			return "T_STRUCT";
		case T_UNION:
			return "T_UNION";
		case T_ENUM:
			return "T_ENUM";
		case T_CONFORMANT_ARRAY:
			return "T_CONFORMANT_ARRAY";
		case T_VOID:
			return "T_VOID";
		case T_BOOL:
			return "T_BOOL";
		case T_U8:
			return "T_U8";
		case T_8:
			return "T_8";
		case T_U16:
			return "T_U16";
		case T_16:
			return "T_16";
		case T_U32:
			return "T_U32";
		case T_32:
			return "T_32";
		case T_U64:
			return "T_U64";
		case T_64:
			return "T_64";
		case T_FLOAT32:
			return "T_FLOAT32";
		case T_FLOAT64:
			return "T_FLOAT64";
		case T_FLOAT128:
			return "T_FLOAT128";
	}
		return string();
}

string Definition::printable()
{
	return "D(" + typePrintable(type) + ") " + name + " " + (type == T_ARRAY ? "[" + patch::toString(blockSize) + "]" : (pointsTo ? "(*)" + pointsTo->printable() : (structure ? structure->printable() : (enumerable ? enumerable->printable() : ";")))) + "\n";
}

string StabsDefinition::printable()
{
	return Definition::printable() + "Token(" + patch::toString(token.file) + "," + patch::toString(token.type) + ")\n";
}

// -------------------------------------------------------------------------------- //

void Structure::clear() {
	for (list<StructEntry *>::iterator it = entries.begin(); it != entries.end(); it++)
		delete *it;
	entries.clear();
}

list<Variable *> Structure::children(Variable *parent)
{
	list<Variable *> result;
	for(list<StructEntry *>::iterator it = entries.begin(); it != entries.end(); it++) {
		Variable::Location location = parent->location;
		uint32_t offset = (location == Variable::L_POINTER ? (*it)->bitOffset >> 3 : parent->offset + (*it)->bitOffset >> 3);
		
		result.push_back(new Variable((*it)->name, (*it)->type, location, offset));
	}
	return result;
}

string Structure::printable()
{
	string result = "STRUCT:{\n";
	for(list<StructEntry *>::iterator it = entries.begin(); it != entries.end(); it++)
		result += "E" + (*it)->type->printable() + " " + (*it)->name + " [" + patch::toString((*it)->bitOffset) + "," + patch::toString((*it)->bitSize) + "]\n";
	result += "}\n";
	return result;
}
// ---------------------------------------------------------------------------

// -------------------------------------------------------------------------

void Enumerable::clear() {
	for (list<EnumEntry *>::iterator it = entries.begin(); it != entries.end(); it++)
		delete *it;
	entries.clear();
}

string Enumerable::valuePrintable(int value32)
{
	for (list<struct EnumEntry *>::iterator it = entries.begin(); it != entries.end (); it++)
		if (value32 == (*it)->value)
			return printStringFormat ("%s <%d>", (*it)->name.c_str(), value32);
	return printStringFormat ("0x%x <enum undefined>", value32);
}

string Enumerable::printable()
{
	string result = "ENUM{\n";
	for(list<EnumEntry *>::iterator it = entries.begin(); it != entries.end(); it++)
		result += "E:" + (*it)->name + "(" + patch::toString((*it)->value) + ")\n";
	result += "}\n";
	return result;
}

// ------------------------------------------------------------------------- //

string Header::printable()
{
	return "HEADER: " + name + " from " + object->name + "\n";
}

// --------------------------------------------------------------------------

void Function::clear() {
	for (vector<Line *>::iterator it = lines.begin(); it != lines.end(); it++)
		delete *it;
	for (list<Variable *>::iterator it = variables.begin(); it != variables.end(); it++)
		delete *it;
	for (list<Variable *>::iterator it = parameters.begin(); it != parameters.end(); it++)
		delete *it;
	lines.clear();
	variables.clear();
	parameters.clear();
}

Function::Line *Function::lineFromAddress(uint32_t address)
{
	if (!addressInFunction(address))
		return 0;
	for (vector<Line *>::iterator it = lines.begin(); it != lines.end(); it++)
		if (address < this->address + (*it)->offset)
			return *it;
	return 0;
}

uint32_t Function::addressFromLine(int line)
{
	for (vector<Line *>::iterator it = lines.begin(); it != lines.end(); it++)
		if ((*it)->line == line)
			return address + (*it)->offset;
	return 0; //throw?
}

bool Function::isSourceLine(int line)
{
	for (vector<Line *>::iterator it = lines.begin(); it != lines.end(); it++)
		if ((*it)->line == line)
			return true;
	return false;
}

string Function::fileFromAddress(uint32_t address)
{
	Line *line = lineFromAddress(address);
	if(line)
		return line->header ? line->header->name : object->name;
	return object->name;
}

bool Function::isSourceLine (int lineNumber, uint32_t address)
{
	Line *line = lineFromAddress (address);
	Header *header = line ? line->header : 0;
	for (list<Function *>::iterator itf = object->functions.begin(); itf != object->functions.end(); itf++)
		for (vector<Line *>::iterator itl = (*itf)->lines.begin(); itl != (*itf)->lines.end(); itl++)
			if ((*itl)->line == lineNumber && (*itl)->header == header)
				return true;
	return false;
}

string Function::printable() {
	string result = "FUNC:" + name + "(" + patch::toString(address) + "," + patch::toString(size) + " - " + object->name + ") {\n";

	for(vector<Line *>::iterator it = lines.begin(); it != lines.end(); it++)
		result += "L:<" + patch::toString((*it)->type) + "> " + "line " + patch::toString((*it)->line) + " offset " + patch::toString((*it)->offset) + " function " + (*it)->function->name + " header " + ((*it)->header ? (*it)->header->name : "*") + "\n";
	result += "VARIABLES:\n";
	for(list<Variable *>::iterator it = variables.begin(); it != variables.end(); it++)
		result += (*it)->printable();
	result += "PARAMETERS:\n";
	for(list<Variable *>::iterator it = parameters.begin(); it != parameters.end(); it++)
		result += (*it)->printable();
	result += "}\n";
	return result;
}
/*
struct stabs_function *stabs_checkbox_at_line (struct stabs_object *object, struct stabs_header *header, struct stabs_function *function, int sline, int *nline)
{
    if (!object)
    {
        if (header)
            object = header->object;
        else if (function)
            object = function->object;
        else
            return 0;
    }

    *nline = -1;
    if (function && !header)
    {
        int l;
        struct stabs_header *h = 0;
        for (l = 0; l < function->numberoflines; l++)
        {
            if (function->lines[l].infile == sline)
            {
                h = function->lines[l].header;
            }
        }
    }
    struct stabs_function *f = (struct stabs_function *)IExec->GetHead ((struct List *)&object->functions_list);
    while (f)
    {
        int l;
        for (l = 0; l <f->numberoflines; l++)
        {
            if (f->lines[l].header == header && f->lines[l].infile == sline)
            {
                *nline = l;
                return f;
            }
        }
        f = (struct stabs_function *)IExec->GetSucc ((struct Node *)f);
    }
    return 0;
}
*/

/*
asm (
"	.globl _blr	\n"
"_blr:			\n"
"	blr			\n"
);


extern unsigned int _blr;
*/

// ---------------------------------------------------------------------------------------- //

Function *Object::functionFromName(string name) {
	for(list<Function *>::iterator it = functions.begin(); it != functions.end(); it++)
		if (!name.compare((*it)->name))
			return *it;
	return 0;
}

Function *Object::functionFromAddress(uint32_t address)  {
	for (list<Function *>::iterator it = functions.begin(); it != functions.end(); it++)
		if ((*it)->addressInFunction(address))
			return *it;
	return 0;
}

bool Object::isSourceLine (string fileName, int lineNumber)
{
	for (list<Function *>::iterator itf = functions.begin(); itf != functions.end(); itf++) {
		for (vector <Function::Line *>::iterator itl = (*itf)->lines.begin(); itl != (*itf)->lines.end(); itl++) {
			if ((((*itl)->header && !(*itl)->header->name.compare(fileName) || !name.compare(fileName))
				&& lineNumber == (*itl)->line)) {
				return true;
			}
		}
	}
	return false;
}

string Object::printable()
{
	string result = "OBJECT(" + name + ") from (" + module->name + ") offset(" + patch::toString(startOffset) + "," + patch::toString(endOffset) + ") {\n";

	result += "TYPES:\n";
	for(list<Definition *>::iterator it = types.begin(); it != types.end(); it++)
		result += (*it)->printable();
	result += "UNKNOWN TYPES:\n";
	for(list<Definition *>::iterator it = unknownTypes.begin(); it != unknownTypes.end(); it++)
		result += (*it)->printable();
	result += "HEADERS:\n";
	for(list<Header *>::iterator it = headers.begin(); it != headers.end(); it++)
		result += (*it)->printable();
	result += "FUNCTIONS:\n";
	for(list<Function *>::iterator it = functions.begin(); it != functions.end(); it++)
		result += (*it)->printable();
	result += "}\n";
	return result;
}
// ------------------------------------------------------------------------- //

Object *Module::objectFromName(string name) {
	for (list<Object *>::iterator it = objects.begin(); it != objects.end(); it++)
		if (!name.compare ((*it)->name))
			return *it;
	return 0;
}

Object *Module::objectFromAddress(uint32_t address) {
	for (list<Object *>::iterator it = objects.begin(); it != objects.end(); it++)
		if ((*it)->addressInsideObject(address))
			return *it;
	return 0;
}

string Module::printable()
{
	string result = "MODULE (" + name + ") [" + patch::toString(addressBegin) + ", " + patch::toString(addressEnd) + "] {\n";
	for(list<Object *>::iterator it = objects.begin(); it != objects.end(); it++)
		result += (*it)->printable();
	result += "GLOBALS:\n";
	for(list<Variable *>::iterator it = globals.begin(); it != globals.end(); it++)
		result += (*it)->printable();
	result += "}\n";
	return result;
}