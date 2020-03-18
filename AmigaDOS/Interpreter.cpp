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

SymtabEntry *StabsFunction::interpretFromSymtabs (SymtabEntry *sym, char *stabstr, Object *object, Header **header, uint32_t endOfSymtab)   
{
	while (!sym->n_strx) {
		sym++;
	}
	
	int brac = 0;
	int size = 0;
	
	name = getStringUntil (&stabstr[sym->n_strx], ':'));
	
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
				Definition *type = object->getInterpretType (strptr);
	
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



StabsTypeDefinition::StabsSimpleType StabsObject::interpretRange (char *strptr)
{
	if (*strptr != 'r')
		return StabsTypeDefinition::T_UNKNOWN;
	strptr = skip_in_string (strptr, ";");
	if (!strptr)
		return StabsTypeDefinition::T_UNKNOWN;
	
	long long lower_bound = atoi (strptr);
	strptr = skip_in_string (strptr, ";");
	long long upper_bound = atoi (strptr);
	
	StabsTypeDefinition::StabsSimpleType ret = StabsTypeDefinition::T_UNKNOWN;
	
	if (lower_bound == 0 && upper_bound == -1)
		;
	else if (lower_bound > 0 && upper_bound == 0)
	{
		switch(lower_bound)
		{
			case 4:
				ret = StabsTypeDefinition::T_FLOAT32;
				break;
			case 8:
				ret = StabsTypeDefinition::T_FLOAT64;
				break;
			case 16:
				ret = StabsTypeDefinition::T_FLOAT128;
				break;
			default:
				ret = StabsTypeDefinition::T_UNKNOWN;
				break;
		}
	}
	else
	{
		long long range = upper_bound - lower_bound;
		if(lower_bound < 0)
		{
			if(range <= 0xff)
				ret = StabsTypeDefinition::T_8;
			else if(range <= 0xffff)
				ret = StabsTypeDefinition::T_16;
			else if(range <= 0xffffffff)
				ret = StabsTypeDefinition::T_32;
			//else if(range <= 0xffffffffffffffff)
			//	ret = T_64;
			//else if(range <= 0xffffffffffffffffffffffffffffffff)
			//	ret = T_128;
			else
				ret = StabsTypeDefinition::T_UNKNOWN;
		}
		else // if unsigned
		{
			if(range <= 0xff)
				ret = StabsTypeDefinition::T_U8;
			else if(range <= 0xffff)
				ret = StabsTypeDefinition::T_U16;
			else if(range <= 0xffffffff)
				ret = StabsTypeDefinition::T_U32;
			//else if(range <= 0xffffffffffffffff)
			//	ret = T_U64;
			//else if(range <= 0xffffffffffffffffffffffffffffffff)
			//	ret = T_U128;
			else
				ret = StabsTypeDefinition::T_UNKNOWN;
		}
	}
	return ret;
}

