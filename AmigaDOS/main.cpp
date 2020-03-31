#include "OSSymbols.hpp"
#include "Process.hpp"
#include "Breaks.hpp"

#include "Definitions02.hpp"
#include "ElfHandle.hpp"

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

	ElfHandle *elfHandle = 0;
	Binary *binary = 0;

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
						elfHandle->performRelocation();

						binary = new Binary(elfHandle->getName(), (SymtabEntry *)elfHandle->getStabSection(), elfHandle->getStabstrSection(), elfHandle->getStabsSize());
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
						elfHandle->performRelocation();

						binary = new Binary(elfHandle->getName(), (SymtabEntry *)elfHandle->getStabSection(), elfHandle->getStabstrSection(), elfHandle->getStabsSize());
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
				} else if(args.size() == 2) {
                    uint32 address = symbols.valueOf(args[1].c_str());
					if(address) {
                        cout << "BREAK " << (void *)address << "\n";
						breaks.insert(address);
                    }
				} else {
					//Function::SLine *line = binary->find(args[1].c_str(), );
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
				
			case 'i':
				cout << "ip: " << (void *)handler.ip() << "\n";
				break;

            case 'o':
                cout << "--OSSymbols:--\n";
                cout << symbols.printable();
                break;

			case 'p':
				cout << "--Binary structure:--\n";
				cout << binary->toString();
				break;
			
			case 's': {
                handler.asmStep();
				exit = handleMessages(port, &handler);

                breaks.activate();

				handler.go();
				handler.waitTrap();

                breaks.suspend();

				exit = handleMessages(port, &handler);
				break;
			}

			case 'k':
				cout << "==SKIP\n";
				handler.asmSkip();
				break;

			case 'z':
				cout << "==ASM STEP\n";
				handler.asmStep();

				exit = handleMessages(port, &handler);
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
				cout << "z: asm step\n";

				cout << "b <symname>: break at symbol\n";
				cout << "c: clear break\n";

				cout << "i: print ip\n";
				cout << "o: print os symbol list\n";
                cout << "p: print binary structure\n";

				cout << "q: quit debugger\n";
				break;

			default:
				break;
		}
	}
	handleMessages(port, &handler);

//	handler.cleanup();
	IExec->FreeSysObject(ASOT_PORT, port);

    return 0;
}