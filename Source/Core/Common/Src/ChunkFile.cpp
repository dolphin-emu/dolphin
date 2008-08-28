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

#include "Common.h"
#include "ChunkFile.h"

#include <stdio.h>

// Not using file mapping, just bog standard fopen and friends. We trust them to be fast
// enough to not be the bottleneck.

ChunkFile::ChunkFile(const char *filename, ChunkFileMode _mode)
{
    mode = _mode;
    data = 0;

    didFail = false;
    f = fopen(filename, mode == MODE_WRITE ? "wb" : "rb");
    if (!f) {
        didFail = true;
        return;
    }
	
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    eof = size;

    stack_ptr = 0;
}

ChunkFile::~ChunkFile()
{
    if (f)
        fclose(f);
}

int ChunkFile::ReadInt()
{
    int x;
    fread(&x, 4, 1, f);
    return x;
}

void ChunkFile::WriteInt(int x)
{
    fwrite(&x, 4, 1, f);
}

bool ChunkFile::Do(void *ptr, int size)
{
    int sz;
    switch (mode) {
    case MODE_READ:
        sz = ReadInt();
        if (sz != size)
            return false;
        fread(ptr, size, 1, f);
        fseek(f, ((size + 3) & ~3) - size, SEEK_CUR);
        break;
    case MODE_WRITE:
        WriteInt(size);
        fwrite(ptr, size, 1, f);
        fseek(f, ((size + 3) & ~3) - size, SEEK_CUR);
        break;
    case MODE_VERIFY:
        sz = ReadInt();
        if (sz != size)
            return false;
        fseek(f, (size + 3) & ~3, SEEK_CUR);
        break;
    }
    return true;
}

// Do variable size array (probably heap-allocated)
bool DoArray(void *ptr, int size, int arrSize) {
    int sz;
    
    if(ptr == NULL)
        return false;
    
    switch (mode) {
    case MODE_READ:
        sz = ReadInt();
        if (sz != size)
            return false;
        sz = ReadInt();
        if (sz != arrSize)
            return false;
                
        for(int i = 0; i < arrSize; i++) {
            fread(ptr[i], size, 1, f);
            fseek(f, ((size + 3) & ~3) - size, SEEK_CUR);
        }
        break;
    case MODE_WRITE:
        WriteInt(size);
        WriteInt(arrSize);
        for(int i = 0; i < arrSize; i++) {
            fwrite(ptr[i], size, 1, f);
            fseek(f, ((size + 3) & ~3) - size, SEEK_CUR);
        }
        break;
    case MODE_VERIFY:
        sz = ReadInt();
        if (sz != size)
            return false;
        sz = ReadInt();
        if (sz != arrSize)
            return false;
                
        for(int i = 0; i < arrSize; i++)
            fseek(f, (size + 3) & ~3, SEEK_CUR);
        break;
    }
    return true;
}

//let's get into the business
bool ChunkFile::Descend(const char *cid)
{
    unsigned int id = *reinterpret_cast<const unsigned int*>(cid);
    if (mode == MODE_READ)
	{
            bool found = false;
            int startPos = ftell(f);
            ChunkInfo temp = stack[stack_ptr];

            //save information to restore after the next Ascend
            stack[stack_ptr].parentStartLocation = startPos;
            stack[stack_ptr].parentEOF = eof;

            unsigned int firstID = 0;
            //let's search through children..
            while (ftell(f) < eof)
		{
                    stack[stack_ptr].ID = ReadInt();
                    if (firstID == 0)
                        firstID = stack[stack_ptr].ID|1;
                    stack[stack_ptr].length = ReadInt();
                    stack[stack_ptr].startLocation = ftell(f);
                    if (stack[stack_ptr].ID == id)
			{
                            found = true;
                            break;
			}
                    else
			{
                            fseek(f, stack[stack_ptr].length, SEEK_CUR); //try next block
			}
		} 

            //if we found nothing, return false so the caller can skip this
            if (!found)
		{
                    stack[stack_ptr] = temp;
                    fseek(f, stack[stack_ptr].parentStartLocation, SEEK_SET);
                    return false;
		}

            //descend into it
            //pos was set inside the loop above
            eof = stack[stack_ptr].startLocation + stack[stack_ptr].length;
            stack_ptr++;
            return true;
	}
    else
	{
            //write a chunk id, and prepare for filling in length later
            WriteInt(id);
            WriteInt(0); //will be filled in by Ascend
            stack[stack_ptr].startLocation = ftell(f);
            stack_ptr++;
            return true;
	}
}

//let's ascend out
void ChunkFile::Ascend()
{
    if (mode == MODE_READ)
	{
            //ascend, and restore information
            stack_ptr--;
            fseek(f, stack[stack_ptr].parentStartLocation, SEEK_SET);
            eof = stack[stack_ptr].parentEOF;
	}
    else 
	{
            stack_ptr--;
            //now fill in the written length automatically
            int posNow = ftell(f);
            fseek(f, stack[stack_ptr].startLocation - 4, SEEK_SET);
            WriteInt(posNow - stack[stack_ptr].startLocation);
            fseek(f, posNow, SEEK_SET);
	}
}

int ChunkFile::GetCurrentChunkSize()
{
    if (stack_ptr)
        return stack[stack_ptr - 1].length;
    else
        return 0;
}
