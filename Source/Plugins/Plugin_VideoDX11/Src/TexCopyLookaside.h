// TexCopyLookaside.h
// Nolan Check
// Created 4/4/2011

#ifndef _TEXCOPYLOOKASIDE_H
#define _TEXCOPYLOOKASIDE_H

#include "VideoCommon.h"
#include "D3DBase.h"

namespace DX11
{

class TexCopyLookaside
{

public:

	TexCopyLookaside();

	void Update(u32 dstAddr, unsigned int dstFormat, unsigned int srcFormat,
		const EFBRectangle& srcRect, bool isIntensity, bool scaleByHalf);

	// Returns NULL if texture could not be faked (caller should decode from
	// RAM instead)
	D3DTexture2D* FakeTexture(u32 ramAddr, u32 width, u32 height, u32 levels,
		u32 format, u32 tlutAddr, u32 tlutFormat);

	unsigned int GetRealWidth() const { return m_realW; }
	unsigned int GetRealHeight() const { return m_realH; }

	u64 GetHash() const { return m_hash; }
	void SetHash(u64 hash) { m_hash = hash; }

	bool IsDirty() const { return m_dirty; }
	void ResetDirty() { m_dirty = false; }

private:

	void FakeEncodeShade(D3DTexture2D* srcTex, unsigned int srcFormat,
		bool yuva, bool scale,
		unsigned int posX, unsigned int posY,
		unsigned int virtualW, unsigned int virtualH,
		const float* colorMatrix, const float* colorAdd);

	// Properties of the "real" texture: width, height, hash of encoded data, etc.
	unsigned int m_realW;
	unsigned int m_realH;
	unsigned int m_virtualW;
	unsigned int m_virtualH;
	unsigned int m_dstFormat;

	// This is not maintained by TexCopyLookaside. It must be handled externally.
	u64 m_hash;

	// Fake base: Created and updated at the time of EFB copy
	std::unique_ptr<D3DTexture2D> m_fakeBase;
	bool m_dirty;

};

}

#endif
