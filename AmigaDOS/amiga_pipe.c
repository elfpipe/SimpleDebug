int amiga_pipe(BPTR fd[2], int noblock)
{
	struct ExamineData *data;
	char filename[120] = "";
	int ret=0;

	fd[1] = IDOS->Open ("PIPE:/UNIQUE/NOBLOCK", MODE_NEWFILE);
	if (fd[1] == 0)
	{
		printf("DOS couldn't open PIPE:/UNIQUE\n");
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
		printf("DOS couldn't open %s\n", filename);
		return(-1);
	}
	return (0);
}