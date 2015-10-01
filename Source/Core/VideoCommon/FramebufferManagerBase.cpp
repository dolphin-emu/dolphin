// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include "VideoCommon/FramebufferManagerBase.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/VR.h"

FramebufferManagerBase *g_framebuffer_manager;

XFBSourceBase *FramebufferManagerBase::m_realXFBSource; // Only used in Real XFB mode
FramebufferManagerBase::VirtualXFBListType FramebufferManagerBase::m_virtualXFBList; // Only used in Virtual XFB mode
const XFBSourceBase* FramebufferManagerBase::m_overlappingXFBArray[MAX_VIRTUAL_XFB];

unsigned int FramebufferManagerBase::s_last_xfb_width = 1;
unsigned int FramebufferManagerBase::s_last_xfb_height = 1;

unsigned int FramebufferManagerBase::m_EFBLayers = 1;

FramebufferManagerBase::FramebufferManagerBase()
{
	m_realXFBSource = nullptr;

	// can't hurt
	memset(m_overlappingXFBArray, 0, sizeof(m_overlappingXFBArray));
}

FramebufferManagerBase::~FramebufferManagerBase()
{
	for (VirtualXFB& vxfb : m_virtualXFBList)
	{
		delete vxfb.xfbSource;
	}
	m_virtualXFBList.clear();

	delete m_realXFBSource;
}

const XFBSourceBase* const* FramebufferManagerBase::GetXFBSource(u32 xfbAddr, u32 fbWidth, u32 fbHeight, u32* xfbCountP)
{
	if (!g_ActiveConfig.bUseXFB)
		return nullptr;

	if (g_ActiveConfig.bUseRealXFB)
		return GetRealXFBSource(xfbAddr, fbWidth, fbHeight, xfbCountP);
	else
		return GetVirtualXFBSource(xfbAddr, fbWidth, fbHeight, xfbCountP);
}

const XFBSourceBase* const* FramebufferManagerBase::GetRealXFBSource(u32 xfbAddr, u32 fbWidth, u32 fbHeight, u32* xfbCountP)
{
	*xfbCountP = 1;

	// recreate if needed
	if (m_realXFBSource && (m_realXFBSource->texWidth != fbWidth || m_realXFBSource->texHeight != fbHeight))
	{
		delete m_realXFBSource;
		m_realXFBSource = nullptr;
	}

	if (!m_realXFBSource)
		m_realXFBSource = g_framebuffer_manager->CreateXFBSource(fbWidth, fbHeight, 1);

	m_realXFBSource->srcAddr = xfbAddr;

	m_realXFBSource->srcWidth = MAX_XFB_WIDTH;
	m_realXFBSource->srcHeight = MAX_XFB_HEIGHT;

	m_realXFBSource->texWidth = fbWidth;
	m_realXFBSource->texHeight = fbHeight;

	// OpenGL texture coordinates originate at the lower left, which is why
	// sourceRc.top = fbHeight and sourceRc.bottom = 0.
	m_realXFBSource->sourceRc.left = 0;
	m_realXFBSource->sourceRc.top = fbHeight;
	m_realXFBSource->sourceRc.right = fbWidth;
	m_realXFBSource->sourceRc.bottom = 0;

	// Decode YUYV data from GameCube RAM
	m_realXFBSource->DecodeToTexture(xfbAddr, fbWidth, fbHeight);

	m_overlappingXFBArray[0] = m_realXFBSource;
	return &m_overlappingXFBArray[0];
}

const XFBSourceBase* const* FramebufferManagerBase::GetVirtualXFBSource(u32 xfbAddr, u32 fbWidth, u32 fbHeight, u32* xfbCountP)
{
	u32 xfbCount = 0;

	if (m_virtualXFBList.empty())  // no Virtual XFBs available
		return nullptr;

	u32 srcLower = xfbAddr;
	u32 srcUpper = xfbAddr + 2 * fbWidth * fbHeight;

	VirtualXFBListType::reverse_iterator
		it = m_virtualXFBList.rbegin(),
		vlend = m_virtualXFBList.rend();
	for (; it != vlend; ++it)
	{
		VirtualXFB* vxfb = &*it;

		u32 dstLower = vxfb->xfbAddr;
		u32 dstUpper = vxfb->xfbAddr + 2 * vxfb->xfbWidth * vxfb->xfbHeight;

		if (AddressRangesOverlap(srcLower, srcUpper, dstLower, dstUpper))
		{
			m_overlappingXFBArray[xfbCount] = vxfb->xfbSource;
			++xfbCount;
		}
	}

	*xfbCountP = xfbCount;
	return &m_overlappingXFBArray[0];
}

void FramebufferManagerBase::CopyToXFB(u32 xfbAddr, u32 fbStride, u32 fbHeight, const EFBRectangle& sourceRc, float Gamma)
{
	if (g_ActiveConfig.bUseRealXFB)
		g_framebuffer_manager->CopyToRealXFB(xfbAddr, fbStride, fbHeight, sourceRc, Gamma);
	else
		CopyToVirtualXFB(xfbAddr, fbStride, fbHeight, sourceRc, Gamma);
}

