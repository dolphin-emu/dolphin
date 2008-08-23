// gcmc.cpp: define el punto de entrada de la aplicación de consola.
//
#ifdef _WIN32
#include "stdafx.h"
#endif
#include <assert.h>
#include <memory.h>
#include <stdio.h>

#include "GCMemcard.h"

void GCMemcard::calc_checksumsBE(u16 *buf, u32 num, u16 *c1, u16 *c2)
{
    *c1 = 0;*c2 = 0;
    for (u32 i = 0; i < num; ++i)
    {
				//weird warnings here
        *c1 += bswap16(buf[i]);
        *c2 += bswap16((u16)(buf[i] ^ 0xffff));
    }
    if (*c1 == 0xffff)
    {
        *c1 = 0;
    }
    if (*c2 == 0xffff)
    {
        *c2 = 0;
    }
}

u32  GCMemcard::GetNumFiles()
{
	if(!mcdFile) return 0;

	for(int i=0;i<127;i++)
	{
		if(BE32(dir.Dir[i].Gamecode)==0xFFFFFFFF)
			return i;
	}
	return 127;
}

bool GCMemcard::DeleteFile(u32 index) //index in the directory array
{
	if(!mcdFile) return false;

	//backup the directory and bat (not really needed here but meh :P
	dir_backup=dir;
	bat_backup=bat;

	int totalspace = (((u32)BE16(hdr.Size)*16)-5);

	//free the blocks
	int blocks_left = BE16(dir.Dir[index].BlockCount);
	int block = BE16(dir.Dir[index].FirstBlock);
	do
	{
		int cbi = block-5;
		int nextblock=bswap16(bat.Map[cbi]);
		//assert(nextblock!=0);
		if(nextblock==0)
		{
			nextblock = block+1;
		}

		bat.Map[cbi]=0;

		block=nextblock;
		blocks_left--;
	}
	while((block!=0xffff)&&(blocks_left>0)); 

	//delete directory entry
	for(int i=index;i<126;i++)
	{
		dir.Dir[i]=dir.Dir[i+1];
	}
	memset(&(dir.Dir[126]),0xFF,sizeof(DEntry));

	//pack blocks to remove free space partitioning, assume no fragmentation.
	u8 *mc_data2 = new u8[mc_data_size];

	int firstFree=0;
	for(int i=0;i<127;i++)
	{
		if(BE32(dir.Dir[i].Gamecode)==0xFFFFFFFF)
		{
			break;
		}

		int fb = BE16(dir.Dir[i].FirstBlock);
		int bc = BE16(dir.Dir[i].BlockCount);

		u8* src = mc_data + (fb-5)*0x2000;
		u8* dst = mc_data2 + firstFree*0x2000;

		memcpy(dst,src,bc*0x2000);

		for(int j=0;j<bc;j++)
		{
			bat.Map[firstFree+j] = bswap16(u16(firstFree+j+6)); 
		}
		bat.Map[firstFree+bc-1] = 0xFFFF;

		dir.Dir[i].FirstBlock[0] = u8(firstFree>>8);
		dir.Dir[i].FirstBlock[1] = u8(firstFree);

		firstFree += bc;
	}

	for(int j=firstFree;j<totalspace;j++)
	{
		bat.Map[j] = 0; 
	}

	firstFree+=4;
	bat.LastAllocated[0] = u8(firstFree>>8);
	bat.LastAllocated[1] = u8(firstFree);

	delete mc_data;
	mc_data=mc_data2;
	//--
	
	//update freespace counter
	int freespace1 = totalspace - firstFree;
	bat.FreeBlocks[0] = u8(freespace1>>8);
	bat.FreeBlocks[1] = u8(freespace1);

	//fix checksums
	u16 csum1=0,csum2=0;
	calc_checksumsBE((u16*)&dir,0xFFE,&csum1,&csum2);
	dir.CheckSum1[0]=u8(csum1>>8);
	dir.CheckSum1[1]=u8(csum1);
	dir.CheckSum2[0]=u8(csum2>>8);
	dir.CheckSum2[1]=u8(csum2);
	calc_checksumsBE((u16*)(((u8*)&bat)+4),0xFFE,&csum1,&csum2);
	bat.CheckSum1[0]=u8(csum1>>8);
	bat.CheckSum1[1]=u8(csum1);
	bat.CheckSum2[0]=u8(csum2>>8);
	bat.CheckSum2[1]=u8(csum2);

	return true;
}

