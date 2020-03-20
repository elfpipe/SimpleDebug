#include "Definitions.hpp"
#include "Interpreter.hpp"
#include "ElfHandle.hpp"
#include "StringTools.hpp"

#include <string>
#include <string.h>

#define MAX(a,b)	((a)>(b)?(a):(b))
#define MIN(a,b)	((a)<(b)?(a):(b))

StabsDefinition::Token interpretNumberToken(char *strptr)
{
	StabsDefinition::Token token (-1, -1);
	
	if(!strptr) {
		return token;
	}
	
	if ( (*strptr != '(') && !( '0' <= *strptr && *strptr <= '9') ) {
		strptr++;
	}
	
	token.file = 0;
	
	if (*strptr == '(') {
		strptr++;
		token.file = atoi(strptr);
		strptr = skip_in_string (strptr, ",");
	}
	token.type = atoi(strptr);
	
	return token;
}

bool str_is_typedef (char *str)
{
	char *ptr = str;
	ptr = skip_in_string (ptr, ":");
	return (*ptr == 't' || *ptr == 'T' ? true : false);
}

SymtabEntry *StabsFunction::interpret (SymtabEntry *sym, char *stabstr, StabsObject *object, Header **header, uint32_t endOfSymtab)   
{
	while (!sym->n_strx) {
		sym++;
	}
	
	int brac = 0;
	int size = 0;
	
	name = getStringUntil (&stabstr[sym->n_strx], ':');
	
	this->object = object;
	this->address = sym->n_value;
	
	uint32_t previousAddress = address;
	
	bool done = false;
	while (!done) {
		sym++;
		
		if ((uint32)sym >= endOfSymtab)
			break;

		switch (sym->n_type) {
			case N_LBRAC:
				brac++;
				break;
						
			case N_RBRAC:
				brac--;
			
				if(brac == 0) {
					size = sym->n_value; //should this be n_value-bracstart??
					done = TRUE;
				}
				
				if(brac < 0)
					printf("Stabs interpretation fault: Closing bracket without opening bracket in function interpretation");

				break;

			case N_SLINE: {
				Line::Type type;
			
				if (previousAddress > sym->n_value)
					type = Line::LINE_LOOP;
				else {
					size = sym->n_type + 4;
					type = Line::LINE_NORMAL;
				}
				previousAddress = sym->n_value;
				
				addLine (type, sym->n_value, sym->n_desc, this, *header);
				
				break;
			}
			case N_LSYM: {
				char *strptr = &stabstr[sym->n_strx];
				
				string name = getStringUntil (strptr, ':');
				
				strptr = skip_in_string (strptr, ":");
				StabsDefinition *type = object->getInterpretType (strptr);
	
				variables.push_back(new Variable(name, type, Variable::L_STACK, sym->n_value));

				break;
			}
			
			case N_PSYM: {
				char *strptr = &stabstr[sym->n_strx];
				
				string name = getStringUntil (strptr, ':');
				
				strptr = skip_in_string (strptr, ":");
				Definition *type = object->getInterpretType (strptr);
				
				parameters.push_back(new Variable(name, type, Variable::L_STACK, sym->n_value));

				break;
			}
			
			case N_SOL:				
				if(string (&stabstr[sym->n_strx]) != object->name) {
					*header = object->addHeader(string(&stabstr[sym->n_strx]));
				} else {
					*header = 0;
				}
						
				break;
			
			case N_FUN:
				if (sym->n_strx) {
					sym--;
					this->size = size;
				} else {
					size = sym->n_value;
				}
				done = true;
						
				break;
					
			case N_SO:
				size = this->size;
				sym--;
				done = true;

				break;

			default:
				break;
		}
		object->endOffset = MAX(object->endOffset, address + size);
	}
	
	return sym;
}

// ---------------------------------------------------------------------------------- //

StabsDefinition *StabsObject::getTypeFromToken (StabsDefinition::Token token) {
	for (list<Definition *>::iterator it = types.begin(); it != types.end(); it++) {
		StabsDefinition *def = (StabsDefinition *)*it;
		if (token == def->token)
			return def;
	}
	return 0;
}

