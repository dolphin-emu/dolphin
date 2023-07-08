/*
** Copyright (c) 2013-2015 The Khronos Group Inc.
** SPDX-License-Identifier: MIT
*/

#include "Common/GL/GLExtensions/gl_common.h"

/*
 * Constants
 */

/* Boolean values */
#define GL_FALSE 0
#define GL_TRUE 1

/* Data types */
#define GL_BYTE 0x1400
#define GL_UNSIGNED_BYTE 0x1401
#define GL_SHORT 0x1402
#define GL_UNSIGNED_SHORT 0x1403
#define GL_INT 0x1404
#define GL_UNSIGNED_INT 0x1405
#define GL_FLOAT 0x1406
#define GL_HALF_FLOAT 0x140B
#define GL_2_BYTES 0x1407
#define GL_3_BYTES 0x1408
#define GL_4_BYTES 0x1409
#define GL_DOUBLE 0x140A

/* Primitives */
#define GL_POINTS 0x0000
#define GL_LINES 0x0001
#define GL_LINE_LOOP 0x0002
#define GL_LINE_STRIP 0x0003
#define GL_TRIANGLES 0x0004
#define GL_TRIANGLE_STRIP 0x0005
#define GL_TRIANGLE_FAN 0x0006
#define GL_QUADS 0x0007
#define GL_QUAD_STRIP 0x0008
#define GL_POLYGON 0x0009

/* Vertex Arrays */
#define GL_VERTEX_ARRAY 0x8074
#define GL_NORMAL_ARRAY 0x8075
#define GL_COLOR_ARRAY 0x8076
#define GL_INDEX_ARRAY 0x8077
#define GL_TEXTURE_COORD_ARRAY 0x8078
#define GL_EDGE_FLAG_ARRAY 0x8079
#define GL_VERTEX_ARRAY_SIZE 0x807A
#define GL_VERTEX_ARRAY_TYPE 0x807B
#define GL_VERTEX_ARRAY_STRIDE 0x807C
#define GL_NORMAL_ARRAY_TYPE 0x807E
#define GL_NORMAL_ARRAY_STRIDE 0x807F
#define GL_COLOR_ARRAY_SIZE 0x8081
#define GL_COLOR_ARRAY_TYPE 0x8082
#define GL_COLOR_ARRAY_STRIDE 0x8083
#define GL_INDEX_ARRAY_TYPE 0x8085
#define GL_INDEX_ARRAY_STRIDE 0x8086
#define GL_TEXTURE_COORD_ARRAY_SIZE 0x8088
#define GL_TEXTURE_COORD_ARRAY_TYPE 0x8089
#define GL_TEXTURE_COORD_ARRAY_STRIDE 0x808A
#define GL_EDGE_FLAG_ARRAY_STRIDE 0x808C
#define GL_VERTEX_ARRAY_POINTER 0x808E
#define GL_NORMAL_ARRAY_POINTER 0x808F
#define GL_COLOR_ARRAY_POINTER 0x8090
#define GL_INDEX_ARRAY_POINTER 0x8091
#define GL_TEXTURE_COORD_ARRAY_POINTER 0x8092
#define GL_EDGE_FLAG_ARRAY_POINTER 0x8093
#define GL_V2F 0x2A20
#define GL_V3F 0x2A21
#define GL_C4UB_V2F 0x2A22
#define GL_C4UB_V3F 0x2A23
#define GL_C3F_V3F 0x2A24
#define GL_N3F_V3F 0x2A25
#define GL_C4F_N3F_V3F 0x2A26
#define GL_T2F_V3F 0x2A27
#define GL_T4F_V4F 0x2A28
#define GL_T2F_C4UB_V3F 0x2A29
#define GL_T2F_C3F_V3F 0x2A2A
#define GL_T2F_N3F_V3F 0x2A2B
#define GL_T2F_C4F_N3F_V3F 0x2A2C
#define GL_T4F_C4F_N3F_V4F 0x2A2D

/* Matrix Mode */
#define GL_MATRIX_MODE 0x0BA0
#define GL_MODELVIEW 0x1700
#define GL_PROJECTION 0x1701
#define GL_TEXTURE 0x1702

/* Points */
#define GL_POINT_SMOOTH 0x0B10
#define GL_POINT_SIZE 0x0B11
#define GL_POINT_SIZE_GRANULARITY 0x0B13
#define GL_POINT_SIZE_RANGE 0x0B12

/* Lines */
#define GL_LINE_SMOOTH 0x0B20
#define GL_LINE_STIPPLE 0x0B24
#define GL_LINE_STIPPLE_PATTERN 0x0B25
#define GL_LINE_STIPPLE_REPEAT 0x0B26
#define GL_LINE_WIDTH 0x0B21
#define GL_LINE_WIDTH_GRANULARITY 0x0B23
#define GL_LINE_WIDTH_RANGE 0x0B22

/* Polygons */
#define GL_POINT 0x1B00
#define GL_LINE 0x1B01
#define GL_FILL 0x1B02
#define GL_CW 0x0900
#define GL_CCW 0x0901
#define GL_FRONT 0x0404
#define GL_BACK 0x0405
#define GL_POLYGON_MODE 0x0B40
#define GL_POLYGON_SMOOTH 0x0B41
#define GL_POLYGON_STIPPLE 0x0B42
#define GL_EDGE_FLAG 0x0B43
#define GL_CULL_FACE 0x0B44
#define GL_CULL_FACE_MODE 0x0B45
#define GL_FRONT_FACE 0x0B46
#define GL_POLYGON_OFFSET_FACTOR 0x8038
#define GL_POLYGON_OFFSET_UNITS 0x2A00
#define GL_POLYGON_OFFSET_POINT 0x2A01
#define GL_POLYGON_OFFSET_LINE 0x2A02
#define GL_POLYGON_OFFSET_FILL 0x8037

/* Display Lists */
#define GL_COMPILE 0x1300
#define GL_COMPILE_AND_EXECUTE 0x1301
#define GL_LIST_BASE 0x0B32
#define GL_LIST_INDEX 0x0B33
#define GL_LIST_MODE 0x0B30

/* Depth buffer */
#define GL_NEVER 0x0200
#define GL_LESS 0x0201
#define GL_EQUAL 0x0202
#define GL_LEQUAL 0x0203
#define GL_GREATER 0x0204
#define GL_NOTEQUAL 0x0205
#define GL_GEQUAL 0x0206
#define GL_ALWAYS 0x0207
#define GL_DEPTH_TEST 0x0B71
#define GL_DEPTH_BITS 0x0D56
#define GL_DEPTH_CLEAR_VALUE 0x0B73
#define GL_DEPTH_FUNC 0x0B74
#define GL_DEPTH_RANGE 0x0B70
#define GL_DEPTH_WRITEMASK 0x0B72
#define GL_DEPTH_COMPONENT 0x1902

/* Lighting */
#define GL_LIGHTING 0x0B50
#define GL_LIGHT0 0x4000
#define GL_LIGHT1 0x4001
#define GL_LIGHT2 0x4002
#define GL_LIGHT3 0x4003
#define GL_LIGHT4 0x4004
#define GL_LIGHT5 0x4005
#define GL_LIGHT6 0x4006
#define GL_LIGHT7 0x4007
#define GL_SPOT_EXPONENT 0x1205
#define GL_SPOT_CUTOFF 0x1206
#define GL_CONSTANT_ATTENUATION 0x1207
#define GL_LINEAR_ATTENUATION 0x1208
#define GL_QUADRATIC_ATTENUATION 0x1209
#define GL_AMBIENT 0x1200
#define GL_DIFFUSE 0x1201
#define GL_SPECULAR 0x1202
#define GL_SHININESS 0x1601
#define GL_EMISSION 0x1600
#define GL_POSITION 0x1203
#define GL_SPOT_DIRECTION 0x1204
#define GL_AMBIENT_AND_DIFFUSE 0x1602
#define GL_COLOR_INDEXES 0x1603
#define GL_LIGHT_MODEL_TWO_SIDE 0x0B52
#define GL_LIGHT_MODEL_LOCAL_VIEWER 0x0B51
#define GL_LIGHT_MODEL_AMBIENT 0x0B53
#define GL_FRONT_AND_BACK 0x0408
#define GL_SHADE_MODEL 0x0B54
#define GL_FLAT 0x1D00
#define GL_SMOOTH 0x1D01
#define GL_COLOR_MATERIAL 0x0B57
#define GL_COLOR_MATERIAL_FACE 0x0B55
#define GL_COLOR_MATERIAL_PARAMETER 0x0B56
#define GL_NORMALIZE 0x0BA1

/* User clipping planes */
#define GL_CLIP_PLANE0 0x3000
#define GL_CLIP_PLANE1 0x3001
#define GL_CLIP_PLANE2 0x3002
#define GL_CLIP_PLANE3 0x3003
#define GL_CLIP_PLANE4 0x3004
#define GL_CLIP_PLANE5 0x3005

/* Accumulation buffer */
#define GL_ACCUM_RED_BITS 0x0D58
#define GL_ACCUM_GREEN_BITS 0x0D59
#define GL_ACCUM_BLUE_BITS 0x0D5A
#define GL_ACCUM_ALPHA_BITS 0x0D5B
#define GL_ACCUM_CLEAR_VALUE 0x0B80
#define GL_ACCUM 0x0100
#define GL_ADD 0x0104
#define GL_LOAD 0x0101
#define GL_MULT 0x0103
#define GL_RETURN 0x0102

/* Alpha testing */
#define GL_ALPHA_TEST 0x0BC0
#define GL_ALPHA_TEST_REF 0x0BC2
#define GL_ALPHA_TEST_FUNC 0x0BC1

/* Blending */
#define GL_BLEND 0x0BE2
#define GL_BLEND_SRC 0x0BE1
#define GL_BLEND_DST 0x0BE0
#define GL_ZERO 0
#define GL_ONE 1
#define GL_SRC_COLOR 0x0300
#define GL_ONE_MINUS_SRC_COLOR 0x0301
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_DST_ALPHA 0x0304
#define GL_ONE_MINUS_DST_ALPHA 0x0305
#define GL_DST_COLOR 0x0306
#define GL_ONE_MINUS_DST_COLOR 0x0307
#define GL_SRC_ALPHA_SATURATE 0x0308

/* Render Mode */
#define GL_FEEDBACK 0x1C01
#define GL_RENDER 0x1C00
#define GL_SELECT 0x1C02

/* Feedback */
#define GL_2D 0x0600
#define GL_3D 0x0601
#define GL_3D_COLOR 0x0602
#define GL_3D_COLOR_TEXTURE 0x0603
#define GL_4D_COLOR_TEXTURE 0x0604
#define GL_POINT_TOKEN 0x0701
#define GL_LINE_TOKEN 0x0702
#define GL_LINE_RESET_TOKEN 0x0707
#define GL_POLYGON_TOKEN 0x0703
#define GL_BITMAP_TOKEN 0x0704
#define GL_DRAW_PIXEL_TOKEN 0x0705
#define GL_COPY_PIXEL_TOKEN 0x0706
#define GL_PASS_THROUGH_TOKEN 0x0700
#define GL_FEEDBACK_BUFFER_POINTER 0x0DF0
#define GL_FEEDBACK_BUFFER_SIZE 0x0DF1
#define GL_FEEDBACK_BUFFER_TYPE 0x0DF2

/* Selection */
#define GL_SELECTION_BUFFER_POINTER 0x0DF3
#define GL_SELECTION_BUFFER_SIZE 0x0DF4

/* Fog */
#define GL_FOG 0x0B60
#define GL_FOG_MODE 0x0B65
#define GL_FOG_DENSITY 0x0B62
#define GL_FOG_COLOR 0x0B66
#define GL_FOG_INDEX 0x0B61
#define GL_FOG_START 0x0B63
#define GL_FOG_END 0x0B64
#define GL_LINEAR 0x2601
#define GL_EXP 0x0800
#define GL_EXP2 0x0801

/* Logic Ops */
#define GL_LOGIC_OP 0x0BF1
#define GL_INDEX_LOGIC_OP 0x0BF1
#define GL_COLOR_LOGIC_OP 0x0BF2
#define GL_LOGIC_OP_MODE 0x0BF0
#define GL_CLEAR 0x1500
#define GL_SET 0x150F
#define GL_COPY 0x1503
#define GL_COPY_INVERTED 0x150C
#define GL_NOOP 0x1505
#define GL_INVERT 0x150A
#define GL_AND 0x1501
#define GL_NAND 0x150E
#define GL_OR 0x1507
#define GL_NOR 0x1508
#define GL_XOR 0x1506
#define GL_EQUIV 0x1509
#define GL_AND_REVERSE 0x1502
#define GL_AND_INVERTED 0x1504
#define GL_OR_REVERSE 0x150B
#define GL_OR_INVERTED 0x150D

/* Stencil */
#define GL_STENCIL_BITS 0x0D57
#define GL_STENCIL_TEST 0x0B90
#define GL_STENCIL_CLEAR_VALUE 0x0B91
#define GL_STENCIL_FUNC 0x0B92
#define GL_STENCIL_VALUE_MASK 0x0B93
#define GL_STENCIL_FAIL 0x0B94
#define GL_STENCIL_PASS_DEPTH_FAIL 0x0B95
#define GL_STENCIL_PASS_DEPTH_PASS 0x0B96
#define GL_STENCIL_REF 0x0B97
#define GL_STENCIL_WRITEMASK 0x0B98
#define GL_STENCIL_INDEX 0x1901
#define GL_KEEP 0x1E00
#define GL_REPLACE 0x1E01
#define GL_INCR 0x1E02
#define GL_DECR 0x1E03