//This function is f***'ed
StabsStructureType *StabsObject::interpretStructOrUnion (char *strptr)
{
	int size = atoi (strptr);
	StabsStructureType *newStruct = new StabsStructureType (size);
	
	strptr = skip_numbers (strptr);
	while (*strptr != ';')
	{	
		std::string name = getStringUntil (strptr, ':');
		strptr = skip_in_string (strptr, ":");
		
		StabsTypeDefinition *type = getInterpretType (strptr);
		if (!type) {
			return NULL;
		}

		if (type->_simpleType == StabsTypeDefinition::T_CONFORMANT_ARRAY
			|| (type->_simpleType == StabsTypeDefinition::T_POINTER && type->_pointsToType->_simpleType == StabsTypeDefinition::T_CONFORMANT_ARRAY))
		{
			strptr = skip_in_string(strptr, "=x");	//skip over 'unknown size' marker
			strptr++;								//skip over 's' or 'u' or 'a'
			strptr = skip_in_string(strptr, ":");	//skip to the numerals
			if(*strptr != ',')
			{
				newStruct->addNewEntry (
					name,
					-1,
					-1,
					type
				);
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
		
		newStruct->addNewEntry (
			name,
			bitOffset,
			bitSize,
			type
		);
		//_structures.push_back (newStruct);
	}
	return newStruct;
}

StabsEnumType *StabsObject::interpretEnum (char *strptr)
{
	StabsEnumType *newEnum = new StabsEnumType;

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

StabsTypeDefinition *StabsObject::getInterpretType (char *strptr)
{
	if (!strptr)
		return 0;

	BOOL isPointer = FALSE;
//	BOOL isarray = FALSE;
		
	StabsTypeDefinition::TypeToken typeToken = interpretNumberToken (strptr);
	StabsTypeDefinition *type = getTypeFromToken (typeToken);

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
				
				StabsTypeDefinition *newType = new StabsTypeDefinition (
					std::string(),
					typeToken,
					StabsTypeDefinition::T_ARRAY,
					size,
					type);
				
				_types.push_back (newType);
				type = newType;
			}
		}
		else if (*strptr == 'r')
		{
			StabsTypeDefinition::StabsSimpleType simpleType = interpretRange (strptr);
			type = new StabsTypeDefinition (
				std::string(),
				typeToken,
				simpleType
			);
			
			_types.push_back (type);
		}
		else if (*strptr == 's')
		{
			// structure
			StabsStructureType *structureType = interpretStructOrUnion (++strptr);
			
			type = new StabsTypeDefinition (
				std::string(),
				typeToken,
				StabsTypeDefinition::T_STRUCT,
				0,
				0,
				structureType
			);
			_types.push_back (type);
		}
		else if (*strptr == 'u')
		{
			// union
			StabsStructureType *structureType = interpretStructOrUnion (++strptr);
			
			type = new StabsTypeDefinition (
				std::string(),
				typeToken,
				StabsTypeDefinition::T_UNION,
				0,
				0,
				structureType
			);
			_types.push_back (type);
		}
		else if (*strptr == 'e')
		{
			// enum
			StabsEnumType *enumType = interpretEnum(++strptr);
			
			type = new StabsTypeDefinition (
				std::string(),
				typeToken,
				StabsTypeDefinition::T_ENUM,
				0,
				0,
				0,
				enumType
			);
			_types.push_back (type);
		}
		else if(*strptr == 'x')	//unknown size
		{
			strptr++;

			type = new StabsTypeDefinition (
				std::string(),
				typeToken,
				StabsTypeDefinition::T_CONFORMANT_ARRAY
			);
			_unknownTypes.push_back (type);
		}
		else
		{
			if(*strptr == '(') //in case of a pointer
			{
				StabsTypeDefinition::TypeToken typeToken2 = interpretNumberToken (strptr);
				if (typeToken == typeToken2)
				{
					// points to itself: to be interpreted as 'void'
					type = new StabsTypeDefinition (
						std::string(),
						typeToken,
						StabsTypeDefinition::T_VOID
					);
					_types.push_back (type);
					return type;
				}
				
			}

			type = getInterpretType (strptr);

			if (type)
			{
				StabsTypeDefinition *newType = new StabsTypeDefinition (
					std::string(),
					typeToken,
					isPointer ? StabsTypeDefinition::T_POINTER : type->_simpleType,
					0,
					isPointer ? type : (type->_pointsToType ? type->_pointsToType : type)
				);
				_types.push_back (newType);
				type = newType;
			}
		}
	}
	
	if (type) {
		//flushUnknownTypes (type);
	}
	
	return type;
}

