#include "Globals.h"

#include <fstream>
#include <vector>

#include "Config.h"
#include "Statistics.h"
#include "MemoryUtil.h"
#include "Profiler.h"
#include "Render.h"
#include "ImageWrite.h"
#include "BPMemory.h"
#include "TextureMngr.h"
#include "PixelShaderCache.h"
#include "PixelShaderManager.h"
#include "VertexShaderCache.h"
#include "VertexShaderManager.h"
#include "VertexShaderGen.h"
#include "VertexLoader.h"
#include "VertexManager.h"

#define MAX_BUFFER_SIZE 0x4000

// internal state for loading vertices
extern NativeVertexFormat *g_nativeVertexFmt;

namespace VertexManager
{

static GLuint s_vboBuffers[0x40] = {0};
static int s_nCurVBOIndex = 0; // current free buffer
static u8 *s_pBaseBufferPointer = NULL;
static std::vector< GLint > s_vertexFirstOffset;
static std::vector< GLsizei > s_vertexGroupSize;
static std::vector< std::pair< GLenum, int > > s_vertexGroups;
u32 s_vertexCount;

static const GLenum c_primitiveType[8] =
{
    GL_QUADS,
    0, //nothing
    GL_TRIANGLES,
    GL_TRIANGLE_STRIP,
    GL_TRIANGLE_FAN,
    GL_LINES,
    GL_LINE_STRIP,
    GL_POINTS
};

bool Init()
{
	s_pBaseBufferPointer = (u8*)AllocateMemoryPages(MAX_BUFFER_SIZE);
	s_pCurBufferPointer = s_pBaseBufferPointer;

	s_nCurVBOIndex = 0;
	glGenBuffers(ARRAYSIZE(s_vboBuffers), s_vboBuffers);
	for (u32 i = 0; i < ARRAYSIZE(s_vboBuffers); ++i) {
		glBindBuffer(GL_ARRAY_BUFFER, s_vboBuffers[i]);
		glBufferData(GL_ARRAY_BUFFER, MAX_BUFFER_SIZE, NULL, GL_STREAM_DRAW);
	}

	glEnableClientState(GL_VERTEX_ARRAY);
	g_nativeVertexFmt = NULL;
	GL_REPORT_ERRORD();

	return true;
}

void Shutdown()
{
	FreeMemoryPages(s_pBaseBufferPointer, MAX_BUFFER_SIZE); s_pBaseBufferPointer = s_pCurBufferPointer = NULL;
	glDeleteBuffers(ARRAYSIZE(s_vboBuffers), s_vboBuffers);
	memset(s_vboBuffers, 0, sizeof(s_vboBuffers));

	s_vertexFirstOffset.resize(0);
	s_vertexGroupSize.resize(0);
	s_vertexGroups.resize(0);
	s_vertexCount = 0;
	s_nCurVBOIndex = 0;
	ResetBuffer();
}

void ResetBuffer()
{
	s_nCurVBOIndex = (s_nCurVBOIndex + 1) % ARRAYSIZE(s_vboBuffers);
	s_pCurBufferPointer = s_pBaseBufferPointer;
	s_vertexFirstOffset.resize(0);
	s_vertexGroupSize.resize(0);
	s_vertexGroups.resize(0);
	s_vertexCount = 0;
}

int GetRemainingSize()
{
	return MAX_BUFFER_SIZE - (int)(s_pCurBufferPointer - s_pBaseBufferPointer);
}

void AddVertices(int primitive, int numvertices)
{
	_assert_(numvertices > 0);
	_assert_(g_nativeVertexFmt != NULL);

	ADDSTAT(stats.thisFrame.numPrims, numvertices);
	if (!s_vertexGroups.empty() && s_vertexGroups.back().first == c_primitiveType[primitive]) {
		// We can join primitives for free here. Not likely to help much, though, but whatever...
		if (c_primitiveType[primitive] == GL_TRIANGLES ||
			c_primitiveType[primitive] == GL_LINES ||
			c_primitiveType[primitive] == GL_POINTS ||
			c_primitiveType[primitive] == GL_QUADS) {
			INCSTAT(stats.thisFrame.numPrimitiveJoins);
			// Easy join
			s_vertexGroupSize.back() += numvertices;
			s_vertexCount += numvertices;
			return;
		}
	}

	s_vertexFirstOffset.push_back(s_vertexCount);
	s_vertexGroupSize.push_back(numvertices);
	s_vertexCount += numvertices;
	if (!s_vertexGroups.empty() && s_vertexGroups.back().first == c_primitiveType[primitive])
		s_vertexGroups.back().second++;
	else
		s_vertexGroups.push_back(std::make_pair(c_primitiveType[primitive], 1));

#if defined(_DEBUG) || defined(DEBUGFAST) 
	static const char *sprims[8] = {"quads", "nothing", "tris", "tstrip", "tfan", "lines", "lstrip", "points"};
	PRIM_LOG("prim: %s, c=%d\n", sprims[primitive], numvertices);
#endif
}

void Flush()
{
	if (s_vertexCount == 0)
		return;

	_assert_(s_pCurBufferPointer != s_pBaseBufferPointer);

#if defined(_DEBUG) || defined(DEBUGFAST) 
	PRIM_LOG("frame%d:\ncomps=0x%x, numchan=%d, dualtex=%d, ztex=%d, cole=%d, alpe=%d, ze=%d\n", g_Config.iSaveTargetId, xfregs.numTexGens,
		xfregs.nNumChans, (int)xfregs.bEnableDualTexTransform, bpmem.ztex2.op,
		bpmem.blendmode.colorupdate, bpmem.blendmode.alphaupdate, bpmem.zmode.updateenable);
	for (int i = 0; i < xfregs.nNumChans; ++i) {
		LitChannel* ch = &xfregs.colChans[i].color;
		PRIM_LOG("colchan%d: matsrc=%d, light=0x%x, ambsrc=%d, diffunc=%d, attfunc=%d\n", i, ch->matsource, ch->GetFullLightMask(), ch->ambsource, ch->diffusefunc, ch->attnfunc);
		ch = &xfregs.colChans[i].alpha;
		PRIM_LOG("alpchan%d: matsrc=%d, light=0x%x, ambsrc=%d, diffunc=%d, attfunc=%d\n", i, ch->matsource, ch->GetFullLightMask(), ch->ambsource, ch->diffusefunc, ch->attnfunc);
	}

	for (int i = 0; i < xfregs.numTexGens; ++i) {
		TexMtxInfo tinfo = xfregs.texcoords[i].texmtxinfo;
		if (tinfo.texgentype != XF_TEXGEN_EMBOSS_MAP) tinfo.hex &= 0x7ff;
		if (tinfo.texgentype != XF_TEXGEN_REGULAR) tinfo.projection = 0;

		PRIM_LOG("txgen%d: proj=%d, input=%d, gentype=%d, srcrow=%d, embsrc=%d, emblght=%d, postmtx=%d, postnorm=%d\n",
			i, tinfo.projection, tinfo.inputform, tinfo.texgentype, tinfo.sourcerow, tinfo.embosssourceshift, tinfo.embosslightshift,
			xfregs.texcoords[i].postmtxinfo.index, xfregs.texcoords[i].postmtxinfo.normalize);
	}

	PRIM_LOG("pixel: tev=%d, ind=%d, texgen=%d, dstalpha=%d, alphafunc=0x%x\n", bpmem.genMode.numtevstages+1, bpmem.genMode.numindstages,
		bpmem.genMode.numtexgens, (u32)bpmem.dstalpha.enable, (bpmem.alphaFunc.hex>>16)&0xff);
#endif

	DVSTARTPROFILE();

	GL_REPORT_ERRORD(); 

	glBindBuffer(GL_ARRAY_BUFFER, s_vboBuffers[s_nCurVBOIndex]);
	glBufferData(GL_ARRAY_BUFFER, s_pCurBufferPointer - s_pBaseBufferPointer, s_pBaseBufferPointer, GL_STREAM_DRAW);
	GL_REPORT_ERRORD();

	// setup the pointers
	if(g_nativeVertexFmt)
		g_nativeVertexFmt->SetupVertexPointers();
	GL_REPORT_ERRORD();

	// set the textures
	{
		DVSTARTSUBPROFILE("VertexManager::Flush:textures");

		u32 usedtextures = 0;
		for (u32 i = 0; i < (u32)bpmem.genMode.numtevstages + 1; ++i) {
			if (bpmem.tevorders[i/2].getEnable(i & 1))
				usedtextures |= 1 << bpmem.tevorders[i/2].getTexMap(i & 1);
		}

		if (bpmem.genMode.numindstages > 0) {
			for (u32 i = 0; i < (u32)bpmem.genMode.numtevstages + 1; ++i) {
				if (bpmem.tevind[i].IsActive() && bpmem.tevind[i].bt < bpmem.genMode.numindstages) {
					usedtextures |= 1 << bpmem.tevindref.getTexMap(bpmem.tevind[i].bt);
				}
			}
		}

		u32 nonpow2tex = 0;
		for (int i = 0; i < 8; i++) {
			if (usedtextures & (1 << i)) {
				glActiveTexture(GL_TEXTURE0 + i);
			
				FourTexUnits &tex = bpmem.tex[i >> 2];
				TextureMngr::TCacheEntry* tentry = TextureMngr::Load(i, (tex.texImage3[i&3].image_base/* & 0x1FFFFF*/) << 5,
					tex.texImage0[i&3].width+1, tex.texImage0[i&3].height+1,
					tex.texImage0[i&3].format, tex.texTlut[i&3].tmem_offset<<9, tex.texTlut[i&3].tlut_format);

				if (tentry != NULL) {
					// texture loaded fine, set dims for pixel shader
					if (tentry->isNonPow2) {
						PixelShaderManager::SetTexDims(i, tentry->w, tentry->h, tentry->mode.wrap_s, tentry->mode.wrap_t);
						nonpow2tex |= 1 << i;
						if (tentry->mode.wrap_s > 0) nonpow2tex |= 1 << (8 + i);
						if (tentry->mode.wrap_t > 0) nonpow2tex |= 1 << (16 + i);
						TextureMngr::EnableTexRECT(i);
					}
					// if texture is power of two, set to ones (since don't need scaling)
					// (the above seems to have changed - we set the width and height here too.
					else 
					{
						PixelShaderManager::SetTexDims(i, tentry->w, tentry->h, 0, 0);
						TextureMngr::EnableTex2D(i);
					}
					if (g_Config.iLog & CONF_SAVETEXTURES) {
						// save the textures
						char strfile[255];
						sprintf(strfile, "%sframes/tex%.3d_%d.tga", FULL_DUMP_DIR, g_Config.iSaveTargetId, i);
						SaveTexture(strfile, tentry->isNonPow2?GL_TEXTURE_RECTANGLE_ARB:GL_TEXTURE_2D, tentry->texture, tentry->w, tentry->h);
					}
				}
				else {
					ERROR_LOG("error loading tex\n");
					TextureMngr::DisableStage(i); // disable since won't be used
				}
			}
			else {
				TextureMngr::DisableStage(i); // disable since won't be used
			}
		}

		PixelShaderManager::SetTexturesUsed(nonpow2tex);
	}

	FRAGMENTSHADER* ps = PixelShaderCache::GetShader();
	VERTEXSHADER* vs = VertexShaderCache::GetShader(g_nativeVertexFmt->m_components);

	bool bRestoreBuffers = false;
	if (Renderer::GetZBufferTarget()) {
		if (bpmem.zmode.updateenable) {
			if (!bpmem.blendmode.colorupdate) {
				Renderer::SetRenderMode(bpmem.blendmode.alphaupdate ? Renderer::RM_ZBufferAlpha : Renderer::RM_ZBufferOnly);    
			}
		}
		else {
			Renderer::SetRenderMode(Renderer::RM_Normal);
			// remove temporarily
			glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
			bRestoreBuffers = true;
		}
	} else {
		Renderer::SetRenderMode(Renderer::RM_Normal);
	}

	// set global constants
	VertexShaderManager::SetConstants(g_Config.bProjectionHax1, g_Config.bProjectionHax2);
	PixelShaderManager::SetConstants();

	// finally bind

	// TODO - cache progid, check if same as before. Maybe GL does this internally, though.
	// This is the really annoying problem with GL - you never know whether it's worth caching stuff yourself.
	if (vs) glBindProgramARB(GL_VERTEX_PROGRAM_ARB, vs->glprogid);
	if (ps) glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, ps->glprogid); // Lego Star Wars crashes here.

#if defined(_DEBUG) || defined(DEBUGFAST) 
	PRIM_LOG("\n");
#endif

