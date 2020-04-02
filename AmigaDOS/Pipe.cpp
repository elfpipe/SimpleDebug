#include <proto/exec.h>
#include <proto/dos.h>

#include <string.h>

#include "Pipe.hpp"
#include "Strings.hpp"
#include "async.h"

Pipe::Pipe() {
	init();
}

Pipe::~Pipe() {
	cleanup();
}

void Pipe::init() {
	isReady = false;
	bool noblock = true;
	ExamineData *data;

	fd[0] = 0;
	fd[1] = 0;

	fd[1] = IDOS->Open ("PIPE:/UNIQUE/NOBLOCK", MODE_NEWFILE);
	if (fd[1] == 0) return;

	data = IDOS->ExamineObjectTags(EX_FileHandleInput, fd[1], TAG_END);
	if (data == 0) {
		IDOS->PrintFault(IDOS->IoErr(), 0); /* failure - why ? */
		IDOS->Close(fd[1]);
		return;
	}

	string file = string("PIPE:") + (const char *)data->Name;
	if (noblock) file += "/NOBLOCK";
	IDOS->FreeDosObject(DOS_EXAMINEDATA, data);


	fd[0] = IDOS->Open (file.c_str(), MODE_OLDFILE);
	if (fd[0] == 0) {
		IDOS->Close(fd[1]);
		return;
	}

	size = CHUNK_SIZE;
	buffer = new char[size];
	buffer[0] = '\0';
	bytes = 0;
	isReady = true;
}

void Pipe::cleanup()
{
	if(fd[0]) IDOS->Close(fd[0]);
	if(fd[1]) IDOS->Close(fd[1]);
}

int Pipe::write(string text) {
	if(!isReady) return 0;
	int bytes = IDOS->Write(fd[1], text.c_str(), text.size());
	if(bytes == -1) {
		IDOS->PrintFault(IDOS->IoErr(), "IO error: "); /* failure - why ? */
		bytes = 0;
	}
	return bytes;
}

int Pipe::read()
{
	if(!isReady) return 0;
	bytes = IDOS->Read(fd[0], buffer, size-1);
	if(bytes == -1) {
		IDOS->PrintFault(IDOS->IoErr(), "IO error: "); /* failure - why ? */
		bytes = 0;
	}
	buffer[bytes] = '\0';
	return bytes;
}

bool Pipe::bytesToRead() {
	return bytes;
}

vector<string> Pipe::emptyPipe() {
	read();
	return astream(buffer).split('\n');
}