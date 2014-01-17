// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "gl_common.h"

#ifndef GL_VERSION_1_1
#define GL_VERSION_1_1 1

/*
 * Datatypes
 */
typedef unsigned int	GLenum;
typedef unsigned char	GLboolean;
typedef unsigned int	GLbitfield;
typedef void		GLvoid;
typedef signed char	GLbyte;		/* 1-byte signed */
typedef short		GLshort;	/* 2-byte signed */
typedef int		GLint;		/* 4-byte signed */
typedef unsigned char	GLubyte;	/* 1-byte unsigned */
typedef unsigned short	GLushort;	/* 2-byte unsigned */
typedef unsigned int	GLuint;		/* 4-byte unsigned */
typedef int		GLsizei;	/* 4-byte signed */
typedef float		GLfloat;	/* single precision float */
typedef float		GLclampf;	/* single precision float in [0,1] */
typedef double		GLdouble;	/* double precision float */
typedef double		GLclampd;	/* double precision float in [0,1] */



/*
 * Constants
 */

/* Boolean values */
#define GL_FALSE				0
#define GL_TRUE					1

/* Data types */
#define GL_BYTE					0x1400
#define GL_UNSIGNED_BYTE			0x1401
#define GL_SHORT				0x1402
#define GL_UNSIGNED_SHORT			0x1403
#define GL_INT					0x1404
#define GL_UNSIGNED_INT				0x1405
#define GL_FLOAT				0x1406
#define GL_2_BYTES				0x1407
#define GL_3_BYTES				0x1408
#define GL_4_BYTES				0x1409
#define GL_DOUBLE				0x140A

/* Primitives */
#define GL_POINTS				0x0000
#define GL_LINES				0x0001
#define GL_LINE_LOOP				0x0002
#define GL_LINE_STRIP				0x0003
#define GL_TRIANGLES				0x0004
#define GL_TRIANGLE_STRIP			0x0005
#define GL_TRIANGLE_FAN				0x0006
#define GL_QUADS				0x0007
#define GL_QUAD_STRIP				0x0008
#define GL_POLYGON				0x0009

/* Vertex Arrays */
#define GL_VERTEX_ARRAY				0x8074
#define GL_NORMAL_ARRAY				0x8075
#define GL_COLOR_ARRAY				0x8076
#define GL_INDEX_ARRAY				0x8077
#define GL_TEXTURE_COORD_ARRAY			0x8078
#define GL_EDGE_FLAG_ARRAY			0x8079
#define GL_VERTEX_ARRAY_SIZE			0x807A
#define GL_VERTEX_ARRAY_TYPE			0x807B
#define GL_VERTEX_ARRAY_STRIDE			0x807C
#define GL_NORMAL_ARRAY_TYPE			0x807E
#define GL_NORMAL_ARRAY_STRIDE			0x807F
#define GL_COLOR_ARRAY_SIZE			0x8081
#define GL_COLOR_ARRAY_TYPE			0x8082
#define GL_COLOR_ARRAY_STRIDE			0x8083
#define GL_INDEX_ARRAY_TYPE			0x8085
#define GL_INDEX_ARRAY_STRIDE			0x8086
#define GL_TEXTURE_COORD_ARRAY_SIZE		0x8088
#define GL_TEXTURE_COORD_ARRAY_TYPE		0x8089
#define GL_TEXTURE_COORD_ARRAY_STRIDE		0x808A
#define GL_EDGE_FLAG_ARRAY_STRIDE		0x808C
#define GL_VERTEX_ARRAY_POINTER			0x808E
#define GL_NORMAL_ARRAY_POINTER			0x808F
#define GL_COLOR_ARRAY_POINTER			0x8090
#define GL_INDEX_ARRAY_POINTER			0x8091
#define GL_TEXTURE_COORD_ARRAY_POINTER		0x8092
#define GL_EDGE_FLAG_ARRAY_POINTER		0x8093
#define GL_V2F					0x2A20
#define GL_V3F					0x2A21
#define GL_C4UB_V2F				0x2A22
#define GL_C4UB_V3F				0x2A23
#define GL_C3F_V3F				0x2A24
#define GL_N3F_V3F				0x2A25
#define GL_C4F_N3F_V3F				0x2A26
#define GL_T2F_V3F				0x2A27
#define GL_T4F_V4F				0x2A28
#define GL_T2F_C4UB_V3F				0x2A29
#define GL_T2F_C3F_V3F				0x2A2A
#define GL_T2F_N3F_V3F				0x2A2B
#define GL_T2F_C4F_N3F_V3F			0x2A2C
#define GL_T4F_C4F_N3F_V4F			0x2A2D

/* Matrix Mode */
#define GL_MATRIX_MODE				0x0BA0
#define GL_MODELVIEW				0x1700
#define GL_PROJECTION				0x1701
#define GL_TEXTURE				0x1702

/* Points */
#define GL_POINT_SMOOTH				0x0B10
#define GL_POINT_SIZE				0x0B11
#define GL_POINT_SIZE_GRANULARITY 		0x0B13
#define GL_POINT_SIZE_RANGE			0x0B12

/* Lines */
#define GL_LINE_SMOOTH				0x0B20
#define GL_LINE_STIPPLE				0x0B24
#define GL_LINE_STIPPLE_PATTERN			0x0B25
#define GL_LINE_STIPPLE_REPEAT			0x0B26
#define GL_LINE_WIDTH				0x0B21
#define GL_LINE_WIDTH_GRANULARITY		0x0B23
#define GL_LINE_WIDTH_RANGE			0x0B22

/* Polygons */
#define GL_POINT				0x1B00
#define GL_LINE					0x1B01
#define GL_FILL					0x1B02
#define GL_CW					0x0900
#define GL_CCW					0x0901
#define GL_FRONT				0x0404
#define GL_BACK					0x0405
#define GL_POLYGON_MODE				0x0B40
#define GL_POLYGON_SMOOTH			0x0B41
#define GL_POLYGON_STIPPLE			0x0B42
#define GL_EDGE_FLAG				0x0B43
#define GL_CULL_FACE				0x0B44
#define GL_CULL_FACE_MODE			0x0B45
#define GL_FRONT_FACE				0x0B46
#define GL_POLYGON_OFFSET_FACTOR		0x8038
#define GL_POLYGON_OFFSET_UNITS			0x2A00
#define GL_POLYGON_OFFSET_POINT			0x2A01
#define GL_POLYGON_OFFSET_LINE			0x2A02
#define GL_POLYGON_OFFSET_FILL			0x8037

/* Display Lists */
#define GL_COMPILE				0x1300
#define GL_COMPILE_AND_EXECUTE			0x1301
#define GL_LIST_BASE				0x0B32
#define GL_LIST_INDEX				0x0B33
#define GL_LIST_MODE				0x0B30

/* Depth buffer */
#define GL_NEVER				0x0200
#define GL_LESS					0x0201
#define GL_EQUAL				0x0202
#define GL_LEQUAL				0x0203
#define GL_GREATER				0x0204
#define GL_NOTEQUAL				0x0205
#define GL_GEQUAL				0x0206
#define GL_ALWAYS				0x0207
#define GL_DEPTH_TEST				0x0B71
#define GL_DEPTH_BITS				0x0D56
#define GL_DEPTH_CLEAR_VALUE			0x0B73
#define GL_DEPTH_FUNC				0x0B74
#define GL_DEPTH_RANGE				0x0B70
#define GL_DEPTH_WRITEMASK			0x0B72
#define GL_DEPTH_COMPONENT			0x1902

/* Lighting */
#define GL_LIGHTING				0x0B50
#define GL_LIGHT0				0x4000
#define GL_LIGHT1				0x4001
#define GL_LIGHT2				0x4002
#define GL_LIGHT3				0x4003
#define GL_LIGHT4				0x4004
#define GL_LIGHT5				0x4005
#define GL_LIGHT6				0x4006
#define GL_LIGHT7				0x4007
#define GL_SPOT_EXPONENT			0x1205
#define GL_SPOT_CUTOFF				0x1206
#define GL_CONSTANT_ATTENUATION			0x1207
#define GL_LINEAR_ATTENUATION			0x1208
#define GL_QUADRATIC_ATTENUATION		0x1209
#define GL_AMBIENT				0x1200
#define GL_DIFFUSE				0x1201
#define GL_SPECULAR				0x1202
#define GL_SHININESS				0x1601
#define GL_EMISSION				0x1600
#define GL_POSITION				0x1203
#define GL_SPOT_DIRECTION			0x1204
#define GL_AMBIENT_AND_DIFFUSE			0x1602
#define GL_COLOR_INDEXES			0x1603
#define GL_LIGHT_MODEL_TWO_SIDE			0x0B52
#define GL_LIGHT_MODEL_LOCAL_VIEWER		0x0B51
#define GL_LIGHT_MODEL_AMBIENT			0x0B53
#define GL_FRONT_AND_BACK			0x0408
#define GL_SHADE_MODEL				0x0B54
#define GL_FLAT					0x1D00
#define GL_SMOOTH				0x1D01
#define GL_COLOR_MATERIAL			0x0B57
#define GL_COLOR_MATERIAL_FACE			0x0B55
#define GL_COLOR_MATERIAL_PARAMETER		0x0B56
#define GL_NORMALIZE				0x0BA1

/* User clipping planes */
#define GL_CLIP_PLANE0				0x3000
#define GL_CLIP_PLANE1				0x3001
#define GL_CLIP_PLANE2				0x3002
#define GL_CLIP_PLANE3				0x3003
#define GL_CLIP_PLANE4				0x3004
#define GL_CLIP_PLANE5				0x3005

/* Accumulation buffer */
#define GL_ACCUM_RED_BITS			0x0D58
#define GL_ACCUM_GREEN_BITS			0x0D59
#define GL_ACCUM_BLUE_BITS			0x0D5A
#define GL_ACCUM_ALPHA_BITS			0x0D5B
#define GL_ACCUM_CLEAR_VALUE			0x0B80
#define GL_ACCUM				0x0100
#define GL_ADD					0x0104
#define GL_LOAD					0x0101
#define GL_MULT					0x0103
#define GL_RETURN				0x0102

/* Alpha testing */
#define GL_ALPHA_TEST				0x0BC0
#define GL_ALPHA_TEST_REF			0x0BC2
#define GL_ALPHA_TEST_FUNC			0x0BC1

/* Blending */
#define GL_BLEND				0x0BE2
#define GL_BLEND_SRC				0x0BE1
#define GL_BLEND_DST				0x0BE0
#define GL_ZERO					0
#define GL_ONE					1
#define GL_SRC_COLOR				0x0300
#define GL_ONE_MINUS_SRC_COLOR			0x0301
#define GL_SRC_ALPHA				0x0302
#define GL_ONE_MINUS_SRC_ALPHA			0x0303
#define GL_DST_ALPHA				0x0304
#define GL_ONE_MINUS_DST_ALPHA			0x0305
#define GL_DST_COLOR				0x0306
#define GL_ONE_MINUS_DST_COLOR			0x0307
#define GL_SRC_ALPHA_SATURATE			0x0308

/* Render Mode */
#define GL_FEEDBACK				0x1C01
#define GL_RENDER				0x1C00
#define GL_SELECT				0x1C02

/* Feedback */
#define GL_2D					0x0600
#define GL_3D					0x0601
#define GL_3D_COLOR				0x0602
#define GL_3D_COLOR_TEXTURE			0x0603
#define GL_4D_COLOR_TEXTURE			0x0604
#define GL_POINT_TOKEN				0x0701
#define GL_LINE_TOKEN				0x0702
#define GL_LINE_RESET_TOKEN			0x0707
#define GL_POLYGON_TOKEN			0x0703
#define GL_BITMAP_TOKEN				0x0704
#define GL_DRAW_PIXEL_TOKEN			0x0705
#define GL_COPY_PIXEL_TOKEN			0x0706
#define GL_PASS_THROUGH_TOKEN			0x0700
#define GL_FEEDBACK_BUFFER_POINTER		0x0DF0
#define GL_FEEDBACK_BUFFER_SIZE			0x0DF1
#define GL_FEEDBACK_BUFFER_TYPE			0x0DF2

