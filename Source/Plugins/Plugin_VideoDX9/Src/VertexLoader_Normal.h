//__________________________________________________________________________________________________
// F|RES 2003-2005
//

#pragma	once

#include "CommonTypes.h"

class VertexLoader_Normal
{
public:

	static bool index3;
	// Init
	static void Init(void);

	// GetSize
	static unsigned int GetSize(unsigned int _type, unsigned int _format, unsigned int _elements);

	// GetFunction
	static TPipelineFunction GetFunction(unsigned int _type, unsigned int _format, unsigned int _elements);

private:
	enum ENormalType
	{
		NRM_NOT_PRESENT		= 0,
		NRM_DIRECT			= 1,
		NRM_INDEX8			= 2,
		NRM_INDEX16			= 3,
		NUM_NRM_TYPE
	};

	enum ENormalFormat
	{
		FORMAT_UBYTE		= 0,
		FORMAT_BYTE			= 1,
		FORMAT_USHORT		= 2,
		FORMAT_SHORT		= 3,
		FORMAT_FLOAT		= 4,
		NUM_NRM_FORMAT
	};

	enum ENormalElements
	{
		NRM_NBT				= 0,
		NRM_NBT3			= 1,
		NUM_NRM_ELEMENTS
	};

	// tables
	static u8 m_sizeTable[NUM_NRM_TYPE][NUM_NRM_FORMAT][NUM_NRM_ELEMENTS];
	static TPipelineFunction m_funcTable[NUM_NRM_TYPE][NUM_NRM_FORMAT][NUM_NRM_ELEMENTS];

	// direct
	static void LOADERDECL Normal_DirectByte(void* _p);
	static void LOADERDECL Normal_DirectShort(void* _p);
	static void LOADERDECL Normal_DirectFloat(void* _p);
	static void LOADERDECL Normal_DirectByte3(void* _p);
	static void LOADERDECL Normal_DirectShort3(void* _p);
	static void LOADERDECL Normal_DirectFloat3(void* _p);

	// index8
	static void LOADERDECL Normal_Index8_Byte(void* _p);
	static void LOADERDECL Normal_Index8_Short(void* _p);
	static void LOADERDECL Normal_Index8_Float(void* _p);
	static void LOADERDECL Normal_Index8_Byte3(void* _p);
	static void LOADERDECL Normal_Index8_Short3(void* _p);
	static void LOADERDECL Normal_Index8_Float3(void* _p);

	// index16
	static void LOADERDECL Normal_Index16_Byte(void* _p);
	static void LOADERDECL Normal_Index16_Short(void* _p);
	static void LOADERDECL Normal_Index16_Float(void* _p);
	static void LOADERDECL Normal_Index16_Byte3(void* _p);
	static void LOADERDECL Normal_Index16_Short3(void* _p);
	static void LOADERDECL Normal_Index16_Float3(void* _p);
};