u32  GCMemcard::ImportFile(DEntry& direntry, u8* contents)
{
	if(!mcdFile) return 0;

	if(BE16(bat.FreeBlocks)<BE16(direntry.BlockCount))
	{
		return 0;
	}

	// find first free data block -- assume no freespace fragmentation
	int totalspace = (((u32)BE16(hdr.Size)*16)-5);

	int firstFree1 = BE16(bat.LastAllocated)+1;

	int firstFree2 = 0;
	for(int i=0;i<totalspace;i++)
	{
		if(bat.Map[i]==0)
		{
			firstFree2=i+5;
			break;
		}
	}

	int firstFree3 = 0;
	for(int i=0;i<127;i++)
	{
		if(BE32(dir.Dir[i].Gamecode)==0xFFFFFFFF)
		{
			break;
		}
		else
		{
			firstFree3 = max(firstFree3,BE16(dir.Dir[i].FirstBlock) + BE16(dir.Dir[i].BlockCount));
		}
	}

	if(firstFree2 > firstFree1) firstFree1 = firstFree2;
	if(firstFree3 > firstFree1) firstFree1 = firstFree3;

	// find first free dir entry
	int index=-1;
	for(int i=0;i<127;i++)
	{
		if(BE32(dir.Dir[i].Gamecode)==0xFFFFFFFF)
		{
			index=i;
			dir.Dir[i] = direntry;
			dir.Dir[i].FirstBlock[0] = u8(firstFree1>>8);
			dir.Dir[i].FirstBlock[1] = u8(firstFree1);
			break;
		}
	}

	// keep assuming no freespace fragmentation, and copy over all the data
	u8*destination = mc_data + (firstFree1-5)*0x2000;

	int fileBlocks=BE16(direntry.BlockCount);
	memcpy(destination,contents,0x2000*fileBlocks);


	//update freespace counter
	int freespace1 = totalspace - firstFree1;
	bat.FreeBlocks[0] = u8(freespace1>>8);
	bat.FreeBlocks[1] = u8(freespace1);

	//fix checksums
	u16 csum1=0,csum2=0;
	calc_checksumsBE((u16*)&dir,0xFFE,&csum1,&csum2);
	dir.CheckSum1[0]=u8(csum1>>8);
	dir.CheckSum1[1]=u8(csum1);
	dir.CheckSum2[0]=u8(csum2>>8);
	dir.CheckSum2[1]=u8(csum2);
	calc_checksumsBE((u16*)(((u8*)&bat)+4),0xFFE,&csum1,&csum2);
	bat.CheckSum1[0]=u8(csum1>>8);
	bat.CheckSum1[1]=u8(csum1);
	bat.CheckSum2[0]=u8(csum2>>8);
	bat.CheckSum2[1]=u8(csum2);

	return fileBlocks;
}

bool GCMemcard::GetFileData(u32 index, u8*dest) //index in the directory array
{
	if(!mcdFile) return false;

	int block = BE16(dir.Dir[index].FirstBlock);
	assert((block!=0xFFFF)&&(block>0));
	do
	{
		int nextblock=bswap16(bat.Map[block-5]);
		assert(nextblock>0);

		memcpy(dest,mc_data + 0x2000*(block-5),0x2000);
		dest+=0x2000;

		block=nextblock;
	}
	while(block!=0xffff); 

	return true;
}

u32  GCMemcard::GetFileSize(u32 index) //index in the directory array
{
	if(!mcdFile) return 0;

	return BE16(dir.Dir[index].BlockCount);
}

bool GCMemcard::GetFileInfo(u32 index, GCMemcard::DEntry& info) //index in the directory array
{
	if(!mcdFile) return false;

	info = dir.Dir[index];
	return true;
}
bool GCMemcard::GetFileName(u32 index, char *fn) //index in the directory array
{
	if(!mcdFile) return false;

	memcpy(fn,(const char*)dir.Dir[index].Filename,32);
	fn[31]=0;
	return true;
}

bool GCMemcard::GetComment1(u32 index, char *fn) //index in the directory array
{
	if(!mcdFile) return false;

	u32 Comment1  =BE32(dir.Dir[index].CommentsAddr);
	u32 DataBlock =BE16(dir.Dir[index].FirstBlock)-5;
	if(Comment1==0xFFFFFFFF)
	{
		fn[0]=0;
		return false;
	}

	memcpy(fn,mc_data +(DataBlock*0x2000) + Comment1,32);
	fn[31]=0;
	return true;
}