/* Selection */
#define GL_SELECTION_BUFFER_POINTER		0x0DF3
#define GL_SELECTION_BUFFER_SIZE		0x0DF4

/* Fog */
#define GL_FOG					0x0B60
#define GL_FOG_MODE				0x0B65
#define GL_FOG_DENSITY				0x0B62
#define GL_FOG_COLOR				0x0B66
#define GL_FOG_INDEX				0x0B61
#define GL_FOG_START				0x0B63
#define GL_FOG_END				0x0B64
#define GL_LINEAR				0x2601
#define GL_EXP					0x0800
#define GL_EXP2					0x0801

/* Logic Ops */
#define GL_LOGIC_OP				0x0BF1
#define GL_INDEX_LOGIC_OP			0x0BF1
#define GL_COLOR_LOGIC_OP			0x0BF2
#define GL_LOGIC_OP_MODE			0x0BF0
#define GL_CLEAR				0x1500
#define GL_SET					0x150F
#define GL_COPY					0x1503
#define GL_COPY_INVERTED			0x150C
#define GL_NOOP					0x1505
#define GL_INVERT				0x150A
#define GL_AND					0x1501
#define GL_NAND					0x150E
#define GL_OR					0x1507
#define GL_NOR					0x1508
#define GL_XOR					0x1506
#define GL_EQUIV				0x1509
#define GL_AND_REVERSE				0x1502
#define GL_AND_INVERTED				0x1504
#define GL_OR_REVERSE				0x150B
#define GL_OR_INVERTED				0x150D

/* Stencil */
#define GL_STENCIL_BITS				0x0D57
#define GL_STENCIL_TEST				0x0B90
#define GL_STENCIL_CLEAR_VALUE			0x0B91
#define GL_STENCIL_FUNC				0x0B92
#define GL_STENCIL_VALUE_MASK			0x0B93
#define GL_STENCIL_FAIL				0x0B94
#define GL_STENCIL_PASS_DEPTH_FAIL		0x0B95
#define GL_STENCIL_PASS_DEPTH_PASS		0x0B96
#define GL_STENCIL_REF				0x0B97
#define GL_STENCIL_WRITEMASK			0x0B98
#define GL_STENCIL_INDEX			0x1901
#define GL_KEEP					0x1E00
#define GL_REPLACE				0x1E01
#define GL_INCR					0x1E02
#define GL_DECR					0x1E03

/* Buffers, Pixel Drawing/Reading */
#define GL_NONE					0
#define GL_LEFT					0x0406
#define GL_RIGHT				0x0407
/*GL_FRONT					0x0404 */
/*GL_BACK					0x0405 */
/*GL_FRONT_AND_BACK				0x0408 */
#define GL_FRONT_LEFT				0x0400
#define GL_FRONT_RIGHT				0x0401
#define GL_BACK_LEFT				0x0402
#define GL_BACK_RIGHT				0x0403
#define GL_AUX0					0x0409
#define GL_AUX1					0x040A
#define GL_AUX2					0x040B
#define GL_AUX3					0x040C
#define GL_COLOR_INDEX				0x1900
#define GL_RED					0x1903
#define GL_GREEN				0x1904
#define GL_BLUE					0x1905
#define GL_ALPHA				0x1906
#define GL_LUMINANCE				0x1909
#define GL_LUMINANCE_ALPHA			0x190A
#define GL_ALPHA_BITS				0x0D55
#define GL_RED_BITS				0x0D52
#define GL_GREEN_BITS				0x0D53
#define GL_BLUE_BITS				0x0D54
#define GL_INDEX_BITS				0x0D51
#define GL_SUBPIXEL_BITS			0x0D50
#define GL_AUX_BUFFERS				0x0C00
#define GL_READ_BUFFER				0x0C02
#define GL_DRAW_BUFFER				0x0C01
#define GL_DOUBLEBUFFER				0x0C32
#define GL_STEREO				0x0C33
#define GL_BITMAP				0x1A00
#define GL_COLOR				0x1800
#define GL_DEPTH				0x1801
#define GL_STENCIL				0x1802
#define GL_DITHER				0x0BD0
#define GL_RGB					0x1907
#define GL_RGBA					0x1908

/* Implementation limits */
#define GL_MAX_LIST_NESTING			0x0B31
#define GL_MAX_EVAL_ORDER			0x0D30
#define GL_MAX_LIGHTS				0x0D31
#define GL_MAX_CLIP_PLANES			0x0D32
#define GL_MAX_TEXTURE_SIZE			0x0D33
#define GL_MAX_PIXEL_MAP_TABLE			0x0D34
#define GL_MAX_ATTRIB_STACK_DEPTH		0x0D35
#define GL_MAX_MODELVIEW_STACK_DEPTH		0x0D36
#define GL_MAX_NAME_STACK_DEPTH			0x0D37
#define GL_MAX_PROJECTION_STACK_DEPTH		0x0D38
#define GL_MAX_TEXTURE_STACK_DEPTH		0x0D39
#define GL_MAX_VIEWPORT_DIMS			0x0D3A
#define GL_MAX_CLIENT_ATTRIB_STACK_DEPTH	0x0D3B

/* Gets */
#define GL_ATTRIB_STACK_DEPTH			0x0BB0
#define GL_CLIENT_ATTRIB_STACK_DEPTH		0x0BB1
#define GL_COLOR_CLEAR_VALUE			0x0C22
#define GL_COLOR_WRITEMASK			0x0C23
#define GL_CURRENT_INDEX			0x0B01
#define GL_CURRENT_COLOR			0x0B00
#define GL_CURRENT_NORMAL			0x0B02
#define GL_CURRENT_RASTER_COLOR			0x0B04
#define GL_CURRENT_RASTER_DISTANCE		0x0B09
#define GL_CURRENT_RASTER_INDEX			0x0B05
#define GL_CURRENT_RASTER_POSITION		0x0B07
#define GL_CURRENT_RASTER_TEXTURE_COORDS	0x0B06
#define GL_CURRENT_RASTER_POSITION_VALID	0x0B08
#define GL_CURRENT_TEXTURE_COORDS		0x0B03
#define GL_INDEX_CLEAR_VALUE			0x0C20
#define GL_INDEX_MODE				0x0C30
#define GL_INDEX_WRITEMASK			0x0C21
#define GL_MODELVIEW_MATRIX			0x0BA6
#define GL_MODELVIEW_STACK_DEPTH		0x0BA3
#define GL_NAME_STACK_DEPTH			0x0D70
#define GL_PROJECTION_MATRIX			0x0BA7
#define GL_PROJECTION_STACK_DEPTH		0x0BA4
#define GL_RENDER_MODE				0x0C40
#define GL_RGBA_MODE				0x0C31
#define GL_TEXTURE_MATRIX			0x0BA8
#define GL_TEXTURE_STACK_DEPTH			0x0BA5
#define GL_VIEWPORT				0x0BA2

/* Evaluators */
#define GL_AUTO_NORMAL				0x0D80
#define GL_MAP1_COLOR_4				0x0D90
#define GL_MAP1_INDEX				0x0D91
#define GL_MAP1_NORMAL				0x0D92
#define GL_MAP1_TEXTURE_COORD_1			0x0D93
#define GL_MAP1_TEXTURE_COORD_2			0x0D94
#define GL_MAP1_TEXTURE_COORD_3			0x0D95
#define GL_MAP1_TEXTURE_COORD_4			0x0D96
#define GL_MAP1_VERTEX_3			0x0D97
#define GL_MAP1_VERTEX_4			0x0D98
#define GL_MAP2_COLOR_4				0x0DB0
#define GL_MAP2_INDEX				0x0DB1
#define GL_MAP2_NORMAL				0x0DB2
#define GL_MAP2_TEXTURE_COORD_1			0x0DB3
#define GL_MAP2_TEXTURE_COORD_2			0x0DB4
#define GL_MAP2_TEXTURE_COORD_3			0x0DB5
#define GL_MAP2_TEXTURE_COORD_4			0x0DB6
#define GL_MAP2_VERTEX_3			0x0DB7
#define GL_MAP2_VERTEX_4			0x0DB8
#define GL_MAP1_GRID_DOMAIN			0x0DD0
#define GL_MAP1_GRID_SEGMENTS			0x0DD1
#define GL_MAP2_GRID_DOMAIN			0x0DD2
#define GL_MAP2_GRID_SEGMENTS			0x0DD3
#define GL_COEFF				0x0A00
#define GL_ORDER				0x0A01
#define GL_DOMAIN				0x0A02

/* Hints */
#define GL_PERSPECTIVE_CORRECTION_HINT		0x0C50
#define GL_POINT_SMOOTH_HINT			0x0C51
#define GL_LINE_SMOOTH_HINT			0x0C52
#define GL_POLYGON_SMOOTH_HINT			0x0C53
#define GL_FOG_HINT				0x0C54
#define GL_DONT_CARE				0x1100
#define GL_FASTEST				0x1101
#define GL_NICEST				0x1102

/* Scissor box */
#define GL_SCISSOR_BOX				0x0C10
#define GL_SCISSOR_TEST				0x0C11

/* Pixel Mode / Transfer */
#define GL_MAP_COLOR				0x0D10
#define GL_MAP_STENCIL				0x0D11
#define GL_INDEX_SHIFT				0x0D12
#define GL_INDEX_OFFSET				0x0D13
#define GL_RED_SCALE				0x0D14
#define GL_RED_BIAS				0x0D15
#define GL_GREEN_SCALE				0x0D18
#define GL_GREEN_BIAS				0x0D19
#define GL_BLUE_SCALE				0x0D1A
#define GL_BLUE_BIAS				0x0D1B
#define GL_ALPHA_SCALE				0x0D1C
#define GL_ALPHA_BIAS				0x0D1D
#define GL_DEPTH_SCALE				0x0D1E
#define GL_DEPTH_BIAS				0x0D1F
#define GL_PIXEL_MAP_S_TO_S_SIZE		0x0CB1
#define GL_PIXEL_MAP_I_TO_I_SIZE		0x0CB0
#define GL_PIXEL_MAP_I_TO_R_SIZE		0x0CB2
#define GL_PIXEL_MAP_I_TO_G_SIZE		0x0CB3
#define GL_PIXEL_MAP_I_TO_B_SIZE		0x0CB4
#define GL_PIXEL_MAP_I_TO_A_SIZE		0x0CB5
#define GL_PIXEL_MAP_R_TO_R_SIZE		0x0CB6
#define GL_PIXEL_MAP_G_TO_G_SIZE		0x0CB7
#define GL_PIXEL_MAP_B_TO_B_SIZE		0x0CB8
#define GL_PIXEL_MAP_A_TO_A_SIZE		0x0CB9
#define GL_PIXEL_MAP_S_TO_S			0x0C71
#define GL_PIXEL_MAP_I_TO_I			0x0C70
#define GL_PIXEL_MAP_I_TO_R			0x0C72
#define GL_PIXEL_MAP_I_TO_G			0x0C73
#define GL_PIXEL_MAP_I_TO_B			0x0C74
#define GL_PIXEL_MAP_I_TO_A			0x0C75
#define GL_PIXEL_MAP_R_TO_R			0x0C76
#define GL_PIXEL_MAP_G_TO_G			0x0C77
#define GL_PIXEL_MAP_B_TO_B			0x0C78
#define GL_PIXEL_MAP_A_TO_A			0x0C79
#define GL_PACK_ALIGNMENT			0x0D05
#define GL_PACK_LSB_FIRST			0x0D01
#define GL_PACK_ROW_LENGTH			0x0D02
#define GL_PACK_SKIP_PIXELS			0x0D04
#define GL_PACK_SKIP_ROWS			0x0D03
#define GL_PACK_SWAP_BYTES			0x0D00
#define GL_UNPACK_ALIGNMENT			0x0CF5
#define GL_UNPACK_LSB_FIRST			0x0CF1
#define GL_UNPACK_ROW_LENGTH			0x0CF2
#define GL_UNPACK_SKIP_PIXELS			0x0CF4
#define GL_UNPACK_SKIP_ROWS			0x0CF3
#define GL_UNPACK_SWAP_BYTES			0x0CF0
#define GL_ZOOM_X				0x0D16
#define GL_ZOOM_Y				0x0D17

