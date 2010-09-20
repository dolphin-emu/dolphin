
#ifndef _VERTEXSHADERCACHE_H_
#define _VERTEXSHADERCACHE_H_

class VertexShaderCacheBase
{
public:
	virtual ~VertexShaderCacheBase() {}

	virtual bool SetShader(u32 components) = 0;

	virtual void SetVSConstant4f(unsigned int const_number, float f1, float f2, float f3, float f4) = 0;
	virtual void SetVSConstant4fv(unsigned int const_number, const float* f) = 0;
	virtual void SetMultiVSConstant3fv(unsigned int const_number, unsigned int count, const float* f) = 0;
	virtual void SetMultiVSConstant4fv(unsigned int const_number, unsigned int count, const float* f) = 0;
};

#endif