Definition::Type StabsObject::interpretRange (char *strptr)
{
	if (*strptr != 'r')
		return Definition::T_UNKNOWN;
	strptr = skip_in_string (strptr, ";");
	if (!strptr)
		return Definition::T_UNKNOWN;
	
	long long lower_bound = atoi (strptr);
	strptr = skip_in_string (strptr, ";");
	long long upper_bound = atoi (strptr);
	
	Definition::Type ret = Definition::T_UNKNOWN;
	
	if (lower_bound == 0 && upper_bound == -1)
		;
	else if (lower_bound > 0 && upper_bound == 0)
	{
		switch(lower_bound)
		{
			case 4:
				ret = Definition::T_FLOAT32;
				break;
			case 8:
				ret = Definition::T_FLOAT64;
				break;
			case 16:
				ret = Definition::T_FLOAT128;
				break;
			default:
				ret = Definition::T_UNKNOWN;
				break;
		}
	}
	else
	{
		long long range = upper_bound - lower_bound;
		if(lower_bound < 0)
		{
			if(range <= 0xff)
				ret = Definition::T_8;
			else if(range <= 0xffff)
				ret = Definition::T_16;
			else if(range <= 0xffffffff)
				ret = Definition::T_32;
			else if(range <= 0xffffffffffffffff)
				ret = Definition::T_64;
//			else if(range <= 0xffffffffffffffffffffffffffffffff)
//				ret = Definition::T_128;
			else
				ret = Definition::T_UNKNOWN;
		}
		else // if unsigned
		{
			if(range <= 0xff)
				ret = Definition::T_U8;
			else if(range <= 0xffff)
				ret = Definition::T_U16;
			else if(range <= 0xffffffff)
				ret = Definition::T_U32;
			else if(range <= 0xffffffffffffffff)
				ret = Definition::T_U64;
			//else if(range <= 0xffffffffffffffffffffffffffffffff)
			//	ret = T_U128;
			else
				ret = Definition::T_UNKNOWN;
		}
	}
	return ret;
}

//This function is f***'ed
Structure *StabsObject::interpretStructOrUnion (char *strptr)
{
	int size = atoi (strptr);
	Structure *newStruct = new Structure;
	
	strptr = skip_numbers (strptr);
	while (*strptr != ';')
	{	
		string name = getStringUntil (strptr, ':');
		strptr = skip_in_string (strptr, ":");
		
		StabsDefinition *type = getInterpretType (strptr);
		if (!type) {
			return NULL;
		}

		if (type->type == Definition::T_CONFORMANT_ARRAY
			|| (type->type == Definition::T_POINTER && type->pointsTo->type == Definition::T_CONFORMANT_ARRAY))
		{
			strptr = skip_in_string(strptr, "=x");	//skip over 'unknown size' marker
			strptr++;								//skip over 's' or 'u' or 'a'
			strptr = skip_in_string(strptr, ":");	//skip to the numerals
			if(*strptr != ',')
			{
				newStruct->addEntry (
					name,
					-1,
					-1,
					type);
				// TODO: Shouldn't we give some kind of warning here? What exactly is going on?
				
				return newStruct;
			}
		}
		else
			strptr = skip_in_string(strptr, "),");
		
		int bitOffset = atoi(strptr);
		strptr = skip_in_string(strptr, ",");
		
		int bitSize = atoi(strptr);
		strptr = skip_in_string(strptr, ";");
		
		newStruct->addEntry (
			name,
			bitOffset,
			bitSize,
			type
		);
		//_structures.push_back (newStruct);
	}
	return newStruct;
}

Enumerable *StabsObject::interpretEnum (char *strptr)
{
	Enumerable *newEnum = new Enumerable;

	while (*strptr != ';' && *strptr != '\0') {
		std::string name = getStringUntil (strptr, ':');		
		strptr = skip_in_string(strptr, ":");
		int value = atoi(strptr);
		strptr = skip_in_string(strptr, ",");
				
		newEnum->addEntry (name, value);
	}
	//_enums.push_back (newEnum);
	return newEnum;
}

