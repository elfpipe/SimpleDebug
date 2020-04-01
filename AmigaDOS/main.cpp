#include "Symbols.hpp"
#include "Process.hpp"
#include "Breaks.hpp"

#include "Binary.hpp"
#include "Handle.hpp"

#include <iostream>
#include <sstream>
#include <string>
#include <algorithm>
#include <iterator>
#include <vector>
#include <string.h>

#include <proto/dos.h>

using namespace std;

template <class Container>
void split2(const string& str, Container& cont, char delim = ' ')
{
    stringstream ss(str);
    string token;
    while (getline(ss, token, delim)) {
        cont.push_back(token);
    }
}

string concat(vector<string> strs, int index)
{
	string result;
	for (int i = index; i < strs.size(); i++)
		result.append(strs[i]);
	return result;
}

// ---------------------------------------------------------------------------- //

vector<string> getInput()
{
	vector<string> cmdArgs;
	
	while(1) {
		cout << "> ";

		string command;
		getline(cin, command);
		split2(command, cmdArgs);

//		if(cmdArgs[0].length() == 1)
			break;
	}

	return cmdArgs;
}

// ---------------------------------------------------------------------------- //

class Debugger {
private:
	AmigaDOSProcess process;
    ElfSymbols symbols;
    Breaks breaks;

	ElfHandle *elfHandle = 0;
	Binary *binary = 0;

public:
	Debugger() {
	}
	void open(APTR handle, string name) {
		symbols.readAll(handle);

		elfHandle = new ElfHandle(handle, name);
		elfHandle->performRelocation();

		binary = new Binary(elfHandle->getName(), (SymtabEntry *)elfHandle->getStabSection(), elfHandle->getStabstrSection(), elfHandle->getStabsSize());
	}
	bool load(string file, string args) {
		APTR handle = process.load("", file, args);
		if (handle) open(handle, file);
		return handle != 0;
	}
	bool attach(string name) {
		APTR handle = process.attach(name);
		if(handle) open(handle, name);
		return handle != 0;
	}
	void detach() {
		process.detach();
	}
	bool handleMessages() {
		return process.handleMessages();
	}
	vector<string> sourceFiles() {
		return binary->getSourceNames();
	}
	vector<string> elfSymbols() {
		return symbols.printable();
	}
	string binaryStructure() {
		return binary->toString();
	}
	bool breakpoint(string file, int line, bool set) {
		uint32_t address = binary->getLineAddress(file, line);
		if(address)	set ? breaks.insert(address) : breaks.remove(address);
		return address != 0;
	}
	bool breakpoint(string function, bool set) {
		uint32_t address = binary->getFunctionAddress(function);
		if(address)	set ? breaks.insert(address) : breaks.remove(address);
		return address != 0;
	}
	bool start() {
		bool exit = false;

		process.step();
		exit = handleMessages(); //?

		breaks.activate();
		process.go();
		process.wait();
		breaks.suspend();

		exit = handleMessages();
		return exit;
	}
	void skip() {
		process.skip();
	}
	bool step() {
		process.step();
		return handleMessages();
	}
	void stepOver() {

	}
	void stepInto() {

	}
	void stepOut() {

	}
	vector<string> printContext() {
		vector<string> result;
		Function *function = binary->getFunction(process.ip());
		for(int i = 0; i < function->params.size(); i++) {
			string value = function->params[i]->toString(); //valueString(process.sp());
			result.push_back("<P>" + function->params[i]->name + " : " + value);
		}

		Scope *scope = function->locals[0];
		if(scope) scope = scope->getScope(process.ip());
		while(scope) {
			for(int i = 0; i < scope->children.size(); i++) {
				for(int j = 0; j < scope->symbols.size(); j++) {
					Symbol *symbol = scope->symbols[i];
					string value = symbol->toString(); //valueString(process.sp());
					result.push_back(symbol->name + " : " + value);
				}
			}
			scope = scope->parent;
		}
		return result;
	}
	vector<string> printGlobals() {
		vector<string> result;
		for(int i = 0; i < binary->objects.size(); i++) {
			SourceObject *object = binary->objects[i];
			for(int j = 0; j < object->globals.size(); j++) {
				Symbol *symbol = object->globals[j];
				if(symbol->symType == Symbol::S_Global) {
					string value = symbol->toString(); //valueString(0x0);
					result.push_back("<G>" + symbol->name + " : " + value);
				}
			}
		}
		return result;
	}
	uint32_t getIp() {
		return process.ip();
	}
};

