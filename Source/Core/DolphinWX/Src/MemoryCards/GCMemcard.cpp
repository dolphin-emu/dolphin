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

	for(int i=0;i<126;i++)
	{
		if(BE32(dir.Dir[i].Gamecode)==0xFFFFFFFF)
			return i;
	}
	return 127;
}

bool GCMemcard::RemoveFile(u32 index) //index in the directory array
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
	for(int i=index;i<125;i++)
	{
		dir.Dir[i]=dir.Dir[i+1];
	}
	memset(&(dir.Dir[125]),0xFF,sizeof(DEntry));

	//pack blocks to remove free space partitioning, assume no fragmentation.
	u8 *mc_data2 = new u8[mc_data_size];

	int firstFree=0;
	for(int i=0;i<126;i++)
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

		dir.Dir[i].FirstBlock[0] = u8((firstFree+5)>>8);
		dir.Dir[i].FirstBlock[1] = u8((firstFree+5));

		firstFree += bc;
	}

	for(int j=firstFree;j<totalspace;j++)
	{
		bat.Map[j] = 0; 
	}

	firstFree+=4;
	bat.LastAllocated[0] = u8(firstFree>>8);
	bat.LastAllocated[1] = u8(firstFree);

	delete [] mc_data;
	mc_data = mc_data2;
	//--
	
	//update freespace counter
	int freespace1 = totalspace - firstFree;
	bat.FreeBlocks[0] = u8(freespace1>>8);
	bat.FreeBlocks[1] = u8(freespace1);

	// ... and update counter
	int updateCtr = BE16(dir.UpdateCounter)+1;
	dir.UpdateCounter[0] = u8(updateCtr>>8);
	dir.UpdateCounter[1] = u8(updateCtr);
	
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

	dir_backup=dir;
	bat_backup=bat;

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
	for(int i=0;i<126;i++)
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

	if(firstFree1>=126)
	{
		// TODO: show messagebox about the error
		return 0;
	}

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
			dir.Dir[i].CopyCounter   = dir.Dir[i].CopyCounter+1;
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

	// ... and update counter
	int updateCtr = BE16(dir.UpdateCounter)+1;
	dir.UpdateCounter[0] = u8(updateCtr>>8);
	dir.UpdateCounter[1] = u8(updateCtr);
	
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

u32 decode5A3(u16 val)
{
	const int lut5to8[] = { 0x00,0x08,0x10,0x18,0x20,0x29,0x31,0x39,
	                        0x41,0x4A,0x52,0x5A,0x62,0x6A,0x73,0x7B,
	                        0x83,0x8B,0x94,0x9C,0xA4,0xAC,0xB4,0xBD,
	                        0xC5,0xCD,0xD5,0xDE,0xE6,0xEE,0xF6,0xFF};
	const int lut4to8[] = { 0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,
	                        0x88,0x99,0xAA,0xBB,0xCC,0xDD,0xEE,0xFF};
	const int lut3to8[] = { 0x00,0x24,0x48,0x6D,0x91,0xB6,0xDA,0xFF};


	int r,g,b,a;
	if ((val&0x8000))
	{
		r=lut5to8[(val>>10) & 0x1f];
		g=lut5to8[(val>>5 ) & 0x1f];
		b=lut5to8[(val    ) & 0x1f];
		a=0xFF;
	}
	else
	{
		a=lut3to8[(val>>12) & 0x7];
		r=lut4to8[(val>>8 ) & 0xf];
		g=lut4to8[(val>>4 ) & 0xf];
		b=lut4to8[(val    ) & 0xf];
	}
	return (a<<24) | (r<<16) | (g<<8) | b;
}

void decode5A3image(u32* dst, u16* src, int width, int height)
{
	for (int y = 0; y < height; y += 4)
	{
		for (int x = 0; x < width; x += 4)
		{
			for (int iy = 0; iy < 4; iy++, src += 4)
			{
				for (int ix = 0; ix < 4; ix++)
				{
					u32 RGBA = decode5A3(bswap16(src[ix]));
					dst[(y + iy) * width + (x + ix)] = RGBA;
				}
			}
		}
	}
}

