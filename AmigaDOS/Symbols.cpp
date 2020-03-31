//
//
// nativesymbols.cpp - native symbols handling by elf.libary (Debug 101)
//
//

#include <proto/elf.h>
#include "Symbols.hpp"

#include <sstream>
#include <string>
#include <string.h>

using namespace std;

ULONG amigaos_symbols_callback(struct Hook *hook, struct Task *task, struct SymbolMsg *symbolmsg) {
	if (symbolmsg->Name) {
		ElfSymbols *symbols = (ElfSymbols *)hook->h_Data;
		symbols->addSymbol (string ((const char *)symbolmsg->Name), symbolmsg->AbsValue);
	}
	return 1;
}

ElfSymbols::ElfSymbols() {
}

ElfSymbols::~ElfSymbols() {
	clear ();
}

void ElfSymbols::clear() {
	for (list<ElfSymbol *>::iterator it = symbols.begin (); it != symbols.end (); it++)
		delete (*it);
}

void ElfSymbols::readAll(APTR handle) {
	IElf->OpenElfTags(OET_ElfHandle, handle, TAG_DONE);
	
	struct Hook hook;
	hook.h_Entry = (ULONG (*)())amigaos_symbols_callback;
	hook.h_Data =  this;

	IElf->ScanSymbolTable((Elf32_Handle)handle, &hook, NULL);

//    IElf->CloseElfTags((Elf32_Handle)handle, CET_ReClose, FALSE, TAG_DONE);
	loaded = true;
}

void ElfSymbols::addSymbol(string name, uint32 value) {
	ElfSymbol *symbol = new ElfSymbol(name, value);
	symbols.push_back(symbol);
}

uint32 ElfSymbols::valueOf (string name) {
	for (list <ElfSymbol *>::iterator it = symbols.begin (); it != symbols.end (); it++)
		if (!(*it)->name.compare(name))
			return (*it)->value;
	return 0; //we should at least throw something in this case ??
}

string ElfSymbols::nameFromValue(uint32 value) {
	for (list <ElfSymbol *>::iterator it = symbols.begin (); it != symbols.end (); it++)
		if ((*it)->value == value)
			return (*it)->name;
	return string(); //throw something in this case too ??
}

vector<string> ElfSymbols::printable(){
    vector<string> result;
	for (list<ElfSymbol *>::iterator it = symbols.begin (); it != symbols.end (); it++) {
		stringstream str;
        str << (*it)->name << ": 0x" << (void *)(*it)->value << "\n";
		result.push_back(str.str());
	}
	return result;
}