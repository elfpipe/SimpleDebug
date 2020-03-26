#include "ElfHandle.hpp"

ElfHandle::ElfHandle (APTR handle, string name, bool isOpen) {
    this->name = name;
    this->handle = handle;
    this->isOpen = isOpen;

//	if (!isOpen)
		open();
    //    lock();
}

ElfHandle::~ElfHandle() {
	close();
}

void ElfHandle::lock()
{
	int numsections;
	int index;
	char *strtable;
	IElf->GetElfAttrsTags ((Elf32_Handle)handle,  EAT_NumSections, &numsections, TAG_DONE);

	int i;
	for (i = 0; i < numsections; i++)
	{
		APTR section = IElf->GetSectionTags((Elf32_Handle)handle, GST_SectionIndex, i, TAG_DONE);
		Elf32_Shdr *header = IElf->GetSectionHeaderTags ((Elf32_Handle)handle, GST_SectionIndex, i, TAG_DONE);
		if (!section)
			continue;
		int size = header->sh_size;
		printf( "Locking memory: 0x%x %d", section, size);
		if(!IExec->LockMem(section, size))
			printf("Failed to lock memory!");
	}
}