	int groupStart = 0;
	for (unsigned i = 0; i < s_vertexGroups.size(); i++)
	{
		INCSTAT(stats.thisFrame.numDrawCalls);
		glMultiDrawArrays(s_vertexGroups[i].first,
		                  &s_vertexFirstOffset[groupStart],
		                  &s_vertexGroupSize[groupStart], 
		                  s_vertexGroups[i].second);
		groupStart += s_vertexGroups[i].second;
	}

#if defined(_DEBUG) || defined(DEBUGFAST) 
	if (g_Config.iLog & CONF_SAVESHADERS) {
		// save the shaders
		char strfile[255];
		sprintf(strfile, "%sframes/ps%.3d.txt", FULL_DUMP_DIR, g_Config.iSaveTargetId);
		std::ofstream fps(strfile);
		fps << ps->strprog.c_str();
		sprintf(strfile, "%sframes/vs%.3d.txt", FULL_DUMP_DIR, g_Config.iSaveTargetId);
		std::ofstream fvs(strfile);
		fvs << vs->strprog.c_str();
	}

	if (g_Config.iLog & CONF_SAVETARGETS) {
		char str[128];
		sprintf(str, "%sframes/targ%.3d.tga", FULL_DUMP_DIR, g_Config.iSaveTargetId);
		Renderer::SaveRenderTarget(str, 0);
	}
#endif
	g_Config.iSaveTargetId++;

	GL_REPORT_ERRORD();

	if (bRestoreBuffers) {
		GLenum s_drawbuffers[2] = {GL_COLOR_ATTACHMENT0_EXT, GL_COLOR_ATTACHMENT1_EXT};
		glDrawBuffers(2, s_drawbuffers);
		Renderer::SetColorMask();
	}

	ResetBuffer();
}

}  // namespace
