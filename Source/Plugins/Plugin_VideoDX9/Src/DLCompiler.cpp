#include "stdafx.h"

#if 0

#include "OpcodeDecoding.h"	
#include "VertexLoader.h"
#include "VertexHandler.h"
#include "DataReader.h"
#include "BPStructs.h"
#include "CPStructs.h"
#include "XFStructs.h"
#include "DLCompiler.h"
#include "x86.h"
#include "main.h"
#include "Utils.h"

CompiledDList::CompiledDList(u32 _addr, u32 _size)
{
	dataSize = 0;
	data = 0;
	code = 0;
	addr = _addr;
	size = _size;
	pass = 0;
	numBatches = 0;
	batches = 0;
}

CompiledDList::~CompiledDList()
{
	if (data)
		delete [] data;
	if (code)
		delete [] code; 
	if (batches)
		delete [] batches;
}


bool CompiledDList::Call()
{
	switch(pass) {
	case 0: // First compiling pass : find data size
		if (Pass1())
		{
			pass = 1;
			return true;
		}
		else
			return false;
	case 1: // Second compiling pass : actually compile
		//if pass1 succeeded, pass2 will too
		Pass2();
		pass = 2;
		return true;
	case 2: // Run pass - we have a compiled dlist, just call it
		Run();
		return true;
	default:
		//ERROR
		return false;
	}
}


bool CompiledDList::Pass1()
{
/*	//find the size of code + data, if the dlist is worth recompiling etc
	// at the same time, do the ordinary stuff
	g_pDataReader = &dlistReader;
	OpcodeReaders::SetDListReader(addr, addr+size);
	dataSize = 0;
	codeSize = 0;
	numBatches = 0;
	bool lastIsPrim = false;
	while (OpcodeReaders::IsDListOKToRead())
	{
		int Cmd = g_pDataReader->Read8();
		switch(Cmd)
		{
		case GX_LOAD_CP_REG: //0x08
			{
				u32 SubCmd = g_pDataReader->Read8();
				u32 Value = g_pDataReader->Read32();
				LoadCPReg(SubCmd,Value);
				//COMPILER
				codeSize+=13;
			}
			break;

		case GX_LOAD_XF_REG:
			{
				u32 Cmd2 = g_pDataReader->Read32();
				int dwTransferSize = ((Cmd2>>16)&15) + 1;
				DWORD dwAddress = Cmd2 & 0xFFFF;
				static u32 pData[16];
				for (int i=0; i<dwTransferSize; i++)
					pData[i] = g_pDataReader->Read32();
				LoadXFReg(dwTransferSize,dwAddress,pData);
				//COMPILER
				dataSize+=dwTransferSize;
				codeSize+=17;
			}
			break;

		case GX_LOAD_BP_REG: //0x61
			{
				u32 cmd=g_pDataReader->Read32();
				LoadBPReg(cmd);
				codeSize+=9;
			}
			break;

		case GX_LOAD_INDX_A: //used for position matrices
			LoadIndexedXF(g_pDataReader->Read32(),0xC);
			codeSize+=13;
			break;
		case GX_LOAD_INDX_B: //used for normal matrices
			LoadIndexedXF(g_pDataReader->Read32(),0xD);
			codeSize+=13;
			break;
		case GX_LOAD_INDX_C: //used for postmatrices
			LoadIndexedXF(g_pDataReader->Read32(),0xE);
			codeSize+=13;
			break;
		case GX_LOAD_INDX_D: //used for lights
			LoadIndexedXF(g_pDataReader->Read32(),0xF);
			codeSize+=13;
			break;

		case GX_CMD_CALL_DL:
			MessageBox(0,"Display lists can't recurse!!","error",0);
			break;

		case GX_CMD_INVL_VC:// Invalidate	(vertex cache?)	
			break;
		case GX_NOP:
			break;
		default:
			if (Cmd&0x80)
			{
				int primitive = (Cmd&GX_PRIMITIVE_MASK) >> GX_PRIMITIVE_SHIFT;
				if (lastIsPrim)
				{
					//join to last
				}
				else
				{
					//finish up last and commit
				}
				u16 numVertices = g_pDataReader->Read16();
				tempvarray.Reset();
				VertexLoader::SetVArray(&tempvarray);
				VertexLoader *loader = &VertexLoader[Cmd&GX_VAT_MASK];
				loader->Setup();
				loader->PrepareRun();
				int vsize = loader->GetVertexSize();
				loader->RunVertices(numVertices);
				CVertexHandler::DrawVertices(primitive, numVertices, &tempvarray);
				CVertexHandler::Flush();
				//COMPILER
				codeSize+=21;
				numBatches++;
				lastIsPrim = true;
			}
			break;
		} 
	}
	if (lastIsPrim)
	{
		//finish up last and commit
	}
	codeSize*=2;*/
	return true;
}


