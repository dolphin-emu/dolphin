
#ifndef _PIXELSHADERCACHE_H_
#define _PIXELSHADERCACHE_H_

class PixelShaderCacheBase
{
public:
	virtual ~PixelShaderCacheBase() {}

	virtual bool SetShader(bool dstAlphaEnable) = 0;

	virtual void SetPSConstant4f(unsigned int const_number, float f1, float f2, float f3, float f4) = 0;
	virtual void SetPSConstant4fv(unsigned int const_number, const float *f) = 0;
	virtual void SetMultiPSConstant4fv(unsigned int const_number, unsigned int count, const float *f) = 0;

protected:

};

#endif