/* Buffers, Pixel Drawing/Reading */
#define GL_NONE 0
#define GL_LEFT 0x0406
#define GL_RIGHT 0x0407
/*GL_FRONT					0x0404 */
/*GL_BACK					0x0405 */
/*GL_FRONT_AND_BACK				0x0408 */
#define GL_FRONT_LEFT 0x0400
#define GL_FRONT_RIGHT 0x0401
#define GL_BACK_LEFT 0x0402
#define GL_BACK_RIGHT 0x0403
#define GL_AUX0 0x0409
#define GL_AUX1 0x040A
#define GL_AUX2 0x040B
#define GL_AUX3 0x040C
#define GL_COLOR_INDEX 0x1900
#define GL_RED 0x1903
#define GL_GREEN 0x1904
#define GL_BLUE 0x1905
#define GL_ALPHA 0x1906
#define GL_LUMINANCE 0x1909
#define GL_LUMINANCE_ALPHA 0x190A
#define GL_ALPHA_BITS 0x0D55
#define GL_RED_BITS 0x0D52
#define GL_GREEN_BITS 0x0D53
#define GL_BLUE_BITS 0x0D54
#define GL_INDEX_BITS 0x0D51
#define GL_SUBPIXEL_BITS 0x0D50
#define GL_AUX_BUFFERS 0x0C00
#define GL_READ_BUFFER 0x0C02
#define GL_DRAW_BUFFER 0x0C01
#define GL_DOUBLEBUFFER 0x0C32
#define GL_STEREO 0x0C33
#define GL_BITMAP 0x1A00
#define GL_COLOR 0x1800
#define GL_DEPTH 0x1801
#define GL_STENCIL 0x1802
#define GL_DITHER 0x0BD0
#define GL_RGB 0x1907
#define GL_RGBA 0x1908

/* Implementation limits */
#define GL_MAX_LIST_NESTING 0x0B31
#define GL_MAX_EVAL_ORDER 0x0D30
#define GL_MAX_LIGHTS 0x0D31
#define GL_MAX_CLIP_PLANES 0x0D32
#define GL_MAX_TEXTURE_SIZE 0x0D33
#define GL_MAX_PIXEL_MAP_TABLE 0x0D34
#define GL_MAX_ATTRIB_STACK_DEPTH 0x0D35
#define GL_MAX_MODELVIEW_STACK_DEPTH 0x0D36
#define GL_MAX_NAME_STACK_DEPTH 0x0D37
#define GL_MAX_PROJECTION_STACK_DEPTH 0x0D38
#define GL_MAX_TEXTURE_STACK_DEPTH 0x0D39
#define GL_MAX_VIEWPORT_DIMS 0x0D3A
#define GL_MAX_CLIENT_ATTRIB_STACK_DEPTH 0x0D3B

/* Gets */
#define GL_ATTRIB_STACK_DEPTH 0x0BB0
#define GL_CLIENT_ATTRIB_STACK_DEPTH 0x0BB1
#define GL_COLOR_CLEAR_VALUE 0x0C22
#define GL_COLOR_WRITEMASK 0x0C23
#define GL_CURRENT_INDEX 0x0B01
#define GL_CURRENT_COLOR 0x0B00
#define GL_CURRENT_NORMAL 0x0B02
#define GL_CURRENT_RASTER_COLOR 0x0B04
#define GL_CURRENT_RASTER_DISTANCE 0x0B09
#define GL_CURRENT_RASTER_INDEX 0x0B05
#define GL_CURRENT_RASTER_POSITION 0x0B07
#define GL_CURRENT_RASTER_TEXTURE_COORDS 0x0B06
#define GL_CURRENT_RASTER_POSITION_VALID 0x0B08
#define GL_CURRENT_TEXTURE_COORDS 0x0B03
#define GL_INDEX_CLEAR_VALUE 0x0C20
#define GL_INDEX_MODE 0x0C30
#define GL_INDEX_WRITEMASK 0x0C21
#define GL_MODELVIEW_MATRIX 0x0BA6
#define GL_MODELVIEW_STACK_DEPTH 0x0BA3
#define GL_NAME_STACK_DEPTH 0x0D70
#define GL_PROJECTION_MATRIX 0x0BA7
#define GL_PROJECTION_STACK_DEPTH 0x0BA4
#define GL_RENDER_MODE 0x0C40
#define GL_RGBA_MODE 0x0C31
#define GL_TEXTURE_MATRIX 0x0BA8
#define GL_TEXTURE_STACK_DEPTH 0x0BA5
#define GL_VIEWPORT 0x0BA2

/* Evaluators */
#define GL_AUTO_NORMAL 0x0D80
#define GL_MAP1_COLOR_4 0x0D90
#define GL_MAP1_INDEX 0x0D91
#define GL_MAP1_NORMAL 0x0D92
#define GL_MAP1_TEXTURE_COORD_1 0x0D93
#define GL_MAP1_TEXTURE_COORD_2 0x0D94
#define GL_MAP1_TEXTURE_COORD_3 0x0D95
#define GL_MAP1_TEXTURE_COORD_4 0x0D96
#define GL_MAP1_VERTEX_3 0x0D97
#define GL_MAP1_VERTEX_4 0x0D98
#define GL_MAP2_COLOR_4 0x0DB0
#define GL_MAP2_INDEX 0x0DB1
#define GL_MAP2_NORMAL 0x0DB2
#define GL_MAP2_TEXTURE_COORD_1 0x0DB3
#define GL_MAP2_TEXTURE_COORD_2 0x0DB4
#define GL_MAP2_TEXTURE_COORD_3 0x0DB5
#define GL_MAP2_TEXTURE_COORD_4 0x0DB6
#define GL_MAP2_VERTEX_3 0x0DB7
#define GL_MAP2_VERTEX_4 0x0DB8
#define GL_MAP1_GRID_DOMAIN 0x0DD0
#define GL_MAP1_GRID_SEGMENTS 0x0DD1
#define GL_MAP2_GRID_DOMAIN 0x0DD2
#define GL_MAP2_GRID_SEGMENTS 0x0DD3
#define GL_COEFF 0x0A00
#define GL_ORDER 0x0A01
#define GL_DOMAIN 0x0A02

/* Hints */
#define GL_PERSPECTIVE_CORRECTION_HINT 0x0C50
#define GL_POINT_SMOOTH_HINT 0x0C51
#define GL_LINE_SMOOTH_HINT 0x0C52
#define GL_POLYGON_SMOOTH_HINT 0x0C53
#define GL_FOG_HINT 0x0C54
#define GL_DONT_CARE 0x1100
#define GL_FASTEST 0x1101
#define GL_NICEST 0x1102

/* Scissor box */
#define GL_SCISSOR_BOX 0x0C10
#define GL_SCISSOR_TEST 0x0C11

/* Pixel Mode / Transfer */
#define GL_MAP_COLOR 0x0D10
#define GL_MAP_STENCIL 0x0D11
#define GL_INDEX_SHIFT 0x0D12
#define GL_INDEX_OFFSET 0x0D13
#define GL_RED_SCALE 0x0D14
#define GL_RED_BIAS 0x0D15
#define GL_GREEN_SCALE 0x0D18
#define GL_GREEN_BIAS 0x0D19
#define GL_BLUE_SCALE 0x0D1A
#define GL_BLUE_BIAS 0x0D1B
#define GL_ALPHA_SCALE 0x0D1C
#define GL_ALPHA_BIAS 0x0D1D
#define GL_DEPTH_SCALE 0x0D1E
#define GL_DEPTH_BIAS 0x0D1F
#define GL_PIXEL_MAP_S_TO_S_SIZE 0x0CB1
#define GL_PIXEL_MAP_I_TO_I_SIZE 0x0CB0
#define GL_PIXEL_MAP_I_TO_R_SIZE 0x0CB2
#define GL_PIXEL_MAP_I_TO_G_SIZE 0x0CB3
#define GL_PIXEL_MAP_I_TO_B_SIZE 0x0CB4
#define GL_PIXEL_MAP_I_TO_A_SIZE 0x0CB5
#define GL_PIXEL_MAP_R_TO_R_SIZE 0x0CB6
#define GL_PIXEL_MAP_G_TO_G_SIZE 0x0CB7
#define GL_PIXEL_MAP_B_TO_B_SIZE 0x0CB8
#define GL_PIXEL_MAP_A_TO_A_SIZE 0x0CB9
#define GL_PIXEL_MAP_S_TO_S 0x0C71
#define GL_PIXEL_MAP_I_TO_I 0x0C70
#define GL_PIXEL_MAP_I_TO_R 0x0C72
#define GL_PIXEL_MAP_I_TO_G 0x0C73
#define GL_PIXEL_MAP_I_TO_B 0x0C74
#define GL_PIXEL_MAP_I_TO_A 0x0C75
#define GL_PIXEL_MAP_R_TO_R 0x0C76
#define GL_PIXEL_MAP_G_TO_G 0x0C77
#define GL_PIXEL_MAP_B_TO_B 0x0C78
#define GL_PIXEL_MAP_A_TO_A 0x0C79
#define GL_PACK_ALIGNMENT 0x0D05
#define GL_PACK_LSB_FIRST 0x0D01
#define GL_PACK_ROW_LENGTH 0x0D02
#define GL_PACK_SKIP_PIXELS 0x0D04
#define GL_PACK_SKIP_ROWS 0x0D03
#define GL_PACK_SWAP_BYTES 0x0D00
#define GL_UNPACK_ALIGNMENT 0x0CF5
#define GL_UNPACK_LSB_FIRST 0x0CF1
#define GL_UNPACK_ROW_LENGTH 0x0CF2
#define GL_UNPACK_SKIP_PIXELS 0x0CF4
#define GL_UNPACK_SKIP_ROWS 0x0CF3
#define GL_UNPACK_SWAP_BYTES 0x0CF0
#define GL_ZOOM_X 0x0D16
#define GL_ZOOM_Y 0x0D17

/* Texture mapping */
#define GL_TEXTURE_ENV 0x2300
#define GL_TEXTURE_ENV_MODE 0x2200
#define GL_TEXTURE_1D 0x0DE0
#define GL_TEXTURE_2D 0x0DE1
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_ENV_COLOR 0x2201
#define GL_TEXTURE_GEN_S 0x0C60
#define GL_TEXTURE_GEN_T 0x0C61
#define GL_TEXTURE_GEN_R 0x0C62
#define GL_TEXTURE_GEN_Q 0x0C63
#define GL_TEXTURE_GEN_MODE 0x2500
#define GL_TEXTURE_BORDER_COLOR 0x1004
#define GL_TEXTURE_WIDTH 0x1000
#define GL_TEXTURE_HEIGHT 0x1001
#define GL_TEXTURE_BORDER 0x1005
#define GL_TEXTURE_COMPONENTS 0x1003
#define GL_TEXTURE_RED_SIZE 0x805C
#define GL_TEXTURE_GREEN_SIZE 0x805D
#define GL_TEXTURE_BLUE_SIZE 0x805E
#define GL_TEXTURE_ALPHA_SIZE 0x805F
#define GL_TEXTURE_LUMINANCE_SIZE 0x8060
#define GL_TEXTURE_INTENSITY_SIZE 0x8061
#define GL_NEAREST_MIPMAP_NEAREST 0x2700
#define GL_NEAREST_MIPMAP_LINEAR 0x2702
#define GL_LINEAR_MIPMAP_NEAREST 0x2701
#define GL_LINEAR_MIPMAP_LINEAR 0x2703
#define GL_OBJECT_LINEAR 0x2401
#define GL_OBJECT_PLANE 0x2501
#define GL_EYE_LINEAR 0x2400
#define GL_EYE_PLANE 0x2502
#define GL_SPHERE_MAP 0x2402
#define GL_DECAL 0x2101
#define GL_MODULATE 0x2100
#define GL_NEAREST 0x2600
#define GL_REPEAT 0x2901
#define GL_CLAMP 0x2900
#define GL_S 0x2000
#define GL_T 0x2001
#define GL_R 0x2002
#define GL_Q 0x2003

/* Utility */
#define GL_VENDOR 0x1F00
#define GL_RENDERER 0x1F01
#define GL_VERSION 0x1F02
#define GL_EXTENSIONS 0x1F03

/* Errors */
#define GL_NO_ERROR 0
#define GL_INVALID_ENUM 0x0500
#define GL_INVALID_VALUE 0x0501
#define GL_INVALID_OPERATION 0x0502
#define GL_STACK_OVERFLOW 0x0503
#define GL_STACK_UNDERFLOW 0x0504
#define GL_OUT_OF_MEMORY 0x0505

/* glPush/PopAttrib bits */
#define GL_CURRENT_BIT 0x00000001
#define GL_POINT_BIT 0x00000002
#define GL_LINE_BIT 0x00000004
#define GL_POLYGON_BIT 0x00000008
#define GL_POLYGON_STIPPLE_BIT 0x00000010
#define GL_PIXEL_MODE_BIT 0x00000020
#define GL_LIGHTING_BIT 0x00000040
#define GL_FOG_BIT 0x00000080
#define GL_DEPTH_BUFFER_BIT 0x00000100
#define GL_ACCUM_BUFFER_BIT 0x00000200
#define GL_STENCIL_BUFFER_BIT 0x00000400
#define GL_VIEWPORT_BIT 0x00000800
#define GL_TRANSFORM_BIT 0x00001000
#define GL_ENABLE_BIT 0x00002000
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_HINT_BIT 0x00008000
#define GL_EVAL_BIT 0x00010000
#define GL_LIST_BIT 0x00020000
#define GL_TEXTURE_BIT 0x00040000
#define GL_SCISSOR_BIT 0x00080000
#define GL_ALL_ATTRIB_BITS 0x000FFFFF

