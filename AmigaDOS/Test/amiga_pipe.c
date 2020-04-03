#include <proto/dos.h>

#include <string.h>

int amiga_pipe(BPTR fd[2], int noblock)
{
	struct ExamineData *data;
	char filename[120] = "";
	int ret=0;

	fd[1] = IDOS->Open ("PIPE:/UNIQUE/NOBLOCK", MODE_NEWFILE);
	if (fd[1] == 0)
	{
		IDOS->Printf("DOS couldn't open PIPE:/UNIQUE\n");
		return(-1);
	}
	data = IDOS->ExamineObjectTags(EX_FileHandleInput, fd[1], TAG_END);
	if (data == NULL)
	{
		IDOS->PrintFault(IDOS->IoErr(),NULL); /* failure - why ? */
		IDOS->Close(fd[1]);
		return(-1);
	}

	strlcpy(filename, "PIPE:", sizeof(filename));
	strlcat(filename, data->Name, sizeof(filename));
	IDOS->FreeDosObject(DOS_EXAMINEDATA, data);

	if (noblock)
		strlcat(filename, "/NOBLOCK", sizeof(filename));

	fd[0] = IDOS->Open (filename, MODE_OLDFILE);
	if (fd[0] == 0)
	{
		IDOS->Printf("DOS couldn't open %s\n", filename);
		return(-1);
	}
	return (0);
}

int main() {
	BPTR fd[2];
	amiga_pipe(fd, FALSE);

	BPTR seglist = IDOS->LoadSeg("C:dir");
	struct Process *process = IDOS->CreateNewProcTags(
		NP_Seglist,			seglist,
		NP_Output,			fd[1],
		NP_CloseOutput,		FALSE,
		TAG_DONE);

	IDOS->Delay(50);

	char buffer[1024];
	IDOS->Read(fd[0], buffer, 1023);
	buffer[1023] = '\0';
	IDOS->Printf(buffer);

	return 0;
}