SymtabEntry *StabsObject::interpretFromSymtabs (char *stabstr, SymtabEntry *stab, uint32 stabsize)
{
	SymtabEntry *sym = stab;
	
	if (sym->n_type != N_SO) {
		Console::printToConsole (Console::OUTPUT_WARNING, "StabsObject: First symbol must be N_SO.");
		return stab;
	}
	sym++;
	
	//bool inHeader = false;
	StabsHeader *header = 0;
	
	//StabsVariable *variable;

	while ((uint32 *)sym < (uint32 *)stab + stabsize)
	{
		switch (sym->n_type)
		{
			case N_SO:
				
				_endOffset = MAX(_endOffset, sym->n_value);
				if (!strlen (&stabstr[sym->n_strx]))
					sym++;

				_interpreted = true;
				
				return sym;

			case N_SOL:
				
				if(std::string (&stabstr[sym->n_strx]) != fileName ()) {
					header = addHeader (std::string (&stabstr[sym->n_strx]));
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
						struct StabsTypeDefinition * type = getInterpretType (strptr);
						if(type)
							type->_typeName = name;
					}
				}
				break;
			}
			case N_FUN:
			{
				StabsFunction *function = new StabsFunction;
				sym = function->interpretFromSymtabs (sym, stabstr, this, &header, (uint32)stab + stabsize);
				_functions.push_back (function);
			}
		}
		sym++;
	}
	
	_interpreted = true;
	
	return sym;
}

bool StabsModule::interpretGlobals ()
{
	SymtabEntry *sym = (SymtabEntry *)_stab;
//	StabsVariable *variable;
	StabsObject *object;
	
	Progress::open ("Reading global symbols from symbols table...", _stabsize, 0);

	while ((uint32)sym < (uint32)_stab + _stabsize)
	{
		switch (sym->n_type)
		{
			case N_SO:

				object = objectFromName (std::string (&_stabstr[sym->n_strx]));
				break;

			case N_GSYM:
			{
				//bool keepopen = Db101Preferences::getValue("PREFS_KEEP_ELF_OPEN").bool();
			
				//if (!object->hasBeenInterpreted ())
				//	object->load (_handle.elfHandle ());
			//reset keep open??

				char *strptr = &_stabstr[sym->n_strx];
				
				std::string name = getStringUntil (strptr, ':');
				
				strptr = skip_in_string (strptr, ":");
				StabsTypeDefinition *type = object->getInterpretType (strptr);

				uint32 address = _nativeSymbols.valueOf (name);
				// if (!address) do_something?
				
				StabsVariable *variable = new StabsVariable (name, type, StabsVariable::L_ABSOLUTE, address);
				_globals.push_back (variable);
			}
			break;
		}
		sym++;
		Progress::updateVal ((int)sym - (int)_stab);
	}
	Progress::close ();
	
	return true;
}


bool StabsModule::loadInterpretObject (StabsObject *object)
{
	bool closeElf = true;
	
	if(Settings::getValue ("PREFS_KEEP_ELF_OPEN").asBool ())
		closeElf = false;
	
	_nativeSymbols.dummy (elfHandle);
	elfHandle->performRelocation ();

/*	Elf32_Handle elfhandle = sourcefile->module->elfhandle;
	if(!elfisopen)
	{
		elfhandle = open_elfhandle(elfhandle);
		sourcefile->module->elfhasbeenopened = TRUE;
		symbols_dummy(elfhandle);
		relocate_elfhandle(elfhandle);
	}*/
	
	_stabstr = elfHandle->getStabstrSection ();
	_stab = elfHandle->getStabSection ();
	
	if (!_stab) {
		Console::printToConsole (Console::OUTPUT_WARNING, "Failed to open stabs section for module %s", _name.c_str());
		return false;
	}
	
	_stabsize = elfHandle->getStabsSize ();
	
	object->interpretFromSymtabs (_stabstr, object->stabsOffset(), _stabsize);

	if (closeElf) {
		elfHandle->close ();
	}
	
	return true;
}