void FramebufferManagerBase::CopyToVirtualXFB(u32 xfbAddr, u32 fbStride, u32 fbHeight, const EFBRectangle& sourceRc, float Gamma)
{
	VirtualXFBListType::iterator vxfb = FindVirtualXFB(xfbAddr, sourceRc.GetWidth(), fbHeight);

	if (m_virtualXFBList.end() == vxfb)
	{
		if (m_virtualXFBList.size() < MAX_VIRTUAL_XFB)
		{
			// create a new Virtual XFB and place it at the front of the list
			m_virtualXFBList.emplace_front();
			vxfb = m_virtualXFBList.begin();
		}
		else
		{
			// Replace the last virtual XFB
			--vxfb;
		}
	}
	//else // replace existing virtual XFB

	// move this Virtual XFB to the front of the list.
	if (m_virtualXFBList.begin() != vxfb)
		m_virtualXFBList.splice(m_virtualXFBList.begin(), m_virtualXFBList, vxfb);

	unsigned int target_width, target_height;
	g_framebuffer_manager->GetTargetSize(&target_width, &target_height);

	// recreate if needed
	if (vxfb->xfbSource && (vxfb->xfbSource->texWidth != target_width || vxfb->xfbSource->texHeight != target_height))
	{
		delete vxfb->xfbSource;
		vxfb->xfbSource = nullptr;
	}

	if (!vxfb->xfbSource)
	{
		vxfb->xfbSource = g_framebuffer_manager->CreateXFBSource(target_width, target_height, m_EFBLayers);
		vxfb->xfbSource->texWidth = target_width;
		vxfb->xfbSource->texHeight = target_height;
	}

	vxfb->xfbSource->srcAddr = vxfb->xfbAddr = xfbAddr;
	vxfb->xfbSource->srcWidth = vxfb->xfbWidth = sourceRc.GetWidth();
	vxfb->xfbSource->srcHeight = vxfb->xfbHeight = fbHeight;

	if (g_has_hmd && g_ActiveConfig.bEnableVR)
		vxfb->xfbSource->sourceRc = g_renderer->ConvertEFBRectangle(EFBRectangle(0, 0, EFB_WIDTH, EFB_HEIGHT));
	else
		vxfb->xfbSource->sourceRc = g_renderer->ConvertEFBRectangle(sourceRc);

	// keep stale XFB data from being used
	ReplaceVirtualXFB();

	// Copy EFB data to XFB and restore render target again
	vxfb->xfbSource->CopyEFB(Gamma);
}

FramebufferManagerBase::VirtualXFBListType::iterator FramebufferManagerBase::FindVirtualXFB(u32 xfbAddr, u32 width, u32 height)
{
	const u32 srcLower = xfbAddr;
	const u32 srcUpper = xfbAddr + 2 * width * height;

	return std::find_if(m_virtualXFBList.begin(), m_virtualXFBList.end(), [srcLower, srcUpper](const VirtualXFB& xfb) {
		const u32 dstLower = xfb.xfbAddr;
		const u32 dstUpper = xfb.xfbAddr + 2 * xfb.xfbWidth * xfb.xfbHeight;

		return dstLower >= srcLower && dstUpper <= srcUpper;
	});
}

void FramebufferManagerBase::ReplaceVirtualXFB()
{
	VirtualXFBListType::iterator it = m_virtualXFBList.begin();

	const s32 srcLower = it->xfbAddr;
	const s32 srcUpper = it->xfbAddr + 2 * it->xfbWidth * it->xfbHeight;
	const s32 lineSize = 2 * it->xfbWidth;

	++it;

	for (; it != m_virtualXFBList.end(); ++it)
	{
		s32 dstLower = it->xfbAddr;
		s32 dstUpper = it->xfbAddr + 2 * it->xfbWidth * it->xfbHeight;

		if (dstLower >= srcLower && dstUpper <= srcUpper)
		{
			// Invalidate the data
			it->xfbAddr = 0;
			it->xfbHeight = 0;
			it->xfbWidth = 0;
		}
		else if (AddressRangesOverlap(srcLower, srcUpper, dstLower, dstUpper))
		{
			s32 upperOverlap = (srcUpper - dstLower) / lineSize;
			s32 lowerOverlap = (dstUpper - srcLower) / lineSize;

			if (upperOverlap > 0 && lowerOverlap < 0)
			{
				it->xfbAddr += lineSize * upperOverlap;
				it->xfbHeight -= upperOverlap;
			}
			else if (lowerOverlap > 0)
			{
				it->xfbHeight -= lowerOverlap;
			}
		}
	}
}

int FramebufferManagerBase::ScaleToVirtualXfbWidth(int x)
{
	if (g_ActiveConfig.RealXFBEnabled())
		return x;

	return x * (int)Renderer::GetTargetRectangle().GetWidth() / (int)FramebufferManagerBase::LastXfbWidth();
}

int FramebufferManagerBase::ScaleToVirtualXfbHeight(int y)
{
	if (g_ActiveConfig.RealXFBEnabled())
		return y;

	return y * (int)Renderer::GetTargetRectangle().GetHeight() / (int)FramebufferManagerBase::LastXfbHeight();
}