void decodeCI8image(u32* dst, u8* src, u16* pal, int width, int height)
{
    for (int y = 0; y < height; y += 4)
	{
        for (int x = 0; x < width; x += 8)
		{
            for (int iy = 0; iy < 4; iy++, src += 8)
			{
				u32 *tdst = dst+(y+iy)*width+x;
				for (int ix = 0; ix < 8; ix++)
				{
					tdst[ix] = decode5A3(bswap16(pal[src[ix]]));
				}
			}
		}
	}
}

bool GCMemcard::ReadBannerRGBA8(u32 index, u32* buffer)
{
	if(!mcdFile) return false;

	int flags = dir.Dir[index].BIFlags;

	int  bnrFormat = (flags&3);

	if(bnrFormat==0)
		return false;

	u32 DataOffset=BE32(dir.Dir[index].ImageOffset);
	u32 DataBlock =BE16(dir.Dir[index].FirstBlock)-5;

	if(DataOffset==0xFFFFFFFF)
	{
		return false;
	}

	const int pixels = 96*32;

	if(bnrFormat&1)
	{
		u8  *pxdata  = (u8* )(mc_data +(DataBlock*0x2000) + DataOffset);
		u16 *paldata = (u16*)(mc_data +(DataBlock*0x2000) + DataOffset + pixels);

		decodeCI8image(buffer,pxdata,paldata,96,32);
	}
	else
	{
		u16 *pxdata = (u16*)(mc_data +(DataBlock*0x2000) + DataOffset);

		decode5A3image(buffer,pxdata,96,32);
	}
	return true;
}

u32  GCMemcard::ReadAnimRGBA8(u32 index, u32* buffer, u8 *delays)
{
	if(!mcdFile) return 0;

	int formats = BE16(dir.Dir[index].IconFmt);
	int fdelays  = BE16(dir.Dir[index].AnimSpeed);

	int flags = dir.Dir[index].BIFlags;

	int  bnrFormat = (flags&3);

	u32 DataOffset=BE32(dir.Dir[index].ImageOffset);
	u32 DataBlock =BE16(dir.Dir[index].FirstBlock)-5;

	if(DataOffset==0xFFFFFFFF)
	{
		return 0;
	}

	u8* animData=(u8*)(mc_data +(DataBlock*0x2000) + DataOffset);

	switch(bnrFormat)
	{
	case 1:
	case 3:
		animData+=96*32 + 2*256; // image+palette
		break;
	case 2:
		animData+=96*32*2;
		break;
	}

	int fmts[8];
	u8* data[8];
	int frames = 0;


	for(int i=0;i<8;i++)
	{
		fmts[i] = (formats>>(2*i))&3;
		delays[i] = ((fdelays>>(2*i))&3)<<2;
		data[i] = animData;

		switch(fmts[i])
		{
		case 1: // CI8 with shared palette
			animData+=32*32;
			frames++;
			break;
		case 2: // RGB5A3
			animData+=32*32*2;
			frames++;
			break;
		case 3: // CI8 with own palette
			animData+=32*32 + 2*256;
			frames++;
			break;
		}
	}

	u16* sharedPal = (u16*)(animData);

	for(int i=0;i<8;i++)
	{
		switch(fmts[i])
		{
		case 1: // CI8 with shared palette
			decodeCI8image(buffer,data[i],sharedPal,32,32);
			buffer+=32*32;
			break;
		case 2: // RGB5A3
			decode5A3image(buffer,(u16*)(data[i]),32,32);
			break;
		case 3: // CI8 with own palette
			u16 *paldata = (u16*)(data[i]+32*32);
			decodeCI8image(buffer,data[i],paldata,32,32);
			buffer+=32*32;
			break;
		}
	}

	return frames;
}