/* Texture mapping */
#define GL_TEXTURE_ENV				0x2300
#define GL_TEXTURE_ENV_MODE			0x2200
#define GL_TEXTURE_1D				0x0DE0
#define GL_TEXTURE_2D				0x0DE1
#define GL_TEXTURE_WRAP_S			0x2802
#define GL_TEXTURE_WRAP_T			0x2803
#define GL_TEXTURE_MAG_FILTER			0x2800
#define GL_TEXTURE_MIN_FILTER			0x2801
#define GL_TEXTURE_ENV_COLOR			0x2201
#define GL_TEXTURE_GEN_S			0x0C60
#define GL_TEXTURE_GEN_T			0x0C61
#define GL_TEXTURE_GEN_R			0x0C62
#define GL_TEXTURE_GEN_Q			0x0C63
#define GL_TEXTURE_GEN_MODE			0x2500
#define GL_TEXTURE_BORDER_COLOR			0x1004
#define GL_TEXTURE_WIDTH			0x1000
#define GL_TEXTURE_HEIGHT			0x1001
#define GL_TEXTURE_BORDER			0x1005
#define GL_TEXTURE_COMPONENTS			0x1003
#define GL_TEXTURE_RED_SIZE			0x805C
#define GL_TEXTURE_GREEN_SIZE			0x805D
#define GL_TEXTURE_BLUE_SIZE			0x805E
#define GL_TEXTURE_ALPHA_SIZE			0x805F
#define GL_TEXTURE_LUMINANCE_SIZE		0x8060
#define GL_TEXTURE_INTENSITY_SIZE		0x8061
#define GL_NEAREST_MIPMAP_NEAREST		0x2700
#define GL_NEAREST_MIPMAP_LINEAR		0x2702
#define GL_LINEAR_MIPMAP_NEAREST		0x2701
#define GL_LINEAR_MIPMAP_LINEAR			0x2703
#define GL_OBJECT_LINEAR			0x2401
#define GL_OBJECT_PLANE				0x2501
#define GL_EYE_LINEAR				0x2400
#define GL_EYE_PLANE				0x2502
#define GL_SPHERE_MAP				0x2402
#define GL_DECAL				0x2101
#define GL_MODULATE				0x2100
#define GL_NEAREST				0x2600
#define GL_REPEAT				0x2901
#define GL_CLAMP				0x2900
#define GL_S					0x2000
#define GL_T					0x2001
#define GL_R					0x2002
#define GL_Q					0x2003

/* Utility */
#define GL_VENDOR				0x1F00
#define GL_RENDERER				0x1F01
#define GL_VERSION				0x1F02
#define GL_EXTENSIONS				0x1F03

/* Errors */
#define GL_NO_ERROR 				0
#define GL_INVALID_ENUM				0x0500
#define GL_INVALID_VALUE			0x0501
#define GL_INVALID_OPERATION			0x0502
#define GL_STACK_OVERFLOW			0x0503
#define GL_STACK_UNDERFLOW			0x0504
#define GL_OUT_OF_MEMORY			0x0505

/* glPush/PopAttrib bits */
#define GL_CURRENT_BIT				0x00000001
#define GL_POINT_BIT				0x00000002
#define GL_LINE_BIT				0x00000004
#define GL_POLYGON_BIT				0x00000008
#define GL_POLYGON_STIPPLE_BIT			0x00000010
#define GL_PIXEL_MODE_BIT			0x00000020
#define GL_LIGHTING_BIT				0x00000040
#define GL_FOG_BIT				0x00000080
#define GL_DEPTH_BUFFER_BIT			0x00000100
#define GL_ACCUM_BUFFER_BIT			0x00000200
#define GL_STENCIL_BUFFER_BIT			0x00000400
#define GL_VIEWPORT_BIT				0x00000800
#define GL_TRANSFORM_BIT			0x00001000
#define GL_ENABLE_BIT				0x00002000
#define GL_COLOR_BUFFER_BIT			0x00004000
#define GL_HINT_BIT				0x00008000
#define GL_EVAL_BIT				0x00010000
#define GL_LIST_BIT				0x00020000
#define GL_TEXTURE_BIT				0x00040000
#define GL_SCISSOR_BIT				0x00080000
#define GL_ALL_ATTRIB_BITS			0x000FFFFF


/* OpenGL 1.1 */
#define GL_PROXY_TEXTURE_1D			0x8063
#define GL_PROXY_TEXTURE_2D			0x8064
#define GL_TEXTURE_PRIORITY			0x8066
#define GL_TEXTURE_RESIDENT			0x8067
#define GL_TEXTURE_BINDING_1D			0x8068
#define GL_TEXTURE_BINDING_2D			0x8069
#define GL_TEXTURE_INTERNAL_FORMAT		0x1003
#define GL_ALPHA4				0x803B
#define GL_ALPHA8				0x803C
#define GL_ALPHA12				0x803D
#define GL_ALPHA16				0x803E
#define GL_LUMINANCE4				0x803F
#define GL_LUMINANCE8				0x8040
#define GL_LUMINANCE12				0x8041
#define GL_LUMINANCE16				0x8042
#define GL_LUMINANCE4_ALPHA4			0x8043
#define GL_LUMINANCE6_ALPHA2			0x8044
#define GL_LUMINANCE8_ALPHA8			0x8045
#define GL_LUMINANCE12_ALPHA4			0x8046
#define GL_LUMINANCE12_ALPHA12			0x8047
#define GL_LUMINANCE16_ALPHA16			0x8048
#define GL_INTENSITY				0x8049
#define GL_INTENSITY4				0x804A
#define GL_INTENSITY8				0x804B
#define GL_INTENSITY12				0x804C
#define GL_INTENSITY16				0x804D
#define GL_R3_G3_B2				0x2A10
#define GL_RGB4					0x804F
#define GL_RGB5					0x8050
#define GL_RGB8					0x8051
#define GL_RGB10				0x8052
#define GL_RGB12				0x8053
#define GL_RGB16				0x8054
#define GL_RGBA2				0x8055
#define GL_RGBA4				0x8056
#define GL_RGB5_A1				0x8057
#define GL_RGBA8				0x8058
#define GL_RGB10_A2				0x8059
#define GL_RGBA12				0x805A
#define GL_RGBA16				0x805B
#define GL_CLIENT_PIXEL_STORE_BIT		0x00000001
#define GL_CLIENT_VERTEX_ARRAY_BIT		0x00000002
#define GL_ALL_CLIENT_ATTRIB_BITS 		0xFFFFFFFF
#define GL_CLIENT_ALL_ATTRIB_BITS 		0xFFFFFFFF

#endif

