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

// -------------------------------------------------------------------------------- //

void Structure::clear() {
	for (list<StructEntry *>::iterator it = entries.begin(); it != entries.end(); it++)
		delete *it;
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

// ---------------------------------------------------------------------------

// -------------------------------------------------------------------------

void Enumerable::clear() {
	for (list<EnumEntry *>::iterator it = entries.begin(); it != entries.end(); it++)
		delete *it;
}

string Enumerable::valuePrintable(int value32)
{
	for (list<struct EnumEntry *>::iterator it = entries.begin(); it != entries.end (); it++)
		if (value32 == (*it)->value)
			return printStringFormat ("%s <%d>", (*it)->name.c_str(), value32);
	return printStringFormat ("0x%x <enum undefined>", value32);
}

// --------------------------------------------------------------------------

void Function::clear() {
	for (vector<Line *>::iterator it = lines.begin(); it != lines.end(); it++) {
		delete (*it);
		lines.erase(it);
	}
	for (list<Variable *>::iterator it = variables.begin(); it != variables.end(); it++) {
		delete (*it);
		variables.erase(it);
	}
	for (list<Variable *>::iterator it = parameters.begin(); it != parameters.end(); it++) {
		delete (*it);
		variables.erase(it);
	}
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

StabsDefinition *StabsObject::getTypeFromToken (StabsDefinition::Token token) {
	for (list<Definition *>::iterator it = types.begin(); it != types.end(); it++) {
		StabsDefinition *def = (StabsDefinition *)*it;
		if (token == def->token)
			return def;
	}
	return 0;
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