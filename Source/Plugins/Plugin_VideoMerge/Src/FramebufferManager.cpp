
#include "FramebufferManager.h"

#include "VideoConfig.h"
#include "Renderer.h"

#include "Main.h"

FramebufferManagerBase::VirtualXFBListType FramebufferManagerBase::m_virtualXFBList; // Only used in Virtual XFB mode

const XFBSourceBase* FramebufferManagerBase::m_overlappingXFBArray[];

FramebufferManagerBase::~FramebufferManagerBase()
{
	VirtualXFBListType::iterator
		it = m_virtualXFBList.begin(),
		vlend = m_virtualXFBList.end();
	for (; it != vlend; ++it)
		delete it->xfbSource;
}

const XFBSourceBase** FramebufferManagerBase::GetXFBSource(u32 xfbAddr, u32 fbWidth, u32 fbHeight, u32 &xfbCount)
{
	//if (g_ActiveConfig.bUseRealXFB)
	//	return getRealXFBSource(xfbAddr, fbWidth, fbHeight, xfbCount);
	//else
		return getVirtualXFBSource(xfbAddr, fbWidth, fbHeight, xfbCount);
}

inline bool addrRangesOverlap(u32 aLower, u32 aUpper, u32 bLower, u32 bUpper)
{
	return !((aLower >= bUpper) || (bLower >= aUpper));
}

void FramebufferManagerBase::CopyToXFB(u32 xfbAddr, u32 fbWidth, u32 fbHeight, const EFBRectangle& sourceRc)
{
	//if (g_ActiveConfig.bUseRealXFB)
	//	copyToRealXFB(xfbAddr, fbWidth, fbHeight, sourceRc);
	//else
		copyToVirtualXFB(xfbAddr, fbWidth, fbHeight, sourceRc);
}

void FramebufferManagerBase::replaceVirtualXFB()
{
	VirtualXFBListType::iterator
		it = m_virtualXFBList.begin(),
		vlend = m_virtualXFBList.end();

	VirtualXFB *vxfb = &*it;

	const s32 srcLower = vxfb->xfbAddr;
	const s32 srcUpper = vxfb->xfbAddr + 2 * vxfb->xfbWidth * vxfb->xfbHeight;
	const s32 lineSize = 2 * vxfb->xfbWidth;

	while (++it != vlend)
	{
		vxfb = &*it;

		const s32 dstLower = vxfb->xfbAddr;
		const s32 dstUpper = vxfb->xfbAddr + 2 * vxfb->xfbWidth * vxfb->xfbHeight;

		if (dstLower >= srcLower && dstUpper <= srcUpper)
		{
			// invalidate the data
			vxfb->xfbAddr = 0;
			vxfb->xfbHeight = 0;
			vxfb->xfbWidth = 0;
		}
		else if (addrRangesOverlap(srcLower, srcUpper, dstLower, dstUpper))
		{		
			const s32 upperOverlap = (srcUpper - dstLower) / lineSize;
			const s32 lowerOverlap = (dstUpper - srcLower) / lineSize;

			if (upperOverlap > 0 && lowerOverlap < 0)
			{
				vxfb->xfbAddr += lineSize * upperOverlap;
				vxfb->xfbHeight -= upperOverlap;
			}
			else if (lowerOverlap > 0)
			{
				vxfb->xfbHeight -= lowerOverlap;
			}
		}
	}
}

FramebufferManagerBase::VirtualXFBListType::iterator
FramebufferManagerBase::findVirtualXFB(u32 xfbAddr, u32 width, u32 height)
{
	const u32 srcLower = xfbAddr;
	const u32 srcUpper = xfbAddr + 2 * width * height;

	VirtualXFBListType::iterator
		it = m_virtualXFBList.begin(),
		vlend = m_virtualXFBList.end();
	for (; it != vlend; ++it)
	{
		const VirtualXFB &vxfb = *it;

		const u32 dstLower = vxfb.xfbAddr;
		const u32 dstUpper = vxfb.xfbAddr + 2 * vxfb.xfbWidth * vxfb.xfbHeight;

		if (dstLower >= srcLower && dstUpper <= srcUpper)
			return it;
	}

	// That address is not in the Virtual XFB list.
	return m_virtualXFBList.end();
}

void FramebufferManagerBase::copyToVirtualXFB(u32 xfbAddr, u32 fbWidth, u32 fbHeight, const EFBRectangle& sourceRc)
{
	const VirtualXFBListType::iterator it = findVirtualXFB(xfbAddr, fbWidth, fbHeight);
	
	VirtualXFB *vxfb = NULL;

	if (m_virtualXFBList.end() == it)
	{
		if (m_virtualXFBList.size() >= MAX_VIRTUAL_XFB)
		{
			PanicAlert("Requested creating a new virtual XFB although the maximum number has already been reached! Report this to the devs");
			return;
			// TODO, possible alternative to failing: just delete the oldest virtual XFB:
			// delete m_virtualXFBList.back().xfbSource;
			// m_virtualXFBList.pop_back();
		}
		else
		{
			// create a new Virtual XFB and place it at the front of the list
			VirtualXFB v;
			m_virtualXFBList.push_front(v);
			vxfb = &m_virtualXFBList.front();
		}
	}
	else
	{
		vxfb = &*it;
		delete vxfb->xfbSource;
	}

	vxfb->xfbAddr = xfbAddr;
	vxfb->xfbWidth = fbWidth;
	vxfb->xfbHeight = fbHeight;

	const float scaleX = RendererBase::GetXFBScaleX();
	const float scaleY = RendererBase::GetXFBScaleY();

	TargetRectangle targetSource;
	targetSource.top = (int)(sourceRc.top * scaleY);
	targetSource.bottom = (int)(sourceRc.bottom * scaleY);
	targetSource.left = (int)(sourceRc.left * scaleX);
	targetSource.right = (int)(sourceRc.right * scaleX);

	const unsigned int target_width = targetSource.right - targetSource.left;
	const unsigned int target_height = targetSource.bottom - targetSource.top;

	vxfb->xfbSource = g_framebuffer_manager->CreateXFBSource(target_width, target_height);
	if (NULL == vxfb->xfbSource)
	{
		PanicAlert("Failed to create virtual XFB");
		return;
	}

	// why do both of these have a height/width/addr ?
	vxfb->xfbSource->srcAddr = xfbAddr;
	vxfb->xfbSource->srcWidth = fbWidth;
	vxfb->xfbSource->srcHeight = fbHeight;

	vxfb->xfbSource->texWidth = target_width;
	vxfb->xfbSource->texHeight = target_height;

	if (m_virtualXFBList.end() != it)
	{
		// move this Virtual XFB to the front of the list.
		m_virtualXFBList.splice(m_virtualXFBList.begin(), m_virtualXFBList, it);

		// keep stale XFB data from being used
		replaceVirtualXFB();
	}
  
	g_renderer->ResetAPIState(); // reset any game specific settings

	vxfb->xfbSource->CopyEFB(RendererBase::ConvertEFBRectangle(sourceRc));

	g_renderer->RestoreAPIState();
}

const XFBSourceBase** FramebufferManagerBase::getVirtualXFBSource(u32 xfbAddr, u32 fbWidth, u32 fbHeight, u32 &xfbCount)
{
	xfbCount = 0;

	if (0 == m_virtualXFBList.size())  // no Virtual XFBs available
		return NULL;

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

		if (addrRangesOverlap(srcLower, srcUpper, dstLower, dstUpper))
		{
			m_overlappingXFBArray[xfbCount] = vxfb->xfbSource;
			xfbCount++;
		}
	}

	return &m_overlappingXFBArray[0];
}
