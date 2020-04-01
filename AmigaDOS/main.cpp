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

	cout << "> ";

	string command;
	getline(cin, command);
	split2(command, cmdArgs);

	return cmdArgs;
}

// ---------------------------------------------------------------------------- //

class Debugger {
private:
	AmigaProcess process;
    ElfSymbols symbols;
    Breaks breaks;

	ElfHandle *handle = 0;
	Binary *binary = 0;

public:
	Debugger() {
	}
	~Debugger() {
		if(handle) delete handle;
		if(binary) delete binary;
	}
	void open(APTR _handle, string name) {
		handle = new ElfHandle(_handle, name);
		handle->performRelocation();

		symbols.readAll(handle);

		binary = new Binary(handle->getName(), (SymtabEntry *)handle->getStabSection(), handle->getStabstrSection(), handle->getStabsSize());
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
		process.step();

		breaks.activate();
		process.go();
		process.wait();
		breaks.deactivate();

		cout << "At: " << location() << "\n";
		return handleMessages();
	}
	void skip() {
		process.skip();
	}
	bool step() {
		process.step();
		return handleMessages();
	}
	bool stepOver() {
		Breaks linebreaks;
		Function *function = binary->getFunction(process.ip());
		for(int i = 0; i < function->lines.size(); i++) {
			linebreaks.insert(function->address + function->lines[i]->address);
		}
		process.step();

		breaks.activate();
		linebreaks.activate();

		process.go();
		process.wait();

		linebreaks.deactivate();
		breaks.deactivate();

		cout << "At: " << location() << "\n";
		return handleMessages();
	}
	bool stepInto() {
		bool atLine = false;
		while(!atLine) {
			process.step();
			atLine = binary->getLocation(process.ip()).size() > 0;
		}
		cout << "At: " << location() << "\n";
		return handleMessages();
	}
	bool stepOut() {
		Breaks outBreak;
		outBreak.insert(process.lr());

		process.step();

		breaks.activate();
		outBreak.activate();

		process.go();
		process.wait();

		outBreak.deactivate();
		breaks.deactivate();

		cout << "At: " << location() << "\n";
		return handleMessages();
	}
	vector<string> context() {
		return binary->getContext(process.ip(), process.sp());
	}
	vector<string> globals() {
		return binary->getGlobals();
	}
	uint32_t getIp() {
		return process.ip();
	}
	uint32_t getSp() {
		return process.sp();
	}
	string location() {
		return binary->getLocation(process.ip());
	}
};

int main(int argc, char *argv[])
{
	Debugger debugger;

	bool exit = false;
	while(!exit) {
		vector<string> args = getInput();

		exit = debugger.handleMessages();
		if (exit) break;

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

			case 'v':
				cout << "sp : 0x" << (void *)debugger.getSp() << "\n";
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
				vector<string> symbols = debugger.context();
				int indent = 0;
				for(int i = 0; i < symbols.size(); i++) {
					astream str(symbols[i]);
					if(str.endsWith('}')) indent--;
					for(int j = 0; j < indent; j++)
						cout << "\t";
					if(str.endsWith('{')) indent++;
                	cout << symbols[i] << "\n";
				}
                break;
			}

			case 'g': { // write global symbols
				vector<string> symbols = debugger.globals();
				for(int i = 0; i < symbols.size(); i++)
                	cout << symbols[i] << "\n";
                break;
			}

			case '1': // step over
				debugger.stepOver();
				break;

			case '2': // step into
				debugger.stepInto();
				break;

			case '3': // step out
				debugger.stepOut();
				break;

			case 'q':
				exit = true;
				break;

			case 'h':
				cout << "-- SimpleDebug --\n";
				cout << "\n";
				cout << "l <file> <args>: load child from file\n";
				cout << "a <name>: attach to process in memory\n";
				cout << "d: detach from child\n";
				cout << "\n";
				cout << "n: print source file names\n";
				cout << "i: print instruction pointer\n";
				cout << "o: print os symbol list\n";
                cout << "p: print binary structure\n";
				cout << "\n";
				cout << "b <file> <line>: insert breakpoint\n";
				cout << "b <function>: insert breakpoint\n";
				cout << "c: clear breakpoint\n";
				cout << "\n";
				cout << "s: start execution\n";
				cout << "k: skip instruction\n";
				cout << "z: execute instruction\n";
				cout << "\n";
				cout << "w: write context data\n";
				cout << "g: write global symbols data\n";
				cout << "\n";
				cout << "1: step over\n";
				cout << "2: step into\n";
				cout << "3: step out\n";
				cout << "\n";
				cout << "q: quit debugger\n";
				break;

			default:
				break;
		}
	}
    return 0;
}