void CompiledDList::Pass2()
{
/*	OpcodeReaders::SetDListReader(addr, addr+size);

	data = new u32[dataSize];
	code = new u8[codeSize]; //at least

	batches = new Batch[numBatches];
	int batchCount = 0;
	u32 *dataptr = data;

	x86Init();
	x86SetPtr((s8*)code);
	//WC8(0xCC);

    //actually do the recompiling, emit code and data, protect the memory
	// but again, at the same time do the ordinary stuff
	// so the compiled display list won't be run until the third time actually
	bool dump = false,lastIsGeom=false;
	FILE *f;

#ifndef TEASER
	if (dump)
	{
		f=fopen("D:\\dlistlogs.txt","a");
		fprintf(f,"===========================================\n");
	}
#endif

	while (OpcodeReaders::IsDListOKToRead())
	{
		int Cmd = g_pDataReader->Read8();
		switch(Cmd)
		{
		case GX_LOAD_CP_REG: //0x08
			{
				lastIsGeom = false;
				u32 SubCmd = g_pDataReader->Read8();
				u32 Value = g_pDataReader->Read32();
				if (dump)
					fprintf(f,"CP   | %02x %08x\n",SubCmd,Value);

				LoadCPReg(SubCmd,Value);
				//COMPILER
				PUSH_WordToStack(Value);
				PUSH_WordToStack(SubCmd);
				CALLFunc((u32)LoadCPReg);
			}
			break;

		case GX_LOAD_XF_REG:
			{
				lastIsGeom = false;
				u32 Cmd2 = g_pDataReader->Read32();
				int dwTransferSize = ((Cmd2>>16)&15) + 1;
				u32 dwAddress = Cmd2 & 0xFFFF;
				static u32 pData[16];

				u32 *oldDataPtr = dataptr;
				
				if (dump)
				{
					fprintf(f,"XF   | %01xx %04x\n",dwTransferSize,dwAddress);
					for (int i=0; i<dwTransferSize; i++)
						fprintf(f, "%08x | %f\n",oldDataPtr[i], *((float*)oldDataPtr+i));
				}


				for (int i=0; i<dwTransferSize; i++) // a little compiler here too
					*dataptr++ = g_pDataReader->Read32();


				LoadXFReg(dwTransferSize,dwAddress,oldDataPtr);
				
				//COMPILER
				PUSH_WordToStack((u32)oldDataPtr);
				PUSH_WordToStack(dwAddress);
				PUSH_WordToStack(dwTransferSize);
				CALLFunc((u32)LoadXFReg);
			}
			break;

		case GX_LOAD_BP_REG: //0x61
			{
				lastIsGeom = false;
				u32 cmd=g_pDataReader->Read32();
				if (dump)
					fprintf(f,"BP   | %08x\n",cmd);

				LoadBPReg(cmd);
				//COMPILER
				PUSH_WordToStack(cmd);
				CALLFunc((u32)LoadBPReg);
			}
			break;

		case GX_LOAD_INDX_A: //usually used for position matrices
			{
				lastIsGeom = false;
				u32 value = g_pDataReader->Read32();
				LoadIndexedXF(value,0xC);
				//COMPILER
				PUSH_WordToStack(0xC);
				PUSH_WordToStack(value);
				CALLFunc((u32)LoadIndexedXF);
				if (dump)
					fprintf(f,"LOADINDEXA | pos matrix\n");
			}
			break;
		case GX_LOAD_INDX_B: //usually used for normal matrices
			{
				lastIsGeom = false;
				u32 value = g_pDataReader->Read32();
				LoadIndexedXF(value,0xD);
				//COMPILER
				PUSH_WordToStack(0xD);
				PUSH_WordToStack(value);
				CALLFunc((u32)LoadIndexedXF);
				if (dump)
					fprintf(f,"LOADINDEXB | nrm matrix\n");
			}
			break;
		case GX_LOAD_INDX_C: //usually used for postmatrices
			{
				lastIsGeom = false;
				u32 value = g_pDataReader->Read32();
				LoadIndexedXF(value,0xE);
				//COMPILER
				PUSH_WordToStack(0xE);
				PUSH_WordToStack(value);
				CALLFunc((u32)LoadIndexedXF);
				if (dump)
					fprintf(f,"LOADINDEXC | post matrix\n");
			}
			break;
		case GX_LOAD_INDX_D: //usually used for lights
			{
				lastIsGeom = false;
				u32 value = g_pDataReader->Read32();
				LoadIndexedXF(value,0xF);
				//COMPILER
				PUSH_WordToStack(0xF);
				PUSH_WordToStack(value);
				CALLFunc((u32)LoadIndexedXF);
				if (dump)
					fprintf(f,"LOADINDEXD | light\n");
			}
			break;
		case GX_CMD_CALL_DL:
			// ERORRR
			break;

		case GX_CMD_INVL_VC:// Invalidate	(vertex cache?)	
			if (dump)
				fprintf(f,"invalidate vc\n");
			break;
		case GX_NOP:
			if (dump)
				fprintf(f,"nop\n");
			break;
		default:
			if (Cmd&0x80)
			{
				int primitive = (Cmd&GX_PRIMITIVE_MASK) >> GX_PRIMITIVE_SHIFT;
				//if (lastIsGeom) INCSTAT(stats.numJoins);
				u16 numVertices = g_pDataReader->Read16();
				if (dump)
					fprintf(f,"DP: prim=%02x numv=%i\n",primitive,numVertices);
				DecodedVArray &va = batches[batchCount].varray;

				VertexLoader *loader = &VertexLoader[Cmd&GX_VAT_MASK];
				TVtxDesc &vd = loader->GetVtxDesc();

				VertexLoader::SetVArray(&va);
				loader->Setup();
				loader->PrepareRun();
//					va.numColors  = loader->GetNumColors();
//					va.numUVs     = loader->GetNumTCs();
//					va.numNormals = loader->GetNumNormals();
				//va.num
				va.Create(numVertices,vd.PosMatIdx,
					vd.Tex0MatIdx+vd.Tex1MatIdx+vd.Tex2MatIdx+vd.Tex3MatIdx+
					vd.Tex4MatIdx+vd.Tex5MatIdx+vd.Tex6MatIdx+vd.Tex7MatIdx,
					va.numNormals, va.numColors, va.numTCs);
				
				int vsize = loader->GetVertexSize();
				loader->RunVertices(numVertices);
				CVertexHandler::DrawVertices(primitive, numVertices, &va);
				CVertexHandler::Flush();
				// YES we have now filled our varray
				//LETS COMPILE
				PUSH_WordToStack(primitive);
				PUSH_WordToStack(batchCount);
				PUSH_WordToStack((u32)this);
				CALLFunc((u32)DrawHelperHelper);
				batchCount++;
				lastIsGeom = true;
				if (dump)
					fprintf(f,"DRAW PRIMITIVE: prim=%02x numv=%i\n",primitive,numVertices);
			}
			break;
		}
	}
	if (dump)
	{
		fprintf(f,"***************************************\n\n\n");
	}
	RET();
	if (dump)
		fclose(f);*/
	//we're done, next time just kick the compiled list off, much much faster than interpreting!
}

