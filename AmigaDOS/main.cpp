#include "OSSymbols.hpp"
#include "Process.hpp"
#include "Breaks.hpp"

#include "Interpreter.hpp"

#include <iostream>
#include <sstream>
#include <string>
#include <algorithm>
#include <iterator>
#include <vector>
#include <string.h>

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

bool handleMessages(struct MsgPort *port, AmigaDOSProcess *handler) {
	bool exit = false;
	struct AmigaDOSProcess::DebugMessage *message = (struct AmigaDOSProcess::DebugMessage *)IExec->GetMsg(port);
	while(message) {
		switch(message->type) {
			case AmigaDOSProcess::MSGTYPE_EXCEPTION:
				cout << "==EXCEPTION (ip = 0x" << (void *)handler->ip() << ")\n";
				break;

			case AmigaDOSProcess::MSGTYPE_TRAP:
				cout << "==TRAP (ip = 0x" << (void *)handler->ip() << ")\n";
				break;

			case AmigaDOSProcess::MSGTYPE_CRASH:
				cout << "==CRASH (ip = 0x" << (void *)handler->ip() << ")\n";
				break;

			case AmigaDOSProcess::MSGTYPE_OPENLIB:
				cout << "==OPENLIB\n";
				break;

			case AmigaDOSProcess::MSGTYPE_CLOSELIB:
				cout << "==CLOSELIB\n";
				break;

			case AmigaDOSProcess::MSGTYPE_CHILDDIED:
				cout << "Child has DIED (exit)\n";
				exit = true;
				break;
		}
		message = (struct AmigaDOSProcess::DebugMessage *)IExec->GetMsg(port);
	}
	return exit;
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

int main(int argc, char *argv[])
{
	struct MsgPort *port = (struct MsgPort *)IExec->AllocSysObject(ASOT_PORT, TAG_DONE);

	AmigaDOSProcess handler(port);
    OSSymbols symbols;
    Breaks breaks;

	StabsInterpreter interpreter;

	ElfHandle *elfHandle = 0;

	handler.init();

	bool exit = false;
	while(!exit) {
		vector<string> args = getInput();

		exit = handleMessages(port, &handler);

		if(args.size() > 0) 
		switch(args[0][0]) {
			case 'l': {
				string shellArgs;
				if(args.size() >= 3) {
					shellArgs = concat(args, 2);
				}
				if(args.size() >= 2) {
					APTR handle = handler.loadChildProcess("", args[1].c_str(), shellArgs.c_str());
					if (handle) {
						cout << "Child process loaded\n";
                        symbols.readAll(handle);

 						elfHandle = new ElfHandle(handle, args[1].c_str(), false);
						interpreter.loadModule(elfHandle);
					}
				}
				if(args.size() < 2)
					cout << "Too few arguments\n";
			}
			break;

			case 'a': {
				if(args.size() < 2) {
					cout << "Not enough arguments\n";
				} else {
					APTR handle = handler.attachToProcess(args[1].c_str());
					if(handle) {
						cout << "Attached to process\n";
                        symbols.readAll(handle);

						elfHandle = new ElfHandle(handle, args[1].c_str(), false);
						interpreter.loadModule(elfHandle);
                    }
				}
			}
			break;

			case 'd':
				handler.detachFromChild();
				break;

			case 'b':
				if(args.size() < 2) {
					cout << "Too few arguments";
				} else {
                    uint32 address = symbols.valueOf(args[1].c_str());
					if(address) {
                        cout << "BREAK " << (void *)address << "\n";
						breaks.insert(address);
                    }
				}
				break;

			case 'c':
				if(args.size() < 2) {
					cout << "Too few arguments";
				} else {
                    uint32 address = symbols.valueOf(args[1].c_str());
					if(address) {
                        cout << "CLEAR " << (void *)address << "\n";
						breaks.remove(address);
                    }
				}
				break;
				
			case 'r':
				handler.readTaskContext();
				break;

			case 'i':
				cout << "ip: " << (void *)handler.ip() << "\n";
				break;

            case 'p':
                cout << "--Symbols:--\n";
                cout << symbols.printable();
                break;

			case 'w':
				cout << "--Stabs:--\n";
				cout << interpreter.printable();
				break;
			
			case 'z': {
				cout << "--Symtabs:--\n";
				StabsModule *module = interpreter.moduleFromName(elfHandle->getName());
				if(module) {
					cout << module->symtabsPrintable();
				}
				break;
			}
			case 't':
				handler.setTraceBit();
                cout << "TRACE bit set\n";
				break;

			case 'u':
				handler.unsetTraceBit();
                cout << "TRACE bit unset\n";
				break;

			case 's': {
                handler.asmStep();
                breaks.activate();
				handler.go();
				IExec->WaitPort(port);
				exit = handleMessages(port, &handler);
                breaks.suspend();
				break;
			}

			case 'k':
				cout << "==SKIP\n";
				handler.asmSkip();
				break;

			case 'q':
				exit = true;
				break;

			case 'h':
				cout << "==HELP==\n";
				cout << "l <file> <args>: load child from file\n";
				cout << "a <name>: attach to process in memory\n";
				cout << "d: detach from child\n";
				cout << "s: start execution\n";
				cout << "k: skip instruction\n";
				cout << "t: set trace bit\n";
				cout << "u: unset trace bit\n";
				cout << "r: read task context\n";
				cout << "b <symname>: break at symbol\n";
				cout << "c: clear break\n";
				cout << "i: print ip\n";
                cout << "p: print symbol list\n";
				cout << "q: quit debugger\n";
				break;

			default:
				break;

		}
	}

	handleMessages(port, &handler);

	handler.cleanup();
	IExec->FreeSysObject(ASOT_PORT, port);

    return 0;
}