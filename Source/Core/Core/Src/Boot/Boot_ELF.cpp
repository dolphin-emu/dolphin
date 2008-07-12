#include "../PowerPC/PowerPC.h"
#include "Boot.h"
#include "Boot_ELF.h"
#include "ElfReader.h"
#include "MappedFile.h"

bool CBoot::IsElfWii(const char *filename)
{
	Common::IMappedFile *mapfile = Common::IMappedFile::CreateMappedFile();
	mapfile->Open(filename);
	u8 *ptr = mapfile->Lock(0, mapfile->GetSize());
	u8 *mem = new u8[(size_t)mapfile->GetSize()];
	memcpy(mem, ptr, (size_t)mapfile->GetSize());
	mapfile->Unlock(ptr);
	mapfile->Close();
	
	ElfReader reader(mem);
	
	// TODO: Find a more reliable way to distinguish.
	bool isWii = reader.GetEntryPoint() >= 0x80004000;
	delete [] mem;
    return isWii;
}


bool CBoot::Boot_ELF(const char *filename)
{
	Common::IMappedFile *mapfile = Common::IMappedFile::CreateMappedFile();
	mapfile->Open(filename);
	u8 *ptr = mapfile->Lock(0, mapfile->GetSize());
	u8 *mem = new u8[(size_t)mapfile->GetSize()];
	memcpy(mem, ptr, (size_t)mapfile->GetSize());
	mapfile->Unlock(ptr);
	mapfile->Close();
	
	ElfReader reader(mem);
	reader.LoadInto(0x80000000);
	if (!reader.LoadSymbols())
	{
		LoadMapFromFilename(filename);
	}
	delete [] mem;
	
	PC = reader.GetEntryPoint();

    return true;
}

