// Copyright (C) 2003-2008 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#ifdef _WIN32
#include "stdafx.h"
#endif
#include <assert.h>
#include <stdio.h>
#include "wx/ffile.h"

#include "GCMemcard.h"


// i think there is support for this stuff in the common lib... if not there should be support
// and to get a file exentions there is a function called SplitPath()
#define BE16(x) ((u16((x)[0])<<8) | u16((x)[1]))
#define BE32(x) ((u32((x)[0])<<24) | (u32((x)[1])<<16) | (u32((x)[2])<<8) | u32((x)[3]))
#define ArrayByteSwap(a) (ByteSwap(a, a+sizeof(u8)));

// undefined functions... prolly it means something like that
void ByteSwap(u8 *valueA, u8 *valueB)
{
	u8 tmp = *valueA;
	*valueA = *valueB;
	*valueB = tmp;
}

u16 __inline bswap16(u16 s)
{
	return (s>>8) | (s<<8);
}

u32 __inline bswap32(u32 s)
{
	return (u32)bswap16((u16)(s>>16)) | ((u32)bswap16((u16)s)<<16);
}





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
			firstFree3 = max<int>(firstFree3,(int)(BE16(dir.Dir[i].FirstBlock) + BE16(dir.Dir[i].BlockCount)));
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
	int saveLength = BE16(dir.Dir[index].BlockCount);
	int memcardSize = BE16(hdr.Size) * 0x0010;
	assert((block!=0xFFFF)&&(block>0));
	do
	{
		memcpy(dest,mc_data + 0x2000*(block-5),0x2000);
		dest+=0x2000;

		if(block + saveLength != memcardSize)
		{
			int nextblock=bswap16(bat.Map[block-5]);
			assert(nextblock>0);
			block=nextblock;
		}else block=0xffff;
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

bool GCMemcard::FixChecksums()
{
	if(!mcdFile) return false;

	u16 csum1=0,csum2=0;

	calc_checksumsBE((u16*)&dir,0xFFE,&csum1,&csum2);
	dir.CheckSum1[0]=u8(csum1>>8);
	dir.CheckSum1[1]=u8(csum1);
	dir.CheckSum2[0]=u8(csum2>>8);
	dir.CheckSum2[1]=u8(csum2);

	calc_checksumsBE((u16*)&dir_backup,0xFFE,&csum1,&csum2);
	dir_backup.CheckSum1[0]=u8(csum1>>8);
	dir_backup.CheckSum1[1]=u8(csum1);
	dir_backup.CheckSum2[0]=u8(csum2>>8);
	dir_backup.CheckSum2[1]=u8(csum2);

	calc_checksumsBE((u16*)(((u8*)&bat)+4),0xFFE,&csum1,&csum2);
	bat.CheckSum1[0]=u8(csum1>>8);
	bat.CheckSum1[1]=u8(csum1);
	bat.CheckSum2[0]=u8(csum2>>8);
	bat.CheckSum2[1]=u8(csum2);

	calc_checksumsBE((u16*)(((u8*)&bat_backup)+4),0xFFE,&csum1,&csum2);
	bat_backup.CheckSum1[0]=u8(csum1>>8);
	bat_backup.CheckSum1[1]=u8(csum1);
	bat_backup.CheckSum2[0]=u8(csum2>>8);
	bat_backup.CheckSum2[1]=u8(csum2);

	return true;
}

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

u32  GCMemcard::ImportGci(const char *fileName, int endFile, const char *fileName2)
{
	if (!mcdFile && !fileName2) return 0;

	wxFFile gci(wxString::FromAscii(fileName), _T("rb"));	
	if (!gci.IsOpened()) return 0;

	enum
	{
		GCI = 0,
		SAV = 0x80,
		GCS = 0x110
	};
	int offset;
	char * tmp = new char[0xD];
	u16 tmpU16;

	const char * fileType = (char*) fileName + endFile - 3;

	if( !strcasecmp(fileType, "gci") && !fileName2) // Extension can be either case
		offset = GCI;
	else
	{
		gci.Read(tmp, 0xD);
		if (!strcasecmp(fileType, "gcs")) // Extension can be either case
		{		
			if (!memcmp(tmp, "GCSAVE", 6))	// Header must be uppercase
				offset = GCS;
			else
			{
				// TODO: Add error message
				// file has gsc extension but does not have a correct header
				return 0;
			}
		}
		else{
			if (!strcasecmp(fileType, "sav")) // Extension can be either case
			{
				if (!memcmp(tmp, "DATELGC_SAVE", 0xC)) // Header must be uppercase
					offset = SAV;
				else
				{
					// TODO: Add error message
					//file has sav extension but does not have a correct header
					return 0;
				}
			}
			else
			{
				// TODO: Add error message, file has invalid extension
				return 0;
			}
		}
	}
	gci.Seek(offset, wxFromStart);
		
	DEntry *d = new DEntry;
	gci.Read(d, 0x40);

	switch(offset){
		case GCS:
			// field containing the Block count as displayed within
			// the GameSaves software is not stored in the GCS file.
			// It is stored only within the corresponding GSV file.
			// If the GCS file is added without using the GameSaves software,
			// the value stored is always "1"
			tmpU16 = (((int)gci.Length() - offset - 0x40) / 0x2000);
			if (tmpU16<0x100)
			{
				d->BlockCount[1] = (u8)tmpU16;
			}
			else{
				d->BlockCount[0] = (u8)(tmpU16 - 0xFF);
				d->BlockCount[1] = 0xFF;
			}
			break;
		case SAV:
			// swap byte pairs
			// 0x2C and 0x2D, 0x2E and 0x2F, 0x30 and 0x31, 0x32 and 0x33,
			// 0x34 and 0x35, 0x36 and 0x37, 0x38 and 0x39, 0x3A and 0x3B,
			// 0x3C and 0x3D,0x3E and 0x3F.
			ArrayByteSwap((d->ImageOffset));
			ArrayByteSwap(&(d->ImageOffset[2]));
			ArrayByteSwap((d->IconFmt));
			ArrayByteSwap((d->AnimSpeed));
			ByteSwap(&d->Permissions, &d->CopyCounter);
			ArrayByteSwap((d->FirstBlock));
			ArrayByteSwap((d->BlockCount));
			ArrayByteSwap((d->Unused2));
			ArrayByteSwap((d->CommentsAddr));
			ArrayByteSwap(&(d->CommentsAddr[2]));
			break;
		default:
			break;
	}
	// TODO: verify file length
	assert(((int)gci.Length() - offset) == ((BE16(d->BlockCount) * 0x2000) + 0x40));


	u32 size = BE16((d->BlockCount)) * 0x2000;
	u8 *t = new u8[size];
	gci.Read(t, size);

	gci.Close();
	
	u32 ret = 0;
	if(!fileName2)
	{
		wxFFile gci2(wxString::FromAscii(fileName2), _T("wb"));
		if (!gci2.IsOpened()) return 0;
		gci2.Seek(0, wxFromStart);
		gci2.Write(d, 0x40);
		int fileBlocks = BE16(d->BlockCount);
		gci2.Seek(0x40, wxFromStart);
		gci2.Write(t, 0x2000 * fileBlocks);
		gci2.Close();
	}
	else	ret = ImportFile(*d, t);
	
	
	delete []t;
	delete []tmp;
	delete d;
	return ret;
}

bool GCMemcard::ExportGci(u32 index, const char *fileName)
{
	wxFFile gci(wxString::FromAscii(fileName), _T("wb"));
	
	if (!gci.IsOpened()) return false;
	
	gci.Seek(0, wxFromStart);

	DEntry d;
	if(!this->GetFileInfo(index, d)) return false;
	gci.Write(&d, 0x40);

	u8 *t = new u8[this->GetFileSize(index) * 0x2000];

	if (!this->GetFileData(index, t)) return false;

	int fileBlocks = BE16(d.BlockCount);
	
	gci.Seek(0x40, wxFromStart);
	gci.Write(t, 0x2000 * fileBlocks);

	gci.Close();
	delete []t;
	
	return true;
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

	assert(BE16(hdr.Size)!=0xFFFF);
	mc_data_size=(((u32)BE16(hdr.Size)*16)-5)*0x2000;
	mc_data = new u8[mc_data_size];

	size_t read = fread(mc_data,1,mc_data_size,mcd);
	assert(mc_data_size==read);
}

GCMemcard::~GCMemcard()
{
	fclose((FILE*)mcdFile);
}

void varSwap(u8 *valueA,u8 *valueB){
	u8 temp = *valueA;
	*valueA = *valueB;
	*valueB = temp;
}