bool StabsModule::initialPass (bool loadEverything)
{
	Progress::open ("Reading symtabs from executable...", _stabsize, 0);
	
	StabsObject *object = 0;
	SymtabEntry *sym = (SymtabEntry *)_stab;	
	bool firstTime = true;	
	
	while ((uint32)sym < (uint32)_stab + _stabsize) {
		
		switch (sym->n_type) {
			
			case N_SO: {
				char *strptr = &_stabstr[sym->n_strx];
				
				if (strlen(strptr)) {
					if (strptr[strlen(strptr)-1] == '/') //this will happen in executables with multiple static libs. We don't need that info
					{
						sym++;
						continue;
					}
				}
				if(firstTime) {
					_addressBegin = sym->n_value;
					firstTime = false;
				}
				
				object = new StabsObject (std::string (strptr), this, sym, sym->n_value);
				
				_objects.push_back (object);
				
				if(loadEverything) {
					sym = object->interpretFromSymtabs (_stabstr, sym, _stabsize);
					Progress::updateVal ((int)sym - (int)_stab);
				}
				else
				{
					//in case we are not loading everything, we need to mark our endings (for some reason)
					
					object->setEndOffset (sym->n_value);
					_addressEnd = MAX(_addressEnd, object->endOffset());
					
					sym++;
					Progress::updateVal ((int)sym - (int)_stab);
				}
			break;
			}
			
			case N_FUN:
				 // ???
			
			default:
				sym++;
		}
	}
	Progress::close ();
	
	return true;
}


void StabsModule::readNativeSymbols()
{
	_nativeSymbols.readNativeSymbols (elfHandle);
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
StabsModule::StabsModule (std::string name, ElfHandle *handle)
	: _name(name),
	elfHandle(handle)
{
	_addressBegin = 0;
	_addressEnd = 0;
	
	_stabstr = elfHandle->getStabstrSection ();
	_stab = elfHandle->getStabSection ();
	_stabsize = elfHandle->getStabsSize ();
}

StabsInterpreter::StabsInterpreter()
{
	setName("STABS_INTERPRETER");
}

void StabsInterpreter::clear()
{
	for(list<StabsModule *>::iterator it = _modules.begin(); it != _modules.end(); it++)
		delete *it;
	_modules.clear();
}

bool StabsInterpreter::loadModule (OSHandle *osHandle)
{
	Console::printToConsole (Console::OUTPUT_SYSTEM, "Load module %s", osHandle->name().c_str());
	
	if (osHandle->format() != OSHandle::FORMAT_Elf) {
		Console::printToConsole (Console::OUTPUT_WARNING, "Application is not an elf object. Stabs will not be available.");
		return false;
	}
	
	ElfHandle *elfHandle = (ElfHandle *)osHandle;
	
	//What is this?
	//_nativeSymbols.dummy (elfHandle);
	
	bool success = elfHandle->performRelocation();
	
	if (success) {
		Console::printToConsole (Console::OUTPUT_WARNING, "Failed to perform relocation on oshandle. Stabs symbols will not be available.");
		return false;
	}
	
	bool closeElfHandles = false;
	if (!Settings::getValue ("PREFS_ALL_ELF_HANDLES_OPEN").asBool()) {
		closeElfHandles = true;
	}	

	StabsModule *module = new StabsModule (elfHandle->name(), elfHandle); //'true' for 'elf handle has been opened'
	_modules.push_back (module);
	
	bool loadEverything = false;
	
	SettingsObject value = Settings::getValue ("PREFS_LOAD_EVERYTHING_AT_ENTRY");
	switch (value.numerical()) {
		case SETTINGS_LoadCompleteModule:
			closeElfHandles = true;
			loadEverything = true;
			module->initialPass (loadEverything);
			break;
			
		case SETTINGS_LoadObjectLists:
			module->initialPass (loadEverything);
			break;
		
		case SETTINGS_LoadOnlyHeaders:
			break;
	}
	module->readNativeSymbols();

	if (Settings::getValue ("AUTO_LOAD_GLOBALS").asBool()) {
		module->interpretGlobals ();
	}
	
	if(closeElfHandles) {
		elfHandle->close();
	}

	return true;
}