typedef void (GLAPIENTRY * PFNGLCLEARINDEXPROC) ( GLfloat c );
typedef void (GLAPIENTRY * PFNGLCLEARCOLORPROC) ( GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha );
typedef void (GLAPIENTRY * PFNGLCLEARPROC) ( GLbitfield mask );
typedef void (GLAPIENTRY * PFNGLINDEXMASKPROC) ( GLuint mask );
typedef void (GLAPIENTRY * PFNGLCOLORMASKPROC) ( GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha );
typedef void (GLAPIENTRY * PFNGLALPHAFUNCPROC) ( GLenum func, GLclampf ref );
typedef void (GLAPIENTRY * PFNGLBLENDFUNCPROC) ( GLenum sfactor, GLenum dfactor );
typedef void (GLAPIENTRY * PFNGLLOGICOPPROC) ( GLenum opcode );
typedef void (GLAPIENTRY * PFNGLCULLFACEPROC) ( GLenum mode );
typedef void (GLAPIENTRY * PFNGLFRONTFACEPROC) ( GLenum mode );
typedef void (GLAPIENTRY * PFNGLPOINTSIZEPROC) ( GLfloat size );
typedef void (GLAPIENTRY * PFNGLLINEWIDTHPROC) ( GLfloat width );
typedef void (GLAPIENTRY * PFNGLLINESTIPPLEPROC) ( GLint factor, GLushort pattern );
typedef void (GLAPIENTRY * PFNGLPOLYGONMODEPROC) ( GLenum face, GLenum mode );
typedef void (GLAPIENTRY * PFNGLPOLYGONOFFSETPROC) ( GLfloat factor, GLfloat units );
typedef void (GLAPIENTRY * PFNGLPOLYGONSTIPPLEPROC) ( const GLubyte *mask );
typedef void (GLAPIENTRY * PFNGLGETPOLYGONSTIPPLEPROC) ( GLubyte *mask );
typedef void (GLAPIENTRY * PFNGLEDGEFLAGPROC) ( GLboolean flag );
typedef void (GLAPIENTRY * PFNGLEDGEFLAGVPROC) ( const GLboolean *flag );
typedef void (GLAPIENTRY * PFNGLSCISSORPROC) ( GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (GLAPIENTRY * PFNGLCLIPPLANEPROC) ( GLenum plane, const GLdouble *equation );
typedef void (GLAPIENTRY * PFNGLGETCLIPPLANEPROC) ( GLenum plane, GLdouble *equation );
typedef void (GLAPIENTRY * PFNGLDRAWBUFFERPROC) ( GLenum mode );
typedef void (GLAPIENTRY * PFNGLREADBUFFERPROC) ( GLenum mode );
typedef void (GLAPIENTRY * PFNGLENABLEPROC) ( GLenum cap );
typedef void (GLAPIENTRY * PFNGLDISABLEPROC) ( GLenum cap );
typedef GLboolean (GLAPIENTRY * PFNGLISENABLEDPROC) ( GLenum cap );
typedef void (GLAPIENTRY * PFNGLENABLECLIENTSTATEPROC) ( GLenum cap ); /* 1.1 */
typedef void (GLAPIENTRY * PFNGLDISABLECLIENTSTATEPROC) ( GLenum cap ); /* 1.1 */
typedef void (GLAPIENTRY * PFNGLGETBOOLEANVPROC) ( GLenum pname, GLboolean *params );
typedef void (GLAPIENTRY * PFNGLGETDOUBLEVPROC) ( GLenum pname, GLdouble *params );
typedef void (GLAPIENTRY * PFNGLGETFLOATVPROC) ( GLenum pname, GLfloat *params );
typedef void (GLAPIENTRY * PFNGLGETINTEGERVPROC) ( GLenum pname, GLint *params );
typedef void (GLAPIENTRY * PFNGLPUSHATTRIBPROC) ( GLbitfield mask );
typedef void (GLAPIENTRY * PFNGLPOPATTRIBPROC) ( void );
typedef void (GLAPIENTRY * PFNGLPUSHCLIENTATTRIBPROC) ( GLbitfield mask ); /* 1.1 */
typedef void (GLAPIENTRY * PFNGLPOPCLIENTATTRIBPROC) ( void ); /* 1.1 */
typedef GLint (GLAPIENTRY * PFNGLRENDERMODEPROC) ( GLenum mode );
typedef GLenum (GLAPIENTRY * PFNGLGETERRORPROC) ( void );
typedef const GLubyte* (GLAPIENTRY * PFNGLGETSTRINGPROC) ( GLenum name );
typedef void (GLAPIENTRY * PFNGLFINISHPROC) ( void );
typedef void (GLAPIENTRY * PFNGLFLUSHPROC) ( void );
typedef void (GLAPIENTRY * PFNGLHINTPROC) ( GLenum target, GLenum mode );
typedef void (GLAPIENTRY * PFNGLCLEARDEPTHPROC) ( GLclampd depth );
typedef void (GLAPIENTRY * PFNGLDEPTHFUNCPROC) ( GLenum func );
typedef void (GLAPIENTRY * PFNGLDEPTHMASKPROC) ( GLboolean flag );
typedef void (GLAPIENTRY * PFNGLDEPTHRANGEPROC) ( GLclampd near_val, GLclampd far_val );
typedef void (GLAPIENTRY * PFNGLCLEARACCUMPROC) ( GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha );
typedef void (GLAPIENTRY * PFNGLACCUMPROC) ( GLenum op, GLfloat value );
typedef void (GLAPIENTRY * PFNGLMATRIXMODEPROC) ( GLenum mode );
typedef void (GLAPIENTRY * PFNGLORTHOPROC) ( GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble near_val, GLdouble far_val );
typedef void (GLAPIENTRY * PFNGLFRUSTUMPROC) ( GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble near_val, GLdouble far_val );
typedef void (GLAPIENTRY * PFNGLVIEWPORTPROC) ( GLint x, GLint y, GLsizei width, GLsizei height );
typedef void (GLAPIENTRY * PFNGLPUSHMATRIXPROC) ( void );
typedef void (GLAPIENTRY * PFNGLPOPMATRIXPROC) ( void );
typedef void (GLAPIENTRY * PFNGLLOADIDENTITYPROC) ( void );
typedef void (GLAPIENTRY * PFNGLLOADMATRIXDPROC) ( const GLdouble *m );
typedef void (GLAPIENTRY * PFNGLLOADMATRIXFPROC) ( const GLfloat *m );
typedef void (GLAPIENTRY * PFNGLMULTMATRIXDPROC) ( const GLdouble *m );
typedef void (GLAPIENTRY * PFNGLMULTMATRIXFPROC) ( const GLfloat *m );
typedef void (GLAPIENTRY * PFNGLROTATEDPROC) ( GLdouble angle, GLdouble x, GLdouble y, GLdouble z );
typedef void (GLAPIENTRY * PFNGLROTATEFPROC) ( GLfloat angle, GLfloat x, GLfloat y, GLfloat z );
typedef void (GLAPIENTRY * PFNGLSCALEDPROC) ( GLdouble x, GLdouble y, GLdouble z );
typedef void (GLAPIENTRY * PFNGLSCALEFPROC) ( GLfloat x, GLfloat y, GLfloat z );
typedef void (GLAPIENTRY * PFNGLTRANSLATEDPROC) ( GLdouble x, GLdouble y, GLdouble z );
typedef void (GLAPIENTRY * PFNGLTRANSLATEFPROC) ( GLfloat x, GLfloat y, GLfloat z );
typedef GLboolean (GLAPIENTRY * PFNGLISLISTPROC) ( GLuint list );
typedef void (GLAPIENTRY * PFNGLDELETELISTSPROC) ( GLuint list, GLsizei range );
typedef GLuint (GLAPIENTRY * PFNGLGENLISTSPROC) ( GLsizei range );
typedef void (GLAPIENTRY * PFNGLNEWLISTPROC) ( GLuint list, GLenum mode );
typedef void (GLAPIENTRY * PFNGLENDLISTPROC) ( void );
typedef void (GLAPIENTRY * PFNGLCALLLISTPROC) ( GLuint list );
typedef void (GLAPIENTRY * PFNGLCALLLISTSPROC) ( GLsizei n, GLenum type, const GLvoid *lists );
typedef void (GLAPIENTRY * PFNGLLISTBASEPROC) ( GLuint base );
typedef void (GLAPIENTRY * PFNGLBEGINPROC) ( GLenum mode );
typedef void (GLAPIENTRY * PFNGLENDPROC) ( void );
typedef void (GLAPIENTRY * PFNGLVERTEX2DPROC) ( GLdouble x, GLdouble y );
typedef void (GLAPIENTRY * PFNGLVERTEX2FPROC) ( GLfloat x, GLfloat y );
typedef void (GLAPIENTRY * PFNGLVERTEX2IPROC) ( GLint x, GLint y );
typedef void (GLAPIENTRY * PFNGLVERTEX2SPROC) ( GLshort x, GLshort y );
typedef void (GLAPIENTRY * PFNGLVERTEX3DPROC) ( GLdouble x, GLdouble y, GLdouble z );
typedef void (GLAPIENTRY * PFNGLVERTEX3FPROC) ( GLfloat x, GLfloat y, GLfloat z );
typedef void (GLAPIENTRY * PFNGLVERTEX3IPROC) ( GLint x, GLint y, GLint z );
typedef void (GLAPIENTRY * PFNGLVERTEX3SPROC) ( GLshort x, GLshort y, GLshort z );
typedef void (GLAPIENTRY * PFNGLVERTEX4DPROC) ( GLdouble x, GLdouble y, GLdouble z, GLdouble w );
typedef void (GLAPIENTRY * PFNGLVERTEX4FPROC) ( GLfloat x, GLfloat y, GLfloat z, GLfloat w );
typedef void (GLAPIENTRY * PFNGLVERTEX4IPROC) ( GLint x, GLint y, GLint z, GLint w );
typedef void (GLAPIENTRY * PFNGLVERTEX4SPROC) ( GLshort x, GLshort y, GLshort z, GLshort w );
typedef void (GLAPIENTRY * PFNGLVERTEX2DVPROC) ( const GLdouble *v );
typedef void (GLAPIENTRY * PFNGLVERTEX2FVPROC) ( const GLfloat *v );
typedef void (GLAPIENTRY * PFNGLVERTEX2IVPROC) ( const GLint *v );
typedef void (GLAPIENTRY * PFNGLVERTEX2SVPROC) ( const GLshort *v );
typedef void (GLAPIENTRY * PFNGLVERTEX3DVPROC) ( const GLdouble *v );
typedef void (GLAPIENTRY * PFNGLVERTEX3FVPROC) ( const GLfloat *v );
typedef void (GLAPIENTRY * PFNGLVERTEX3IVPROC) ( const GLint *v );
typedef void (GLAPIENTRY * PFNGLVERTEX3SVPROC) ( const GLshort *v );
typedef void (GLAPIENTRY * PFNGLVERTEX4DVPROC) ( const GLdouble *v );
typedef void (GLAPIENTRY * PFNGLVERTEX4FVPROC) ( const GLfloat *v );
typedef void (GLAPIENTRY * PFNGLVERTEX4IVPROC) ( const GLint *v );
typedef void (GLAPIENTRY * PFNGLVERTEX4SVPROC) ( const GLshort *v );
typedef void (GLAPIENTRY * PFNGLNORMAL3BPROC) ( GLbyte nx, GLbyte ny, GLbyte nz );
typedef void (GLAPIENTRY * PFNGLNORMAL3DPROC) ( GLdouble nx, GLdouble ny, GLdouble nz );
typedef void (GLAPIENTRY * PFNGLNORMAL3FPROC) ( GLfloat nx, GLfloat ny, GLfloat nz );
typedef void (GLAPIENTRY * PFNGLNORMAL3IPROC) ( GLint nx, GLint ny, GLint nz );
typedef void (GLAPIENTRY * PFNGLNORMAL3SPROC) ( GLshort nx, GLshort ny, GLshort nz );
typedef void (GLAPIENTRY * PFNGLNORMAL3BVPROC) ( const GLbyte *v );
typedef void (GLAPIENTRY * PFNGLNORMAL3DVPROC) ( const GLdouble *v );
typedef void (GLAPIENTRY * PFNGLNORMAL3FVPROC) ( const GLfloat *v );
typedef void (GLAPIENTRY * PFNGLNORMAL3IVPROC) ( const GLint *v );
typedef void (GLAPIENTRY * PFNGLNORMAL3SVPROC) ( const GLshort *v );
typedef void (GLAPIENTRY * PFNGLINDEXDPROC) ( GLdouble c );
typedef void (GLAPIENTRY * PFNGLINDEXFPROC) ( GLfloat c );
typedef void (GLAPIENTRY * PFNGLINDEXIPROC) ( GLint c );
typedef void (GLAPIENTRY * PFNGLINDEXSPROC) ( GLshort c );
typedef void (GLAPIENTRY * PFNGLINDEXUBPROC) ( GLubyte c ); /* 1.1 */
typedef void (GLAPIENTRY * PFNGLINDEXDVPROC) ( const GLdouble *c );
typedef void (GLAPIENTRY * PFNGLINDEXFVPROC) ( const GLfloat *c );
typedef void (GLAPIENTRY * PFNGLINDEXIVPROC) ( const GLint *c );
typedef void (GLAPIENTRY * PFNGLINDEXSVPROC) ( const GLshort *c );
typedef void (GLAPIENTRY * PFNGLINDEXUBVPROC) ( const GLubyte *c ); /* 1.1 */
typedef void (GLAPIENTRY * PFNGLCOLOR3BPROC) ( GLbyte red, GLbyte green, GLbyte blue );
typedef void (GLAPIENTRY * PFNGLCOLOR3DPROC) ( GLdouble red, GLdouble green, GLdouble blue );
typedef void (GLAPIENTRY * PFNGLCOLOR3FPROC) ( GLfloat red, GLfloat green, GLfloat blue );
typedef void (GLAPIENTRY * PFNGLCOLOR3IPROC) ( GLint red, GLint green, GLint blue );
typedef void (GLAPIENTRY * PFNGLCOLOR3SPROC) ( GLshort red, GLshort green, GLshort blue );
typedef void (GLAPIENTRY * PFNGLCOLOR3UBPROC) ( GLubyte red, GLubyte green, GLubyte blue );
typedef void (GLAPIENTRY * PFNGLCOLOR3UIPROC) ( GLuint red, GLuint green, GLuint blue );
typedef void (GLAPIENTRY * PFNGLCOLOR3USPROC) ( GLushort red, GLushort green, GLushort blue );
typedef void (GLAPIENTRY * PFNGLCOLOR4BPROC) ( GLbyte red, GLbyte green, GLbyte blue, GLbyte alpha );
typedef void (GLAPIENTRY * PFNGLCOLOR4DPROC) ( GLdouble red, GLdouble green, GLdouble blue, GLdouble alpha );
typedef void (GLAPIENTRY * PFNGLCOLOR4FPROC) ( GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha );
typedef void (GLAPIENTRY * PFNGLCOLOR4IPROC) ( GLint red, GLint green, GLint blue, GLint alpha );
typedef void (GLAPIENTRY * PFNGLCOLOR4SPROC) ( GLshort red, GLshort green, GLshort blue, GLshort alpha );
typedef void (GLAPIENTRY * PFNGLCOLOR4UBPROC) ( GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha );
typedef void (GLAPIENTRY * PFNGLCOLOR4UIPROC) ( GLuint red, GLuint green, GLuint blue, GLuint alpha );
typedef void (GLAPIENTRY * PFNGLCOLOR4USPROC) ( GLushort red, GLushort green, GLushort blue, GLushort alpha );
typedef void (GLAPIENTRY * PFNGLCOLOR3BVPROC) ( const GLbyte *v );
typedef void (GLAPIENTRY * PFNGLCOLOR3DVPROC) ( const GLdouble *v );
typedef void (GLAPIENTRY * PFNGLCOLOR3FVPROC) ( const GLfloat *v );
typedef void (GLAPIENTRY * PFNGLCOLOR3IVPROC) ( const GLint *v );
typedef void (GLAPIENTRY * PFNGLCOLOR3SVPROC) ( const GLshort *v );
typedef void (GLAPIENTRY * PFNGLCOLOR3UBVPROC) ( const GLubyte *v );
typedef void (GLAPIENTRY * PFNGLCOLOR3UIVPROC) ( const GLuint *v );
typedef void (GLAPIENTRY * PFNGLCOLOR3USVPROC) ( const GLushort *v );
typedef void (GLAPIENTRY * PFNGLCOLOR4BVPROC) ( const GLbyte *v );
typedef void (GLAPIENTRY * PFNGLCOLOR4DVPROC) ( const GLdouble *v );
typedef void (GLAPIENTRY * PFNGLCOLOR4FVPROC) ( const GLfloat *v );
typedef void (GLAPIENTRY * PFNGLCOLOR4IVPROC) ( const GLint *v );
typedef void (GLAPIENTRY * PFNGLCOLOR4SVPROC) ( const GLshort *v );
typedef void (GLAPIENTRY * PFNGLCOLOR4UBVPROC) ( const GLubyte *v );
typedef void (GLAPIENTRY * PFNGLCOLOR4UIVPROC) ( const GLuint *v );
typedef void (GLAPIENTRY * PFNGLCOLOR4USVPROC) ( const GLushort *v );
typedef void (GLAPIENTRY * PFNGLTEXCOORD1DPROC) ( GLdouble s );
typedef void (GLAPIENTRY * PFNGLTEXCOORD1FPROC) ( GLfloat s );
typedef void (GLAPIENTRY * PFNGLTEXCOORD1IPROC) ( GLint s );
typedef void (GLAPIENTRY * PFNGLTEXCOORD1SPROC) ( GLshort s );
typedef void (GLAPIENTRY * PFNGLTEXCOORD2DPROC) ( GLdouble s, GLdouble t );
typedef void (GLAPIENTRY * PFNGLTEXCOORD2FPROC) ( GLfloat s, GLfloat t );
typedef void (GLAPIENTRY * PFNGLTEXCOORD2IPROC) ( GLint s, GLint t );
typedef void (GLAPIENTRY * PFNGLTEXCOORD2SPROC) ( GLshort s, GLshort t );
typedef void (GLAPIENTRY * PFNGLTEXCOORD3DPROC) ( GLdouble s, GLdouble t, GLdouble r );
typedef void (GLAPIENTRY * PFNGLTEXCOORD3FPROC) ( GLfloat s, GLfloat t, GLfloat r );
typedef void (GLAPIENTRY * PFNGLTEXCOORD3IPROC) ( GLint s, GLint t, GLint r );
typedef void (GLAPIENTRY * PFNGLTEXCOORD3SPROC) ( GLshort s, GLshort t, GLshort r );
typedef void (GLAPIENTRY * PFNGLTEXCOORD4DPROC) ( GLdouble s, GLdouble t, GLdouble r, GLdouble q );
typedef void (GLAPIENTRY * PFNGLTEXCOORD4FPROC) ( GLfloat s, GLfloat t, GLfloat r, GLfloat q );
typedef void (GLAPIENTRY * PFNGLTEXCOORD4IPROC) ( GLint s, GLint t, GLint r, GLint q );
typedef void (GLAPIENTRY * PFNGLTEXCOORD4SPROC) ( GLshort s, GLshort t, GLshort r, GLshort q );
typedef void (GLAPIENTRY * PFNGLTEXCOORD1DVPROC) ( const GLdouble *v );
typedef void (GLAPIENTRY * PFNGLTEXCOORD1FVPROC) ( const GLfloat *v );
typedef void (GLAPIENTRY * PFNGLTEXCOORD1IVPROC) ( const GLint *v );
typedef void (GLAPIENTRY * PFNGLTEXCOORD1SVPROC) ( const GLshort *v );
typedef void (GLAPIENTRY * PFNGLTEXCOORD2DVPROC) ( const GLdouble *v );
typedef void (GLAPIENTRY * PFNGLTEXCOORD2FVPROC) ( const GLfloat *v );
typedef void (GLAPIENTRY * PFNGLTEXCOORD2IVPROC) ( const GLint *v );
typedef void (GLAPIENTRY * PFNGLTEXCOORD2SVPROC) ( const GLshort *v );
typedef void (GLAPIENTRY * PFNGLTEXCOORD3DVPROC) ( const GLdouble *v );
typedef void (GLAPIENTRY * PFNGLTEXCOORD3FVPROC) ( const GLfloat *v );
typedef void (GLAPIENTRY * PFNGLTEXCOORD3IVPROC) ( const GLint *v );
typedef void (GLAPIENTRY * PFNGLTEXCOORD3SVPROC) ( const GLshort *v );
typedef void (GLAPIENTRY * PFNGLTEXCOORD4DVPROC) ( const GLdouble *v );
typedef void (GLAPIENTRY * PFNGLTEXCOORD4FVPROC) ( const GLfloat *v );
typedef void (GLAPIENTRY * PFNGLTEXCOORD4IVPROC) ( const GLint *v );
typedef void (GLAPIENTRY * PFNGLTEXCOORD4SVPROC) ( const GLshort *v );
typedef void (GLAPIENTRY * PFNGLRASTERPOS2DPROC) ( GLdouble x, GLdouble y );
typedef void (GLAPIENTRY * PFNGLRASTERPOS2FPROC) ( GLfloat x, GLfloat y );
typedef void (GLAPIENTRY * PFNGLRASTERPOS2IPROC) ( GLint x, GLint y );
typedef void (GLAPIENTRY * PFNGLRASTERPOS2SPROC) ( GLshort x, GLshort y );
typedef void (GLAPIENTRY * PFNGLRASTERPOS3DPROC) ( GLdouble x, GLdouble y, GLdouble z );
typedef void (GLAPIENTRY * PFNGLRASTERPOS3FPROC) ( GLfloat x, GLfloat y, GLfloat z );
typedef void (GLAPIENTRY * PFNGLRASTERPOS3IPROC) ( GLint x, GLint y, GLint z );
typedef void (GLAPIENTRY * PFNGLRASTERPOS3SPROC) ( GLshort x, GLshort y, GLshort z );
typedef void (GLAPIENTRY * PFNGLRASTERPOS4DPROC) ( GLdouble x, GLdouble y, GLdouble z, GLdouble w );
typedef void (GLAPIENTRY * PFNGLRASTERPOS4FPROC) ( GLfloat x, GLfloat y, GLfloat z, GLfloat w );
typedef void (GLAPIENTRY * PFNGLRASTERPOS4IPROC) ( GLint x, GLint y, GLint z, GLint w );
typedef void (GLAPIENTRY * PFNGLRASTERPOS4SPROC) ( GLshort x, GLshort y, GLshort z, GLshort w );
typedef void (GLAPIENTRY * PFNGLRASTERPOS2DVPROC) ( const GLdouble *v );
typedef void (GLAPIENTRY * PFNGLRASTERPOS2FVPROC) ( const GLfloat *v );
typedef void (GLAPIENTRY * PFNGLRASTERPOS2IVPROC) ( const GLint *v );
typedef void (GLAPIENTRY * PFNGLRASTERPOS2SVPROC) ( const GLshort *v );
typedef void (GLAPIENTRY * PFNGLRASTERPOS3DVPROC) ( const GLdouble *v );
typedef void (GLAPIENTRY * PFNGLRASTERPOS3FVPROC) ( const GLfloat *v );
typedef void (GLAPIENTRY * PFNGLRASTERPOS3IVPROC) ( const GLint *v );
typedef void (GLAPIENTRY * PFNGLRASTERPOS3SVPROC) ( const GLshort *v );
typedef void (GLAPIENTRY * PFNGLRASTERPOS4DVPROC) ( const GLdouble *v );
typedef void (GLAPIENTRY * PFNGLRASTERPOS4FVPROC) ( const GLfloat *v );
typedef void (GLAPIENTRY * PFNGLRASTERPOS4IVPROC) ( const GLint *v );
typedef void (GLAPIENTRY * PFNGLRASTERPOS4SVPROC) ( const GLshort *v );
typedef void (GLAPIENTRY * PFNGLRECTDPROC) ( GLdouble x1, GLdouble y1, GLdouble x2, GLdouble y2 );
typedef void (GLAPIENTRY * PFNGLRECTFPROC) ( GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2 );
typedef void (GLAPIENTRY * PFNGLRECTIPROC) ( GLint x1, GLint y1, GLint x2, GLint y2 );
typedef void (GLAPIENTRY * PFNGLRECTSPROC) ( GLshort x1, GLshort y1, GLshort x2, GLshort y2 );
typedef void (GLAPIENTRY * PFNGLRECTDVPROC) ( const GLdouble *v1, const GLdouble *v2 );
typedef void (GLAPIENTRY * PFNGLRECTFVPROC) ( const GLfloat *v1, const GLfloat *v2 );
typedef void (GLAPIENTRY * PFNGLRECTIVPROC) ( const GLint *v1, const GLint *v2 );
typedef void (GLAPIENTRY * PFNGLRECTSVPROC) ( const GLshort *v1, const GLshort *v2 );
typedef void (GLAPIENTRY * PFNGLVERTEXPOINTERPROC) ( GLint size, GLenum type, GLsizei stride, const GLvoid *ptr );
typedef void (GLAPIENTRY * PFNGLNORMALPOINTERPROC) ( GLenum type, GLsizei stride, const GLvoid *ptr );
typedef void (GLAPIENTRY * PFNGLCOLORPOINTERPROC) ( GLint size, GLenum type, GLsizei stride, const GLvoid *ptr );
typedef void (GLAPIENTRY * PFNGLINDEXPOINTERPROC) ( GLenum type, GLsizei stride, const GLvoid *ptr );
typedef void (GLAPIENTRY * PFNGLTEXCOORDPOINTERPROC) ( GLint size, GLenum type, GLsizei stride, const GLvoid *ptr );
typedef void (GLAPIENTRY * PFNGLEDGEFLAGPOINTERPROC) ( GLsizei stride, const GLvoid *ptr );
typedef void (GLAPIENTRY * PFNGLGETPOINTERVPROC) ( GLenum pname, GLvoid **params );
typedef void (GLAPIENTRY * PFNGLARRAYELEMENTPROC) ( GLint i );
typedef void (GLAPIENTRY * PFNGLDRAWARRAYSPROC) ( GLenum mode, GLint first, GLsizei count );
typedef void (GLAPIENTRY * PFNGLDRAWELEMENTSPROC) ( GLenum mode, GLsizei count, GLenum type, const GLvoid *indices );
typedef void (GLAPIENTRY * PFNGLINTERLEAVEDARRAYSPROC) ( GLenum format, GLsizei stride, const GLvoid *pointer );
typedef void (GLAPIENTRY * PFNGLSHADEMODELPROC) ( GLenum mode );
typedef void (GLAPIENTRY * PFNGLLIGHTFPROC) ( GLenum light, GLenum pname, GLfloat param );
typedef void (GLAPIENTRY * PFNGLLIGHTIPROC) ( GLenum light, GLenum pname, GLint param );
typedef void (GLAPIENTRY * PFNGLLIGHTFVPROC) ( GLenum light, GLenum pname, const GLfloat *params );
typedef void (GLAPIENTRY * PFNGLLIGHTIVPROC) ( GLenum light, GLenum pname, const GLint *params );
typedef void (GLAPIENTRY * PFNGLGETLIGHTFVPROC) ( GLenum light, GLenum pname, GLfloat *params );
typedef void (GLAPIENTRY * PFNGLGETLIGHTIVPROC) ( GLenum light, GLenum pname, GLint *params );
typedef void (GLAPIENTRY * PFNGLLIGHTMODELFPROC) ( GLenum pname, GLfloat param );
typedef void (GLAPIENTRY * PFNGLLIGHTMODELIPROC) ( GLenum pname, GLint param );
typedef void (GLAPIENTRY * PFNGLLIGHTMODELFVPROC) ( GLenum pname, const GLfloat *params );
typedef void (GLAPIENTRY * PFNGLLIGHTMODELIVPROC) ( GLenum pname, const GLint *params );
typedef void (GLAPIENTRY * PFNGLMATERIALFPROC) ( GLenum face, GLenum pname, GLfloat param );
typedef void (GLAPIENTRY * PFNGLMATERIALIPROC) ( GLenum face, GLenum pname, GLint param );
typedef void (GLAPIENTRY * PFNGLMATERIALFVPROC) ( GLenum face, GLenum pname, const GLfloat *params );
typedef void (GLAPIENTRY * PFNGLMATERIALIVPROC) ( GLenum face, GLenum pname, const GLint *params );
typedef void (GLAPIENTRY * PFNGLGETMATERIALFVPROC) ( GLenum face, GLenum pname, GLfloat *params );
typedef void (GLAPIENTRY * PFNGLGETMATERIALIVPROC) ( GLenum face, GLenum pname, GLint *params );
typedef void (GLAPIENTRY * PFNGLCOLORMATERIALPROC) ( GLenum face, GLenum mode );
typedef void (GLAPIENTRY * PFNGLPIXELZOOMPROC) ( GLfloat xfactor, GLfloat yfactor );
typedef void (GLAPIENTRY * PFNGLPIXELSTOREFPROC) ( GLenum pname, GLfloat param );
typedef void (GLAPIENTRY * PFNGLPIXELSTOREIPROC) ( GLenum pname, GLint param );
typedef void (GLAPIENTRY * PFNGLPIXELTRANSFERFPROC) ( GLenum pname, GLfloat param );
typedef void (GLAPIENTRY * PFNGLPIXELTRANSFERIPROC) ( GLenum pname, GLint param );
typedef void (GLAPIENTRY * PFNGLPIXELMAPFVPROC) ( GLenum map, GLsizei mapsize, const GLfloat *values );
typedef void (GLAPIENTRY * PFNGLPIXELMAPUIVPROC) ( GLenum map, GLsizei mapsize, const GLuint *values );
typedef void (GLAPIENTRY * PFNGLPIXELMAPUSVPROC) ( GLenum map, GLsizei mapsize, const GLushort *values );
typedef void (GLAPIENTRY * PFNGLGETPIXELMAPFVPROC) ( GLenum map, GLfloat *values );
typedef void (GLAPIENTRY * PFNGLGETPIXELMAPUIVPROC) ( GLenum map, GLuint *values );
typedef void (GLAPIENTRY * PFNGLGETPIXELMAPUSVPROC) ( GLenum map, GLushort *values );
typedef void (GLAPIENTRY * PFNGLBITMAPPROC) ( GLsizei width, GLsizei height, GLfloat xorig, GLfloat yorig, GLfloat xmove, GLfloat ymove, const GLubyte *bitmap );
typedef void (GLAPIENTRY * PFNGLREADPIXELSPROC) ( GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels );
typedef void (GLAPIENTRY * PFNGLDRAWPIXELSPROC) ( GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels );
typedef void (GLAPIENTRY * PFNGLCOPYPIXELSPROC) ( GLint x, GLint y, GLsizei width, GLsizei height, GLenum type );
typedef void (GLAPIENTRY * PFNGLSTENCILFUNCPROC) ( GLenum func, GLint ref, GLuint mask );
typedef void (GLAPIENTRY * PFNGLSTENCILMASKPROC) ( GLuint mask );
typedef void (GLAPIENTRY * PFNGLSTENCILOPPROC) ( GLenum fail, GLenum zfail, GLenum zpass );
typedef void (GLAPIENTRY * PFNGLCLEARSTENCILPROC) ( GLint s );
typedef void (GLAPIENTRY * PFNGLTEXGENDPROC) ( GLenum coord, GLenum pname, GLdouble param );
typedef void (GLAPIENTRY * PFNGLTEXGENFPROC) ( GLenum coord, GLenum pname, GLfloat param );
typedef void (GLAPIENTRY * PFNGLTEXGENIPROC) ( GLenum coord, GLenum pname, GLint param );
typedef void (GLAPIENTRY * PFNGLTEXGENDVPROC) ( GLenum coord, GLenum pname, const GLdouble *params );
typedef void (GLAPIENTRY * PFNGLTEXGENFVPROC) ( GLenum coord, GLenum pname, const GLfloat *params );
typedef void (GLAPIENTRY * PFNGLTEXGENIVPROC) ( GLenum coord, GLenum pname, const GLint *params );
typedef void (GLAPIENTRY * PFNGLGETTEXGENDVPROC) ( GLenum coord, GLenum pname, GLdouble *params );
typedef void (GLAPIENTRY * PFNGLGETTEXGENFVPROC) ( GLenum coord, GLenum pname, GLfloat *params );
typedef void (GLAPIENTRY * PFNGLGETTEXGENIVPROC) ( GLenum coord, GLenum pname, GLint *params );
typedef void (GLAPIENTRY * PFNGLTEXENVFPROC) ( GLenum target, GLenum pname, GLfloat param );
typedef void (GLAPIENTRY * PFNGLTEXENVIPROC) ( GLenum target, GLenum pname, GLint param );
typedef void (GLAPIENTRY * PFNGLTEXENVFVPROC) ( GLenum target, GLenum pname, const GLfloat *params );
typedef void (GLAPIENTRY * PFNGLTEXENVIVPROC) ( GLenum target, GLenum pname, const GLint *params );
typedef void (GLAPIENTRY * PFNGLGETTEXENVFVPROC) ( GLenum target, GLenum pname, GLfloat *params );
typedef void (GLAPIENTRY * PFNGLGETTEXENVIVPROC) ( GLenum target, GLenum pname, GLint *params );
typedef void (GLAPIENTRY * PFNGLTEXPARAMETERFPROC) ( GLenum target, GLenum pname, GLfloat param );
typedef void (GLAPIENTRY * PFNGLTEXPARAMETERIPROC) ( GLenum target, GLenum pname, GLint param );
typedef void (GLAPIENTRY * PFNGLTEXPARAMETERFVPROC) ( GLenum target, GLenum pname, const GLfloat *params );
typedef void (GLAPIENTRY * PFNGLTEXPARAMETERIVPROC) ( GLenum target, GLenum pname, const GLint *params );
typedef void (GLAPIENTRY * PFNGLGETTEXPARAMETERFVPROC) ( GLenum target, GLenum pname, GLfloat *params);
typedef void (GLAPIENTRY * PFNGLGETTEXPARAMETERIVPROC) ( GLenum target, GLenum pname, GLint *params );
typedef void (GLAPIENTRY * PFNGLGETTEXLEVELPARAMETERFVPROC) ( GLenum target, GLint level, GLenum pname, GLfloat *params );
typedef void (GLAPIENTRY * PFNGLGETTEXLEVELPARAMETERIVPROC) ( GLenum target, GLint level, GLenum pname, GLint *params );
typedef void (GLAPIENTRY * PFNGLTEXIMAGE1DPROC) ( GLenum target, GLint level, GLint internalFormat, GLsizei width, GLint border, GLenum format, GLenum type, const GLvoid *pixels );
typedef void (GLAPIENTRY * PFNGLTEXIMAGE2DPROC) ( GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels );
typedef void (GLAPIENTRY * PFNGLGETTEXIMAGEPROC) ( GLenum target, GLint level, GLenum format, GLenum type, GLvoid *pixels );
typedef void (GLAPIENTRY * PFNGLGENTEXTURESPROC) ( GLsizei n, GLuint *textures );
typedef void (GLAPIENTRY * PFNGLDELETETEXTURESPROC) ( GLsizei n, const GLuint *textures);
typedef void (GLAPIENTRY * PFNGLBINDTEXTUREPROC) ( GLenum target, GLuint texture );
typedef void (GLAPIENTRY * PFNGLPRIORITIZETEXTURESPROC) ( GLsizei n, const GLuint *textures, const GLclampf *priorities );
typedef GLboolean (GLAPIENTRY * PFNGLARETEXTURESRESIDENTPROC) ( GLsizei n, const GLuint *textures, GLboolean *residences );
typedef GLboolean (GLAPIENTRY * PFNGLISTEXTUREPROC) ( GLuint texture );
typedef void (GLAPIENTRY * PFNGLTEXSUBIMAGE1DPROC) ( GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const GLvoid *pixels );
typedef void (GLAPIENTRY * PFNGLTEXSUBIMAGE2DPROC) ( GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels );
typedef void (GLAPIENTRY * PFNGLCOPYTEXIMAGE1DPROC) ( GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLint border );
typedef void (GLAPIENTRY * PFNGLCOPYTEXIMAGE2DPROC) ( GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border );
typedef void (GLAPIENTRY * PFNGLCOPYTEXSUBIMAGE1DPROC) ( GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width );
typedef void (GLAPIENTRY * PFNGLCOPYTEXSUBIMAGE2DPROC) ( GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height );
typedef void (GLAPIENTRY * PFNGLMAP1DPROC) ( GLenum target, GLdouble u1, GLdouble u2, GLint stride, GLint order, const GLdouble *points );
typedef void (GLAPIENTRY * PFNGLMAP1FPROC) ( GLenum target, GLfloat u1, GLfloat u2, GLint stride, GLint order, const GLfloat *points );
typedef void (GLAPIENTRY * PFNGLMAP2DPROC) ( GLenum target, GLdouble u1, GLdouble u2, GLint ustride, GLint uorder, GLdouble v1, GLdouble v2, GLint vstride, GLint vorder, const GLdouble *points );
typedef void (GLAPIENTRY * PFNGLMAP2FPROC) ( GLenum target, GLfloat u1, GLfloat u2, GLint ustride, GLint uorder, GLfloat v1, GLfloat v2, GLint vstride, GLint vorder, const GLfloat *points );
typedef void (GLAPIENTRY * PFNGLGETMAPDVPROC) ( GLenum target, GLenum query, GLdouble *v );
typedef void (GLAPIENTRY * PFNGLGETMAPFVPROC) ( GLenum target, GLenum query, GLfloat *v );
typedef void (GLAPIENTRY * PFNGLGETMAPIVPROC) ( GLenum target, GLenum query, GLint *v );
typedef void (GLAPIENTRY * PFNGLEVALCOORD1DPROC) ( GLdouble u );
typedef void (GLAPIENTRY * PFNGLEVALCOORD1FPROC) ( GLfloat u );
typedef void (GLAPIENTRY * PFNGLEVALCOORD1DVPROC) ( const GLdouble *u );
typedef void (GLAPIENTRY * PFNGLEVALCOORD1FVPROC) ( const GLfloat *u );
typedef void (GLAPIENTRY * PFNGLEVALCOORD2DPROC) ( GLdouble u, GLdouble v );
typedef void (GLAPIENTRY * PFNGLEVALCOORD2FPROC) ( GLfloat u, GLfloat v );
typedef void (GLAPIENTRY * PFNGLEVALCOORD2DVPROC) ( const GLdouble *u );
typedef void (GLAPIENTRY * PFNGLEVALCOORD2FVPROC) ( const GLfloat *u );
typedef void (GLAPIENTRY * PFNGLMAPGRID1DPROC) ( GLint un, GLdouble u1, GLdouble u2 );
typedef void (GLAPIENTRY * PFNGLMAPGRID1FPROC) ( GLint un, GLfloat u1, GLfloat u2 );
typedef void (GLAPIENTRY * PFNGLMAPGRID2DPROC) ( GLint un, GLdouble u1, GLdouble u2, GLint vn, GLdouble v1, GLdouble v2 );
typedef void (GLAPIENTRY * PFNGLMAPGRID2FPROC) ( GLint un, GLfloat u1, GLfloat u2, GLint vn, GLfloat v1, GLfloat v2 );
typedef void (GLAPIENTRY * PFNGLEVALPOINT1PROC) ( GLint i );
typedef void (GLAPIENTRY * PFNGLEVALPOINT2PROC) ( GLint i, GLint j );
typedef void (GLAPIENTRY * PFNGLEVALMESH1PROC) ( GLenum mode, GLint i1, GLint i2 );
typedef void (GLAPIENTRY * PFNGLEVALMESH2PROC) ( GLenum mode, GLint i1, GLint i2, GLint j1, GLint j2 );
typedef void (GLAPIENTRY * PFNGLFOGFPROC) ( GLenum pname, GLfloat param );
typedef void (GLAPIENTRY * PFNGLFOGIPROC) ( GLenum pname, GLint param );
typedef void (GLAPIENTRY * PFNGLFOGFVPROC) ( GLenum pname, const GLfloat *params );
typedef void (GLAPIENTRY * PFNGLFOGIVPROC) ( GLenum pname, const GLint *params );
typedef void (GLAPIENTRY * PFNGLFEEDBACKBUFFERPROC) ( GLsizei size, GLenum type, GLfloat *buffer );
typedef void (GLAPIENTRY * PFNGLPASSTHROUGHPROC) ( GLfloat token );
typedef void (GLAPIENTRY * PFNGLSELECTBUFFERPROC) ( GLsizei size, GLuint *buffer );
typedef void (GLAPIENTRY * PFNGLINITNAMESPROC) ( void );
typedef void (GLAPIENTRY * PFNGLLOADNAMEPROC) ( GLuint name );
typedef void (GLAPIENTRY * PFNGLPUSHNAMEPROC) ( GLuint name );
typedef void (GLAPIENTRY * PFNGLPOPNAMEPROC) ( void );

extern PFNGLCLEARINDEXPROC glClearIndex;
extern PFNGLCLEARCOLORPROC glClearColor;
extern PFNGLCLEARPROC glClear;
extern PFNGLINDEXMASKPROC glIndexMask;
extern PFNGLCOLORMASKPROC glColorMask;
extern PFNGLALPHAFUNCPROC glAlphaFunc;
extern PFNGLBLENDFUNCPROC glBlendFunc;
extern PFNGLLOGICOPPROC glLogicOp;
extern PFNGLCULLFACEPROC glCullFace;
extern PFNGLFRONTFACEPROC glFrontFace;
extern PFNGLPOINTSIZEPROC glPointSize;
extern PFNGLLINEWIDTHPROC glLineWidth;
extern PFNGLLINESTIPPLEPROC glLineStipple;
extern PFNGLPOLYGONMODEPROC glPolygonMode;
extern PFNGLPOLYGONOFFSETPROC glPolygonOffset;
extern PFNGLPOLYGONSTIPPLEPROC glPolygonStipple;
extern PFNGLGETPOLYGONSTIPPLEPROC glGetPolygonStipple;
extern PFNGLEDGEFLAGPROC glEdgeFlag;
extern PFNGLEDGEFLAGVPROC glEdgeFlagv;
extern PFNGLSCISSORPROC glScissor;
extern PFNGLCLIPPLANEPROC glClipPlane;
extern PFNGLGETCLIPPLANEPROC glGetClipPlane;
extern PFNGLDRAWBUFFERPROC glDrawBuffer;
extern PFNGLREADBUFFERPROC glReadBuffer;
extern PFNGLENABLEPROC glEnable;
extern PFNGLDISABLEPROC glDisable;
extern PFNGLISENABLEDPROC glIsEnabled;
extern PFNGLENABLECLIENTSTATEPROC glEnableClientState;
extern PFNGLDISABLECLIENTSTATEPROC glDisableClientState;
extern PFNGLGETBOOLEANVPROC glGetBooleanv;
extern PFNGLGETDOUBLEVPROC glGetDoublev;
extern PFNGLGETFLOATVPROC glGetFloatv;
extern PFNGLGETINTEGERVPROC glGetIntegerv;
extern PFNGLPUSHATTRIBPROC glPushAttrib;
extern PFNGLPOPATTRIBPROC glPopAttrib;
extern PFNGLPUSHCLIENTATTRIBPROC glPushClientAttrib;
extern PFNGLPOPCLIENTATTRIBPROC glPopClientAttrib;
extern PFNGLRENDERMODEPROC glRenderMode;
extern PFNGLGETERRORPROC glGetError;
extern PFNGLGETSTRINGPROC glGetString;
extern PFNGLFINISHPROC glFinish;
extern PFNGLFLUSHPROC glFlush;
extern PFNGLHINTPROC glHint;
extern PFNGLCLEARDEPTHPROC glClearDepth;
extern PFNGLDEPTHFUNCPROC glDepthFunc;
extern PFNGLDEPTHMASKPROC glDepthMask;
extern PFNGLDEPTHRANGEPROC glDepthRange;
extern PFNGLCLEARACCUMPROC glClearAccum;
extern PFNGLACCUMPROC glAccum;
extern PFNGLMATRIXMODEPROC glMatrixMode;
extern PFNGLORTHOPROC glOrtho;
extern PFNGLFRUSTUMPROC glFrustum;
extern PFNGLVIEWPORTPROC glViewport;
extern PFNGLPUSHMATRIXPROC glPushMatrix;
extern PFNGLPOPMATRIXPROC glPopMatrix;
extern PFNGLLOADIDENTITYPROC glLoadIdentity;
extern PFNGLLOADMATRIXDPROC glLoadMatrixd;
extern PFNGLLOADMATRIXFPROC glLoadMatrixf;
extern PFNGLMULTMATRIXDPROC glMultMatrixd;
extern PFNGLMULTMATRIXFPROC glMultMatrixf;
extern PFNGLROTATEDPROC glRotated;
extern PFNGLROTATEFPROC glRotatef;
extern PFNGLSCALEDPROC glScaled;
extern PFNGLSCALEFPROC glScalef;
extern PFNGLTRANSLATEDPROC glTranslated;
extern PFNGLTRANSLATEFPROC glTranslatef;
extern PFNGLISLISTPROC glIsList;
extern PFNGLDELETELISTSPROC glDeleteLists;
extern PFNGLGENLISTSPROC glGenLists;
extern PFNGLNEWLISTPROC glNewList;
extern PFNGLENDLISTPROC glEndList;
extern PFNGLCALLLISTPROC glCallList;
extern PFNGLCALLLISTSPROC glCallLists;
extern PFNGLLISTBASEPROC glListBase;
extern PFNGLBEGINPROC glBegin;
extern PFNGLENDPROC glEnd;
extern PFNGLVERTEX2DPROC glVertex2d;
extern PFNGLVERTEX2FPROC glVertex2f;
extern PFNGLVERTEX2IPROC glVertex2i;
extern PFNGLVERTEX2SPROC glVertex2s;
extern PFNGLVERTEX3DPROC glVertex3d;
extern PFNGLVERTEX3FPROC glVertex3f;
extern PFNGLVERTEX3IPROC glVertex3i;
extern PFNGLVERTEX3SPROC glVertex3s;
extern PFNGLVERTEX4DPROC glVertex4d;
extern PFNGLVERTEX4FPROC glVertex4f;
extern PFNGLVERTEX4IPROC glVertex4i;
extern PFNGLVERTEX4SPROC glVertex4s;
extern PFNGLVERTEX2DVPROC glVertex2dv;
extern PFNGLVERTEX2FVPROC glVertex2fv;
extern PFNGLVERTEX2IVPROC glVertex2iv;
extern PFNGLVERTEX2SVPROC glVertex2sv;
extern PFNGLVERTEX3DVPROC glVertex3dv;
extern PFNGLVERTEX3FVPROC glVertex3fv;
extern PFNGLVERTEX3IVPROC glVertex3iv;
extern PFNGLVERTEX3SVPROC glVertex3sv;
extern PFNGLVERTEX4DVPROC glVertex4dv;
extern PFNGLVERTEX4FVPROC glVertex4fv;
extern PFNGLVERTEX4IVPROC glVertex4iv;
extern PFNGLVERTEX4SVPROC glVertex4sv;
extern PFNGLNORMAL3BPROC glNormal3b;
extern PFNGLNORMAL3DPROC glNormal3d;
extern PFNGLNORMAL3FPROC glNormal3f;
extern PFNGLNORMAL3IPROC glNormal3i;
extern PFNGLNORMAL3SPROC glNormal3s;
extern PFNGLNORMAL3BVPROC glNormal3bv;
extern PFNGLNORMAL3DVPROC glNormal3dv;
extern PFNGLNORMAL3FVPROC glNormal3fv;
extern PFNGLNORMAL3IVPROC glNormal3iv;
extern PFNGLNORMAL3SVPROC glNormal3sv;
extern PFNGLINDEXDPROC glIndexd;
extern PFNGLINDEXFPROC glIndexf;
extern PFNGLINDEXIPROC glIndexi;
extern PFNGLINDEXSPROC glIndexs;
extern PFNGLINDEXUBPROC glIndexub;
extern PFNGLINDEXDVPROC glIndexdv;
extern PFNGLINDEXFVPROC glIndexfv;
extern PFNGLINDEXIVPROC glIndexiv;
extern PFNGLINDEXSVPROC glIndexsv;
extern PFNGLINDEXUBVPROC glIndexubv;
extern PFNGLCOLOR3BPROC glColor3b;
extern PFNGLCOLOR3DPROC glColor3d;
extern PFNGLCOLOR3FPROC glColor3f;
extern PFNGLCOLOR3IPROC glColor3i;
extern PFNGLCOLOR3SPROC glColor3s;
extern PFNGLCOLOR3UBPROC glColor3ub;
extern PFNGLCOLOR3UIPROC glColor3ui;
extern PFNGLCOLOR3USPROC glColor3us;
extern PFNGLCOLOR4BPROC glColor4b;
extern PFNGLCOLOR4DPROC glColor4d;
extern PFNGLCOLOR4FPROC glColor4f;
extern PFNGLCOLOR4IPROC glColor4i;
extern PFNGLCOLOR4SPROC glColor4s;
extern PFNGLCOLOR4UBPROC glColor4ub;
extern PFNGLCOLOR4UIPROC glColor4ui;
extern PFNGLCOLOR4USPROC glColor4us;
extern PFNGLCOLOR3BVPROC glColor3bv;
extern PFNGLCOLOR3DVPROC glColor3dv;
extern PFNGLCOLOR3FVPROC glColor3fv;
extern PFNGLCOLOR3IVPROC glColor3iv;
extern PFNGLCOLOR3SVPROC glColor3sv;
extern PFNGLCOLOR3UBVPROC glColor3ubv;
extern PFNGLCOLOR3UIVPROC glColor3uiv;
extern PFNGLCOLOR3USVPROC glColor3usv;
extern PFNGLCOLOR4BVPROC glColor4bv;
extern PFNGLCOLOR4DVPROC glColor4dv;
extern PFNGLCOLOR4FVPROC glColor4fv;
extern PFNGLCOLOR4IVPROC glColor4iv;
extern PFNGLCOLOR4SVPROC glColor4sv;
extern PFNGLCOLOR4UBVPROC glColor4ubv;
extern PFNGLCOLOR4UIVPROC glColor4uiv;
extern PFNGLCOLOR4USVPROC glColor4usv;
extern PFNGLTEXCOORD1DPROC glTexCoord1d;
extern PFNGLTEXCOORD1FPROC glTexCoord1f;
extern PFNGLTEXCOORD1IPROC glTexCoord1i;
extern PFNGLTEXCOORD1SPROC glTexCoord1s;
extern PFNGLTEXCOORD2DPROC glTexCoord2d;
extern PFNGLTEXCOORD2FPROC glTexCoord2f;
extern PFNGLTEXCOORD2IPROC glTexCoord2i;
extern PFNGLTEXCOORD2SPROC glTexCoord2s;
extern PFNGLTEXCOORD3DPROC glTexCoord3d;
extern PFNGLTEXCOORD3FPROC glTexCoord3f;
extern PFNGLTEXCOORD3IPROC glTexCoord3i;
extern PFNGLTEXCOORD3SPROC glTexCoord3s;
extern PFNGLTEXCOORD4DPROC glTexCoord4d;
extern PFNGLTEXCOORD4FPROC glTexCoord4f;
extern PFNGLTEXCOORD4IPROC glTexCoord4i;
extern PFNGLTEXCOORD4SPROC glTexCoord4s;
extern PFNGLTEXCOORD1DVPROC glTexCoord1dv;
extern PFNGLTEXCOORD1FVPROC glTexCoord1fv;
extern PFNGLTEXCOORD1IVPROC glTexCoord1iv;
extern PFNGLTEXCOORD1SVPROC glTexCoord1sv;
extern PFNGLTEXCOORD2DVPROC glTexCoord2dv;
extern PFNGLTEXCOORD2FVPROC glTexCoord2fv;
extern PFNGLTEXCOORD2IVPROC glTexCoord2iv;
extern PFNGLTEXCOORD2SVPROC glTexCoord2sv;
extern PFNGLTEXCOORD3DVPROC glTexCoord3dv;
extern PFNGLTEXCOORD3FVPROC glTexCoord3fv;
extern PFNGLTEXCOORD3IVPROC glTexCoord3iv;
extern PFNGLTEXCOORD3SVPROC glTexCoord3sv;
extern PFNGLTEXCOORD4DVPROC glTexCoord4dv;
extern PFNGLTEXCOORD4FVPROC glTexCoord4fv;
extern PFNGLTEXCOORD4IVPROC glTexCoord4iv;
extern PFNGLTEXCOORD4SVPROC glTexCoord4sv;
extern PFNGLRASTERPOS2DPROC glRasterPos2d;
extern PFNGLRASTERPOS2FPROC glRasterPos2f;
extern PFNGLRASTERPOS2IPROC glRasterPos2i;
extern PFNGLRASTERPOS2SPROC glRasterPos2s;
extern PFNGLRASTERPOS3DPROC glRasterPos3d;
extern PFNGLRASTERPOS3FPROC glRasterPos3f;
extern PFNGLRASTERPOS3IPROC glRasterPos3i;
extern PFNGLRASTERPOS3SPROC glRasterPos3s;
extern PFNGLRASTERPOS4DPROC glRasterPos4d;
extern PFNGLRASTERPOS4FPROC glRasterPos4f;
extern PFNGLRASTERPOS4IPROC glRasterPos4i;
extern PFNGLRASTERPOS4SPROC glRasterPos4s;
extern PFNGLRASTERPOS2DVPROC glRasterPos2dv;
extern PFNGLRASTERPOS2FVPROC glRasterPos2fv;
extern PFNGLRASTERPOS2IVPROC glRasterPos2iv;
extern PFNGLRASTERPOS2SVPROC glRasterPos2sv;
extern PFNGLRASTERPOS3DVPROC glRasterPos3dv;
extern PFNGLRASTERPOS3FVPROC glRasterPos3fv;
extern PFNGLRASTERPOS3IVPROC glRasterPos3iv;
extern PFNGLRASTERPOS3SVPROC glRasterPos3sv;
extern PFNGLRASTERPOS4DVPROC glRasterPos4dv;
extern PFNGLRASTERPOS4FVPROC glRasterPos4fv;
extern PFNGLRASTERPOS4IVPROC glRasterPos4iv;
extern PFNGLRASTERPOS4SVPROC glRasterPos4sv;
extern PFNGLRECTDPROC glRectd;
extern PFNGLRECTFPROC glRectf;
extern PFNGLRECTIPROC glRecti;
extern PFNGLRECTSPROC glRects;
extern PFNGLRECTDVPROC glRectdv;
extern PFNGLRECTFVPROC glRectfv;
extern PFNGLRECTIVPROC glRectiv;
extern PFNGLRECTSVPROC glRectsv;
extern PFNGLVERTEXPOINTERPROC glVertexPointer;
extern PFNGLNORMALPOINTERPROC glNormalPointer;
extern PFNGLCOLORPOINTERPROC glColorPointer;
extern PFNGLINDEXPOINTERPROC glIndexPointer;
extern PFNGLTEXCOORDPOINTERPROC glTexCoordPointer;
extern PFNGLEDGEFLAGPOINTERPROC glEdgeFlagPointer;
extern PFNGLGETPOINTERVPROC glGetPointerv;
extern PFNGLARRAYELEMENTPROC glArrayElement;
extern PFNGLDRAWARRAYSPROC glDrawArrays;
extern PFNGLDRAWELEMENTSPROC glDrawElements;
extern PFNGLINTERLEAVEDARRAYSPROC glInterleavedArrays;
extern PFNGLSHADEMODELPROC glShadeModel;
extern PFNGLLIGHTFPROC glLightf;
extern PFNGLLIGHTIPROC glLighti;
extern PFNGLLIGHTFVPROC glLightfv;
extern PFNGLLIGHTIVPROC glLightiv;
extern PFNGLGETLIGHTFVPROC glGetLightfv;
extern PFNGLGETLIGHTIVPROC glGetLightiv;
extern PFNGLLIGHTMODELFPROC glLightModelf;
extern PFNGLLIGHTMODELIPROC glLightModeli;
extern PFNGLLIGHTMODELFVPROC glLightModelfv;
extern PFNGLLIGHTMODELIVPROC glLightModeliv;
extern PFNGLMATERIALFPROC glMaterialf;
extern PFNGLMATERIALIPROC glMateriali;
extern PFNGLMATERIALFVPROC glMaterialfv;
extern PFNGLMATERIALIVPROC glMaterialiv;
extern PFNGLGETMATERIALFVPROC glGetMaterialfv;
extern PFNGLGETMATERIALIVPROC glGetMaterialiv;
extern PFNGLCOLORMATERIALPROC glColorMaterial;
extern PFNGLPIXELZOOMPROC glPixelZoom;
extern PFNGLPIXELSTOREFPROC glPixelStoref;
extern PFNGLPIXELSTOREIPROC glPixelStorei;
extern PFNGLPIXELTRANSFERFPROC glPixelTransferf;
extern PFNGLPIXELTRANSFERIPROC glPixelTransferi;
extern PFNGLPIXELMAPFVPROC glPixelMapfv;
extern PFNGLPIXELMAPUIVPROC glPixelMapuiv;
extern PFNGLPIXELMAPUSVPROC glPixelMapusv;
extern PFNGLGETPIXELMAPFVPROC glGetPixelMapfv;
extern PFNGLGETPIXELMAPUIVPROC glGetPixelMapuiv;
extern PFNGLGETPIXELMAPUSVPROC glGetPixelMapusv;
extern PFNGLBITMAPPROC glBitmap;
extern PFNGLREADPIXELSPROC glReadPixels;
extern PFNGLDRAWPIXELSPROC glDrawPixels;
extern PFNGLCOPYPIXELSPROC glCopyPixels;
extern PFNGLSTENCILFUNCPROC glStencilFunc;
extern PFNGLSTENCILMASKPROC glStencilMask;
extern PFNGLSTENCILOPPROC glStencilOp;
extern PFNGLCLEARSTENCILPROC glClearStencil;
extern PFNGLTEXGENDPROC glTexGend;
extern PFNGLTEXGENFPROC glTexGenf;
extern PFNGLTEXGENIPROC glTexGeni;
extern PFNGLTEXGENDVPROC glTexGendv;
extern PFNGLTEXGENFVPROC glTexGenfv;
extern PFNGLTEXGENIVPROC glTexGeniv;
extern PFNGLGETTEXGENDVPROC glGetTexGendv;
extern PFNGLGETTEXGENFVPROC glGetTexGenfv;
extern PFNGLGETTEXGENIVPROC glGetTexGeniv;
extern PFNGLTEXENVFPROC glTexEnvf;
extern PFNGLTEXENVIPROC glTexEnvi;
extern PFNGLTEXENVFVPROC glTexEnvfv;
extern PFNGLTEXENVIVPROC glTexEnviv;
extern PFNGLGETTEXENVFVPROC glGetTexEnvfv;
extern PFNGLGETTEXENVIVPROC glGetTexEnviv;
extern PFNGLTEXPARAMETERFPROC glTexParameterf;
extern PFNGLTEXPARAMETERIPROC glTexParameteri;
extern PFNGLTEXPARAMETERFVPROC glTexParameterfv;
extern PFNGLTEXPARAMETERIVPROC glTexParameteriv;
extern PFNGLGETTEXPARAMETERFVPROC glGetTexParameterfv;
extern PFNGLGETTEXPARAMETERIVPROC glGetTexParameteriv;
extern PFNGLGETTEXLEVELPARAMETERFVPROC glGetTexLevelParameterfv;
extern PFNGLGETTEXLEVELPARAMETERIVPROC glGetTexLevelParameteriv;
extern PFNGLTEXIMAGE1DPROC glTexImage1D;
extern PFNGLTEXIMAGE2DPROC glTexImage2D;
extern PFNGLGETTEXIMAGEPROC glGetTexImage;
extern PFNGLGENTEXTURESPROC glGenTextures;
extern PFNGLDELETETEXTURESPROC glDeleteTextures;
extern PFNGLBINDTEXTUREPROC glBindTexture;
extern PFNGLPRIORITIZETEXTURESPROC glPrioritizeTextures;
extern PFNGLARETEXTURESRESIDENTPROC glAreTexturesResident;
extern PFNGLISTEXTUREPROC glIsTexture;
extern PFNGLTEXSUBIMAGE1DPROC glTexSubImage1D;
extern PFNGLTEXSUBIMAGE2DPROC glTexSubImage2D;
extern PFNGLCOPYTEXIMAGE1DPROC glCopyTexImage1D;
extern PFNGLCOPYTEXIMAGE2DPROC glCopyTexImage2D;
extern PFNGLCOPYTEXSUBIMAGE1DPROC glCopyTexSubImage1D;
extern PFNGLCOPYTEXSUBIMAGE2DPROC glCopyTexSubImage2D;
extern PFNGLMAP1DPROC glMap1d;
extern PFNGLMAP1FPROC glMap1f;
extern PFNGLMAP2DPROC glMap2d;
extern PFNGLMAP2FPROC glMap2f;
extern PFNGLGETMAPDVPROC glGetMapdv;
extern PFNGLGETMAPFVPROC glGetMapfv;
extern PFNGLGETMAPIVPROC glGetMapiv;
extern PFNGLEVALCOORD1DPROC glEvalCoord1d;
extern PFNGLEVALCOORD1FPROC glEvalCoord1f;
extern PFNGLEVALCOORD1DVPROC glEvalCoord1dv;
extern PFNGLEVALCOORD1FVPROC glEvalCoord1fv;
extern PFNGLEVALCOORD2DPROC glEvalCoord2d;
extern PFNGLEVALCOORD2FPROC glEvalCoord2f;
extern PFNGLEVALCOORD2DVPROC glEvalCoord2dv;
extern PFNGLEVALCOORD2FVPROC glEvalCoord2fv;
extern PFNGLMAPGRID1DPROC glMapGrid1d;
extern PFNGLMAPGRID1FPROC glMapGrid1f;
extern PFNGLMAPGRID2DPROC glMapGrid2d;
extern PFNGLMAPGRID2FPROC glMapGrid2f;
extern PFNGLEVALPOINT1PROC glEvalPoint1;
extern PFNGLEVALPOINT2PROC glEvalPoint2;
extern PFNGLEVALMESH1PROC glEvalMesh1;
extern PFNGLEVALMESH2PROC glEvalMesh2;
extern PFNGLFOGFPROC glFogf;
extern PFNGLFOGIPROC glFogi;
extern PFNGLFOGFVPROC glFogfv;
extern PFNGLFOGIVPROC glFogiv;
extern PFNGLFEEDBACKBUFFERPROC glFeedbackBuffer;
extern PFNGLPASSTHROUGHPROC glPassThrough;
extern PFNGLSELECTBUFFERPROC glSelectBuffer;
extern PFNGLINITNAMESPROC glInitNames;
extern PFNGLLOADNAMEPROC glLoadName;
extern PFNGLPUSHNAMEPROC glPushName;
extern PFNGLPOPNAMEPROC glPopName;
