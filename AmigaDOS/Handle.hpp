//
//
// elfhandle.hpp - keep track of elf handles in application runtime flow (Debug 101)
//
//

#ifndef DB101_ELFHANDLE_HPP
#define DB101_ELFHANDLE_HPP

#include <proto/exec.h>
#include <proto/elf.h>

#include <string>

using namespace std;

class ElfHandle
{
private:
    string name;
	APTR handle;  //problem with c++ and Elf32_Handle typedef in AmigaOS
	bool isOpen;

public:
	ElfHandle (APTR handle, string name, bool isOpen = false);
	~ElfHandle();
	
	APTR getHandle () { return handle; }
    string getName() { return name; }

	bool open () {	
		handle = IElf->OpenElfTags(OET_ReadOnlyCopy, TRUE, OET_ElfHandle, handle, TAG_DONE);
        isOpen = true;
		return isOpen;
	}
	void close () {
		IElf->CloseElfTags ((Elf32_Handle)handle, CET_ReClose, FALSE, TAG_DONE);
	}
	
	bool performRelocation () {
		IElf->RelocateSectionTags((Elf32_Handle)handle, GST_SectionName, ".stabstr", GST_Load, TRUE, TAG_DONE );
		return IElf->RelocateSectionTags((Elf32_Handle)handle, GST_SectionName, ".stab", GST_Load, TRUE, TAG_DONE );
	}
	
	char *getStabstrSection () { return (char *)IElf->GetSectionTags((Elf32_Handle)handle, GST_SectionName, ".stabstr", TAG_DONE); }
	uint32_t *getStabSection () { return (uint32_t *)IElf->GetSectionTags((Elf32_Handle)handle, GST_SectionName, ".stab", TAG_DONE); }
	Elf32_Shdr *getStabSectionHeader () { return IElf->GetSectionHeaderTags ((Elf32_Handle)handle, GST_SectionName, ".stab", TAG_DONE); }
	uint32_t getStabsSize () {
		Elf32_Shdr *header = IElf->GetSectionHeaderTags ((Elf32_Handle)handle, GST_SectionName, ".stab", TAG_DONE);
		return header->sh_size;
	}
	void lock();
};
#endif //DB101_ELFHANDLE_HPP