/* OpenGL 1.1 */
#define GL_PROXY_TEXTURE_1D 0x8063
#define GL_PROXY_TEXTURE_2D 0x8064
#define GL_TEXTURE_PRIORITY 0x8066
#define GL_TEXTURE_RESIDENT 0x8067
#define GL_TEXTURE_BINDING_1D 0x8068
#define GL_TEXTURE_BINDING_2D 0x8069
#define GL_TEXTURE_INTERNAL_FORMAT 0x1003
#define GL_ALPHA4 0x803B
#define GL_ALPHA8 0x803C
#define GL_ALPHA12 0x803D
#define GL_ALPHA16 0x803E
#define GL_LUMINANCE4 0x803F
#define GL_LUMINANCE8 0x8040
#define GL_LUMINANCE12 0x8041
#define GL_LUMINANCE16 0x8042
#define GL_LUMINANCE4_ALPHA4 0x8043
#define GL_LUMINANCE6_ALPHA2 0x8044
#define GL_LUMINANCE8_ALPHA8 0x8045
#define GL_LUMINANCE12_ALPHA4 0x8046
#define GL_LUMINANCE12_ALPHA12 0x8047
#define GL_LUMINANCE16_ALPHA16 0x8048
#define GL_INTENSITY 0x8049
#define GL_INTENSITY4 0x804A
#define GL_INTENSITY8 0x804B
#define GL_INTENSITY12 0x804C
#define GL_INTENSITY16 0x804D
#define GL_R3_G3_B2 0x2A10
#define GL_RGB4 0x804F
#define GL_RGB5 0x8050
#define GL_RGB8 0x8051
#define GL_RGB10 0x8052
#define GL_RGB12 0x8053
#define GL_RGB16 0x8054
#define GL_RGBA2 0x8055
#define GL_RGBA4 0x8056
#define GL_RGB5_A1 0x8057
#define GL_RGBA8 0x8058
#define GL_RGB10_A2 0x8059
#define GL_RGBA12 0x805A
#define GL_RGBA16 0x805B
#define GL_CLIENT_PIXEL_STORE_BIT 0x00000001
#define GL_CLIENT_VERTEX_ARRAY_BIT 0x00000002
#define GL_ALL_CLIENT_ATTRIB_BITS 0xFFFFFFFF
#define GL_CLIENT_ALL_ATTRIB_BITS 0xFFFFFFFF

typedef void(APIENTRYP PFNDOLCLEARINDEXPROC)(GLfloat c);
typedef void(APIENTRYP PFNDOLCLEARCOLORPROC)(GLclampf red, GLclampf green, GLclampf blue,
                                             GLclampf alpha);
typedef void(APIENTRYP PFNDOLCLEARPROC)(GLbitfield mask);
typedef void(APIENTRYP PFNDOLINDEXMASKPROC)(GLuint mask);
typedef void(APIENTRYP PFNDOLCOLORMASKPROC)(GLboolean red, GLboolean green, GLboolean blue,
                                            GLboolean alpha);
typedef void(APIENTRYP PFNDOLALPHAFUNCPROC)(GLenum func, GLclampf ref);
typedef void(APIENTRYP PFNDOLBLENDFUNCPROC)(GLenum sfactor, GLenum dfactor);
typedef void(APIENTRYP PFNDOLLOGICOPPROC)(GLenum opcode);
typedef void(APIENTRYP PFNDOLCULLFACEPROC)(GLenum mode);
typedef void(APIENTRYP PFNDOLFRONTFACEPROC)(GLenum mode);
typedef void(APIENTRYP PFNDOLPOINTSIZEPROC)(GLfloat size);
typedef void(APIENTRYP PFNDOLLINEWIDTHPROC)(GLfloat width);
typedef void(APIENTRYP PFNDOLLINESTIPPLEPROC)(GLint factor, GLushort pattern);
typedef void(APIENTRYP PFNDOLPOLYGONMODEPROC)(GLenum face, GLenum mode);
typedef void(APIENTRYP PFNDOLPOLYGONOFFSETPROC)(GLfloat factor, GLfloat units);
typedef void(APIENTRYP PFNDOLPOLYGONSTIPPLEPROC)(const GLubyte* mask);
typedef void(APIENTRYP PFNDOLGETPOLYGONSTIPPLEPROC)(GLubyte* mask);
typedef void(APIENTRYP PFNDOLEDGEFLAGPROC)(GLboolean flag);
typedef void(APIENTRYP PFNDOLEDGEFLAGVPROC)(const GLboolean* flag);
typedef void(APIENTRYP PFNDOLSCISSORPROC)(GLint x, GLint y, GLsizei width, GLsizei height);
typedef void(APIENTRYP PFNDOLCLIPPLANEPROC)(GLenum plane, const GLdouble* equation);
typedef void(APIENTRYP PFNDOLGETCLIPPLANEPROC)(GLenum plane, GLdouble* equation);
typedef void(APIENTRYP PFNDOLDRAWBUFFERPROC)(GLenum mode);
typedef void(APIENTRYP PFNDOLREADBUFFERPROC)(GLenum mode);
typedef void(APIENTRYP PFNDOLENABLEPROC)(GLenum cap);
typedef void(APIENTRYP PFNDOLDISABLEPROC)(GLenum cap);
typedef GLboolean(APIENTRYP PFNDOLISENABLEDPROC)(GLenum cap);
typedef void(APIENTRYP PFNDOLENABLECLIENTSTATEPROC)(GLenum cap);  /* 1.1 */
typedef void(APIENTRYP PFNDOLDISABLECLIENTSTATEPROC)(GLenum cap); /* 1.1 */
typedef void(APIENTRYP PFNDOLGETBOOLEANVPROC)(GLenum pname, GLboolean* params);
typedef void(APIENTRYP PFNDOLGETDOUBLEVPROC)(GLenum pname, GLdouble* params);
typedef void(APIENTRYP PFNDOLGETFLOATVPROC)(GLenum pname, GLfloat* params);
typedef void(APIENTRYP PFNDOLGETINTEGERVPROC)(GLenum pname, GLint* params);
typedef void(APIENTRYP PFNDOLPUSHATTRIBPROC)(GLbitfield mask);
typedef void(APIENTRYP PFNDOLPOPATTRIBPROC)(void);
typedef void(APIENTRYP PFNDOLPUSHCLIENTATTRIBPROC)(GLbitfield mask); /* 1.1 */
typedef void(APIENTRYP PFNDOLPOPCLIENTATTRIBPROC)(void);             /* 1.1 */
typedef GLint(APIENTRYP PFNDOLRENDERMODEPROC)(GLenum mode);
typedef GLenum(APIENTRYP PFNDOLGETERRORPROC)(void);
typedef const GLubyte*(APIENTRYP PFNDOLGETSTRINGPROC)(GLenum name);
typedef void(APIENTRYP PFNDOLFINISHPROC)(void);
typedef void(APIENTRYP PFNDOLFLUSHPROC)(void);
typedef void(APIENTRYP PFNDOLHINTPROC)(GLenum target, GLenum mode);
typedef void(APIENTRYP PFNDOLCLEARDEPTHPROC)(GLclampd depth);
typedef void(APIENTRYP PFNDOLDEPTHFUNCPROC)(GLenum func);
typedef void(APIENTRYP PFNDOLDEPTHMASKPROC)(GLboolean flag);
typedef void(APIENTRYP PFNDOLDEPTHRANGEPROC)(GLclampd near_val, GLclampd far_val);
typedef void(APIENTRYP PFNDOLCLEARACCUMPROC)(GLfloat red, GLfloat green, GLfloat blue,
                                             GLfloat alpha);
typedef void(APIENTRYP PFNDOLACCUMPROC)(GLenum op, GLfloat value);
typedef void(APIENTRYP PFNDOLMATRIXMODEPROC)(GLenum mode);
typedef void(APIENTRYP PFNDOLORTHOPROC)(GLdouble left, GLdouble right, GLdouble bottom,
                                        GLdouble top, GLdouble near_val, GLdouble far_val);
typedef void(APIENTRYP PFNDOLFRUSTUMPROC)(GLdouble left, GLdouble right, GLdouble bottom,
                                          GLdouble top, GLdouble near_val, GLdouble far_val);
typedef void(APIENTRYP PFNDOLVIEWPORTPROC)(GLint x, GLint y, GLsizei width, GLsizei height);
typedef void(APIENTRYP PFNDOLPUSHMATRIXPROC)(void);
typedef void(APIENTRYP PFNDOLPOPMATRIXPROC)(void);
typedef void(APIENTRYP PFNDOLLOADIDENTITYPROC)(void);
typedef void(APIENTRYP PFNDOLLOADMATRIXDPROC)(const GLdouble* m);
typedef void(APIENTRYP PFNDOLLOADMATRIXFPROC)(const GLfloat* m);
typedef void(APIENTRYP PFNDOLMULTMATRIXDPROC)(const GLdouble* m);
typedef void(APIENTRYP PFNDOLMULTMATRIXFPROC)(const GLfloat* m);
typedef void(APIENTRYP PFNDOLROTATEDPROC)(GLdouble angle, GLdouble x, GLdouble y, GLdouble z);
typedef void(APIENTRYP PFNDOLROTATEFPROC)(GLfloat angle, GLfloat x, GLfloat y, GLfloat z);
typedef void(APIENTRYP PFNDOLSCALEDPROC)(GLdouble x, GLdouble y, GLdouble z);
typedef void(APIENTRYP PFNDOLSCALEFPROC)(GLfloat x, GLfloat y, GLfloat z);
typedef void(APIENTRYP PFNDOLTRANSLATEDPROC)(GLdouble x, GLdouble y, GLdouble z);
typedef void(APIENTRYP PFNDOLTRANSLATEFPROC)(GLfloat x, GLfloat y, GLfloat z);
typedef GLboolean(APIENTRYP PFNDOLISLISTPROC)(GLuint list);
typedef void(APIENTRYP PFNDOLDELETELISTSPROC)(GLuint list, GLsizei range);
typedef GLuint(APIENTRYP PFNDOLGENLISTSPROC)(GLsizei range);
typedef void(APIENTRYP PFNDOLNEWLISTPROC)(GLuint list, GLenum mode);
typedef void(APIENTRYP PFNDOLENDLISTPROC)(void);
typedef void(APIENTRYP PFNDOLCALLLISTPROC)(GLuint list);
typedef void(APIENTRYP PFNDOLCALLLISTSPROC)(GLsizei n, GLenum type, const GLvoid* lists);
typedef void(APIENTRYP PFNDOLLISTBASEPROC)(GLuint base);
typedef void(APIENTRYP PFNDOLBEGINPROC)(GLenum mode);
typedef void(APIENTRYP PFNDOLENDPROC)(void);
typedef void(APIENTRYP PFNDOLVERTEX2DPROC)(GLdouble x, GLdouble y);
typedef void(APIENTRYP PFNDOLVERTEX2FPROC)(GLfloat x, GLfloat y);
typedef void(APIENTRYP PFNDOLVERTEX2IPROC)(GLint x, GLint y);
typedef void(APIENTRYP PFNDOLVERTEX2SPROC)(GLshort x, GLshort y);
typedef void(APIENTRYP PFNDOLVERTEX3DPROC)(GLdouble x, GLdouble y, GLdouble z);
typedef void(APIENTRYP PFNDOLVERTEX3FPROC)(GLfloat x, GLfloat y, GLfloat z);
typedef void(APIENTRYP PFNDOLVERTEX3IPROC)(GLint x, GLint y, GLint z);
typedef void(APIENTRYP PFNDOLVERTEX3SPROC)(GLshort x, GLshort y, GLshort z);
typedef void(APIENTRYP PFNDOLVERTEX4DPROC)(GLdouble x, GLdouble y, GLdouble z, GLdouble w);
typedef void(APIENTRYP PFNDOLVERTEX4FPROC)(GLfloat x, GLfloat y, GLfloat z, GLfloat w);
typedef void(APIENTRYP PFNDOLVERTEX4IPROC)(GLint x, GLint y, GLint z, GLint w);
typedef void(APIENTRYP PFNDOLVERTEX4SPROC)(GLshort x, GLshort y, GLshort z, GLshort w);
typedef void(APIENTRYP PFNDOLVERTEX2DVPROC)(const GLdouble* v);
typedef void(APIENTRYP PFNDOLVERTEX2FVPROC)(const GLfloat* v);
typedef void(APIENTRYP PFNDOLVERTEX2IVPROC)(const GLint* v);
typedef void(APIENTRYP PFNDOLVERTEX2SVPROC)(const GLshort* v);
typedef void(APIENTRYP PFNDOLVERTEX3DVPROC)(const GLdouble* v);
typedef void(APIENTRYP PFNDOLVERTEX3FVPROC)(const GLfloat* v);
typedef void(APIENTRYP PFNDOLVERTEX3IVPROC)(const GLint* v);
typedef void(APIENTRYP PFNDOLVERTEX3SVPROC)(const GLshort* v);
typedef void(APIENTRYP PFNDOLVERTEX4DVPROC)(const GLdouble* v);
typedef void(APIENTRYP PFNDOLVERTEX4FVPROC)(const GLfloat* v);
typedef void(APIENTRYP PFNDOLVERTEX4IVPROC)(const GLint* v);
typedef void(APIENTRYP PFNDOLVERTEX4SVPROC)(const GLshort* v);
typedef void(APIENTRYP PFNDOLNORMAL3BPROC)(GLbyte nx, GLbyte ny, GLbyte nz);
typedef void(APIENTRYP PFNDOLNORMAL3DPROC)(GLdouble nx, GLdouble ny, GLdouble nz);
typedef void(APIENTRYP PFNDOLNORMAL3FPROC)(GLfloat nx, GLfloat ny, GLfloat nz);
typedef void(APIENTRYP PFNDOLNORMAL3IPROC)(GLint nx, GLint ny, GLint nz);
typedef void(APIENTRYP PFNDOLNORMAL3SPROC)(GLshort nx, GLshort ny, GLshort nz);
typedef void(APIENTRYP PFNDOLNORMAL3BVPROC)(const GLbyte* v);
typedef void(APIENTRYP PFNDOLNORMAL3DVPROC)(const GLdouble* v);
typedef void(APIENTRYP PFNDOLNORMAL3FVPROC)(const GLfloat* v);
typedef void(APIENTRYP PFNDOLNORMAL3IVPROC)(const GLint* v);
typedef void(APIENTRYP PFNDOLNORMAL3SVPROC)(const GLshort* v);
typedef void(APIENTRYP PFNDOLINDEXDPROC)(GLdouble c);
typedef void(APIENTRYP PFNDOLINDEXFPROC)(GLfloat c);
typedef void(APIENTRYP PFNDOLINDEXIPROC)(GLint c);
typedef void(APIENTRYP PFNDOLINDEXSPROC)(GLshort c);
typedef void(APIENTRYP PFNDOLINDEXUBPROC)(GLubyte c); /* 1.1 */
typedef void(APIENTRYP PFNDOLINDEXDVPROC)(const GLdouble* c);
typedef void(APIENTRYP PFNDOLINDEXFVPROC)(const GLfloat* c);
typedef void(APIENTRYP PFNDOLINDEXIVPROC)(const GLint* c);
typedef void(APIENTRYP PFNDOLINDEXSVPROC)(const GLshort* c);
typedef void(APIENTRYP PFNDOLINDEXUBVPROC)(const GLubyte* c); /* 1.1 */
typedef void(APIENTRYP PFNDOLCOLOR3BPROC)(GLbyte red, GLbyte green, GLbyte blue);
typedef void(APIENTRYP PFNDOLCOLOR3DPROC)(GLdouble red, GLdouble green, GLdouble blue);
typedef void(APIENTRYP PFNDOLCOLOR3FPROC)(GLfloat red, GLfloat green, GLfloat blue);
typedef void(APIENTRYP PFNDOLCOLOR3IPROC)(GLint red, GLint green, GLint blue);
typedef void(APIENTRYP PFNDOLCOLOR3SPROC)(GLshort red, GLshort green, GLshort blue);
typedef void(APIENTRYP PFNDOLCOLOR3UBPROC)(GLubyte red, GLubyte green, GLubyte blue);
typedef void(APIENTRYP PFNDOLCOLOR3UIPROC)(GLuint red, GLuint green, GLuint blue);
typedef void(APIENTRYP PFNDOLCOLOR3USPROC)(GLushort red, GLushort green, GLushort blue);
typedef void(APIENTRYP PFNDOLCOLOR4BPROC)(GLbyte red, GLbyte green, GLbyte blue, GLbyte alpha);
typedef void(APIENTRYP PFNDOLCOLOR4DPROC)(GLdouble red, GLdouble green, GLdouble blue,
                                          GLdouble alpha);