StabsDefinition *StabsObject::getInterpretType (char *strptr)
{
	if (!strptr)
		return 0;

	bool isPointer = FALSE;
//	BOOL isarray = FALSE;
		
	StabsDefinition::Token token = interpretNumberToken (strptr);
	StabsDefinition *type = getTypeFromToken (token);

	if (!type)
	{
		strptr = skip_in_string (strptr, "=");
		
		if (!strptr || *strptr == '\0')
			return NULL;
		
		if (*strptr == '*') {
			isPointer = TRUE;
			strptr++;
		}
		
		if (*strptr == 'a') {
//			isarray = TRUE;
			strptr = skip_in_string(strptr, ")");
			
			if(*strptr == '=' && strptr++ && *strptr == 'r') {
				//skip additional range info
				int i;
				for (i = 0; i < 3; i++)
					strptr = skip_in_string(strptr, ";");
			}
			else if(*strptr == ';') {
				strptr++;
				
				int arrayLowerbound = atoi(strptr);
				strptr = skip_in_string(strptr, ";");
				int arrayUpperbound = atoi(strptr);
				strptr = skip_in_string(strptr, ";");
				int size = arrayUpperbound - arrayLowerbound;
				
				type = getInterpretType (strptr);
				
				StabsDefinition *newType = new StabsDefinition (
					string(),
					token,
					Definition::T_ARRAY,
					size,
					type);
				
				types.push_back (newType);
				type = newType;
			}
		}
		else if (*strptr == 'r')
		{
			Definition::Type simpleType = interpretRange (strptr);
			type = new StabsDefinition (
				string(),
				token,
				simpleType
			);
			
			types.push_back (type);
		}
		else if (*strptr == 's')
		{
			// structure
			Structure *structure = interpretStructOrUnion (++strptr);
			
			type = new StabsDefinition (
				string(),
				token,
				Definition::T_STRUCT,
				0,
				0,
				structure
			);
			types.push_back (type);
		}
		else if (*strptr == 'u')
		{
			// union
			Structure *structure = interpretStructOrUnion (++strptr);
			
			type = new StabsDefinition (
				string(),
				token,
				Definition::T_UNION,
				0,
				0,
				structure
			);
			types.push_back (type);
		}
		else if (*strptr == 'e')
		{
			// enum
			Enumerable *enumerable = interpretEnum(++strptr);
			
			type = new StabsDefinition (
				string(),
				token,
				Definition::T_ENUM,
				0,
				0,
				0,
				enumerable
			);
			types.push_back (type);
		}
		else if(*strptr == 'x')	//unknown size
		{
			strptr++;

			type = new StabsDefinition (
				string(),
				token,
				Definition::T_CONFORMANT_ARRAY
			);
			unknownTypes.push_back (type);
		}
		else
		{
			if(*strptr == '(') //in case of a pointer
			{
				StabsDefinition::Token token2 = interpretNumberToken (strptr);
				if (token == token2)
				{
					// points to itself: to be interpreted as 'void'
					type = new StabsDefinition (
						string(),
						token,
						Definition::T_VOID
					);
					types.push_back (type);
					return type;
				}
				
			}

			type = getInterpretType (strptr);

			if (type)
			{
				StabsDefinition *newType = new StabsDefinition (
					string(),
					token,
					isPointer ? Definition::T_POINTER : type->type,
					0,
					isPointer ? type : (type->pointsTo ? type->pointsTo : type)
				);
				types.push_back (newType);
				type = newType;
			}
		}
	}
	
	if (type) {
		//flushUnknownTypes (type);
	}
	
	return type;
}

SymtabEntry *StabsObject::interpret (char *stabstr, SymtabEntry *stab, uint32_t stabsize)
{
	SymtabEntry *sym = stab;
	
	if (sym->n_type != N_SO) {
		//Failure
		return stab;
	}
	sym++;
	
	//bool inHeader = false;
	Header *header = 0;
	
	//StabsVariable *variable;

	while ((uint32 *)sym < (uint32 *)stab + stabsize)
	{
		switch (sym->n_type)
		{
			case N_SO:
				
				endOffset = MAX(endOffset, sym->n_value);
				if (!strlen (&stabstr[sym->n_strx]))
					sym++;

				wasInterpreted = true;
				
				return sym;

			case N_SOL:
				
				if(string (&stabstr[sym->n_strx]) != name) {
					header = addHeader (string (&stabstr[sym->n_strx]));
				} else {
					header = 0;
				}
				break;

			case N_LSYM: {

				char *strptr = &stabstr[sym->n_strx];

				if (str_is_typedef(strptr)) {
					std::string name = getStringUntil (strptr, ':');
					
					strptr = skip_in_string (strptr, ":");
					if (strptr) {
						struct StabsDefinition *type = getInterpretType (strptr);
						if(type)
							type->name = name;
					}
				}
				break;
			}
			case N_FUN:
			{
				StabsFunction *function = new StabsFunction();
				sym = function->interpret (sym, stabstr, this, &header, (uint32_t)stab + stabsize);
				functions.push_back (function);
			}
		}
		sym++;
	}
	
	wasInterpreted = true;
	
	return sym;
}

string StabsObject::printable()
{
	return Object::printable() + "STABS: " + to_string(stabsOffset) + " INTERPRETED(" + to_string(wasInterpreted) + ")\n";
}

// --------------------------------------------------------------------------------- //