u32  GCMemcard::TestChecksums()
{
	if(!mcdFile) return 0xFFFFFFFF;

	u16 csum1=0,csum2=0;

	u32 results = 0;

	calc_checksumsBE((u16*)&hdr, 0xFE ,&csum1,&csum2);
	if(BE16(hdr.CheckSum1)!=csum1) results |= 1;
	if(BE16(hdr.CheckSum2)!=csum2) results |= 1;

	calc_checksumsBE((u16*)&dir,0xFFE,&csum1,&csum2);
	if(BE16(dir.CheckSum1)!=csum1) results |= 2;
	if(BE16(dir.CheckSum2)!=csum2) results |= 2;

	calc_checksumsBE((u16*)&dir_backup,0xFFE,&csum1,&csum2);
	if(BE16(dir_backup.CheckSum1)!=csum1) results |= 4;
	if(BE16(dir_backup.CheckSum2)!=csum2) results |= 4;

	calc_checksumsBE((u16*)(((u8*)&bat)+4),0xFFE,&csum1,&csum2);
	if(BE16(bat.CheckSum1)!=csum1) results |= 8;
	if(BE16(bat.CheckSum2)!=csum2) results |= 8;

	calc_checksumsBE((u16*)(((u8*)&bat_backup)+4),0xFFE,&csum1,&csum2);
	if(BE16(bat_backup.CheckSum1)!=csum1) results |= 16;
	if(BE16(bat_backup.CheckSum2)!=csum2) results |= 16;

	return 0;
}


// ==========================================================================================
// Fix checksums - I'll begin with fixing Directory and Directory backup. Feel free to add the 
// other blocks.
// ------------------------------------------------------------------------------------------
u32  GCMemcard::FixChecksums()
{
	if(!mcdFile) return 0xFFFFFFFF;

	u16 csum1=0,csum2=0;

	u32 results = 0;

	calc_checksumsBE((u16*)&dir,0xFFE,&csum1,&csum2);
	if(BE16(dir.CheckSum1) != csum1) results |= 2;
	if(BE16(dir.CheckSum2) != csum2) results |= 2;

	// ------------------------------------------------------------------------------------------
	// Save the values we just read
	dir.CheckSum1[0]=u8(csum1>>8);
	dir.CheckSum1[1]=u8(csum1);
	dir.CheckSum2[0]=u8(csum2>>8);
	dir.CheckSum2[1]=u8(csum2);
	// ------------------------------------------------------------------------------------------

	calc_checksumsBE((u16*)&dir_backup,0xFFE,&csum1,&csum2);
	if(BE16(dir_backup.CheckSum1) != csum1) results |= 4;
	if(BE16(dir_backup.CheckSum2) != csum2) results |= 4;

	// ------------------------------------------------------------------------------------------
	// Save the values we just read
	dir_backup.CheckSum1[0]=u8(csum1>>8);
	dir_backup.CheckSum1[1]=u8(csum1);
	dir_backup.CheckSum2[0]=u8(csum2>>8);
	dir_backup.CheckSum2[1]=u8(csum2);
	// ------------------------------------------------------------------------------------------
	return 0;
}
// ==========================================================================================


u32  GCMemcard::CopyFrom(GCMemcard& source, u32 index)
{
	if(!mcdFile) return 0;

	DEntry d;
	if(!source.GetFileInfo(index,d)) return 0;

	u8 *t = new u8[source.GetFileSize(index)*0x2000];

	if(!source.GetFileData(index,t)) return 0;
	u32 ret = ImportFile(d,t);

	delete[] t;

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

	u32 csums = TestChecksums();
	
	if(csums&1)
	{
		// header checksum error!
		// TODO: fail to load
	}

	if(csums&2) // directory checksum error!
	{
		if(csums&4)
		{
			// backup is also wrong!
			// TODO: fail to load
		}
		else
		{
			// backup is correct, restore
			dir = dir_backup;
			bat = bat_backup;

			// update checksums
			csums = TestChecksums();
		}
	}

	if(csums&8) // BAT checksum error!
	{
		if(csums&16)
		{
			// backup is also wrong!
			// TODO: fail to load
		}
		else
		{
			// backup is correct, restore
			dir = dir_backup;
			bat = bat_backup;

			// update checksums
			csums = TestChecksums();
		}
	}

	if(BE16(dir_backup.UpdateCounter) > BE16(dir.UpdateCounter)) //check if the backup is newer
	{
		dir = dir_backup;
		bat = bat_backup; // needed?
	}

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
