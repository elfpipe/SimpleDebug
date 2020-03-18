//
//
// nativesymbols.cpp - native symbols handling by elf.libary (Debug 101)
//
//

#include <proto/elf.h>
#include "OSSymbols.hpp"

#include <sstream>
#include <string>
#include <string.h>

using namespace std;

ULONG amigaos_symbols_callback(struct Hook *hook, struct Task *task, struct SymbolMsg *symbolmsg) {
	if (symbolmsg->Name) {
		OSSymbols *nativeSymbols = (OSSymbols *)hook->h_Data;
		nativeSymbols->addSymbol (string ((const char *)symbolmsg->Name), symbolmsg->AbsValue);
	}
	return 1;
}

OSSymbols::OSSymbols() {
}

OSSymbols::~OSSymbols() {
	clear ();
}

void OSSymbols::clear() {
	for (list<OSSymbol *>::iterator it = symbols.begin (); it != symbols.end (); it++)
		delete (*it);
}

void OSSymbols::readNativeSymbols(APTR handle) {
	IElf->OpenElfTags(OET_ElfHandle, handle, TAG_DONE);
	
	struct Hook hook;
	hook.h_Entry = (ULONG (*)())amigaos_symbols_callback;
	hook.h_Data =  this;

	IElf->ScanSymbolTable((Elf32_Handle)handle, &hook, NULL);

//    IElf->CloseElfTags((Elf32_Handle)handle, CET_ReClose, FALSE, TAG_DONE);
	symbolsLoaded = true;
}

void OSSymbols::addSymbol(string name, uint32 value) {
	OSSymbol *symbol = new OSSymbol(name, value);
	symbols.push_back(symbol);
}

uint32 OSSymbols::valueOf (string name) {
	for (list <OSSymbol *>::iterator it = symbols.begin (); it != symbols.end (); it++)
		if (!(*it)->name.compare(name))
			return (*it)->value;
	return 0; //we should at least throw something in this case ??
}

string OSSymbols::nameFromValue(uint32 value) {
	for (list <OSSymbol *>::iterator it = symbols.begin (); it != symbols.end (); it++)
		if ((*it)->value == value)
			return (*it)->name;
	return string(); //throw something in this case too ??
}

string OSSymbols::printableList() {
    stringstream str;
	for (list<OSSymbol *>::iterator it = symbols.begin (); it != symbols.end (); it++)
        str << (*it)->name << ": 0x" << (void *)(*it)->value << "\n";
    return str.str();
}

void OSSymbols::dummy(APTR handle) //32_Handle elfhandle)
{
	IElf->OpenElfTags(OET_ReadOnlyCopy, TRUE, OET_ElfHandle, handle, TAG_DONE);

	struct Elf32_SymbolQuery query;
	STRPTR tempBuffer = (char *)"(dummy)";
    
	query.Flags      = ELF32_SQ_BYNAME | ELF32_SQ_LOAD;
	query.Name       = tempBuffer;
	query.NameLength = strlen((const char *)tempBuffer);
	query.Value      = 0;

	/*uint32 queryres =*/ IElf->SymbolQuery((Elf32_Handle)handle, 1, &query);

    IElf->CloseElfTags((Elf32_Handle)handle, CET_ReClose, FALSE, TAG_DONE);
}