bool GCMemcard::GetComment2(u32 index, char *fn) //index in the directory array
{
	if(!mcdFile) return false;

	u32 Comment1  =BE32(dir.Dir[index].CommentsAddr);
	u32 Comment2  =Comment1+32;
	u32 DataBlock =BE16(dir.Dir[index].FirstBlock)-5;
	if(Comment1==0xFFFFFFFF)
	{
		fn[0]=0;
		return false;
	}

	memcpy(fn,mc_data +(DataBlock*0x2000) + Comment2,32);
	fn[31]=0;
	return true;
}

u32  GCMemcard::TestChecksums()
{
	if(!mcdFile) return 0xFFFFFFFF;

	u16 csum1=0,csum2=0;

	calc_checksumsBE((u16*)&hdr, 0xFE ,&csum1,&csum2);
	if(BE16(hdr.CheckSum1)!=csum1) return 1;
	if(BE16(hdr.CheckSum2)!=csum2) return 1;

	calc_checksumsBE((u16*)&dir,0xFFE,&csum1,&csum2);
	if(BE16(dir.CheckSum1)!=csum1) return 2;
	if(BE16(dir.CheckSum2)!=csum2) return 2;

	calc_checksumsBE((u16*)&dir_backup,0xFFE,&csum1,&csum2);
	if(BE16(dir_backup.CheckSum1)!=csum1) return 3;
	if(BE16(dir_backup.CheckSum2)!=csum2) return 3;

	calc_checksumsBE((u16*)(((u8*)&bat)+4),0xFFE,&csum1,&csum2);
	if(BE16(bat.CheckSum1)!=csum1) return 4;
	if(BE16(bat.CheckSum2)!=csum2) return 4;

	calc_checksumsBE((u16*)(((u8*)&bat_backup)+4),0xFFE,&csum1,&csum2);
	if(BE16(bat_backup.CheckSum1)!=csum1) return 5;
	if(BE16(bat_backup.CheckSum2)!=csum2) return 5;

	return 0;
}

u32  GCMemcard::CopyFrom(GCMemcard& source, u32 index)
{
	if(!mcdFile) return 0;

	DEntry d;
	if(!source.GetFileInfo(index,d)) return 0;

	u8 *t = new u8[source.GetFileSize(index)*0x2000];

	if(!source.GetFileData(index,t)) return 0;
	u32 ret = ImportFile(d,t);

	delete t;

	return ret;
}

bool GCMemcard::Save()
{
	if(!mcdFile) return false;

	FILE *mcd=(FILE*)mcdFile;
	fseek(mcd,0,SEEK_SET);
	fwrite(&hdr,1,0x2000,mcd);
	fwrite(&dir,1,0x2000,mcd);
	fwrite(&dir_backup,1,0x2000,mcd);
	fwrite(&bat,1,0x2000,mcd);
	fwrite(&bat_backup,1,0x2000,mcd);
	fwrite(mc_data,1,mc_data_size,mcd);
	return true;
}

bool GCMemcard::IsOpen()
{
	return (mcdFile!=NULL);
}

GCMemcard::GCMemcard(const char *filename)
{
	FILE *mcd=fopen(filename,"r+b");
	mcdFile=mcd;
	if(!mcd) return;

	fseek(mcd,0x0000,SEEK_SET);
	assert(fread(&hdr,       1,0x2000,mcd)==0x2000);
	assert(fread(&dir,       1,0x2000,mcd)==0x2000);
	assert(fread(&dir_backup,1,0x2000,mcd)==0x2000);
	assert(fread(&bat,       1,0x2000,mcd)==0x2000);
	assert(fread(&bat_backup,1,0x2000,mcd)==0x2000);

	fseek(mcd,0xa000,SEEK_SET);

	mc_data_size=(((u32)BE16(hdr.Size)*16)-5)*0x2000;
	mc_data = new u8[mc_data_size];

	u32 read = fread(mc_data,1,mc_data_size,mcd);
	assert(mc_data_size==read);
}

GCMemcard::~GCMemcard()
{
	fclose((FILE*)mcdFile);
}
