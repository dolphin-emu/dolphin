#include "stdafx.h"
#include "XFStructs.h"
#include "Render.h"
#include "main.h"
#include "VertexHandler.h"
#include "Utils.h"


//XF state
ColorChannel colChans[2]; //C0A0 C1A1
TexCoordInfo texcoords[8];
MiscXF miscxf;
u32 xfmem[XFMEM_SIZE];

float rawViewPort[6];
float rawProjection[7];



#define BEGINSAVELOAD char *optr=ptr;
#define SAVELOAD(what,size) memcpy((void*)((save)?(void*)(ptr):(void*)(what)),(void*)((save)?(void*)(what):(void*)(ptr)),(size)); ptr+=(size);
#define ENDSAVELOAD return ptr-optr;


// __________________________________________________________________________________________________
// LoadXFReg 0x10
//
void LoadXFReg(u32 transferSize, u32 baseAddress, u32 *pData)
{	
    DVSTARTPROFILE();
	u32 address = baseAddress;
	for (int i=0; i<(int)transferSize; i++)
	{
		address = baseAddress + i;

		// Setup a Matrix
		if (address < 0x1000)
		{
			u32* p1 = &xfmem[address];
			memcpy(p1, &pData[i], transferSize*4);
			i += transferSize;
		}
		else if (address<0x2000)
		{
			u32 data = pData[i];	
			switch (address)
			{
			case 0x1006:
				//SetGPMetric
				break;
			case 0x1008: //__GXXfVtxSpecs, wrote 0004
				break;
			case 0x1009: //GXSetNumChans (no)
				break;
			case 0x100a: colChans[0].ambColor = data; break; //GXSetChanAmbientcolor
			case 0x100b: colChans[1].ambColor = data; break; //GXSetChanAmbientcolor
			case 0x100c: colChans[0].matColor = data; break; //GXSetChanMatcolor (rgba)
			case 0x100d: colChans[1].matColor = data; break; //GXSetChanMatcolor (rgba)

			case 0x100e: colChans[0].color.hex = data; break; //color0
			case 0x100f: colChans[1].color.hex = data; break; //color1
			case 0x1010: colChans[0].alpha.hex = data; break; //alpha0
			case 0x1011: colChans[1].alpha.hex = data; break; //alpha1

			case 0x1018:
				break;

			case 0x101a: 
				CVertexHandler::Flush();
				memcpy(rawViewPort,&pData[i],sizeof(rawViewPort)); 
				XFUpdateVP(); 
				i += 6;
				break;

			case 0x1020: 
				CVertexHandler::Flush();
				memcpy(rawProjection,&pData[i],sizeof(rawProjection)); 
				XFUpdatePJ(); 
				i += 7;
				return;

			case 0x103f: 
				miscxf.numTexGens = data;
				break;

			case 0x1040: texcoords[0].texmtxinfo.hex = data; break;
			case 0x1041: texcoords[1].texmtxinfo.hex = data; break;
			case 0x1042: texcoords[2].texmtxinfo.hex = data; break;
			case 0x1043: texcoords[3].texmtxinfo.hex = data; break;
			case 0x1044: texcoords[4].texmtxinfo.hex = data; break;
			case 0x1045: texcoords[5].texmtxinfo.hex = data; break;
			case 0x1046: texcoords[6].texmtxinfo.hex = data; break;
			case 0x1047: texcoords[7].texmtxinfo.hex = data; break;

			case 0x1050: texcoords[0].postmtxinfo.hex = data; break;
			case 0x1051: texcoords[1].postmtxinfo.hex = data; break;
			case 0x1052: texcoords[2].postmtxinfo.hex = data; break;
			case 0x1053: texcoords[3].postmtxinfo.hex = data; break;
			case 0x1054: texcoords[4].postmtxinfo.hex = data; break;
			case 0x1055: texcoords[5].postmtxinfo.hex = data; break;
			case 0x1056: texcoords[6].postmtxinfo.hex = data; break;
			case 0x1057: texcoords[7].postmtxinfo.hex = data; break;

			default:
				break;
			}
		}
		else if (address>=0x4000)
		{
			// MessageBox(NULL, "1", "1", MB_OK);
			//4010 __GXSetGenMode
		}
	}
}

// Check docs for this sucker...
void LoadIndexedXF(u32 val, int array)
{
    DVSTARTPROFILE();

	int index = val>>16;
	int address = val&0xFFF; //check mask
	int size = ((val>>12)&0xF)+1;
	//load stuff from array to address in xf mem
	for (int i = 0; i < size; i++)
		xfmem[address + i] = Memory_Read_U32(arraybases[array] + arraystrides[array]*index + i*4);
}

void XFUpdateVP()
{
	Renderer::SetViewport(rawViewPort); 
}
void XFUpdatePJ()
{
	Renderer::SetProjection(rawProjection,0); 
}

size_t XFSaveLoadState(char *ptr, BOOL save)
{
	BEGINSAVELOAD;
	SAVELOAD(colChans,2*sizeof(ColorChannel));
	SAVELOAD(texcoords,16*sizeof(TexCoordInfo));
	SAVELOAD(&miscxf,sizeof(MiscXF));
	SAVELOAD(rawViewPort,sizeof(rawViewPort));
	SAVELOAD(rawProjection,sizeof(rawProjection));
	SAVELOAD(xfmem,XFMEM_SIZE*sizeof(u32));

	if (!save)
	{
		XFUpdateVP();
		XFUpdatePJ();
	}

	ENDSAVELOAD;
}