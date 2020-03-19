#include "ElfHandle.hpp"

ElfHandle::ElfHandle (APTR handle, string name, bool isOpen) {
    this->name = name;
    this->handle = handle;
    this->isOpen = isOpen;

	if (!isOpen)
		open();
}

ElfHandle::~ElfHandle() {
	close();
}