int main(int argc, char *argv[])
{
	Debugger debugger;

	bool exit = false;
	while(!exit) {
		vector<string> args = getInput();

		exit = debugger.handleMessages();

		if(args.size() > 0) 
		switch(char c = args[0][0]) {
			case 'l': {
				bool success = false;
				if(args.size() < 2)
					cout << "Missing argument(s)\n";
				else if(args.size() >= 3)
					success = debugger.load(args[1], concat(args, 2));
				else
					success = debugger.load(args[1], "");
				if(success) cout << "Process loaded\n";
			}
			break;

			case 'a': {
				if(args.size() < 2) {
					cout << "Missing argument\n";
				} else {
					if(debugger.attach(args[1]))
						cout << "Attached to process\n";
				}
			}
			break;

			case 'd':
				cout << "Detach\n";
				debugger.detach();
				break;

			case 'n': {
				cout << "-- Source files: --\n";
				vector<string> names = debugger.sourceFiles();
				for(int i = 0; i < names.size(); i++)
					cout << names[i] << "\n";
				break;
			}

			case 'i':
				cout << "ip: " << (void *)debugger.getIp() << "\n";
				break;

            case 'o': {
                cout << "--Elf symbols:--\n";
				vector<string> symbols = debugger.elfSymbols();
				for(int i = 0; i < symbols.size(); i++)
                	cout << symbols[i] << "\n";
                break;
			}
			case 'p':
				cout << "--Binary structure:--\n";
				cout << debugger.binaryStructure();
				break;

			case 'b':
			case 'c':
				if(args.size() < 2) {
					cout << "Missing argument";
				} else if (args.size() == 2) {
					if(debugger.breakpoint(args[1], c == 'b'))
						cout << "Breakpoint set\n";
					else
						cout << "Breakpoint not set\n";
				} else {
					astream str(args[2]);
					int line = str.getInt();
					if(debugger.breakpoint(args[1], line, c == 'b'))
						cout << "Breakpoint set\n";
					else
						cout << "Breakpoint not set\n";
				}
				break;
				
			case 's': {
				cout << "Start\n";
				exit = debugger.start();
				break;
			}

			case 'k':
				cout << "Skip\n";
				debugger.skip();
				break;

			case 'z':
				cout << "Step\n";
				exit = debugger.step();
				break;

			case 'w': { // write context data
				vector<string> symbols = debugger.printContext();
				for(int i = 0; i < symbols.size(); i++)
                	cout << symbols[i] << "\n";
                break;
			}

			case 'g': { // write global symbols
				vector<string> symbols = debugger.printGlobals();
				for(int i = 0; i < symbols.size(); i++)
                	cout << symbols[i] << "\n";
                break;
			}

			case '1': // step over
			case '2': // step into
			case '3': //step out
				break;

			case 'q':
				exit = true;
				break;

			case 'h':
				cout << "-- SimpleDebug --\n";

				cout << "l <file> <args>: load child from file\n";
				cout << "a <name>: attach to process in memory\n";
				cout << "d: detach from child\n";

				cout << "n: print source file names\n";
				cout << "i: print instruction pointer\n";
				cout << "o: print os symbol list\n";
                cout << "p: print binary structure\n";

				cout << "b <file> <line>: insert breakpoint\n";
				cout << "b <function>: insert breakpoint\n";
				cout << "c: clear breakpoint\n";

				cout << "s: start execution\n";
				cout << "k: skip instruction\n";
				cout << "z: execute instruction\n";

				cout << "w: write context data\n";
				cout << "g: write global symbols data\n";

				cout << "1: step over\n";
				cout << "2: step into\n";
				cout << "3: step out\n";

				cout << "q: quit debugger\n";
				break;

			default:
				break;
		}
	}
    return 0;
}