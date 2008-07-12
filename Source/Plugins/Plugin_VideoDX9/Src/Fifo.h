#ifndef _FIFO_H
#define _FIFO_H

#include "Common.h"

// inline for speed!
class FifoReader
{
	u8 *ptr;
	u8 *end;
	u8 *tempPtr; //single element stack :P
	u8 *tempEnd;
	bool pushed;
public:
	void Init(u8 *_ptr, u8 *_end)
	{
		ptr = _ptr; end = _end; pushed = false;
	}
	bool IsPushed() {return pushed;}
	void Push(u8 *_ptr, u8 *_end) {pushed = true; tempPtr = ptr; tempEnd = end; ptr = _ptr; end = _end;}
	void Pop() {pushed = false; ptr = tempPtr; end = tempEnd;}
	u8  Peek8 (int offset) { return ptr[offset]; }
	u16 Peek16(int offset) { return _byteswap_ushort(*(u16*)(ptr+offset)); }
	u32 Peek32(int offset) { return _byteswap_ulong(*(u32*)(ptr+offset)); }
	u8  Read8 () {return *ptr++;}
	u16 Read16() {const u16 value = _byteswap_ushort(*((u16*)ptr)); ptr+=2; return value;}
	u32 Read32() {const u32 value = _byteswap_ulong(*((u32*)ptr)); ptr+=4; return value;}
	float Read32F() {const u32 value = _byteswap_ulong(*((u32*)ptr)); ptr+=4; return *(float*)&value;}
	size_t GetRemainSize() const { return (int)(end - ptr); }
	u8 *GetPtr() const { return ptr; }
	void MoveEndForward() { end += 32; }
	u8 *GetEnd() const { return end; }
};

extern FifoReader fifo;

void Fifo_Init();
void Fifo_Shutdown();

#endif