bool StabsModule::interpretGlobals ()
{
	SymtabEntry *sym = (SymtabEntry *)stab;
	StabsObject *object;
	
	// -- Progress::open ("Reading global symbols from symbols table...", _stabsize, 0);

	while ((uint32_t)sym < (uint32_t)stab + stabsize)
	{
		switch (sym->n_type)
		{
			case N_SO:

				object = (StabsObject *)objectFromName (string (&stabstr[sym->n_strx]));
				break;

			case N_GSYM:
			{
				char *strptr = &stabstr[sym->n_strx];
				
				string name = getStringUntil (strptr, ':');
				
				strptr = skip_in_string (strptr, ":");
				StabsDefinition *type = object->getInterpretType (strptr);

				uint32_t address = symbols.valueOf (name);
				// if (!address) do_something?
				
				Variable *variable = new Variable (name, type, Variable::L_ABSOLUTE, address);
				globals.push_back (variable);
			}
			break;
		}
		sym++;
		// -- Progress::updateVal ((int)sym - (int)_stab);
	}
	// -- Progress::close ();
	
	return true;
}

bool StabsModule::initialPass (bool loadEverything)
{
	// -- Progress::open ("Reading symtabs from executable...", _stabsize, 0);
	
	StabsObject *object = 0;
	SymtabEntry *sym = (SymtabEntry *)stab;	
	bool firstTime = true;	
		
	while ((uint32_t)sym < (uint32_t)stab + stabsize) {
		switch (sym->n_type) {
			
			case N_SO: {
				char *strptr = &stabstr[sym->n_strx];
				
				if (strlen(strptr)) {
					if (strptr[strlen(strptr)-1] == '/') //this will happen in executables with multiple static libs. We don't need that info
					{
						sym++;
						continue;
					}
				}
				if(firstTime) {
					addressBegin = sym->n_value;
					firstTime = false;
				}
				
				object = new StabsObject(string(strptr), this, sym->n_value, sym);
				
				objects.push_back (object);
				
				if(loadEverything) {
					sym = object->interpret (stabstr, sym, stabsize);
					// -- Progress::updateVal ((int)sym - (int)_stab);
				}
				else
				{
					//in case we are not loading everything, we need to mark our endings (for some reason)
					
					object->endOffset = sym->n_value;
					addressEnd = MAX(addressEnd, object->endOffset);
					
					sym++;
					// -- Progress::updateVal ((int)sym - (int)_stab);
				}
			break;
			}
			
			case N_FUN:
				 // ???
			
			default:
				sym++;
		}
	}
	// -- Progress::close ();
	
	return true;
}

void StabsModule::readNativeSymbols()
{
	symbols.readAll(elfHandle->nativeHandle());
}

#if 0
TextFile &StabsModule::openLocateSourceFile (std::string initialName)
{
	std::string realName = unix_to_amiga_path_name (initialName);

	TextFile fileHandle (realName);

	if (fileHandle.openAtPath (_sourcePath))
		return fileHandle;
	
	StabsObject *object = objectFromName (initialName);
	if (object && fileHandle.openAtPath (object->sourcePath ()))
		return fileHandle;

	std::string requestedPath = Requester::selectDirectory (_sourcePath, "Please select project root or directory with source code file %s", realName);
	if (filehandle.openAtPath (requestedPath))
		return fileHandle;
	
	return 0;	
}
#endif

StabsModule::StabsModule (string name, ElfHandle *handle)
:	Module(name),
	elfHandle(handle)
{
	addressBegin = 0;
	addressEnd = 0;
	
	elfHandle->performRelocation();
	stabstr = elfHandle->getStabstrSection();
	stab = elfHandle->getStabSection();
	stabsize = elfHandle->getStabsSize();
}

