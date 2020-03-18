#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/elf.h>

#include <iostream>
#include <sstream>
#include <string>
#include <algorithm>
#include <iterator>
#include <vector>

using namespace std;

template <class Container>
void split2(const std::string& str, Container& cont, char delim = ' ')
{
    std::stringstream ss(str);
    std::string token;
    while (std::getline(ss, token, delim)) {
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

class AmigaDOSProcessHandler {
private:
    struct Process *child;
	struct Hook hook;
	bool childExists;
	bool childIsRunning;
	bool parentIsAttached;

public:
    void init();
    void cleanup();

    APTR loadChildProcess(const char *path, const char *command, const char *arguments);
	APTR attachToProcess(const char *name);
	void detachFromChild();

    void go();
    void waitChild();
};

struct DebugIFace *IDebug = 0;
struct MMUIFace *IMMU = 0;

void AmigaDOSProcessHandler::init ()
{
	IDebug = (struct DebugIFace *)IExec->GetInterface ((struct Library *)SysBase, "debug", 1, 0);
	if (!IDebug) {
		return;
	}

	IMMU = (struct MMUIFace *)IExec->GetInterface ((struct Library *)SysBase, "mmu", 1, 0);
	if (!IMMU) {
		return;
	}
}

void AmigaDOSProcessHandler::cleanup ()
{
	if (IDebug)
		IExec->DropInterface((struct Interface *)IDebug);
	IDebug = 0;

	if (IMMU)
		IExec->DropInterface((struct Interface *)IMMU);
	IMMU = 0;
}

APTR AmigaDOSProcessHandler::loadChildProcess (const char *path, const char *command, const char *arguments)
{
	BPTR lock = IDOS->Lock (path, SHARED_LOCK);
	if (!lock) {
		return 0;
	}
	BPTR homelock = IDOS->DupLock (lock);

	BPTR seglist = IDOS->LoadSeg (command);
	
	if (!seglist) {
		IDOS->UnLock(lock);
		return 0;
	}

	IExec->Forbid(); //can we avoid this?

    child = IDOS->CreateNewProcTags(
		NP_Seglist,					seglist,
//		NP_Entry,					foo,
		NP_FreeSeglist,				TRUE,
		NP_Name,					command,
		NP_CurrentDir,				lock,
		NP_ProgramDir,				homelock,
		NP_StackSize,				2000000,
		NP_Cli,						TRUE,
		NP_Child,					TRUE,
		NP_Arguments,				arguments,
		NP_Input,					IDOS->Input(),
		NP_CloseInput,				FALSE,
		NP_Output,					IDOS->Output(), //pipe_get_write_end(),
		NP_CloseOutput,				FALSE,
		NP_Error,					IDOS->ErrorOutput(),
		NP_CloseError,				FALSE,
		NP_NotifyOnDeathSigTask,	IExec->FindTask(NULL),
		TAG_DONE
	);

	if (!child) {
		IExec->Permit();
		return 0;
	} else {
		IExec->SuspendTask ((struct Task *)child, 0L);
		IExec->Permit();
	}
	
	childExists = true;

	APTR handle;
	
	IDOS->GetSegListInfoTags (seglist, 
		GSLI_ElfHandle, &handle,
		TAG_DONE
	);
	
    return handle;
}

APTR AmigaDOSProcessHandler::attachToProcess (const char *name)
{
	struct Process *process = (struct Process *)IExec->FindTask(name);
	if(!process) return 0;

	if (process->pr_Task.tc_Node.ln_Type != NT_PROCESS) {
		return 0;
	}

	BPTR seglist = IDOS->GetProcSegList (process, GPSLF_SEG|GPSLF_RUN);
  
	if (!seglist) {
		return 0;
	}

	if (process->pr_Task.tc_State == TS_READY || process->pr_Task.tc_State == TS_WAIT) {
		IExec->SuspendTask ((struct Task *)process, 0);
//		IExec->Signal ((struct Task *)_me, _eventSignalMask);
	}

	if (process->pr_Task.tc_State == TS_CRASHED) {
		process->pr_Task.tc_State = TS_SUSPENDED;
	}

	child  = process;

	childExists = true;
	childIsRunning = false;
	parentIsAttached = true;
    
//	hookOn ();

//	readTaskContext ();

	APTR elfHandle;
	IDOS->GetSegListInfoTags (seglist, 
		GSLI_ElfHandle, &elfHandle,
		TAG_DONE
	);
		
	return elfHandle;
}

void AmigaDOSProcessHandler::detachFromChild ()
{
	if (!childIsRunning)
		go();
	
//	hookOff();
	
	childExists = false;
	childIsRunning = false;
	parentIsAttached = false;
}

void AmigaDOSProcessHandler::go()
{
    IExec->RestartTask((struct Task *)child, 0);
}

void AmigaDOSProcessHandler::waitChild()
{
    IExec->Wait(SIGF_CHILD);
}

vector<string> getInput()
{
	vector<string> cmdArgs;
	
	while(1) {
		cout << "> ";

		string command;
		getline(cin, command);
		split2(command, cmdArgs);

		if(cmdArgs[0].length() == 1)
			break;
	}

	return cmdArgs;
}

int main(int argc, char *argv[])
{
	AmigaDOSProcessHandler handler;
	APTR handle;

	bool exit = false;
	while(!exit) {
		vector<string> args = getInput();

		switch(args[0][0]) {
			case 'l': {
				string shellArgs;
				if(args.size() >= 3) {
					shellArgs = concat(args, 2);
				}
				if(args.size() >= 2) {
					handle = handler.loadChildProcess("", args[1].c_str(), shellArgs.c_str());
					if (handle) {
						cout << "Child process loaded";
					}
				}
			}
				break;

			case 'a': {
				if(args.size() < 2) {
					cout << "Not enough arguments";
				} else {
					handle = handler.attachToProcess(args[1].c_str());
					if(handle)
						cout << "Attached to process";
				}
			}

			case 'd':
				handler.detachFromChild();
				break;

			case 's':
				handler.go();
				break;
			
			case 'q':
				exit = true;
				break;

			default:
				break;

		}
	}

    if(handle) {
        handler.go();
		handler.waitChild();
    }

    return 0;
}