void CompiledDList::DrawHelperHelper(CompiledDList *dl, int vno, int prim)
{
	Batch &b = dl->batches[vno];
	CVertexHandler::DrawVertices(prim, b.varray.GetSize(), &b.varray);
}

void CompiledDList::Run()
{
	//run the code
	((void (*)())(code))();
	CVertexHandler::Flush();
}

DListCache::DLCache DListCache::dlists;



void DListCache::Init()
{

}


void DListCache::Shutdown()
{
	DLCache::iterator iter = dlists.begin();
	for (;iter!=dlists.end();iter++)
		iter->second.Destroy();
	dlists.clear();
}


void DListCache::Call(u32 _addr, u32 _size)
{
	DLCache::iterator iter;
	iter = dlists.find(_addr);

	if (iter != dlists.end())
	{
		if (iter->second.size == _size)
		{
			iter->second.dlist->Call();
			return;
		}
		else // wrong size, need to recompile
		{
			iter->second.Destroy();
			iter=dlists.erase(iter);
		}
	}

		//Make an entry in the table
	DLCacheEntry entry;
	entry.dlist = new CompiledDList(_addr, _size);
	entry.dlist->Call();
	entry.frameCount = frameCount;
	entry.size = _size;
	dlists[_addr] = entry;

	INCSTAT(stats.numDListsCreated);
	SETSTAT(stats.numDListsAlive,(int)dlists.size());
}

void DListCache::Cleanup()
{
	for (DLCache::iterator iter=dlists.begin(); iter!=dlists.end();iter++)
	{
		DLCacheEntry &entry = iter->second;
		if (entry.frameCount<frameCount-80)
		{
			entry.Destroy();
			iter = dlists.erase(iter);
		}
	}
	SETSTAT(stats.numDListsAlive,(int)dlists.size());
}

#endif