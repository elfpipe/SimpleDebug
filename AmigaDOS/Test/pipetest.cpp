#include "../Pipe.hpp"
#include <iostream>

#include <proto/exec.h>
#include <proto/dos.h>


int main() {
    Pipe pipe;

#if 1
	BPTR seglist = IDOS->LoadSeg ("C:dir");
    if(!seglist) {
        cout << "Failed to load\n";
        return -1;
    }

    BPTR file = IDOS->Open("output.txt", MODE_NEWFILE);

    Process *process = IDOS->CreateNewProcTags(
		NP_Seglist,					seglist,
		NP_FreeSeglist,				true,
		NP_Cli,						true,
		NP_Child,					true,
		NP_Output,					file, //pipe.getWrite(),
        NP_CloseOutput,             true,
		NP_NotifyOnDeathSigTask,	IExec->FindTask(0),
		TAG_DONE
	);
    
    IExec->Wait(SIGF_CHILD);
#else
    int result = IDOS->SystemTags("dir",
        SYS_Output, pipe.getWrite(),
        TAG_DONE);
#endif

    vector<string> output = pipe.emptyPipe();
    for(int i = 0; i < output.size(); i++)
        cout << "--] " << output[i] << "\n";

    cout << "Done.\n";
    return 0;
}