typedef void(APIENTRYP PFNDOLCOLOR4FPROC)(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
typedef void(APIENTRYP PFNDOLCOLOR4IPROC)(GLint red, GLint green, GLint blue, GLint alpha);
typedef void(APIENTRYP PFNDOLCOLOR4SPROC)(GLshort red, GLshort green, GLshort blue, GLshort alpha);
typedef void(APIENTRYP PFNDOLCOLOR4UBPROC)(GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha);
typedef void(APIENTRYP PFNDOLCOLOR4UIPROC)(GLuint red, GLuint green, GLuint blue, GLuint alpha);
typedef void(APIENTRYP PFNDOLCOLOR4USPROC)(GLushort red, GLushort green, GLushort blue,
                                           GLushort alpha);
typedef void(APIENTRYP PFNDOLCOLOR3BVPROC)(const GLbyte* v);
typedef void(APIENTRYP PFNDOLCOLOR3DVPROC)(const GLdouble* v);
typedef void(APIENTRYP PFNDOLCOLOR3FVPROC)(const GLfloat* v);
typedef void(APIENTRYP PFNDOLCOLOR3IVPROC)(const GLint* v);
typedef void(APIENTRYP PFNDOLCOLOR3SVPROC)(const GLshort* v);
typedef void(APIENTRYP PFNDOLCOLOR3UBVPROC)(const GLubyte* v);
typedef void(APIENTRYP PFNDOLCOLOR3UIVPROC)(const GLuint* v);
typedef void(APIENTRYP PFNDOLCOLOR3USVPROC)(const GLushort* v);
typedef void(APIENTRYP PFNDOLCOLOR4BVPROC)(const GLbyte* v);
typedef void(APIENTRYP PFNDOLCOLOR4DVPROC)(const GLdouble* v);
typedef void(APIENTRYP PFNDOLCOLOR4FVPROC)(const GLfloat* v);
typedef void(APIENTRYP PFNDOLCOLOR4IVPROC)(const GLint* v);
typedef void(APIENTRYP PFNDOLCOLOR4SVPROC)(const GLshort* v);
typedef void(APIENTRYP PFNDOLCOLOR4UBVPROC)(const GLubyte* v);
typedef void(APIENTRYP PFNDOLCOLOR4UIVPROC)(const GLuint* v);
typedef void(APIENTRYP PFNDOLCOLOR4USVPROC)(const GLushort* v);
typedef void(APIENTRYP PFNDOLTEXCOORD1DPROC)(GLdouble s);
typedef void(APIENTRYP PFNDOLTEXCOORD1FPROC)(GLfloat s);
typedef void(APIENTRYP PFNDOLTEXCOORD1IPROC)(GLint s);
typedef void(APIENTRYP PFNDOLTEXCOORD1SPROC)(GLshort s);
typedef void(APIENTRYP PFNDOLTEXCOORD2DPROC)(GLdouble s, GLdouble t);
typedef void(APIENTRYP PFNDOLTEXCOORD2FPROC)(GLfloat s, GLfloat t);
typedef void(APIENTRYP PFNDOLTEXCOORD2IPROC)(GLint s, GLint t);
typedef void(APIENTRYP PFNDOLTEXCOORD2SPROC)(GLshort s, GLshort t);
typedef void(APIENTRYP PFNDOLTEXCOORD3DPROC)(GLdouble s, GLdouble t, GLdouble r);
typedef void(APIENTRYP PFNDOLTEXCOORD3FPROC)(GLfloat s, GLfloat t, GLfloat r);
typedef void(APIENTRYP PFNDOLTEXCOORD3IPROC)(GLint s, GLint t, GLint r);
typedef void(APIENTRYP PFNDOLTEXCOORD3SPROC)(GLshort s, GLshort t, GLshort r);
typedef void(APIENTRYP PFNDOLTEXCOORD4DPROC)(GLdouble s, GLdouble t, GLdouble r, GLdouble q);
typedef void(APIENTRYP PFNDOLTEXCOORD4FPROC)(GLfloat s, GLfloat t, GLfloat r, GLfloat q);
typedef void(APIENTRYP PFNDOLTEXCOORD4IPROC)(GLint s, GLint t, GLint r, GLint q);
typedef void(APIENTRYP PFNDOLTEXCOORD4SPROC)(GLshort s, GLshort t, GLshort r, GLshort q);
typedef void(APIENTRYP PFNDOLTEXCOORD1DVPROC)(const GLdouble* v);
typedef void(APIENTRYP PFNDOLTEXCOORD1FVPROC)(const GLfloat* v);
typedef void(APIENTRYP PFNDOLTEXCOORD1IVPROC)(const GLint* v);
typedef void(APIENTRYP PFNDOLTEXCOORD1SVPROC)(const GLshort* v);
typedef void(APIENTRYP PFNDOLTEXCOORD2DVPROC)(const GLdouble* v);
typedef void(APIENTRYP PFNDOLTEXCOORD2FVPROC)(const GLfloat* v);
typedef void(APIENTRYP PFNDOLTEXCOORD2IVPROC)(const GLint* v);
typedef void(APIENTRYP PFNDOLTEXCOORD2SVPROC)(const GLshort* v);
typedef void(APIENTRYP PFNDOLTEXCOORD3DVPROC)(const GLdouble* v);
typedef void(APIENTRYP PFNDOLTEXCOORD3FVPROC)(const GLfloat* v);
typedef void(APIENTRYP PFNDOLTEXCOORD3IVPROC)(const GLint* v);
typedef void(APIENTRYP PFNDOLTEXCOORD3SVPROC)(const GLshort* v);
typedef void(APIENTRYP PFNDOLTEXCOORD4DVPROC)(const GLdouble* v);
typedef void(APIENTRYP PFNDOLTEXCOORD4FVPROC)(const GLfloat* v);
typedef void(APIENTRYP PFNDOLTEXCOORD4IVPROC)(const GLint* v);
typedef void(APIENTRYP PFNDOLTEXCOORD4SVPROC)(const GLshort* v);
typedef void(APIENTRYP PFNDOLRASTERPOS2DPROC)(GLdouble x, GLdouble y);
typedef void(APIENTRYP PFNDOLRASTERPOS2FPROC)(GLfloat x, GLfloat y);
typedef void(APIENTRYP PFNDOLRASTERPOS2IPROC)(GLint x, GLint y);
typedef void(APIENTRYP PFNDOLRASTERPOS2SPROC)(GLshort x, GLshort y);
typedef void(APIENTRYP PFNDOLRASTERPOS3DPROC)(GLdouble x, GLdouble y, GLdouble z);
typedef void(APIENTRYP PFNDOLRASTERPOS3FPROC)(GLfloat x, GLfloat y, GLfloat z);
typedef void(APIENTRYP PFNDOLRASTERPOS3IPROC)(GLint x, GLint y, GLint z);
typedef void(APIENTRYP PFNDOLRASTERPOS3SPROC)(GLshort x, GLshort y, GLshort z);
typedef void(APIENTRYP PFNDOLRASTERPOS4DPROC)(GLdouble x, GLdouble y, GLdouble z, GLdouble w);
typedef void(APIENTRYP PFNDOLRASTERPOS4FPROC)(GLfloat x, GLfloat y, GLfloat z, GLfloat w);
typedef void(APIENTRYP PFNDOLRASTERPOS4IPROC)(GLint x, GLint y, GLint z, GLint w);
typedef void(APIENTRYP PFNDOLRASTERPOS4SPROC)(GLshort x, GLshort y, GLshort z, GLshort w);
typedef void(APIENTRYP PFNDOLRASTERPOS2DVPROC)(const GLdouble* v);
typedef void(APIENTRYP PFNDOLRASTERPOS2FVPROC)(const GLfloat* v);
typedef void(APIENTRYP PFNDOLRASTERPOS2IVPROC)(const GLint* v);
typedef void(APIENTRYP PFNDOLRASTERPOS2SVPROC)(const GLshort* v);
typedef void(APIENTRYP PFNDOLRASTERPOS3DVPROC)(const GLdouble* v);
typedef void(APIENTRYP PFNDOLRASTERPOS3FVPROC)(const GLfloat* v);
typedef void(APIENTRYP PFNDOLRASTERPOS3IVPROC)(const GLint* v);
typedef void(APIENTRYP PFNDOLRASTERPOS3SVPROC)(const GLshort* v);
typedef void(APIENTRYP PFNDOLRASTERPOS4DVPROC)(const GLdouble* v);
typedef void(APIENTRYP PFNDOLRASTERPOS4FVPROC)(const GLfloat* v);
typedef void(APIENTRYP PFNDOLRASTERPOS4IVPROC)(const GLint* v);
typedef void(APIENTRYP PFNDOLRASTERPOS4SVPROC)(const GLshort* v);
typedef void(APIENTRYP PFNDOLRECTDPROC)(GLdouble x1, GLdouble y1, GLdouble x2, GLdouble y2);
typedef void(APIENTRYP PFNDOLRECTFPROC)(GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2);
typedef void(APIENTRYP PFNDOLRECTIPROC)(GLint x1, GLint y1, GLint x2, GLint y2);
typedef void(APIENTRYP PFNDOLRECTSPROC)(GLshort x1, GLshort y1, GLshort x2, GLshort y2);
typedef void(APIENTRYP PFNDOLRECTDVPROC)(const GLdouble* v1, const GLdouble* v2);
typedef void(APIENTRYP PFNDOLRECTFVPROC)(const GLfloat* v1, const GLfloat* v2);
typedef void(APIENTRYP PFNDOLRECTIVPROC)(const GLint* v1, const GLint* v2);
typedef void(APIENTRYP PFNDOLRECTSVPROC)(const GLshort* v1, const GLshort* v2);
typedef void(APIENTRYP PFNDOLVERTEXPOINTERPROC)(GLint size, GLenum type, GLsizei stride,
                                                const GLvoid* ptr);
typedef void(APIENTRYP PFNDOLNORMALPOINTERPROC)(GLenum type, GLsizei stride, const GLvoid* ptr);
typedef void(APIENTRYP PFNDOLCOLORPOINTERPROC)(GLint size, GLenum type, GLsizei stride,
                                               const GLvoid* ptr);
typedef void(APIENTRYP PFNDOLINDEXPOINTERPROC)(GLenum type, GLsizei stride, const GLvoid* ptr);
typedef void(APIENTRYP PFNDOLTEXCOORDPOINTERPROC)(GLint size, GLenum type, GLsizei stride,
                                                  const GLvoid* ptr);
typedef void(APIENTRYP PFNDOLEDGEFLAGPOINTERPROC)(GLsizei stride, const GLvoid* ptr);
typedef void(APIENTRYP PFNDOLGETPOINTERVPROC)(GLenum pname, GLvoid** params);
typedef void(APIENTRYP PFNDOLARRAYELEMENTPROC)(GLint i);
typedef void(APIENTRYP PFNDOLDRAWARRAYSPROC)(GLenum mode, GLint first, GLsizei count);
typedef void(APIENTRYP PFNDOLDRAWELEMENTSPROC)(GLenum mode, GLsizei count, GLenum type,
                                               const GLvoid* indices);
typedef void(APIENTRYP PFNDOLINTERLEAVEDARRAYSPROC)(GLenum format, GLsizei stride,
                                                    const GLvoid* pointer);
typedef void(APIENTRYP PFNDOLSHADEMODELPROC)(GLenum mode);
typedef void(APIENTRYP PFNDOLLIGHTFPROC)(GLenum light, GLenum pname, GLfloat param);
typedef void(APIENTRYP PFNDOLLIGHTIPROC)(GLenum light, GLenum pname, GLint param);
typedef void(APIENTRYP PFNDOLLIGHTFVPROC)(GLenum light, GLenum pname, const GLfloat* params);
typedef void(APIENTRYP PFNDOLLIGHTIVPROC)(GLenum light, GLenum pname, const GLint* params);
typedef void(APIENTRYP PFNDOLGETLIGHTFVPROC)(GLenum light, GLenum pname, GLfloat* params);
typedef void(APIENTRYP PFNDOLGETLIGHTIVPROC)(GLenum light, GLenum pname, GLint* params);
typedef void(APIENTRYP PFNDOLLIGHTMODELFPROC)(GLenum pname, GLfloat param);
typedef void(APIENTRYP PFNDOLLIGHTMODELIPROC)(GLenum pname, GLint param);
typedef void(APIENTRYP PFNDOLLIGHTMODELFVPROC)(GLenum pname, const GLfloat* params);
typedef void(APIENTRYP PFNDOLLIGHTMODELIVPROC)(GLenum pname, const GLint* params);
typedef void(APIENTRYP PFNDOLMATERIALFPROC)(GLenum face, GLenum pname, GLfloat param);
typedef void(APIENTRYP PFNDOLMATERIALIPROC)(GLenum face, GLenum pname, GLint param);
typedef void(APIENTRYP PFNDOLMATERIALFVPROC)(GLenum face, GLenum pname, const GLfloat* params);
typedef void(APIENTRYP PFNDOLMATERIALIVPROC)(GLenum face, GLenum pname, const GLint* params);
typedef void(APIENTRYP PFNDOLGETMATERIALFVPROC)(GLenum face, GLenum pname, GLfloat* params);
typedef void(APIENTRYP PFNDOLGETMATERIALIVPROC)(GLenum face, GLenum pname, GLint* params);
typedef void(APIENTRYP PFNDOLCOLORMATERIALPROC)(GLenum face, GLenum mode);
typedef void(APIENTRYP PFNDOLPIXELZOOMPROC)(GLfloat xfactor, GLfloat yfactor);
typedef void(APIENTRYP PFNDOLPIXELSTOREFPROC)(GLenum pname, GLfloat param);
typedef void(APIENTRYP PFNDOLPIXELSTOREIPROC)(GLenum pname, GLint param);
typedef void(APIENTRYP PFNDOLPIXELTRANSFERFPROC)(GLenum pname, GLfloat param);
typedef void(APIENTRYP PFNDOLPIXELTRANSFERIPROC)(GLenum pname, GLint param);
typedef void(APIENTRYP PFNDOLPIXELMAPFVPROC)(GLenum map, GLsizei mapsize, const GLfloat* values);
typedef void(APIENTRYP PFNDOLPIXELMAPUIVPROC)(GLenum map, GLsizei mapsize, const GLuint* values);
typedef void(APIENTRYP PFNDOLPIXELMAPUSVPROC)(GLenum map, GLsizei mapsize, const GLushort* values);
typedef void(APIENTRYP PFNDOLGETPIXELMAPFVPROC)(GLenum map, GLfloat* values);
typedef void(APIENTRYP PFNDOLGETPIXELMAPUIVPROC)(GLenum map, GLuint* values);
typedef void(APIENTRYP PFNDOLGETPIXELMAPUSVPROC)(GLenum map, GLushort* values);
typedef void(APIENTRYP PFNDOLBITMAPPROC)(GLsizei width, GLsizei height, GLfloat xorig,
                                         GLfloat yorig, GLfloat xmove, GLfloat ymove,
                                         const GLubyte* bitmap);
typedef void(APIENTRYP PFNDOLREADPIXELSPROC)(GLint x, GLint y, GLsizei width, GLsizei height,
                                             GLenum format, GLenum type, GLvoid* pixels);
typedef void(APIENTRYP PFNDOLDRAWPIXELSPROC)(GLsizei width, GLsizei height, GLenum format,
                                             GLenum type, const GLvoid* pixels);
typedef void(APIENTRYP PFNDOLCOPYPIXELSPROC)(GLint x, GLint y, GLsizei width, GLsizei height,
                                             GLenum type);
typedef void(APIENTRYP PFNDOLSTENCILFUNCPROC)(GLenum func, GLint ref, GLuint mask);
typedef void(APIENTRYP PFNDOLSTENCILMASKPROC)(GLuint mask);
typedef void(APIENTRYP PFNDOLSTENCILOPPROC)(GLenum fail, GLenum zfail, GLenum zpass);
typedef void(APIENTRYP PFNDOLCLEARSTENCILPROC)(GLint s);
typedef void(APIENTRYP PFNDOLTEXGENDPROC)(GLenum coord, GLenum pname, GLdouble param);
typedef void(APIENTRYP PFNDOLTEXGENFPROC)(GLenum coord, GLenum pname, GLfloat param);
typedef void(APIENTRYP PFNDOLTEXGENIPROC)(GLenum coord, GLenum pname, GLint param);
typedef void(APIENTRYP PFNDOLTEXGENDVPROC)(GLenum coord, GLenum pname, const GLdouble* params);
typedef void(APIENTRYP PFNDOLTEXGENFVPROC)(GLenum coord, GLenum pname, const GLfloat* params);
typedef void(APIENTRYP PFNDOLTEXGENIVPROC)(GLenum coord, GLenum pname, const GLint* params);
typedef void(APIENTRYP PFNDOLGETTEXGENDVPROC)(GLenum coord, GLenum pname, GLdouble* params);
typedef void(APIENTRYP PFNDOLGETTEXGENFVPROC)(GLenum coord, GLenum pname, GLfloat* params);
typedef void(APIENTRYP PFNDOLGETTEXGENIVPROC)(GLenum coord, GLenum pname, GLint* params);
typedef void(APIENTRYP PFNDOLTEXENVFPROC)(GLenum target, GLenum pname, GLfloat param);
typedef void(APIENTRYP PFNDOLTEXENVIPROC)(GLenum target, GLenum pname, GLint param);
typedef void(APIENTRYP PFNDOLTEXENVFVPROC)(GLenum target, GLenum pname, const GLfloat* params);
typedef void(APIENTRYP PFNDOLTEXENVIVPROC)(GLenum target, GLenum pname, const GLint* params);
typedef void(APIENTRYP PFNDOLGETTEXENVFVPROC)(GLenum target, GLenum pname, GLfloat* params);
typedef void(APIENTRYP PFNDOLGETTEXENVIVPROC)(GLenum target, GLenum pname, GLint* params);
typedef void(APIENTRYP PFNDOLTEXPARAMETERFPROC)(GLenum target, GLenum pname, GLfloat param);
typedef void(APIENTRYP PFNDOLTEXPARAMETERIPROC)(GLenum target, GLenum pname, GLint param);
typedef void(APIENTRYP PFNDOLTEXPARAMETERFVPROC)(GLenum target, GLenum pname,
                                                 const GLfloat* params);
typedef void(APIENTRYP PFNDOLTEXPARAMETERIVPROC)(GLenum target, GLenum pname, const GLint* params);
typedef void(APIENTRYP PFNDOLGETTEXPARAMETERFVPROC)(GLenum target, GLenum pname, GLfloat* params);
typedef void(APIENTRYP PFNDOLGETTEXPARAMETERIVPROC)(GLenum target, GLenum pname, GLint* params);
typedef void(APIENTRYP PFNDOLGETTEXLEVELPARAMETERFVPROC)(GLenum target, GLint level, GLenum pname,
                                                         GLfloat* params);
typedef void(APIENTRYP PFNDOLGETTEXLEVELPARAMETERIVPROC)(GLenum target, GLint level, GLenum pname,
                                                         GLint* params);
typedef void(APIENTRYP PFNDOLTEXIMAGE1DPROC)(GLenum target, GLint level, GLint internalFormat,
                                             GLsizei width, GLint border, GLenum format,
                                             GLenum type, const GLvoid* pixels);
typedef void(APIENTRYP PFNDOLTEXIMAGE2DPROC)(GLenum target, GLint level, GLint internalFormat,
                                             GLsizei width, GLsizei height, GLint border,
                                             GLenum format, GLenum type, const GLvoid* pixels);
typedef void(APIENTRYP PFNDOLGETTEXIMAGEPROC)(GLenum target, GLint level, GLenum format,
                                              GLenum type, GLvoid* pixels);
typedef void(APIENTRYP PFNDOLGENTEXTURESPROC)(GLsizei n, GLuint* textures);
typedef void(APIENTRYP PFNDOLDELETETEXTURESPROC)(GLsizei n, const GLuint* textures);
typedef void(APIENTRYP PFNDOLBINDTEXTUREPROC)(GLenum target, GLuint texture);
typedef void(APIENTRYP PFNDOLPRIORITIZETEXTURESPROC)(GLsizei n, const GLuint* textures,
                                                     const GLclampf* priorities);
typedef GLboolean(APIENTRYP PFNDOLARETEXTURESRESIDENTPROC)(GLsizei n, const GLuint* textures,
                                                           GLboolean* residences);
typedef GLboolean(APIENTRYP PFNDOLISTEXTUREPROC)(GLuint texture);
typedef void(APIENTRYP PFNDOLTEXSUBIMAGE1DPROC)(GLenum target, GLint level, GLint xoffset,
                                                GLsizei width, GLenum format, GLenum type,
                                                const GLvoid* pixels);
typedef void(APIENTRYP PFNDOLTEXSUBIMAGE2DPROC)(GLenum target, GLint level, GLint xoffset,
                                                GLint yoffset, GLsizei width, GLsizei height,
                                                GLenum format, GLenum type, const GLvoid* pixels);
typedef void(APIENTRYP PFNDOLCOPYTEXIMAGE1DPROC)(GLenum target, GLint level, GLenum internalformat,
                                                 GLint x, GLint y, GLsizei width, GLint border);
typedef void(APIENTRYP PFNDOLCOPYTEXIMAGE2DPROC)(GLenum target, GLint level, GLenum internalformat,
                                                 GLint x, GLint y, GLsizei width, GLsizei height,
                                                 GLint border);
typedef void(APIENTRYP PFNDOLCOPYTEXSUBIMAGE1DPROC)(GLenum target, GLint level, GLint xoffset,
                                                    GLint x, GLint y, GLsizei width);
typedef void(APIENTRYP PFNDOLCOPYTEXSUBIMAGE2DPROC)(GLenum target, GLint level, GLint xoffset,
                                                    GLint yoffset, GLint x, GLint y, GLsizei width,
                                                    GLsizei height);
typedef void(APIENTRYP PFNDOLMAP1DPROC)(GLenum target, GLdouble u1, GLdouble u2, GLint stride,
                                        GLint order, const GLdouble* points);
typedef void(APIENTRYP PFNDOLMAP1FPROC)(GLenum target, GLfloat u1, GLfloat u2, GLint stride,
                                        GLint order, const GLfloat* points);
typedef void(APIENTRYP PFNDOLMAP2DPROC)(GLenum target, GLdouble u1, GLdouble u2, GLint ustride,
                                        GLint uorder, GLdouble v1, GLdouble v2, GLint vstride,
                                        GLint vorder, const GLdouble* points);
typedef void(APIENTRYP PFNDOLMAP2FPROC)(GLenum target, GLfloat u1, GLfloat u2, GLint ustride,
                                        GLint uorder, GLfloat v1, GLfloat v2, GLint vstride,
                                        GLint vorder, const GLfloat* points);
typedef void(APIENTRYP PFNDOLGETMAPDVPROC)(GLenum target, GLenum query, GLdouble* v);
typedef void(APIENTRYP PFNDOLGETMAPFVPROC)(GLenum target, GLenum query, GLfloat* v);
typedef void(APIENTRYP PFNDOLGETMAPIVPROC)(GLenum target, GLenum query, GLint* v);
typedef void(APIENTRYP PFNDOLEVALCOORD1DPROC)(GLdouble u);
typedef void(APIENTRYP PFNDOLEVALCOORD1FPROC)(GLfloat u);
typedef void(APIENTRYP PFNDOLEVALCOORD1DVPROC)(const GLdouble* u);
typedef void(APIENTRYP PFNDOLEVALCOORD1FVPROC)(const GLfloat* u);
typedef void(APIENTRYP PFNDOLEVALCOORD2DPROC)(GLdouble u, GLdouble v);
typedef void(APIENTRYP PFNDOLEVALCOORD2FPROC)(GLfloat u, GLfloat v);
typedef void(APIENTRYP PFNDOLEVALCOORD2DVPROC)(const GLdouble* u);
typedef void(APIENTRYP PFNDOLEVALCOORD2FVPROC)(const GLfloat* u);
typedef void(APIENTRYP PFNDOLMAPGRID1DPROC)(GLint un, GLdouble u1, GLdouble u2);
typedef void(APIENTRYP PFNDOLMAPGRID1FPROC)(GLint un, GLfloat u1, GLfloat u2);
typedef void(APIENTRYP PFNDOLMAPGRID2DPROC)(GLint un, GLdouble u1, GLdouble u2, GLint vn,
                                            GLdouble v1, GLdouble v2);
typedef void(APIENTRYP PFNDOLMAPGRID2FPROC)(GLint un, GLfloat u1, GLfloat u2, GLint vn, GLfloat v1,
                                            GLfloat v2);
typedef void(APIENTRYP PFNDOLEVALPOINT1PROC)(GLint i);
typedef void(APIENTRYP PFNDOLEVALPOINT2PROC)(GLint i, GLint j);
typedef void(APIENTRYP PFNDOLEVALMESH1PROC)(GLenum mode, GLint i1, GLint i2);
typedef void(APIENTRYP PFNDOLEVALMESH2PROC)(GLenum mode, GLint i1, GLint i2, GLint j1, GLint j2);
typedef void(APIENTRYP PFNDOLFOGFPROC)(GLenum pname, GLfloat param);
typedef void(APIENTRYP PFNDOLFOGIPROC)(GLenum pname, GLint param);
typedef void(APIENTRYP PFNDOLFOGFVPROC)(GLenum pname, const GLfloat* params);
typedef void(APIENTRYP PFNDOLFOGIVPROC)(GLenum pname, const GLint* params);
typedef void(APIENTRYP PFNDOLFEEDBACKBUFFERPROC)(GLsizei size, GLenum type, GLfloat* buffer);
typedef void(APIENTRYP PFNDOLPASSTHROUGHPROC)(GLfloat token);
typedef void(APIENTRYP PFNDOLSELECTBUFFERPROC)(GLsizei size, GLuint* buffer);
typedef void(APIENTRYP PFNDOLINITNAMESPROC)(void);
typedef void(APIENTRYP PFNDOLLOADNAMEPROC)(GLuint name);
typedef void(APIENTRYP PFNDOLPUSHNAMEPROC)(GLuint name);
typedef void(APIENTRYP PFNDOLPOPNAMEPROC)(void);

extern PFNDOLCLEARINDEXPROC dolClearIndex;
extern PFNDOLCLEARCOLORPROC dolClearColor;
extern PFNDOLCLEARPROC dolClear;
extern PFNDOLINDEXMASKPROC dolIndexMask;
extern PFNDOLCOLORMASKPROC dolColorMask;
extern PFNDOLALPHAFUNCPROC dolAlphaFunc;
extern PFNDOLBLENDFUNCPROC dolBlendFunc;
extern PFNDOLLOGICOPPROC dolLogicOp;
extern PFNDOLCULLFACEPROC dolCullFace;
extern PFNDOLFRONTFACEPROC dolFrontFace;
extern PFNDOLPOINTSIZEPROC dolPointSize;
extern PFNDOLLINEWIDTHPROC dolLineWidth;
extern PFNDOLLINESTIPPLEPROC dolLineStipple;
extern PFNDOLPOLYGONMODEPROC dolPolygonMode;
extern PFNDOLPOLYGONOFFSETPROC dolPolygonOffset;
extern PFNDOLPOLYGONSTIPPLEPROC dolPolygonStipple;
extern PFNDOLGETPOLYGONSTIPPLEPROC dolGetPolygonStipple;
extern PFNDOLEDGEFLAGPROC dolEdgeFlag;
extern PFNDOLEDGEFLAGVPROC dolEdgeFlagv;
extern PFNDOLSCISSORPROC dolScissor;
extern PFNDOLCLIPPLANEPROC dolClipPlane;
extern PFNDOLGETCLIPPLANEPROC dolGetClipPlane;
extern PFNDOLDRAWBUFFERPROC dolDrawBuffer;
extern PFNDOLREADBUFFERPROC dolReadBuffer;
extern PFNDOLENABLEPROC dolEnable;
extern PFNDOLDISABLEPROC dolDisable;
extern PFNDOLISENABLEDPROC dolIsEnabled;
extern PFNDOLENABLECLIENTSTATEPROC dolEnableClientState;
extern PFNDOLDISABLECLIENTSTATEPROC dolDisableClientState;
extern PFNDOLGETBOOLEANVPROC dolGetBooleanv;
extern PFNDOLGETDOUBLEVPROC dolGetDoublev;
extern PFNDOLGETFLOATVPROC dolGetFloatv;
extern PFNDOLGETINTEGERVPROC dolGetIntegerv;
extern PFNDOLPUSHATTRIBPROC dolPushAttrib;
extern PFNDOLPOPATTRIBPROC dolPopAttrib;
extern PFNDOLPUSHCLIENTATTRIBPROC dolPushClientAttrib;
extern PFNDOLPOPCLIENTATTRIBPROC dolPopClientAttrib;
extern PFNDOLRENDERMODEPROC dolRenderMode;
extern PFNDOLGETERRORPROC dolGetError;
extern PFNDOLGETSTRINGPROC dolGetString;
extern PFNDOLFINISHPROC dolFinish;
extern PFNDOLFLUSHPROC dolFlush;
extern PFNDOLHINTPROC dolHint;
extern PFNDOLCLEARDEPTHPROC dolClearDepth;
extern PFNDOLDEPTHFUNCPROC dolDepthFunc;
extern PFNDOLDEPTHMASKPROC dolDepthMask;
extern PFNDOLDEPTHRANGEPROC dolDepthRange;
extern PFNDOLCLEARACCUMPROC dolClearAccum;
extern PFNDOLACCUMPROC dolAccum;
extern PFNDOLMATRIXMODEPROC dolMatrixMode;
extern PFNDOLORTHOPROC dolOrtho;
extern PFNDOLFRUSTUMPROC dolFrustum;
extern PFNDOLVIEWPORTPROC dolViewport;
extern PFNDOLPUSHMATRIXPROC dolPushMatrix;
extern PFNDOLPOPMATRIXPROC dolPopMatrix;
extern PFNDOLLOADIDENTITYPROC dolLoadIdentity;
extern PFNDOLLOADMATRIXDPROC dolLoadMatrixd;
extern PFNDOLLOADMATRIXFPROC dolLoadMatrixf;
extern PFNDOLMULTMATRIXDPROC dolMultMatrixd;
extern PFNDOLMULTMATRIXFPROC dolMultMatrixf;
extern PFNDOLROTATEDPROC dolRotated;
extern PFNDOLROTATEFPROC dolRotatef;
extern PFNDOLSCALEDPROC dolScaled;
extern PFNDOLSCALEFPROC dolScalef;
extern PFNDOLTRANSLATEDPROC dolTranslated;
extern PFNDOLTRANSLATEFPROC dolTranslatef;
extern PFNDOLISLISTPROC dolIsList;
extern PFNDOLDELETELISTSPROC dolDeleteLists;
extern PFNDOLGENLISTSPROC dolGenLists;
extern PFNDOLNEWLISTPROC dolNewList;
extern PFNDOLENDLISTPROC dolEndList;
extern PFNDOLCALLLISTPROC dolCallList;
extern PFNDOLCALLLISTSPROC dolCallLists;
extern PFNDOLLISTBASEPROC dolListBase;
extern PFNDOLBEGINPROC dolBegin;
extern PFNDOLENDPROC dolEnd;
extern PFNDOLVERTEX2DPROC dolVertex2d;
extern PFNDOLVERTEX2FPROC dolVertex2f;
extern PFNDOLVERTEX2IPROC dolVertex2i;
extern PFNDOLVERTEX2SPROC dolVertex2s;
extern PFNDOLVERTEX3DPROC dolVertex3d;
extern PFNDOLVERTEX3FPROC dolVertex3f;
extern PFNDOLVERTEX3IPROC dolVertex3i;
extern PFNDOLVERTEX3SPROC dolVertex3s;
extern PFNDOLVERTEX4DPROC dolVertex4d;
extern PFNDOLVERTEX4FPROC dolVertex4f;
extern PFNDOLVERTEX4IPROC dolVertex4i;
extern PFNDOLVERTEX4SPROC dolVertex4s;
extern PFNDOLVERTEX2DVPROC dolVertex2dv;
extern PFNDOLVERTEX2FVPROC dolVertex2fv;
extern PFNDOLVERTEX2IVPROC dolVertex2iv;
extern PFNDOLVERTEX2SVPROC dolVertex2sv;
extern PFNDOLVERTEX3DVPROC dolVertex3dv;
extern PFNDOLVERTEX3FVPROC dolVertex3fv;
extern PFNDOLVERTEX3IVPROC dolVertex3iv;
extern PFNDOLVERTEX3SVPROC dolVertex3sv;
extern PFNDOLVERTEX4DVPROC dolVertex4dv;
extern PFNDOLVERTEX4FVPROC dolVertex4fv;
extern PFNDOLVERTEX4IVPROC dolVertex4iv;
extern PFNDOLVERTEX4SVPROC dolVertex4sv;
extern PFNDOLNORMAL3BPROC dolNormal3b;
extern PFNDOLNORMAL3DPROC dolNormal3d;
extern PFNDOLNORMAL3FPROC dolNormal3f;
extern PFNDOLNORMAL3IPROC dolNormal3i;
extern PFNDOLNORMAL3SPROC dolNormal3s;
extern PFNDOLNORMAL3BVPROC dolNormal3bv;
extern PFNDOLNORMAL3DVPROC dolNormal3dv;
extern PFNDOLNORMAL3FVPROC dolNormal3fv;
extern PFNDOLNORMAL3IVPROC dolNormal3iv;
extern PFNDOLNORMAL3SVPROC dolNormal3sv;
extern PFNDOLINDEXDPROC dolIndexd;
extern PFNDOLINDEXFPROC dolIndexf;
extern PFNDOLINDEXIPROC dolIndexi;
extern PFNDOLINDEXSPROC dolIndexs;
extern PFNDOLINDEXUBPROC dolIndexub;
extern PFNDOLINDEXDVPROC dolIndexdv;
extern PFNDOLINDEXFVPROC dolIndexfv;
extern PFNDOLINDEXIVPROC dolIndexiv;
extern PFNDOLINDEXSVPROC dolIndexsv;
extern PFNDOLINDEXUBVPROC dolIndexubv;
extern PFNDOLCOLOR3BPROC dolColor3b;
extern PFNDOLCOLOR3DPROC dolColor3d;
extern PFNDOLCOLOR3FPROC dolColor3f;
extern PFNDOLCOLOR3IPROC dolColor3i;
extern PFNDOLCOLOR3SPROC dolColor3s;
extern PFNDOLCOLOR3UBPROC dolColor3ub;
extern PFNDOLCOLOR3UIPROC dolColor3ui;
extern PFNDOLCOLOR3USPROC dolColor3us;
extern PFNDOLCOLOR4BPROC dolColor4b;
extern PFNDOLCOLOR4DPROC dolColor4d;
extern PFNDOLCOLOR4FPROC dolColor4f;
extern PFNDOLCOLOR4IPROC dolColor4i;
extern PFNDOLCOLOR4SPROC dolColor4s;
extern PFNDOLCOLOR4UBPROC dolColor4ub;
extern PFNDOLCOLOR4UIPROC dolColor4ui;
extern PFNDOLCOLOR4USPROC dolColor4us;
extern PFNDOLCOLOR3BVPROC dolColor3bv;
extern PFNDOLCOLOR3DVPROC dolColor3dv;
extern PFNDOLCOLOR3FVPROC dolColor3fv;
extern PFNDOLCOLOR3IVPROC dolColor3iv;
extern PFNDOLCOLOR3SVPROC dolColor3sv;
extern PFNDOLCOLOR3UBVPROC dolColor3ubv;
extern PFNDOLCOLOR3UIVPROC dolColor3uiv;
extern PFNDOLCOLOR3USVPROC dolColor3usv;
extern PFNDOLCOLOR4BVPROC dolColor4bv;
extern PFNDOLCOLOR4DVPROC dolColor4dv;
extern PFNDOLCOLOR4FVPROC dolColor4fv;
extern PFNDOLCOLOR4IVPROC dolColor4iv;
extern PFNDOLCOLOR4SVPROC dolColor4sv;
extern PFNDOLCOLOR4UBVPROC dolColor4ubv;
extern PFNDOLCOLOR4UIVPROC dolColor4uiv;
extern PFNDOLCOLOR4USVPROC dolColor4usv;
extern PFNDOLTEXCOORD1DPROC dolTexCoord1d;
extern PFNDOLTEXCOORD1FPROC dolTexCoord1f;
extern PFNDOLTEXCOORD1IPROC dolTexCoord1i;
extern PFNDOLTEXCOORD1SPROC dolTexCoord1s;
extern PFNDOLTEXCOORD2DPROC dolTexCoord2d;
extern PFNDOLTEXCOORD2FPROC dolTexCoord2f;
extern PFNDOLTEXCOORD2IPROC dolTexCoord2i;
extern PFNDOLTEXCOORD2SPROC dolTexCoord2s;
extern PFNDOLTEXCOORD3DPROC dolTexCoord3d;
extern PFNDOLTEXCOORD3FPROC dolTexCoord3f;
extern PFNDOLTEXCOORD3IPROC dolTexCoord3i;
extern PFNDOLTEXCOORD3SPROC dolTexCoord3s;
extern PFNDOLTEXCOORD4DPROC dolTexCoord4d;
extern PFNDOLTEXCOORD4FPROC dolTexCoord4f;
extern PFNDOLTEXCOORD4IPROC dolTexCoord4i;
extern PFNDOLTEXCOORD4SPROC dolTexCoord4s;
extern PFNDOLTEXCOORD1DVPROC dolTexCoord1dv;
extern PFNDOLTEXCOORD1FVPROC dolTexCoord1fv;
extern PFNDOLTEXCOORD1IVPROC dolTexCoord1iv;
extern PFNDOLTEXCOORD1SVPROC dolTexCoord1sv;
extern PFNDOLTEXCOORD2DVPROC dolTexCoord2dv;
extern PFNDOLTEXCOORD2FVPROC dolTexCoord2fv;
extern PFNDOLTEXCOORD2IVPROC dolTexCoord2iv;
extern PFNDOLTEXCOORD2SVPROC dolTexCoord2sv;
extern PFNDOLTEXCOORD3DVPROC dolTexCoord3dv;
extern PFNDOLTEXCOORD3FVPROC dolTexCoord3fv;
extern PFNDOLTEXCOORD3IVPROC dolTexCoord3iv;
extern PFNDOLTEXCOORD3SVPROC dolTexCoord3sv;
extern PFNDOLTEXCOORD4DVPROC dolTexCoord4dv;
extern PFNDOLTEXCOORD4FVPROC dolTexCoord4fv;
extern PFNDOLTEXCOORD4IVPROC dolTexCoord4iv;
extern PFNDOLTEXCOORD4SVPROC dolTexCoord4sv;
extern PFNDOLRASTERPOS2DPROC dolRasterPos2d;
extern PFNDOLRASTERPOS2FPROC dolRasterPos2f;
extern PFNDOLRASTERPOS2IPROC dolRasterPos2i;
extern PFNDOLRASTERPOS2SPROC dolRasterPos2s;
extern PFNDOLRASTERPOS3DPROC dolRasterPos3d;
extern PFNDOLRASTERPOS3FPROC dolRasterPos3f;
extern PFNDOLRASTERPOS3IPROC dolRasterPos3i;
extern PFNDOLRASTERPOS3SPROC dolRasterPos3s;
extern PFNDOLRASTERPOS4DPROC dolRasterPos4d;
extern PFNDOLRASTERPOS4FPROC dolRasterPos4f;
extern PFNDOLRASTERPOS4IPROC dolRasterPos4i;
extern PFNDOLRASTERPOS4SPROC dolRasterPos4s;
extern PFNDOLRASTERPOS2DVPROC dolRasterPos2dv;
extern PFNDOLRASTERPOS2FVPROC dolRasterPos2fv;
extern PFNDOLRASTERPOS2IVPROC dolRasterPos2iv;
extern PFNDOLRASTERPOS2SVPROC dolRasterPos2sv;
extern PFNDOLRASTERPOS3DVPROC dolRasterPos3dv;
extern PFNDOLRASTERPOS3FVPROC dolRasterPos3fv;
extern PFNDOLRASTERPOS3IVPROC dolRasterPos3iv;
extern PFNDOLRASTERPOS3SVPROC dolRasterPos3sv;
extern PFNDOLRASTERPOS4DVPROC dolRasterPos4dv;
extern PFNDOLRASTERPOS4FVPROC dolRasterPos4fv;
extern PFNDOLRASTERPOS4IVPROC dolRasterPos4iv;
extern PFNDOLRASTERPOS4SVPROC dolRasterPos4sv;
extern PFNDOLRECTDPROC dolRectd;
extern PFNDOLRECTFPROC dolRectf;
extern PFNDOLRECTIPROC dolRecti;
extern PFNDOLRECTSPROC dolRects;
extern PFNDOLRECTDVPROC dolRectdv;
extern PFNDOLRECTFVPROC dolRectfv;
extern PFNDOLRECTIVPROC dolRectiv;
extern PFNDOLRECTSVPROC dolRectsv;
extern PFNDOLVERTEXPOINTERPROC dolVertexPointer;
extern PFNDOLNORMALPOINTERPROC dolNormalPointer;
extern PFNDOLCOLORPOINTERPROC dolColorPointer;
extern PFNDOLINDEXPOINTERPROC dolIndexPointer;
extern PFNDOLTEXCOORDPOINTERPROC dolTexCoordPointer;
extern PFNDOLEDGEFLAGPOINTERPROC dolEdgeFlagPointer;
extern PFNDOLGETPOINTERVPROC dolGetPointerv;
extern PFNDOLARRAYELEMENTPROC dolArrayElement;
extern PFNDOLDRAWARRAYSPROC dolDrawArrays;
extern PFNDOLDRAWELEMENTSPROC dolDrawElements;
extern PFNDOLINTERLEAVEDARRAYSPROC dolInterleavedArrays;
extern PFNDOLSHADEMODELPROC dolShadeModel;
extern PFNDOLLIGHTFPROC dolLightf;
extern PFNDOLLIGHTIPROC dolLighti;
extern PFNDOLLIGHTFVPROC dolLightfv;
extern PFNDOLLIGHTIVPROC dolLightiv;
extern PFNDOLGETLIGHTFVPROC dolGetLightfv;
extern PFNDOLGETLIGHTIVPROC dolGetLightiv;
extern PFNDOLLIGHTMODELFPROC dolLightModelf;
extern PFNDOLLIGHTMODELIPROC dolLightModeli;
extern PFNDOLLIGHTMODELFVPROC dolLightModelfv;
extern PFNDOLLIGHTMODELIVPROC dolLightModeliv;
extern PFNDOLMATERIALFPROC dolMaterialf;
extern PFNDOLMATERIALIPROC dolMateriali;
extern PFNDOLMATERIALFVPROC dolMaterialfv;
extern PFNDOLMATERIALIVPROC dolMaterialiv;
extern PFNDOLGETMATERIALFVPROC dolGetMaterialfv;
extern PFNDOLGETMATERIALIVPROC dolGetMaterialiv;
extern PFNDOLCOLORMATERIALPROC dolColorMaterial;
extern PFNDOLPIXELZOOMPROC dolPixelZoom;
extern PFNDOLPIXELSTOREFPROC dolPixelStoref;
extern PFNDOLPIXELSTOREIPROC dolPixelStorei;
extern PFNDOLPIXELTRANSFERFPROC dolPixelTransferf;
extern PFNDOLPIXELTRANSFERIPROC dolPixelTransferi;
extern PFNDOLPIXELMAPFVPROC dolPixelMapfv;
extern PFNDOLPIXELMAPUIVPROC dolPixelMapuiv;
extern PFNDOLPIXELMAPUSVPROC dolPixelMapusv;
extern PFNDOLGETPIXELMAPFVPROC dolGetPixelMapfv;
extern PFNDOLGETPIXELMAPUIVPROC dolGetPixelMapuiv;
extern PFNDOLGETPIXELMAPUSVPROC dolGetPixelMapusv;
extern PFNDOLBITMAPPROC dolBitmap;
extern PFNDOLREADPIXELSPROC dolReadPixels;
extern PFNDOLDRAWPIXELSPROC dolDrawPixels;
extern PFNDOLCOPYPIXELSPROC dolCopyPixels;
extern PFNDOLSTENCILFUNCPROC dolStencilFunc;
extern PFNDOLSTENCILMASKPROC dolStencilMask;
extern PFNDOLSTENCILOPPROC dolStencilOp;
extern PFNDOLCLEARSTENCILPROC dolClearStencil;
extern PFNDOLTEXGENDPROC dolTexGend;
extern PFNDOLTEXGENFPROC dolTexGenf;
extern PFNDOLTEXGENIPROC dolTexGeni;
extern PFNDOLTEXGENDVPROC dolTexGendv;
extern PFNDOLTEXGENFVPROC dolTexGenfv;
extern PFNDOLTEXGENIVPROC dolTexGeniv;
extern PFNDOLGETTEXGENDVPROC dolGetTexGendv;
extern PFNDOLGETTEXGENFVPROC dolGetTexGenfv;
extern PFNDOLGETTEXGENIVPROC dolGetTexGeniv;
extern PFNDOLTEXENVFPROC dolTexEnvf;
extern PFNDOLTEXENVIPROC dolTexEnvi;
extern PFNDOLTEXENVFVPROC dolTexEnvfv;
extern PFNDOLTEXENVIVPROC dolTexEnviv;
extern PFNDOLGETTEXENVFVPROC dolGetTexEnvfv;
extern PFNDOLGETTEXENVIVPROC dolGetTexEnviv;
extern PFNDOLTEXPARAMETERFPROC dolTexParameterf;
extern PFNDOLTEXPARAMETERIPROC dolTexParameteri;
extern PFNDOLTEXPARAMETERFVPROC dolTexParameterfv;
extern PFNDOLTEXPARAMETERIVPROC dolTexParameteriv;
extern PFNDOLGETTEXPARAMETERFVPROC dolGetTexParameterfv;
extern PFNDOLGETTEXPARAMETERIVPROC dolGetTexParameteriv;
extern PFNDOLGETTEXLEVELPARAMETERFVPROC dolGetTexLevelParameterfv;
extern PFNDOLGETTEXLEVELPARAMETERIVPROC dolGetTexLevelParameteriv;
extern PFNDOLTEXIMAGE1DPROC dolTexImage1D;
extern PFNDOLTEXIMAGE2DPROC dolTexImage2D;
extern PFNDOLGETTEXIMAGEPROC dolGetTexImage;
extern PFNDOLGENTEXTURESPROC dolGenTextures;
extern PFNDOLDELETETEXTURESPROC dolDeleteTextures;
extern PFNDOLBINDTEXTUREPROC dolBindTexture;
extern PFNDOLPRIORITIZETEXTURESPROC dolPrioritizeTextures;
extern PFNDOLARETEXTURESRESIDENTPROC dolAreTexturesResident;
extern PFNDOLISTEXTUREPROC dolIsTexture;
extern PFNDOLTEXSUBIMAGE1DPROC dolTexSubImage1D;
extern PFNDOLTEXSUBIMAGE2DPROC dolTexSubImage2D;
extern PFNDOLCOPYTEXIMAGE1DPROC dolCopyTexImage1D;
extern PFNDOLCOPYTEXIMAGE2DPROC dolCopyTexImage2D;
extern PFNDOLCOPYTEXSUBIMAGE1DPROC dolCopyTexSubImage1D;
extern PFNDOLCOPYTEXSUBIMAGE2DPROC dolCopyTexSubImage2D;
extern PFNDOLMAP1DPROC dolMap1d;
extern PFNDOLMAP1FPROC dolMap1f;
extern PFNDOLMAP2DPROC dolMap2d;
extern PFNDOLMAP2FPROC dolMap2f;
extern PFNDOLGETMAPDVPROC dolGetMapdv;
extern PFNDOLGETMAPFVPROC dolGetMapfv;
extern PFNDOLGETMAPIVPROC dolGetMapiv;
extern PFNDOLEVALCOORD1DPROC dolEvalCoord1d;
extern PFNDOLEVALCOORD1FPROC dolEvalCoord1f;
extern PFNDOLEVALCOORD1DVPROC dolEvalCoord1dv;
extern PFNDOLEVALCOORD1FVPROC dolEvalCoord1fv;
extern PFNDOLEVALCOORD2DPROC dolEvalCoord2d;
extern PFNDOLEVALCOORD2FPROC dolEvalCoord2f;
extern PFNDOLEVALCOORD2DVPROC dolEvalCoord2dv;
extern PFNDOLEVALCOORD2FVPROC dolEvalCoord2fv;
extern PFNDOLMAPGRID1DPROC dolMapGrid1d;
extern PFNDOLMAPGRID1FPROC dolMapGrid1f;
extern PFNDOLMAPGRID2DPROC dolMapGrid2d;
extern PFNDOLMAPGRID2FPROC dolMapGrid2f;
extern PFNDOLEVALPOINT1PROC dolEvalPoint1;
extern PFNDOLEVALPOINT2PROC dolEvalPoint2;
extern PFNDOLEVALMESH1PROC dolEvalMesh1;
extern PFNDOLEVALMESH2PROC dolEvalMesh2;
extern PFNDOLFOGFPROC dolFogf;
extern PFNDOLFOGIPROC dolFogi;
extern PFNDOLFOGFVPROC dolFogfv;
extern PFNDOLFOGIVPROC dolFogiv;
extern PFNDOLFEEDBACKBUFFERPROC dolFeedbackBuffer;
extern PFNDOLPASSTHROUGHPROC dolPassThrough;
extern PFNDOLSELECTBUFFERPROC dolSelectBuffer;
extern PFNDOLINITNAMESPROC dolInitNames;
extern PFNDOLLOADNAMEPROC dolLoadName;
extern PFNDOLPUSHNAMEPROC dolPushName;
extern PFNDOLPOPNAMEPROC dolPopName;

#define glClearIndex dolClearIndex
#define glClearColor dolClearColor
#define glClear dolClear
#define glIndexMask dolIndexMask
#define glColorMask dolColorMask
#define glAlphaFunc dolAlphaFunc
#define glBlendFunc dolBlendFunc
#define glLogicOp dolLogicOp
#define glCullFace dolCullFace
#define glFrontFace dolFrontFace
#define glPointSize dolPointSize
#define glLineWidth dolLineWidth
#define glLineStipple dolLineStipple
#define glPolygonMode dolPolygonMode
#define glPolygonOffset dolPolygonOffset
#define glPolygonStipple dolPolygonStipple
#define glGetPolygonStipple dolGetPolygonStipple
#define glEdgeFlag dolEdgeFlag
#define glEdgeFlagv dolEdgeFlagv
#define glScissor dolScissor
#define glClipPlane dolClipPlane
#define glGetClipPlane dolGetClipPlane
#define glDrawBuffer dolDrawBuffer
#define glReadBuffer dolReadBuffer
#define glEnable dolEnable
#define glDisable dolDisable
#define glIsEnabled dolIsEnabled
#define glEnableClientState dolEnableClientState
#define glDisableClientState dolDisableClientState
#define glGetBooleanv dolGetBooleanv
#define glGetDoublev dolGetDoublev
#define glGetFloatv dolGetFloatv
#define glGetIntegerv dolGetIntegerv
#define glPushAttrib dolPushAttrib
#define glPopAttrib dolPopAttrib
#define glPushClientAttrib dolPushClientAttrib
#define glPopClientAttrib dolPopClientAttrib
#define glRenderMode dolRenderMode
#define glGetError dolGetError
#define glGetString dolGetString
#define glFinish dolFinish
#define glFlush dolFlush
#define glHint dolHint
#define glClearDepth dolClearDepth
#define glDepthFunc dolDepthFunc
#define glDepthMask dolDepthMask
#define glDepthRange dolDepthRange
#define glClearAccum dolClearAccum
#define glAccum dolAccum
#define glMatrixMode dolMatrixMode
#define glOrtho dolOrtho
#define glFrustum dolFrustum
#define glViewport dolViewport
#define glPushMatrix dolPushMatrix
#define glPopMatrix dolPopMatrix
#define glLoadIdentity dolLoadIdentity
#define glLoadMatrixd dolLoadMatrixd
#define glLoadMatrixf dolLoadMatrixf
#define glMultMatrixd dolMultMatrixd
#define glMultMatrixf dolMultMatrixf
#define glRotated dolRotated
#define glRotatef dolRotatef
#define glScaled dolScaled
#define glScalef dolScalef
#define glTranslated dolTranslated
#define glTranslatef dolTranslatef
#define glIsList dolIsList
#define glDeleteLists dolDeleteLists
#define glGenLists dolGenLists
#define glNewList dolNewList
#define glEndList dolEndList
#define glCallList dolCallList
#define glCallLists dolCallLists
#define glListBase dolListBase
#define glBegin dolBegin
#define glEnd dolEnd
#define glVertex2d dolVertex2d
#define glVertex2f dolVertex2f
#define glVertex2i dolVertex2i
#define glVertex2s dolVertex2s
#define glVertex3d dolVertex3d
#define glVertex3f dolVertex3f
#define glVertex3i dolVertex3i
#define glVertex3s dolVertex3s
#define glVertex4d dolVertex4d
#define glVertex4f dolVertex4f
#define glVertex4i dolVertex4i
#define glVertex4s dolVertex4s
#define glVertex2dv dolVertex2dv
#define glVertex2fv dolVertex2fv
#define glVertex2iv dolVertex2iv
#define glVertex2sv dolVertex2sv
#define glVertex3dv dolVertex3dv
#define glVertex3fv dolVertex3fv
#define glVertex3iv dolVertex3iv
#define glVertex3sv dolVertex3sv
#define glVertex4dv dolVertex4dv
#define glVertex4fv dolVertex4fv
#define glVertex4iv dolVertex4iv
#define glVertex4sv dolVertex4sv
#define glNormal3b dolNormal3b
#define glNormal3d dolNormal3d
#define glNormal3f dolNormal3f
#define glNormal3i dolNormal3i
#define glNormal3s dolNormal3s
#define glNormal3bv dolNormal3bv
#define glNormal3dv dolNormal3dv
#define glNormal3fv dolNormal3fv
#define glNormal3iv dolNormal3iv
#define glNormal3sv dolNormal3sv
#define glIndexd dolIndexd
#define glIndexf dolIndexf
#define glIndexi dolIndexi
#define glIndexs dolIndexs
#define glIndexub dolIndexub
#define glIndexdv dolIndexdv
#define glIndexfv dolIndexfv
#define glIndexiv dolIndexiv
#define glIndexsv dolIndexsv
#define glIndexubv dolIndexubv
#define glColor3b dolColor3b
#define glColor3d dolColor3d
#define glColor3f dolColor3f
#define glColor3i dolColor3i
#define glColor3s dolColor3s
#define glColor3ub dolColor3ub
#define glColor3ui dolColor3ui
#define glColor3us dolColor3us
#define glColor4b dolColor4b
#define glColor4d dolColor4d
#define glColor4f dolColor4f
#define glColor4i dolColor4i
#define glColor4s dolColor4s
#define glColor4ub dolColor4ub
#define glColor4ui dolColor4ui
#define glColor4us dolColor4us
#define glColor3bv dolColor3bv
#define glColor3dv dolColor3dv
#define glColor3fv dolColor3fv
#define glColor3iv dolColor3iv
#define glColor3sv dolColor3sv
#define glColor3ubv dolColor3ubv
#define glColor3uiv dolColor3uiv
#define glColor3usv dolColor3usv
#define glColor4bv dolColor4bv
#define glColor4dv dolColor4dv
#define glColor4fv dolColor4fv
#define glColor4iv dolColor4iv
#define glColor4sv dolColor4sv
#define glColor4ubv dolColor4ubv
#define glColor4uiv dolColor4uiv
#define glColor4usv dolColor4usv
#define glTexCoord1d dolTexCoord1d
#define glTexCoord1f dolTexCoord1f
#define glTexCoord1i dolTexCoord1i
#define glTexCoord1s dolTexCoord1s
#define glTexCoord2d dolTexCoord2d
#define glTexCoord2f dolTexCoord2f
#define glTexCoord2i dolTexCoord2i
#define glTexCoord2s dolTexCoord2s
#define glTexCoord3d dolTexCoord3d
#define glTexCoord3f dolTexCoord3f
#define glTexCoord3i dolTexCoord3i
#define glTexCoord3s dolTexCoord3s
#define glTexCoord4d dolTexCoord4d
#define glTexCoord4f dolTexCoord4f
#define glTexCoord4i dolTexCoord4i
#define glTexCoord4s dolTexCoord4s
#define glTexCoord1dv dolTexCoord1dv
#define glTexCoord1fv dolTexCoord1fv
#define glTexCoord1iv dolTexCoord1iv
#define glTexCoord1sv dolTexCoord1sv
#define glTexCoord2dv dolTexCoord2dv
#define glTexCoord2fv dolTexCoord2fv
#define glTexCoord2iv dolTexCoord2iv
#define glTexCoord2sv dolTexCoord2sv
#define glTexCoord3dv dolTexCoord3dv
#define glTexCoord3fv dolTexCoord3fv
#define glTexCoord3iv dolTexCoord3iv
#define glTexCoord3sv dolTexCoord3sv
#define glTexCoord4dv dolTexCoord4dv
#define glTexCoord4fv dolTexCoord4fv
#define glTexCoord4iv dolTexCoord4iv
#define glTexCoord4sv dolTexCoord4sv
#define glRasterPos2d dolRasterPos2d
#define glRasterPos2f dolRasterPos2f
#define glRasterPos2i dolRasterPos2i
#define glRasterPos2s dolRasterPos2s
#define glRasterPos3d dolRasterPos3d
#define glRasterPos3f dolRasterPos3f
#define glRasterPos3i dolRasterPos3i
#define glRasterPos3s dolRasterPos3s
#define glRasterPos4d dolRasterPos4d
#define glRasterPos4f dolRasterPos4f
#define glRasterPos4i dolRasterPos4i
#define glRasterPos4s dolRasterPos4s
#define glRasterPos2dv dolRasterPos2dv
#define glRasterPos2fv dolRasterPos2fv
#define glRasterPos2iv dolRasterPos2iv
#define glRasterPos2sv dolRasterPos2sv
#define glRasterPos3dv dolRasterPos3dv
#define glRasterPos3fv dolRasterPos3fv
#define glRasterPos3iv dolRasterPos3iv
#define glRasterPos3sv dolRasterPos3sv
#define glRasterPos4dv dolRasterPos4dv
#define glRasterPos4fv dolRasterPos4fv
#define glRasterPos4iv dolRasterPos4iv
#define glRasterPos4sv dolRasterPos4sv
#define glRectd dolRectd
#define glRectf dolRectf
#define glRecti dolRecti
#define glRects dolRects
#define glRectdv dolRectdv
#define glRectfv dolRectfv
#define glRectiv dolRectiv
#define glRectsv dolRectsv
#define glVertexPointer dolVertexPointer
#define glNormalPointer dolNormalPointer
#define glColorPointer dolColorPointer
#define glIndexPointer dolIndexPointer
#define glTexCoordPointer dolTexCoordPointer
#define glEdgeFlagPointer dolEdgeFlagPointer
#define glGetPointerv dolGetPointerv
#define glArrayElement dolArrayElement
#define glDrawArrays dolDrawArrays
#define glDrawElements dolDrawElements
#define glInterleavedArrays dolInterleavedArrays
#define glShadeModel dolShadeModel
#define glLightf dolLightf
#define glLighti dolLighti
#define glLightfv dolLightfv
#define glLightiv dolLightiv
#define glGetLightfv dolGetLightfv
#define glGetLightiv dolGetLightiv
#define glLightModelf dolLightModelf
#define glLightModeli dolLightModeli
#define glLightModelfv dolLightModelfv
#define glLightModeliv dolLightModeliv
#define glMaterialf dolMaterialf
#define glMateriali dolMateriali
#define glMaterialfv dolMaterialfv
#define glMaterialiv dolMaterialiv
#define glGetMaterialfv dolGetMaterialfv
#define glGetMaterialiv dolGetMaterialiv
#define glColorMaterial dolColorMaterial
#define glPixelZoom dolPixelZoom
#define glPixelStoref dolPixelStoref
#define glPixelStorei dolPixelStorei
#define glPixelTransferf dolPixelTransferf
#define glPixelTransferi dolPixelTransferi
#define glPixelMapfv dolPixelMapfv
#define glPixelMapuiv dolPixelMapuiv
#define glPixelMapusv dolPixelMapusv
#define glGetPixelMapfv dolGetPixelMapfv
#define glGetPixelMapuiv dolGetPixelMapuiv
#define glGetPixelMapusv dolGetPixelMapusv
#define glBitmap dolBitmap
#define glReadPixels dolReadPixels
#define glDrawPixels dolDrawPixels
#define glCopyPixels dolCopyPixels
#define glStencilFunc dolStencilFunc
#define glStencilMask dolStencilMask
#define glStencilOp dolStencilOp
#define glClearStencil dolClearStencil
#define glTexGend dolTexGend
#define glTexGenf dolTexGenf
#define glTexGeni dolTexGeni
#define glTexGendv dolTexGendv
#define glTexGenfv dolTexGenfv
#define glTexGeniv dolTexGeniv
#define glGetTexGendv dolGetTexGendv
#define glGetTexGenfv dolGetTexGenfv
#define glGetTexGeniv dolGetTexGeniv
#define glTexEnvf dolTexEnvf
#define glTexEnvi dolTexEnvi
#define glTexEnvfv dolTexEnvfv
#define glTexEnviv dolTexEnviv
#define glGetTexEnvfv dolGetTexEnvfv
#define glGetTexEnviv dolGetTexEnviv
#define glTexParameterf dolTexParameterf
#define glTexParameteri dolTexParameteri
#define glTexParameterfv dolTexParameterfv
#define glTexParameteriv dolTexParameteriv
#define glGetTexParameterfv dolGetTexParameterfv
#define glGetTexParameteriv dolGetTexParameteriv
#define glGetTexLevelParameterfv dolGetTexLevelParameterfv
#define glGetTexLevelParameteriv dolGetTexLevelParameteriv
#define glTexImage1D dolTexImage1D
#define glTexImage2D dolTexImage2D
#define glGetTexImage dolGetTexImage
#define glGenTextures dolGenTextures
#define glDeleteTextures dolDeleteTextures
#define glBindTexture dolBindTexture
#define glPrioritizeTextures dolPrioritizeTextures
#define glAreTexturesResident dolAreTexturesResident
#define glIsTexture dolIsTexture
#define glTexSubImage1D dolTexSubImage1D
#define glTexSubImage2D dolTexSubImage2D
#define glCopyTexImage1D dolCopyTexImage1D
#define glCopyTexImage2D dolCopyTexImage2D
#define glCopyTexSubImage1D dolCopyTexSubImage1D
#define glCopyTexSubImage2D dolCopyTexSubImage2D
#define glMap1d dolMap1d
#define glMap1f dolMap1f
#define glMap2d dolMap2d
#define glMap2f dolMap2f
#define glGetMapdv dolGetMapdv
#define glGetMapfv dolGetMapfv
#define glGetMapiv dolGetMapiv
#define glEvalCoord1d dolEvalCoord1d
#define glEvalCoord1f dolEvalCoord1f
#define glEvalCoord1dv dolEvalCoord1dv
#define glEvalCoord1fv dolEvalCoord1fv
#define glEvalCoord2d dolEvalCoord2d
#define glEvalCoord2f dolEvalCoord2f
#define glEvalCoord2dv dolEvalCoord2dv
#define glEvalCoord2fv dolEvalCoord2fv
#define glMapGrid1d dolMapGrid1d
#define glMapGrid1f dolMapGrid1f
#define glMapGrid2d dolMapGrid2d
#define glMapGrid2f dolMapGrid2f
#define glEvalPoint1 dolEvalPoint1
#define glEvalPoint2 dolEvalPoint2
#define glEvalMesh1 dolEvalMesh1
#define glEvalMesh2 dolEvalMesh2
#define glFogf dolFogf
#define glFogi dolFogi
#define glFogfv dolFogfv
#define glFogiv dolFogiv
#define glFeedbackBuffer dolFeedbackBuffer
#define glPassThrough dolPassThrough
#define glSelectBuffer dolSelectBuffer
#define glInitNames dolInitNames
#define glLoadName dolLoadName
#define glPushName dolPushName
#define glPopName dolPopName
