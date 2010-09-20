
#ifndef _FRAMEBUFFERMANAGER_H_
#define _FRAMEBUFFERMANAGER_H_

#include <list>

#include "CommonTypes.h"
#include "VideoCommon.h"

struct XFBSourceBase
{
	u32 srcAddr;
	u32 srcWidth;
	u32 srcHeight;

	unsigned int texWidth;
	unsigned int texHeight;

	virtual void CopyEFB(const TargetRectangle& efbSource) = 0;

	virtual ~XFBSourceBase() {}

protected:
	XFBSourceBase() : srcAddr(0), srcWidth(0), srcHeight(0), texWidth(0), texHeight(0) {}
};

class FramebufferManagerBase
{
public:

	enum
	{
		MAX_VIRTUAL_XFB = 8,
	};

	virtual ~FramebufferManagerBase();

	//virtual void Init(int targetWidth, int targetHeight, int msaaSamples, int msaaCoverageSamples) = 0;
	//virtual void Shutdown() = 0;

	static void replaceVirtualXFB();

	static void CopyToXFB(u32 xfbAddr, u32 fbWidth, u32 fbHeight, const EFBRectangle& sourceRc);

	static const XFBSourceBase** GetXFBSource(u32 xfbAddr, u32 fbWidth, u32 fbHeight, u32 &xfbCount);
	static const XFBSourceBase** getVirtualXFBSource(u32 xfbAddr, u32 fbWidth, u32 fbHeight, u32 &xfbCount);

//protected:
	struct VirtualXFB
	{
		// Address and size in GameCube RAM
		u32 xfbAddr;
		u32 xfbWidth;
		u32 xfbHeight;

		XFBSourceBase *xfbSource;
	};

	virtual XFBSourceBase* CreateXFBSource(unsigned int target_width, unsigned int target_height) = 0;

	typedef std::list<VirtualXFB> VirtualXFBListType;
	static VirtualXFBListType m_virtualXFBList; // used in virtual XFB mode

	static VirtualXFBListType::iterator findVirtualXFB(u32 xfbAddr, u32 width, u32 height);

	static void copyToVirtualXFB(u32 xfbAddr, u32 fbWidth, u32 fbHeight, const EFBRectangle& sourceRc);

	static const XFBSourceBase* m_overlappingXFBArray[MAX_VIRTUAL_XFB];
};

#endif