string StabsModule::symPrintable(unsigned char code)
{
	switch(code) {
		case N_UNDF:
			return "N_UNDF";
		case N_GSYM:
			return "N_GSYM";
		case N_FNAME:
			return "N_FNAME";
		case N_FUN:
			return "N_FUN";
		case N_STSYM:
			return "N_STSYM";
		case N_LCSYM:
			return "N_LCSYM";
		case N_MAIN:
			return "N_MAIN";
		case N_ROSYM:
			return "N_ROSYM";
		case N_PC:
			return "N_PC";
		case N_NSYMS:
			return "N_NSYMS";
		case N_NOMAP:
			return "N_NOMAP";
		case N_OBJ:
			return "N_OBJ";
		case N_OPT:
			return "N_OPT";
		case N_RSYM:
			return "N_RSYM";
		case N_M2C:
			return "N_M2C";
		case N_SLINE:
			return "N_SLINE";
		case N_DSLINE:
			return "N_DSLINE";
		case N_BSLINE:
			return "N_BSLINE";
//		case N_BROWS: //same value
//			return "N_BROWS";
		case N_DEFD:
			return "N_DEFD";
		case N_FLINE:
			return "N_FLINE";
		case N_EHDECL:
			return "N_EHDECL";
//		case N_MOD2:
//			return "N_MOD2"; //same value
		case N_CATCH:
			return "N_CATCH";
		case N_SSYM:
			return "N_SSYM";
		case N_ENDM:
			return "N_ENDM";
		case N_SO:
			return "N_SO";
		case N_ALIAS:
			return "N_ALIAS";
		case N_LSYM:
			return "N_LSYM";
		case N_BINCL:
			return "N_BINCL";
		case N_SOL:
			return "N_SOL";
		case N_PSYM:
			return "N_PSYM";
		case N_EINCL:
			return "N_EINCL";
		case N_ENTRY:
			return "N_ENTRY";
		case N_LBRAC:
			return "N_LBRAC";
		case N_EXCL:
			return "N_EXCL";
		case N_SCOPE:
			return "N_SCOPE";
		case N_PATCH:
			return "N_PATCH";
		case N_RBRAC:
			return "N_RBRAC";
		case N_BCOMM:
			return "N_BCOMM";
		case N_ECOMM:
			return "N_ECOMM";
		case N_ECOML:
			return "N_ECOML";
		case N_WITH:
			return "N_WITH";
		case N_NBTEXT:
			return "N_BNTEXT";
		case N_NBDATA:
			return "N_NBDATA";
		case N_NBBSS:
			return "N_BNBSS";
		case N_NBSTS:
			return "N_NBSTS";
		case N_NBLCS:
			return "N_NBLCS";
		case N_LENG:
			return "N_LENG";
	}
	return "<UNNOWN STAB>";
}

string StabsModule::symtabsPrintable()
{
	string result;
	SymtabEntry *sym = (SymtabEntry *)stab;	
	while ((uint32_t)sym < (uint32_t)stab + stabsize) {
		result += symPrintable(sym->n_type) + "\n";
		sym++;
	}
	return result;
}

string StabsModule::printable()
{
	string result = Module::printable();
	result += "ELF: " + elfHandle->getName(); + "\n";
	result += "STABS: stabstr(" + to_string((unsigned int)stabstr) + ") stab(" + to_string((unsigned int)stab) + ") stabsize(" + to_string((unsigned int)stabsize) + ")\n";
	result += symbols.printable();
	result += "GLOBALS LOADED(" + to_string(globalsLoaded) + ")\n";
	return result;
}

// ---------------------------------------------------------------------------------

void StabsInterpreter::clear()
{
	for(list<StabsModule *>::iterator it = modules.begin(); it != modules.end(); it++)
		delete *it;
	modules.clear();
}

bool StabsInterpreter::loadModule (ElfHandle *handle)
{	
//	bool success = handle->performRelocation();
/*	
	if (!success) {
		printf("Failed to perform relocation on oshandle. Stabs symbols will not be available.\n");
		return false;
	}
*/	
	bool closeElf = false;

	StabsModule *module = new StabsModule(handle->getName(), handle);
	modules.push_back (module);
	
	bool loadAll = false;
	
	int value = 1;
	switch(value) {
		case 1: //SETTINGS_LoadCompleteModule:
			closeElf = true;
			loadAll = true;
			module->initialPass (loadAll);
			break;
			
		case 2: //SETTINGS_LoadObjectLists:
			module->initialPass (loadAll);
			break;
		
		case 3: //SETTINGS_LoadOnlyHeaders:
			break;
	}
	module->readNativeSymbols();
	module->interpretGlobals ();
	
	if(false) { //closeElf) {
		handle->close();
	}

	return true;
}

StabsModule *StabsInterpreter::moduleFromName (string moduleName) {
	for (list <StabsModule *>::iterator it = modules.begin(); it != modules.end(); it++)
		if (!(*it)->name.compare (moduleName))
			return *it;
	return 0;
}

StabsModule *StabsInterpreter::moduleFromAddress (uint32_t address) {
	for (list <StabsModule *>::iterator it = modules.begin(); it != modules.end(); it++)
		if (address >= (*it)->addressBegin && address <= (*it)->addressEnd)
			return *it;
	return 0;
}

StabsObject *StabsInterpreter::objectFromAddress (uint32_t address) {
	for (list <StabsModule *>::iterator it = modules.begin(); it != modules.end(); it++)
		if(StabsObject *o = (StabsObject *)(*it)->objectFromAddress (address))
			return o;
	return 0;
}

string StabsInterpreter::printable()
{
	string result = "MODULES:\n";
	for(list<StabsModule *>::iterator it = modules.begin(); it != modules.end(); it++)
		result += (*it)->printable();
	return result;
}