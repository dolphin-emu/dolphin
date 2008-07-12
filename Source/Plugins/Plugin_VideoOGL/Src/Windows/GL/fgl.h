
C  GLUT version of "GL/fgl.h"

C  Modifications from SGI IRIX 5.3 version:
C  1)  F prefix removed from OpenGL constants.
C  2)  Constants over 32 characters truncated to 32 characters.

C ***********************************************************

C  AttribMask 
       integer*4   GL_CURRENT_BIT
       parameter ( GL_CURRENT_BIT = 1 ) 	
       integer*4   GL_POINT_BIT
       parameter ( GL_POINT_BIT = 2 ) 	
       integer*4   GL_LINE_BIT
       parameter ( GL_LINE_BIT = 4 ) 	
       integer*4   GL_POLYGON_BIT
       parameter ( GL_POLYGON_BIT = 8 ) 	
       integer*4   GL_POLYGON_STIPPLE_BIT
       parameter ( GL_POLYGON_STIPPLE_BIT = 16 ) 	
       integer*4   GL_PIXEL_MODE_BIT
       parameter ( GL_PIXEL_MODE_BIT = 32 ) 	
       integer*4   GL_LIGHTING_BIT
       parameter ( GL_LIGHTING_BIT = 64 ) 	
       integer*4   GL_FOG_BIT
       parameter ( GL_FOG_BIT = 128 ) 	
       integer*4   GL_DEPTH_BUFFER_BIT
       parameter ( GL_DEPTH_BUFFER_BIT = 256 ) 	
       integer*4   GL_ACCUM_BUFFER_BIT
       parameter ( GL_ACCUM_BUFFER_BIT = 512 ) 	
       integer*4   GL_STENCIL_BUFFER_BIT
       parameter ( GL_STENCIL_BUFFER_BIT = 1024 ) 	
       integer*4   GL_VIEWPORT_BIT
       parameter ( GL_VIEWPORT_BIT = 2048 ) 	
       integer*4   GL_TRANSFORM_BIT
       parameter ( GL_TRANSFORM_BIT = 4096 ) 	
       integer*4   GL_ENABLE_BIT
       parameter ( GL_ENABLE_BIT = 8192 ) 	
       integer*4   GL_COLOR_BUFFER_BIT
       parameter ( GL_COLOR_BUFFER_BIT = 16384 ) 	
       integer*4   GL_HINT_BIT
       parameter ( GL_HINT_BIT = 32768 ) 	
       integer*4   GL_EVAL_BIT
       parameter ( GL_EVAL_BIT = 65536 ) 	
       integer*4   GL_LIST_BIT
       parameter ( GL_LIST_BIT = 131072 ) 	
       integer*4   GL_TEXTURE_BIT
       parameter ( GL_TEXTURE_BIT = 262144 ) 	
       integer*4   GL_SCISSOR_BIT
       parameter ( GL_SCISSOR_BIT = 524288 ) 	
       integer*4   GL_ALL_ATTRIB_BITS
       parameter ( GL_ALL_ATTRIB_BITS = 1048575 ) 	
C       GL_MULTISAMPLE_BIT_EXT 

C  ClearBufferMask 
C       GL_COLOR_BUFFER_BIT 
C       GL_ACCUM_BUFFER_BIT 
C       GL_STENCIL_BUFFER_BIT 
C       GL_DEPTH_BUFFER_BIT 

C  Boolean 
       integer*4   GL_FALSE
       parameter ( GL_FALSE = 0 ) 	
       integer*4   GL_TRUE
       parameter ( GL_TRUE = 1 ) 	

C  BeginMode 
       integer*4   GL_POINTS
       parameter ( GL_POINTS = 0 ) 	
       integer*4   GL_LINES
       parameter ( GL_LINES = 1 ) 	
       integer*4   GL_LINE_LOOP
       parameter ( GL_LINE_LOOP = 2 ) 	
       integer*4   GL_LINE_STRIP
       parameter ( GL_LINE_STRIP = 3 ) 	
       integer*4   GL_TRIANGLES
       parameter ( GL_TRIANGLES = 4 ) 	
       integer*4   GL_TRIANGLE_STRIP
       parameter ( GL_TRIANGLE_STRIP = 5 ) 	
       integer*4   GL_TRIANGLE_FAN
       parameter ( GL_TRIANGLE_FAN = 6 ) 	
       integer*4   GL_QUADS
       parameter ( GL_QUADS = 7 ) 	
       integer*4   GL_QUAD_STRIP
       parameter ( GL_QUAD_STRIP = 8 ) 	
       integer*4   GL_POLYGON
       parameter ( GL_POLYGON = 9 ) 	

C  AccumOp 
       integer*4   GL_ACCUM
       parameter ( GL_ACCUM = 256 ) 	
       integer*4   GL_LOAD
       parameter ( GL_LOAD = 257 ) 	
       integer*4   GL_RETURN
       parameter ( GL_RETURN = 258 ) 	
       integer*4   GL_MULT
       parameter ( GL_MULT = 259 ) 	
       integer*4   GL_ADD
       parameter ( GL_ADD = 260 ) 	

C  AlphaFunction 
       integer*4   GL_NEVER
       parameter ( GL_NEVER = 512 ) 	
       integer*4   GL_LESS
       parameter ( GL_LESS = 513 ) 	
       integer*4   GL_EQUAL
       parameter ( GL_EQUAL = 514 ) 	
       integer*4   GL_LEQUAL
       parameter ( GL_LEQUAL = 515 ) 	
       integer*4   GL_GREATER
       parameter ( GL_GREATER = 516 ) 	
       integer*4   GL_NOTEQUAL
       parameter ( GL_NOTEQUAL = 517 ) 	
       integer*4   GL_GEQUAL
       parameter ( GL_GEQUAL = 518 ) 	
       integer*4   GL_ALWAYS
       parameter ( GL_ALWAYS = 519 ) 	

C  BlendingFactorDest 
       integer*4   GL_ZERO
       parameter ( GL_ZERO = 0 ) 	
       integer*4   GL_ONE
       parameter ( GL_ONE = 1 ) 	
       integer*4   GL_SRC_COLOR
       parameter ( GL_SRC_COLOR = 768 ) 	
       integer*4   GL_ONE_MINUS_SRC_COLOR
       parameter ( GL_ONE_MINUS_SRC_COLOR = 769 ) 	
       integer*4   GL_SRC_ALPHA
       parameter ( GL_SRC_ALPHA = 770 ) 	
       integer*4   GL_ONE_MINUS_SRC_ALPHA
       parameter ( GL_ONE_MINUS_SRC_ALPHA = 771 ) 	
       integer*4   GL_DST_ALPHA
       parameter ( GL_DST_ALPHA = 772 ) 	
       integer*4   GL_ONE_MINUS_DST_ALPHA
       parameter ( GL_ONE_MINUS_DST_ALPHA = 773 ) 	
C       GL_CONSTANT_COLOR_EXT 
C       GL_ONE_MINUS_CONSTANT_COLOR_EXT 
C       GL_CONSTANT_ALPHA_EXT 
C       GL_ONE_MINUS_CONSTANT_ALPHA_EXT 

C  BlendingFactorSrc 
C       GL_ZERO 
C       GL_ONE 
       integer*4   GL_DST_COLOR
       parameter ( GL_DST_COLOR = 774 ) 	
       integer*4   GL_ONE_MINUS_DST_COLOR
       parameter ( GL_ONE_MINUS_DST_COLOR = 775 ) 	
       integer*4   GL_SRC_ALPHA_SATURATE
       parameter ( GL_SRC_ALPHA_SATURATE = 776 ) 	
C       GL_SRC_ALPHA 
C       GL_ONE_MINUS_SRC_ALPHA 
C       GL_DST_ALPHA 
C       GL_ONE_MINUS_DST_ALPHA 
C       GL_CONSTANT_COLOR_EXT 
C       GL_ONE_MINUS_CONSTANT_COLOR_EXT 
C       GL_CONSTANT_ALPHA_EXT 
C       GL_ONE_MINUS_CONSTANT_ALPHA_EXT 

C  BlendingMode 
C       GL_LOGIC_OP 
C       GL_FUNC_ADD_EXT 
C       GL_MIN_EXT 
C       GL_MAX_EXT 
C       GL_FUNC_SUBTRACT_EXT 
C       GL_FUNC_REVERSE_SUBTRACT_EXT 

C  ColorMaterialFace 
C       GL_FRONT 
C       GL_BACK 
C       GL_FRONT_AND_BACK 

C  ColorMaterialParameter 
C       GL_AMBIENT 
C       GL_DIFFUSE 
C       GL_SPECULAR 
C       GL_EMISSION 
C       GL_AMBIENT_AND_DIFFUSE 

C  ConvolutionBorderMode 
C       GL_REDUCE_EXT 

C  ConvolutionParameter 
C       GL_CONVOLUTION_BORDER_MODE_EXT 
C       GL_CONVOLUTION_FILTER_SCALE_EXT 
C       GL_CONVOLUTION_FILTER_BIAS_EXT 

C  ConvolutionTarget 
C       GL_CONVOLUTION_1D_EXT 
C       GL_CONVOLUTION_2D_EXT 

C  CullFaceMode 
C       GL_FRONT 
C       GL_BACK 
C       GL_FRONT_AND_BACK 

C  DepthFunction 
C       GL_NEVER 
C       GL_LESS 
C       GL_EQUAL 
C       GL_LEQUAL 
C       GL_GREATER 
C       GL_NOTEQUAL 
C       GL_GEQUAL 
C       GL_ALWAYS 

C  DrawBufferMode 
       integer*4   GL_NONE
       parameter ( GL_NONE = 0 ) 	
       integer*4   GL_FRONT_LEFT
       parameter ( GL_FRONT_LEFT = 1024 ) 	
       integer*4   GL_FRONT_RIGHT
       parameter ( GL_FRONT_RIGHT = 1025 ) 	
       integer*4   GL_BACK_LEFT
       parameter ( GL_BACK_LEFT = 1026 ) 	
       integer*4   GL_BACK_RIGHT
       parameter ( GL_BACK_RIGHT = 1027 ) 	
       integer*4   GL_FRONT
       parameter ( GL_FRONT = 1028 ) 	
       integer*4   GL_BACK
       parameter ( GL_BACK = 1029 ) 	
       integer*4   GL_LEFT
       parameter ( GL_LEFT = 1030 ) 	
       integer*4   GL_RIGHT
       parameter ( GL_RIGHT = 1031 ) 	
       integer*4   GL_FRONT_AND_BACK
       parameter ( GL_FRONT_AND_BACK = 1032 ) 	
       integer*4   GL_AUX0
       parameter ( GL_AUX0 = 1033 ) 	
       integer*4   GL_AUX1
       parameter ( GL_AUX1 = 1034 ) 	
       integer*4   GL_AUX2
       parameter ( GL_AUX2 = 1035 ) 	
       integer*4   GL_AUX3
       parameter ( GL_AUX3 = 1036 ) 	

C  Enable 
C       GL_FOG 
C       GL_LIGHTING 
C       GL_TEXTURE_1D 
C       GL_TEXTURE_2D 
C       GL_LINE_STIPPLE 
C       GL_POLYGON_STIPPLE 
C       GL_CULL_FACE 
C       GL_ALPHA_TEST 
C       GL_BLEND 
C       GL_LOGIC_OP 
C       GL_DITHER 
C       GL_STENCIL_TEST 
C       GL_DEPTH_TEST 
C       GL_CLIP_PLANE0 
C       GL_CLIP_PLANE1 
C       GL_CLIP_PLANE2 
C       GL_CLIP_PLANE3 
C       GL_CLIP_PLANE4 
C       GL_CLIP_PLANE5 
C       GL_LIGHT0 
C       GL_LIGHT1 
C       GL_LIGHT2 
C       GL_LIGHT3 
C       GL_LIGHT4 
C       GL_LIGHT5 
C       GL_LIGHT6 
C       GL_LIGHT7 
C       GL_TEXTURE_GEN_S 
C       GL_TEXTURE_GEN_T 
C       GL_TEXTURE_GEN_R 
C       GL_TEXTURE_GEN_Q 
C       GL_MAP1_VERTEX_3 
C       GL_MAP1_VERTEX_4 
C       GL_MAP1_COLOR_4 
C       GL_MAP1_INDEX 
C       GL_MAP1_NORMAL 
C       GL_MAP1_TEXTURE_COORD_1 
C       GL_MAP1_TEXTURE_COORD_2 
C       GL_MAP1_TEXTURE_COORD_3 
C       GL_MAP1_TEXTURE_COORD_4 
C       GL_MAP2_VERTEX_3 
C       GL_MAP2_VERTEX_4 
C       GL_MAP2_COLOR_4 
C       GL_MAP2_INDEX 
C       GL_MAP2_NORMAL 
C       GL_MAP2_TEXTURE_COORD_1 
C       GL_MAP2_TEXTURE_COORD_2 
C       GL_MAP2_TEXTURE_COORD_3 
C       GL_MAP2_TEXTURE_COORD_4 
C       GL_POINT_SMOOTH 
C       GL_LINE_SMOOTH 
C       GL_POLYGON_SMOOTH 
C       GL_SCISSOR_TEST 
C       GL_COLOR_MATERIAL 
C       GL_NORMALIZE 
C       GL_AUTO_NORMAL 
C       GL_CONVOLUTION_1D_EXT 
C       GL_CONVOLUTION_2D_EXT 
C       GL_SEPARABLE_2D_EXT 
C       GL_HISTOGRAM_EXT 
C       GL_MINMAX_EXT 
C       GL_POLYGON_OFFSET_EXT 
C       GL_TEXTURE_3D_EXT 
C       GL_MULTISAMPLE_SGIS 
C       GL_SAMPLE_ALPHA_TO_MASK_SGIS 
C       GL_SAMPLE_ALPHA_TO_ONE_SGIS 
C       GL_SAMPLE_MASK_SGIS 

C  ErrorCode 
       integer*4   GL_NO_ERROR
       parameter ( GL_NO_ERROR = 0 ) 	
       integer*4   GL_INVALID_ENUM
       parameter ( GL_INVALID_ENUM = 1280 ) 	
       integer*4   GL_INVALID_VALUE
       parameter ( GL_INVALID_VALUE = 1281 ) 	
       integer*4   GL_INVALID_OPERATION
       parameter ( GL_INVALID_OPERATION = 1282 ) 	
       integer*4   GL_STACK_OVERFLOW
       parameter ( GL_STACK_OVERFLOW = 1283 ) 	
       integer*4   GL_STACK_UNDERFLOW
       parameter ( GL_STACK_UNDERFLOW = 1284 ) 	
       integer*4   GL_OUT_OF_MEMORY
       parameter ( GL_OUT_OF_MEMORY = 1285 ) 	
C       GL_TABLE_TOO_LARGE_EXT 
C       GL_TEXTURE_TOO_LARGE_EXT 

C  FeedBackMode 
       integer*4   GL_2D
       parameter ( GL_2D = 1536 ) 	
       integer*4   GL_3D
       parameter ( GL_3D = 1537 ) 	
       integer*4   GL_3D_COLOR
       parameter ( GL_3D_COLOR = 1538 ) 	
       integer*4   GL_3D_COLOR_TEXTURE
       parameter ( GL_3D_COLOR_TEXTURE = 1539 ) 	
       integer*4   GL_4D_COLOR_TEXTURE
       parameter ( GL_4D_COLOR_TEXTURE = 1540 ) 	

C  FeedBackToken 
       integer*4   GL_PASS_THROUGH_TOKEN
       parameter ( GL_PASS_THROUGH_TOKEN = 1792 ) 	
       integer*4   GL_POINT_TOKEN
       parameter ( GL_POINT_TOKEN = 1793 ) 	
       integer*4   GL_LINE_TOKEN
       parameter ( GL_LINE_TOKEN = 1794 ) 	
       integer*4   GL_POLYGON_TOKEN
       parameter ( GL_POLYGON_TOKEN = 1795 ) 	
       integer*4   GL_BITMAP_TOKEN
       parameter ( GL_BITMAP_TOKEN = 1796 ) 	
       integer*4   GL_DRAW_PIXEL_TOKEN
       parameter ( GL_DRAW_PIXEL_TOKEN = 1797 ) 	
       integer*4   GL_COPY_PIXEL_TOKEN
       parameter ( GL_COPY_PIXEL_TOKEN = 1798 ) 	
       integer*4   GL_LINE_RESET_TOKEN
       parameter ( GL_LINE_RESET_TOKEN = 1799 ) 	

C  FogMode 
C       GL_LINEAR 
       integer*4   GL_EXP
       parameter ( GL_EXP = 2048 ) 	
       integer*4   GL_EXP2
       parameter ( GL_EXP2 = 2049 ) 	

C  FogParameter 
C       GL_FOG_COLOR 
C       GL_FOG_DENSITY 
C       GL_FOG_END 
C       GL_FOG_INDEX 
C       GL_FOG_MODE 
C       GL_FOG_START 

C  FrontFaceDirection 
       integer*4   GL_CW
       parameter ( GL_CW = 2304 ) 	
       integer*4   GL_CCW
       parameter ( GL_CCW = 2305 ) 	

C  GetConvolutionParameter 
C       GL_CONVOLUTION_BORDER_MODE_EXT 
C       GL_CONVOLUTION_FILTER_SCALE_EXT 
C       GL_CONVOLUTION_FILTER_BIAS_EXT 
C       GL_CONVOLUTION_FORMAT_EXT 
C       GL_CONVOLUTION_WIDTH_EXT 
C       GL_CONVOLUTION_HEIGHT_EXT 
C       GL_MAX_CONVOLUTION_WIDTH_EXT 
C       GL_MAX_CONVOLUTION_HEIGHT_EXT 

C  GetHistogramParameter 
C       GL_HISTOGRAM_WIDTH_EXT 
C       GL_HISTOGRAM_FORMAT_EXT 
C       GL_HISTOGRAM_RED_SIZE_EXT 
C       GL_HISTOGRAM_GREEN_SIZE_EXT 
C       GL_HISTOGRAM_BLUE_SIZE_EXT 
C       GL_HISTOGRAM_ALPHA_SIZE_EXT 
C       GL_HISTOGRAM_LUMINANCE_SIZE_EXT 
C       GL_HISTOGRAM_SINK_EXT 

C  GetMapTarget 
       integer*4   GL_COEFF
       parameter ( GL_COEFF = 2560 ) 	
       integer*4   GL_ORDER
       parameter ( GL_ORDER = 2561 ) 	
       integer*4   GL_DOMAIN
       parameter ( GL_DOMAIN = 2562 ) 	

C  GetMinmaxParameter 
C       GL_MINMAX_FORMAT_EXT 
C       GL_MINMAX_SINK_EXT 

C  GetPixelMap 
       integer*4   GL_PIXEL_MAP_I_TO_I
       parameter ( GL_PIXEL_MAP_I_TO_I = 3184 ) 	
       integer*4   GL_PIXEL_MAP_S_TO_S
       parameter ( GL_PIXEL_MAP_S_TO_S = 3185 ) 	
       integer*4   GL_PIXEL_MAP_I_TO_R
       parameter ( GL_PIXEL_MAP_I_TO_R = 3186 ) 	
       integer*4   GL_PIXEL_MAP_I_TO_G
       parameter ( GL_PIXEL_MAP_I_TO_G = 3187 ) 	
       integer*4   GL_PIXEL_MAP_I_TO_B
       parameter ( GL_PIXEL_MAP_I_TO_B = 3188 ) 	
       integer*4   GL_PIXEL_MAP_I_TO_A
       parameter ( GL_PIXEL_MAP_I_TO_A = 3189 ) 	
       integer*4   GL_PIXEL_MAP_R_TO_R
       parameter ( GL_PIXEL_MAP_R_TO_R = 3190 ) 	
       integer*4   GL_PIXEL_MAP_G_TO_G
       parameter ( GL_PIXEL_MAP_G_TO_G = 3191 ) 	
       integer*4   GL_PIXEL_MAP_B_TO_B
       parameter ( GL_PIXEL_MAP_B_TO_B = 3192 ) 	
       integer*4   GL_PIXEL_MAP_A_TO_A
       parameter ( GL_PIXEL_MAP_A_TO_A = 3193 ) 	

C  GetTarget 
       integer*4   GL_CURRENT_COLOR
       parameter ( GL_CURRENT_COLOR = 2816 ) 	
       integer*4   GL_CURRENT_INDEX
       parameter ( GL_CURRENT_INDEX = 2817 ) 	
       integer*4   GL_CURRENT_NORMAL
       parameter ( GL_CURRENT_NORMAL = 2818 ) 	
       integer*4   GL_CURRENT_TEXTURE_COORDS
       parameter ( GL_CURRENT_TEXTURE_COORDS = 2819 ) 	
       integer*4   GL_CURRENT_RASTER_COLOR
       parameter ( GL_CURRENT_RASTER_COLOR = 2820 ) 	
       integer*4   GL_CURRENT_RASTER_INDEX
       parameter ( GL_CURRENT_RASTER_INDEX = 2821 ) 	
       integer*4   GL_CURRENT_RASTER_TEXTURE_COORDS
       parameter ( GL_CURRENT_RASTER_TEXTURE_COORDS = 2822 ) 	
       integer*4   GL_CURRENT_RASTER_POSITION
       parameter ( GL_CURRENT_RASTER_POSITION = 2823 ) 	
       integer*4   GL_CURRENT_RASTER_POSITION_VALID
       parameter ( GL_CURRENT_RASTER_POSITION_VALID = 2824 ) 	
       integer*4   GL_CURRENT_RASTER_DISTANCE
       parameter ( GL_CURRENT_RASTER_DISTANCE = 2825 ) 	
       integer*4   GL_POINT_SMOOTH
       parameter ( GL_POINT_SMOOTH = 2832 ) 	
       integer*4   GL_POINT_SIZE
       parameter ( GL_POINT_SIZE = 2833 ) 	
       integer*4   GL_POINT_SIZE_RANGE
       parameter ( GL_POINT_SIZE_RANGE = 2834 ) 	
       integer*4   GL_POINT_SIZE_GRANULARITY
       parameter ( GL_POINT_SIZE_GRANULARITY = 2835 ) 	
       integer*4   GL_LINE_SMOOTH
       parameter ( GL_LINE_SMOOTH = 2848 ) 	
       integer*4   GL_LINE_WIDTH
       parameter ( GL_LINE_WIDTH = 2849 ) 	
       integer*4   GL_LINE_WIDTH_RANGE
       parameter ( GL_LINE_WIDTH_RANGE = 2850 ) 	
       integer*4   GL_LINE_WIDTH_GRANULARITY
       parameter ( GL_LINE_WIDTH_GRANULARITY = 2851 ) 	
       integer*4   GL_LINE_STIPPLE
       parameter ( GL_LINE_STIPPLE = 2852 ) 	
       integer*4   GL_LINE_STIPPLE_PATTERN
       parameter ( GL_LINE_STIPPLE_PATTERN = 2853 ) 	
       integer*4   GL_LINE_STIPPLE_REPEAT
       parameter ( GL_LINE_STIPPLE_REPEAT = 2854 ) 	
       integer*4   GL_LIST_MODE
       parameter ( GL_LIST_MODE = 2864 ) 	
       integer*4   GL_MAX_LIST_NESTING
       parameter ( GL_MAX_LIST_NESTING = 2865 ) 	
       integer*4   GL_LIST_BASE
       parameter ( GL_LIST_BASE = 2866 ) 	
       integer*4   GL_LIST_INDEX
       parameter ( GL_LIST_INDEX = 2867 ) 	
       integer*4   GL_POLYGON_MODE
       parameter ( GL_POLYGON_MODE = 2880 ) 	
       integer*4   GL_POLYGON_SMOOTH
       parameter ( GL_POLYGON_SMOOTH = 2881 ) 	
       integer*4   GL_POLYGON_STIPPLE
       parameter ( GL_POLYGON_STIPPLE = 2882 ) 	
       integer*4   GL_EDGE_FLAG
       parameter ( GL_EDGE_FLAG = 2883 ) 	
       integer*4   GL_CULL_FACE
       parameter ( GL_CULL_FACE = 2884 ) 	
       integer*4   GL_CULL_FACE_MODE
       parameter ( GL_CULL_FACE_MODE = 2885 ) 	
       integer*4   GL_FRONT_FACE
       parameter ( GL_FRONT_FACE = 2886 ) 	
       integer*4   GL_LIGHTING
       parameter ( GL_LIGHTING = 2896 ) 	
       integer*4   GL_LIGHT_MODEL_LOCAL_VIEWER
       parameter ( GL_LIGHT_MODEL_LOCAL_VIEWER = 2897 ) 	
       integer*4   GL_LIGHT_MODEL_TWO_SIDE
       parameter ( GL_LIGHT_MODEL_TWO_SIDE = 2898 ) 	
       integer*4   GL_LIGHT_MODEL_AMBIENT
       parameter ( GL_LIGHT_MODEL_AMBIENT = 2899 ) 	
       integer*4   GL_SHADE_MODEL
       parameter ( GL_SHADE_MODEL = 2900 ) 	
       integer*4   GL_COLOR_MATERIAL_FACE
       parameter ( GL_COLOR_MATERIAL_FACE = 2901 ) 	
       integer*4   GL_COLOR_MATERIAL_PARAMETER
       parameter ( GL_COLOR_MATERIAL_PARAMETER = 2902 ) 	
       integer*4   GL_COLOR_MATERIAL
       parameter ( GL_COLOR_MATERIAL = 2903 ) 	
       integer*4   GL_FOG
       parameter ( GL_FOG = 2912 ) 	
       integer*4   GL_FOG_INDEX
       parameter ( GL_FOG_INDEX = 2913 ) 	
       integer*4   GL_FOG_DENSITY
       parameter ( GL_FOG_DENSITY = 2914 ) 	
       integer*4   GL_FOG_START
       parameter ( GL_FOG_START = 2915 ) 	
       integer*4   GL_FOG_END
       parameter ( GL_FOG_END = 2916 ) 	
       integer*4   GL_FOG_MODE
       parameter ( GL_FOG_MODE = 2917 ) 	
       integer*4   GL_FOG_COLOR
       parameter ( GL_FOG_COLOR = 2918 ) 	
       integer*4   GL_DEPTH_RANGE
       parameter ( GL_DEPTH_RANGE = 2928 ) 	
       integer*4   GL_DEPTH_TEST
       parameter ( GL_DEPTH_TEST = 2929 ) 	
       integer*4   GL_DEPTH_WRITEMASK
       parameter ( GL_DEPTH_WRITEMASK = 2930 ) 	
       integer*4   GL_DEPTH_CLEAR_VALUE
       parameter ( GL_DEPTH_CLEAR_VALUE = 2931 ) 	
       integer*4   GL_DEPTH_FUNC
       parameter ( GL_DEPTH_FUNC = 2932 ) 	
       integer*4   GL_ACCUM_CLEAR_VALUE
       parameter ( GL_ACCUM_CLEAR_VALUE = 2944 ) 	
       integer*4   GL_STENCIL_TEST
       parameter ( GL_STENCIL_TEST = 2960 ) 	
       integer*4   GL_STENCIL_CLEAR_VALUE
       parameter ( GL_STENCIL_CLEAR_VALUE = 2961 ) 	
       integer*4   GL_STENCIL_FUNC
       parameter ( GL_STENCIL_FUNC = 2962 ) 	
       integer*4   GL_STENCIL_VALUE_MASK
       parameter ( GL_STENCIL_VALUE_MASK = 2963 ) 	
       integer*4   GL_STENCIL_FAIL
       parameter ( GL_STENCIL_FAIL = 2964 ) 	
       integer*4   GL_STENCIL_PASS_DEPTH_FAIL
       parameter ( GL_STENCIL_PASS_DEPTH_FAIL = 2965 ) 	
       integer*4   GL_STENCIL_PASS_DEPTH_PASS
       parameter ( GL_STENCIL_PASS_DEPTH_PASS = 2966 ) 	
       integer*4   GL_STENCIL_REF
       parameter ( GL_STENCIL_REF = 2967 ) 	
       integer*4   GL_STENCIL_WRITEMASK
       parameter ( GL_STENCIL_WRITEMASK = 2968 ) 	
       integer*4   GL_MATRIX_MODE
       parameter ( GL_MATRIX_MODE = 2976 ) 	
       integer*4   GL_NORMALIZE
       parameter ( GL_NORMALIZE = 2977 ) 	
       integer*4   GL_VIEWPORT
       parameter ( GL_VIEWPORT = 2978 ) 	
       integer*4   GL_MODELVIEW_STACK_DEPTH
       parameter ( GL_MODELVIEW_STACK_DEPTH = 2979 ) 	
       integer*4   GL_PROJECTION_STACK_DEPTH
       parameter ( GL_PROJECTION_STACK_DEPTH = 2980 ) 	
       integer*4   GL_TEXTURE_STACK_DEPTH
       parameter ( GL_TEXTURE_STACK_DEPTH = 2981 ) 	
       integer*4   GL_MODELVIEW_MATRIX
       parameter ( GL_MODELVIEW_MATRIX = 2982 ) 	
       integer*4   GL_PROJECTION_MATRIX
       parameter ( GL_PROJECTION_MATRIX = 2983 ) 	
       integer*4   GL_TEXTURE_MATRIX
       parameter ( GL_TEXTURE_MATRIX = 2984 ) 	
       integer*4   GL_ATTRIB_STACK_DEPTH
       parameter ( GL_ATTRIB_STACK_DEPTH = 2992 ) 	
       integer*4   GL_ALPHA_TEST
       parameter ( GL_ALPHA_TEST = 3008 ) 	
       integer*4   GL_ALPHA_TEST_FUNC
       parameter ( GL_ALPHA_TEST_FUNC = 3009 ) 	
       integer*4   GL_ALPHA_TEST_REF
       parameter ( GL_ALPHA_TEST_REF = 3010 ) 	
       integer*4   GL_DITHER
       parameter ( GL_DITHER = 3024 ) 	
       integer*4   GL_BLEND_DST
       parameter ( GL_BLEND_DST = 3040 ) 	
       integer*4   GL_BLEND_SRC
       parameter ( GL_BLEND_SRC = 3041 ) 	
       integer*4   GL_BLEND
       parameter ( GL_BLEND = 3042 ) 	
       integer*4   GL_LOGIC_OP_MODE
       parameter ( GL_LOGIC_OP_MODE = 3056 ) 	
       integer*4   GL_LOGIC_OP
       parameter ( GL_LOGIC_OP = 3057 ) 	
       integer*4   GL_AUX_BUFFERS
       parameter ( GL_AUX_BUFFERS = 3072 ) 	
       integer*4   GL_DRAW_BUFFER
       parameter ( GL_DRAW_BUFFER = 3073 ) 	
       integer*4   GL_READ_BUFFER
       parameter ( GL_READ_BUFFER = 3074 ) 	
       integer*4   GL_SCISSOR_BOX
       parameter ( GL_SCISSOR_BOX = 3088 ) 	
       integer*4   GL_SCISSOR_TEST
       parameter ( GL_SCISSOR_TEST = 3089 ) 	
       integer*4   GL_INDEX_CLEAR_VALUE
       parameter ( GL_INDEX_CLEAR_VALUE = 3104 ) 	
       integer*4   GL_INDEX_WRITEMASK
       parameter ( GL_INDEX_WRITEMASK = 3105 ) 	
       integer*4   GL_COLOR_CLEAR_VALUE
       parameter ( GL_COLOR_CLEAR_VALUE = 3106 ) 	
       integer*4   GL_COLOR_WRITEMASK
       parameter ( GL_COLOR_WRITEMASK = 3107 ) 	
       integer*4   GL_INDEX_MODE
       parameter ( GL_INDEX_MODE = 3120 ) 	
       integer*4   GL_RGBA_MODE
       parameter ( GL_RGBA_MODE = 3121 ) 	
       integer*4   GL_DOUBLEBUFFER
       parameter ( GL_DOUBLEBUFFER = 3122 ) 	
       integer*4   GL_STEREO
       parameter ( GL_STEREO = 3123 ) 	
       integer*4   GL_RENDER_MODE
       parameter ( GL_RENDER_MODE = 3136 ) 	
       integer*4   GL_PERSPECTIVE_CORRECTION_HINT
       parameter ( GL_PERSPECTIVE_CORRECTION_HINT = 3152 ) 	
       integer*4   GL_POINT_SMOOTH_HINT
       parameter ( GL_POINT_SMOOTH_HINT = 3153 ) 	
       integer*4   GL_LINE_SMOOTH_HINT
       parameter ( GL_LINE_SMOOTH_HINT = 3154 ) 	
       integer*4   GL_POLYGON_SMOOTH_HINT
       parameter ( GL_POLYGON_SMOOTH_HINT = 3155 ) 	
       integer*4   GL_FOG_HINT
       parameter ( GL_FOG_HINT = 3156 ) 	
       integer*4   GL_TEXTURE_GEN_S
       parameter ( GL_TEXTURE_GEN_S = 3168 ) 	
       integer*4   GL_TEXTURE_GEN_T
       parameter ( GL_TEXTURE_GEN_T = 3169 ) 	
       integer*4   GL_TEXTURE_GEN_R
       parameter ( GL_TEXTURE_GEN_R = 3170 ) 	
       integer*4   GL_TEXTURE_GEN_Q
       parameter ( GL_TEXTURE_GEN_Q = 3171 ) 	
       integer*4   GL_PIXEL_MAP_I_TO_I_SIZE
       parameter ( GL_PIXEL_MAP_I_TO_I_SIZE = 3248 ) 	
       integer*4   GL_PIXEL_MAP_S_TO_S_SIZE
       parameter ( GL_PIXEL_MAP_S_TO_S_SIZE = 3249 ) 	
       integer*4   GL_PIXEL_MAP_I_TO_R_SIZE
       parameter ( GL_PIXEL_MAP_I_TO_R_SIZE = 3250 ) 	
       integer*4   GL_PIXEL_MAP_I_TO_G_SIZE
       parameter ( GL_PIXEL_MAP_I_TO_G_SIZE = 3251 ) 	
       integer*4   GL_PIXEL_MAP_I_TO_B_SIZE
       parameter ( GL_PIXEL_MAP_I_TO_B_SIZE = 3252 ) 	
       integer*4   GL_PIXEL_MAP_I_TO_A_SIZE
       parameter ( GL_PIXEL_MAP_I_TO_A_SIZE = 3253 ) 	
       integer*4   GL_PIXEL_MAP_R_TO_R_SIZE
       parameter ( GL_PIXEL_MAP_R_TO_R_SIZE = 3254 ) 	
       integer*4   GL_PIXEL_MAP_G_TO_G_SIZE
       parameter ( GL_PIXEL_MAP_G_TO_G_SIZE = 3255 ) 	
       integer*4   GL_PIXEL_MAP_B_TO_B_SIZE
       parameter ( GL_PIXEL_MAP_B_TO_B_SIZE = 3256 ) 	
       integer*4   GL_PIXEL_MAP_A_TO_A_SIZE
       parameter ( GL_PIXEL_MAP_A_TO_A_SIZE = 3257 ) 	
       integer*4   GL_UNPACK_SWAP_BYTES
       parameter ( GL_UNPACK_SWAP_BYTES = 3312 ) 	
       integer*4   GL_UNPACK_LSB_FIRST
       parameter ( GL_UNPACK_LSB_FIRST = 3313 ) 	
       integer*4   GL_UNPACK_ROW_LENGTH
       parameter ( GL_UNPACK_ROW_LENGTH = 3314 ) 	
       integer*4   GL_UNPACK_SKIP_ROWS
       parameter ( GL_UNPACK_SKIP_ROWS = 3315 ) 	
       integer*4   GL_UNPACK_SKIP_PIXELS
       parameter ( GL_UNPACK_SKIP_PIXELS = 3316 ) 	
       integer*4   GL_UNPACK_ALIGNMENT
       parameter ( GL_UNPACK_ALIGNMENT = 3317 ) 	
       integer*4   GL_PACK_SWAP_BYTES
       parameter ( GL_PACK_SWAP_BYTES = 3328 ) 	
       integer*4   GL_PACK_LSB_FIRST
       parameter ( GL_PACK_LSB_FIRST = 3329 ) 	
       integer*4   GL_PACK_ROW_LENGTH
       parameter ( GL_PACK_ROW_LENGTH = 3330 ) 	
       integer*4   GL_PACK_SKIP_ROWS
       parameter ( GL_PACK_SKIP_ROWS = 3331 ) 	
       integer*4   GL_PACK_SKIP_PIXELS
       parameter ( GL_PACK_SKIP_PIXELS = 3332 ) 	
       integer*4   GL_PACK_ALIGNMENT
       parameter ( GL_PACK_ALIGNMENT = 3333 ) 	
       integer*4   GL_MAP_COLOR
       parameter ( GL_MAP_COLOR = 3344 ) 	
       integer*4   GL_MAP_STENCIL
       parameter ( GL_MAP_STENCIL = 3345 ) 	
       integer*4   GL_INDEX_SHIFT
       parameter ( GL_INDEX_SHIFT = 3346 ) 	
       integer*4   GL_INDEX_OFFSET
       parameter ( GL_INDEX_OFFSET = 3347 ) 	
       integer*4   GL_RED_SCALE
       parameter ( GL_RED_SCALE = 3348 ) 	
       integer*4   GL_RED_BIAS
       parameter ( GL_RED_BIAS = 3349 ) 	
       integer*4   GL_ZOOM_X
       parameter ( GL_ZOOM_X = 3350 ) 	
       integer*4   GL_ZOOM_Y
       parameter ( GL_ZOOM_Y = 3351 ) 	
       integer*4   GL_GREEN_SCALE
       parameter ( GL_GREEN_SCALE = 3352 ) 	
       integer*4   GL_GREEN_BIAS
       parameter ( GL_GREEN_BIAS = 3353 ) 	
       integer*4   GL_BLUE_SCALE
       parameter ( GL_BLUE_SCALE = 3354 ) 	
       integer*4   GL_BLUE_BIAS
       parameter ( GL_BLUE_BIAS = 3355 ) 	
       integer*4   GL_ALPHA_SCALE
       parameter ( GL_ALPHA_SCALE = 3356 ) 	
       integer*4   GL_ALPHA_BIAS
       parameter ( GL_ALPHA_BIAS = 3357 ) 	
       integer*4   GL_DEPTH_SCALE
       parameter ( GL_DEPTH_SCALE = 3358 ) 	
       integer*4   GL_DEPTH_BIAS
       parameter ( GL_DEPTH_BIAS = 3359 ) 	
       integer*4   GL_MAX_EVAL_ORDER
       parameter ( GL_MAX_EVAL_ORDER = 3376 ) 	
       integer*4   GL_MAX_LIGHTS
       parameter ( GL_MAX_LIGHTS = 3377 ) 	
       integer*4   GL_MAX_CLIP_PLANES
       parameter ( GL_MAX_CLIP_PLANES = 3378 ) 	
       integer*4   GL_MAX_TEXTURE_SIZE
       parameter ( GL_MAX_TEXTURE_SIZE = 3379 ) 	
       integer*4   GL_MAX_PIXEL_MAP_TABLE
       parameter ( GL_MAX_PIXEL_MAP_TABLE = 3380 ) 	
       integer*4   GL_MAX_ATTRIB_STACK_DEPTH
       parameter ( GL_MAX_ATTRIB_STACK_DEPTH = 3381 ) 	
       integer*4   GL_MAX_MODELVIEW_STACK_DEPTH
       parameter ( GL_MAX_MODELVIEW_STACK_DEPTH = 3382 ) 	
       integer*4   GL_MAX_NAME_STACK_DEPTH
       parameter ( GL_MAX_NAME_STACK_DEPTH = 3383 ) 	
       integer*4   GL_MAX_PROJECTION_STACK_DEPTH
       parameter ( GL_MAX_PROJECTION_STACK_DEPTH = 3384 ) 	
       integer*4   GL_MAX_TEXTURE_STACK_DEPTH
       parameter ( GL_MAX_TEXTURE_STACK_DEPTH = 3385 ) 	
       integer*4   GL_MAX_VIEWPORT_DIMS
       parameter ( GL_MAX_VIEWPORT_DIMS = 3386 ) 	
       integer*4   GL_SUBPIXEL_BITS
       parameter ( GL_SUBPIXEL_BITS = 3408 ) 	
       integer*4   GL_INDEX_BITS
       parameter ( GL_INDEX_BITS = 3409 ) 	
       integer*4   GL_RED_BITS
       parameter ( GL_RED_BITS = 3410 ) 	
       integer*4   GL_GREEN_BITS
       parameter ( GL_GREEN_BITS = 3411 ) 	
       integer*4   GL_BLUE_BITS
       parameter ( GL_BLUE_BITS = 3412 ) 	
       integer*4   GL_ALPHA_BITS
       parameter ( GL_ALPHA_BITS = 3413 ) 	
       integer*4   GL_DEPTH_BITS
       parameter ( GL_DEPTH_BITS = 3414 ) 	
       integer*4   GL_STENCIL_BITS
       parameter ( GL_STENCIL_BITS = 3415 ) 	
       integer*4   GL_ACCUM_RED_BITS
       parameter ( GL_ACCUM_RED_BITS = 3416 ) 	
       integer*4   GL_ACCUM_GREEN_BITS
       parameter ( GL_ACCUM_GREEN_BITS = 3417 ) 	
       integer*4   GL_ACCUM_BLUE_BITS
       parameter ( GL_ACCUM_BLUE_BITS = 3418 ) 	
       integer*4   GL_ACCUM_ALPHA_BITS
       parameter ( GL_ACCUM_ALPHA_BITS = 3419 ) 	
       integer*4   GL_NAME_STACK_DEPTH
       parameter ( GL_NAME_STACK_DEPTH = 3440 ) 	
       integer*4   GL_AUTO_NORMAL
       parameter ( GL_AUTO_NORMAL = 3456 ) 	
       integer*4   GL_MAP1_COLOR_4
       parameter ( GL_MAP1_COLOR_4 = 3472 ) 	
       integer*4   GL_MAP1_INDEX
       parameter ( GL_MAP1_INDEX = 3473 ) 	
       integer*4   GL_MAP1_NORMAL
       parameter ( GL_MAP1_NORMAL = 3474 ) 	
       integer*4   GL_MAP1_TEXTURE_COORD_1
       parameter ( GL_MAP1_TEXTURE_COORD_1 = 3475 ) 	
       integer*4   GL_MAP1_TEXTURE_COORD_2
       parameter ( GL_MAP1_TEXTURE_COORD_2 = 3476 ) 	
       integer*4   GL_MAP1_TEXTURE_COORD_3
       parameter ( GL_MAP1_TEXTURE_COORD_3 = 3477 ) 	
       integer*4   GL_MAP1_TEXTURE_COORD_4
       parameter ( GL_MAP1_TEXTURE_COORD_4 = 3478 ) 	
       integer*4   GL_MAP1_VERTEX_3
       parameter ( GL_MAP1_VERTEX_3 = 3479 ) 	
       integer*4   GL_MAP1_VERTEX_4
       parameter ( GL_MAP1_VERTEX_4 = 3480 ) 	
       integer*4   GL_MAP2_COLOR_4
       parameter ( GL_MAP2_COLOR_4 = 3504 ) 	
       integer*4   GL_MAP2_INDEX
       parameter ( GL_MAP2_INDEX = 3505 ) 	
       integer*4   GL_MAP2_NORMAL
       parameter ( GL_MAP2_NORMAL = 3506 ) 	
       integer*4   GL_MAP2_TEXTURE_COORD_1
       parameter ( GL_MAP2_TEXTURE_COORD_1 = 3507 ) 	
       integer*4   GL_MAP2_TEXTURE_COORD_2
       parameter ( GL_MAP2_TEXTURE_COORD_2 = 3508 ) 	
       integer*4   GL_MAP2_TEXTURE_COORD_3
       parameter ( GL_MAP2_TEXTURE_COORD_3 = 3509 ) 	
       integer*4   GL_MAP2_TEXTURE_COORD_4
       parameter ( GL_MAP2_TEXTURE_COORD_4 = 3510 ) 	
       integer*4   GL_MAP2_VERTEX_3
       parameter ( GL_MAP2_VERTEX_3 = 3511 ) 	
       integer*4   GL_MAP2_VERTEX_4
       parameter ( GL_MAP2_VERTEX_4 = 3512 ) 	
       integer*4   GL_MAP1_GRID_DOMAIN
       parameter ( GL_MAP1_GRID_DOMAIN = 3536 ) 	
       integer*4   GL_MAP1_GRID_SEGMENTS
       parameter ( GL_MAP1_GRID_SEGMENTS = 3537 ) 	
       integer*4   GL_MAP2_GRID_DOMAIN
       parameter ( GL_MAP2_GRID_DOMAIN = 3538 ) 	
       integer*4   GL_MAP2_GRID_SEGMENTS
       parameter ( GL_MAP2_GRID_SEGMENTS = 3539 ) 	
       integer*4   GL_TEXTURE_1D
       parameter ( GL_TEXTURE_1D = 3552 ) 	
       integer*4   GL_TEXTURE_2D
       parameter ( GL_TEXTURE_2D = 3553 ) 	
C       GL_BLEND_COLOR_EXT 
C       GL_BLEND_EQUATION_EXT 
C       GL_CONVOLUTION_1D_EXT 
C       GL_CONVOLUTION_2D_EXT 
C       GL_SEPARABLE_2D_EXT 
C       GL_POST_CONVOLUTION_RED_SCALE_EXT 
C       GL_POST_CONVOLUTION_GREEN_SCALE_EXT 
C       GL_POST_CONVOLUTION_BLUE_SCALE_EXT 
C       GL_POST_CONVOLUTION_ALPHA_SCALE_EXT 
C       GL_POST_CONVOLUTION_RED_BIAS_EXT 
C       GL_POST_CONVOLUTION_GREEN_BIAS_EXT 
C       GL_POST_CONVOLUTION_BLUE_BIAS_EXT 
C       GL_POST_CONVOLUTION_ALPHA_BIAS_EXT 
C       GL_HISTOGRAM_EXT 
C       GL_MINMAX_EXT 
C       GL_POLYGON_OFFSET_EXT 
C       GL_POLYGON_OFFSET_FACTOR_EXT 
C       GL_POLYGON_OFFSET_BIAS_EXT 
C       GL_PACK_SKIP_IMAGES_EXT 
C       GL_PACK_IMAGE_HEIGHT_EXT 
C       GL_UNPACK_SKIP_IMAGES_EXT 
C       GL_UNPACK_IMAGE_HEIGHT_EXT 
C       GL_TEXTURE_3D_EXT 
C       GL_MAX_3D_TEXTURE_SIZE_EXT 
C       GL_DETAIL_TEXTURE_2D_BINDING_SGIS 
C       GL_MULTISAMPLE_SGIS 
C       GL_SAMPLE_ALPHA_TO_MASK_SGIS 
C       GL_SAMPLE_ALPHA_TO_ONE_SGIS 
C       GL_SAMPLE_MASK_SGIS 
C       GL_SAMPLE_BUFFERS_SGIS 
C       GL_SAMPLES_SGIS 
C       GL_SAMPLE_MASK_VALUE_SGIS 
C       GL_SAMPLE_MASK_INVERT_SGIS 
C       GL_SAMPLE_PATTERN_SGIS 

C  GetTextureParameter 
C       GL_TEXTURE_MAG_FILTER 
C       GL_TEXTURE_MIN_FILTER 
C       GL_TEXTURE_WRAP_S 
C       GL_TEXTURE_WRAP_T 
       integer*4   GL_TEXTURE_WIDTH
       parameter ( GL_TEXTURE_WIDTH = 4096 ) 	
       integer*4   GL_TEXTURE_HEIGHT
       parameter ( GL_TEXTURE_HEIGHT = 4097 ) 	
       integer*4   GL_TEXTURE_COMPONENTS
       parameter ( GL_TEXTURE_COMPONENTS = 4099 ) 	
       integer*4   GL_TEXTURE_BORDER_COLOR
       parameter ( GL_TEXTURE_BORDER_COLOR = 4100 ) 	
       integer*4   GL_TEXTURE_BORDER
       parameter ( GL_TEXTURE_BORDER = 4101 ) 	
C       GL_TEXTURE_RED_SIZE_EXT 
C       GL_TEXTURE_GREEN_SIZE_EXT 
C       GL_TEXTURE_BLUE_SIZE_EXT 
C       GL_TEXTURE_ALPHA_SIZE_EXT 
C       GL_TEXTURE_LUMINANCE_SIZE_EXT 
C       GL_TEXTURE_INTENSITY_SIZE_EXT 
C       GL_TEXTURE_DEPTH_EXT 
C       GL_TEXTURE_WRAP_R_EXT 
C       GL_DETAIL_TEXTURE_LEVEL_SGIS 
C       GL_DETAIL_TEXTURE_MODE_SGIS 
C       GL_DETAIL_TEXTURE_FUNC_POINTS_SGIS 
C       GL_SHARPEN_TEXTURE_FUNC_POINTS_SGIS 

C  HintMode 
       integer*4   GL_DONT_CARE
       parameter ( GL_DONT_CARE = 4352 ) 	
       integer*4   GL_FASTEST
       parameter ( GL_FASTEST = 4353 ) 	
       integer*4   GL_NICEST
       parameter ( GL_NICEST = 4354 ) 	

C  HintTarget 
C       GL_PERSPECTIVE_CORRECTION_HINT 
C       GL_POINT_SMOOTH_HINT 
C       GL_LINE_SMOOTH_HINT 
C       GL_POLYGON_SMOOTH_HINT 
C       GL_FOG_HINT 

C  HistogramTarget 
C       GL_HISTOGRAM_EXT 
C       GL_PROXY_HISTOGRAM_EXT 

C  LightModelParameter 
C       GL_LIGHT_MODEL_AMBIENT 
C       GL_LIGHT_MODEL_LOCAL_VIEWER 
C       GL_LIGHT_MODEL_TWO_SIDE 

C  LightParameter 
       integer*4   GL_AMBIENT
       parameter ( GL_AMBIENT = 4608 ) 	
       integer*4   GL_DIFFUSE
       parameter ( GL_DIFFUSE = 4609 ) 	
       integer*4   GL_SPECULAR
       parameter ( GL_SPECULAR = 4610 ) 	
       integer*4   GL_POSITION
       parameter ( GL_POSITION = 4611 ) 	
       integer*4   GL_SPOT_DIRECTION
       parameter ( GL_SPOT_DIRECTION = 4612 ) 	
       integer*4   GL_SPOT_EXPONENT
       parameter ( GL_SPOT_EXPONENT = 4613 ) 	
       integer*4   GL_SPOT_CUTOFF
       parameter ( GL_SPOT_CUTOFF = 4614 ) 	
       integer*4   GL_CONSTANT_ATTENUATION
       parameter ( GL_CONSTANT_ATTENUATION = 4615 ) 	
       integer*4   GL_LINEAR_ATTENUATION
       parameter ( GL_LINEAR_ATTENUATION = 4616 ) 	
       integer*4   GL_QUADRATIC_ATTENUATION
       parameter ( GL_QUADRATIC_ATTENUATION = 4617 ) 	

C  ListMode 
       integer*4   GL_COMPILE
       parameter ( GL_COMPILE = 4864 ) 	
       integer*4   GL_COMPILE_AND_EXECUTE
       parameter ( GL_COMPILE_AND_EXECUTE = 4865 ) 	

C  ListNameType 
       integer*4   GL_BYTE
       parameter ( GL_BYTE = 5120 ) 	
       integer*4   GL_UNSIGNED_BYTE
       parameter ( GL_UNSIGNED_BYTE = 5121 ) 	
       integer*4   GL_SHORT
       parameter ( GL_SHORT = 5122 ) 	
       integer*4   GL_UNSIGNED_SHORT
       parameter ( GL_UNSIGNED_SHORT = 5123 ) 	
       integer*4   GL_INT
       parameter ( GL_INT = 5124 ) 	
       integer*4   GL_UNSIGNED_INT
       parameter ( GL_UNSIGNED_INT = 5125 ) 	
       integer*4   GL_FLOAT
       parameter ( GL_FLOAT = 5126 ) 	
       integer*4   GL_2_BYTES
       parameter ( GL_2_BYTES = 5127 ) 	
       integer*4   GL_3_BYTES
       parameter ( GL_3_BYTES = 5128 ) 	
       integer*4   GL_4_BYTES
       parameter ( GL_4_BYTES = 5129 ) 	

C  LogicOp 
       integer*4   GL_CLEAR
       parameter ( GL_CLEAR = 5376 ) 	
       integer*4   GL_AND
       parameter ( GL_AND = 5377 ) 	
       integer*4   GL_AND_REVERSE
       parameter ( GL_AND_REVERSE = 5378 ) 	
       integer*4   GL_COPY
       parameter ( GL_COPY = 5379 ) 	
       integer*4   GL_AND_INVERTED
       parameter ( GL_AND_INVERTED = 5380 ) 	
       integer*4   GL_NOOP
       parameter ( GL_NOOP = 5381 ) 	
       integer*4   GL_XOR
       parameter ( GL_XOR = 5382 ) 	
       integer*4   GL_OR
       parameter ( GL_OR = 5383 ) 	
       integer*4   GL_NOR
       parameter ( GL_NOR = 5384 ) 	
       integer*4   GL_EQUIV
       parameter ( GL_EQUIV = 5385 ) 	
       integer*4   GL_INVERT
       parameter ( GL_INVERT = 5386 ) 	
       integer*4   GL_OR_REVERSE
       parameter ( GL_OR_REVERSE = 5387 ) 	
       integer*4   GL_COPY_INVERTED
       parameter ( GL_COPY_INVERTED = 5388 ) 	
       integer*4   GL_OR_INVERTED
       parameter ( GL_OR_INVERTED = 5389 ) 	
       integer*4   GL_NAND
       parameter ( GL_NAND = 5390 ) 	
       integer*4   GL_SET
       parameter ( GL_SET = 5391 ) 	

C  MapTarget 
C       GL_MAP1_COLOR_4 
C       GL_MAP1_INDEX 
C       GL_MAP1_NORMAL 
C       GL_MAP1_TEXTURE_COORD_1 
C       GL_MAP1_TEXTURE_COORD_2 
C       GL_MAP1_TEXTURE_COORD_3 
C       GL_MAP1_TEXTURE_COORD_4 
C       GL_MAP1_VERTEX_3 
C       GL_MAP1_VERTEX_4 
C       GL_MAP2_COLOR_4 
C       GL_MAP2_INDEX 
C       GL_MAP2_NORMAL 
C       GL_MAP2_TEXTURE_COORD_1 
C       GL_MAP2_TEXTURE_COORD_2 
C       GL_MAP2_TEXTURE_COORD_3 
C       GL_MAP2_TEXTURE_COORD_4 
C       GL_MAP2_VERTEX_3 
C       GL_MAP2_VERTEX_4 

C  MaterialFace 
C       GL_FRONT 
C       GL_BACK 
C       GL_FRONT_AND_BACK 

C  MaterialParameter 
       integer*4   GL_EMISSION
       parameter ( GL_EMISSION = 5632 ) 	
       integer*4   GL_SHININESS
       parameter ( GL_SHININESS = 5633 ) 	
       integer*4   GL_AMBIENT_AND_DIFFUSE
       parameter ( GL_AMBIENT_AND_DIFFUSE = 5634 ) 	
       integer*4   GL_COLOR_INDEXES
       parameter ( GL_COLOR_INDEXES = 5635 ) 	
C       GL_AMBIENT 
C       GL_DIFFUSE 
C       GL_SPECULAR 

C  MatrixMode 
       integer*4   GL_MODELVIEW
       parameter ( GL_MODELVIEW = 5888 ) 	
       integer*4   GL_PROJECTION
       parameter ( GL_PROJECTION = 5889 ) 	
       integer*4   GL_TEXTURE
       parameter ( GL_TEXTURE = 5890 ) 	

C  MeshMode1 
C       GL_POINT 
C       GL_LINE 

C  MeshMode2 
C       GL_POINT 
C       GL_LINE 
C       GL_FILL 

C  MinmaxTarget 
C       GL_MINMAX_EXT 

C  PixelCopyType 
       integer*4   GL_COLOR
       parameter ( GL_COLOR = 6144 ) 	
       integer*4   GL_DEPTH
       parameter ( GL_DEPTH = 6145 ) 	
       integer*4   GL_STENCIL
       parameter ( GL_STENCIL = 6146 ) 	

C  PixelFormat 
       integer*4   GL_COLOR_INDEX
       parameter ( GL_COLOR_INDEX = 6400 ) 	
       integer*4   GL_STENCIL_INDEX
       parameter ( GL_STENCIL_INDEX = 6401 ) 	
       integer*4   GL_DEPTH_COMPONENT
       parameter ( GL_DEPTH_COMPONENT = 6402 ) 	
       integer*4   GL_RED
       parameter ( GL_RED = 6403 ) 	
       integer*4   GL_GREEN
       parameter ( GL_GREEN = 6404 ) 	
       integer*4   GL_BLUE
       parameter ( GL_BLUE = 6405 ) 	
       integer*4   GL_ALPHA
       parameter ( GL_ALPHA = 6406 ) 	
       integer*4   GL_RGB
       parameter ( GL_RGB = 6407 ) 	
       integer*4   GL_RGBA
       parameter ( GL_RGBA = 6408 ) 	
       integer*4   GL_LUMINANCE
       parameter ( GL_LUMINANCE = 6409 ) 	
       integer*4   GL_LUMINANCE_ALPHA
       parameter ( GL_LUMINANCE_ALPHA = 6410 ) 	
C       GL_ABGR_EXT 

C  PixelInternalFormat 
C       GL_ALPHA4_EXT 
C       GL_ALPHA8_EXT 
C       GL_ALPHA12_EXT 
C       GL_ALPHA16_EXT 
C       GL_LUMINANCE4_EXT 
C       GL_LUMINANCE8_EXT 
C       GL_LUMINANCE12_EXT 
C       GL_LUMINANCE16_EXT 
C       GL_LUMINANCE4_ALPHA4_EXT 
C       GL_LUMINANCE6_ALPHA2_EXT 
C       GL_LUMINANCE8_ALPHA8_EXT 
C       GL_LUMINANCE12_ALPHA4_EXT 
C       GL_LUMINANCE12_ALPHA12_EXT 
C       GL_LUMINANCE16_ALPHA16_EXT 
C       GL_INTENSITY_EXT 
C       GL_INTENSITY4_EXT 
C       GL_INTENSITY8_EXT 
C       GL_INTENSITY12_EXT 
C       GL_INTENSITY16_EXT 
C       GL_RGB2_EXT 
C       GL_RGB4_EXT 
C       GL_RGB5_EXT 
C       GL_RGB8_EXT 
C       GL_RGB10_EXT 
C       GL_RGB12_EXT 
C       GL_RGB16_EXT 
C       GL_RGBA2_EXT 
C       GL_RGBA4_EXT 
C       GL_RGB5_A1_EXT 
C       GL_RGBA8_EXT 
C       GL_RGB10_A2_EXT 
C       GL_RGBA12_EXT 
C       GL_RGBA16_EXT 

C  PixelMap 
C       GL_PIXEL_MAP_I_TO_I 
C       GL_PIXEL_MAP_S_TO_S 
C       GL_PIXEL_MAP_I_TO_R 
C       GL_PIXEL_MAP_I_TO_G 
C       GL_PIXEL_MAP_I_TO_B 
C       GL_PIXEL_MAP_I_TO_A 
C       GL_PIXEL_MAP_R_TO_R 
C       GL_PIXEL_MAP_G_TO_G 
C       GL_PIXEL_MAP_B_TO_B 
C       GL_PIXEL_MAP_A_TO_A 

C  PixelStore 
C       GL_UNPACK_SWAP_BYTES 
C       GL_UNPACK_LSB_FIRST 
C       GL_UNPACK_ROW_LENGTH 
C       GL_UNPACK_SKIP_ROWS 
C       GL_UNPACK_SKIP_PIXELS 
C       GL_UNPACK_ALIGNMENT 
C       GL_PACK_SWAP_BYTES 
C       GL_PACK_LSB_FIRST 
C       GL_PACK_ROW_LENGTH 
C       GL_PACK_SKIP_ROWS 
C       GL_PACK_SKIP_PIXELS 
C       GL_PACK_ALIGNMENT 
C       GL_PACK_SKIP_IMAGES_EXT 
C       GL_PACK_IMAGE_HEIGHT_EXT 
C       GL_UNPACK_SKIP_IMAGES_EXT 
C       GL_UNPACK_IMAGE_HEIGHT_EXT 

C  PixelTransfer 
C       GL_MAP_COLOR 
C       GL_MAP_STENCIL 
C       GL_INDEX_SHIFT 
C       GL_INDEX_OFFSET 
C       GL_RED_SCALE 
C       GL_RED_BIAS 
C       GL_GREEN_SCALE 
C       GL_GREEN_BIAS 
C       GL_BLUE_SCALE 
C       GL_BLUE_BIAS 
C       GL_ALPHA_SCALE 
C       GL_ALPHA_BIAS 
C       GL_DEPTH_SCALE 
C       GL_DEPTH_BIAS 
C       GL_POST_CONVOLUTION_RED_SCALE_EXT 
C       GL_POST_CONVOLUTION_GREEN_SCALE_EXT 
C       GL_POST_CONVOLUTION_BLUE_SCALE_EXT 
C       GL_POST_CONVOLUTION_ALPHA_SCALE_EXT 
C       GL_POST_CONVOLUTION_RED_BIAS_EXT 
C       GL_POST_CONVOLUTION_GREEN_BIAS_EXT 
C       GL_POST_CONVOLUTION_BLUE_BIAS_EXT 
C       GL_POST_CONVOLUTION_ALPHA_BIAS_EXT 

C  PixelType 
       integer*4   GL_BITMAP
       parameter ( GL_BITMAP = 6656 ) 	
C       GL_BYTE 
C       GL_UNSIGNED_BYTE 
C       GL_SHORT 
C       GL_UNSIGNED_SHORT 
C       GL_INT 
C       GL_UNSIGNED_INT 
C       GL_FLOAT 

C  PolygonMode 
       integer*4   GL_POINT
       parameter ( GL_POINT = 6912 ) 	
       integer*4   GL_LINE
       parameter ( GL_LINE = 6913 ) 	
       integer*4   GL_FILL
       parameter ( GL_FILL = 6914 ) 	

C  ReadBufferMode 
C       GL_FRONT_LEFT 
C       GL_FRONT_RIGHT 
C       GL_BACK_LEFT 
C       GL_BACK_RIGHT 
C       GL_FRONT 
C       GL_BACK 
C       GL_LEFT 
C       GL_RIGHT 
C       GL_AUX0 
C       GL_AUX1 
C       GL_AUX2 
C       GL_AUX3 

C  RenderingMode 
       integer*4   GL_RENDER
       parameter ( GL_RENDER = 7168 ) 	
       integer*4   GL_FEEDBACK
       parameter ( GL_FEEDBACK = 7169 ) 	
       integer*4   GL_SELECT
       parameter ( GL_SELECT = 7170 ) 	

C  SamplePattern 
C       GL_1PASS_SGIS 
C       GL_2PASS_0_SGIS 
C       GL_2PASS_1_SGIS 
C       GL_4PASS_0_SGIS 
C       GL_4PASS_1_SGIS 
C       GL_4PASS_2_SGIS 
C       GL_4PASS_3_SGIS 

C  SeparableTarget 
C       GL_SEPARABLE_2D_EXT 

C  ShadingModel 
       integer*4   GL_FLAT
       parameter ( GL_FLAT = 7424 ) 	
       integer*4   GL_SMOOTH
       parameter ( GL_SMOOTH = 7425 ) 	

C  StencilFunction 
C       GL_NEVER 
C       GL_LESS 
C       GL_EQUAL 
C       GL_LEQUAL 
C       GL_GREATER 
C       GL_NOTEQUAL 
C       GL_GEQUAL 
C       GL_ALWAYS 

C  StencilOp 
C       GL_ZERO 
       integer*4   GL_KEEP
       parameter ( GL_KEEP = 7680 ) 	
       integer*4   GL_REPLACE
       parameter ( GL_REPLACE = 7681 ) 	
       integer*4   GL_INCR
       parameter ( GL_INCR = 7682 ) 	
       integer*4   GL_DECR
       parameter ( GL_DECR = 7683 ) 	
C       GL_INVERT 

C  StringName 
       integer*4   GL_VENDOR
       parameter ( GL_VENDOR = 7936 ) 	
       integer*4   GL_RENDERER
       parameter ( GL_RENDERER = 7937 ) 	
       integer*4   GL_VERSION
       parameter ( GL_VERSION = 7938 ) 	
       integer*4   GL_EXTENSIONS
       parameter ( GL_EXTENSIONS = 7939 ) 	

C  TextureCoordName 
       integer*4   GL_S
       parameter ( GL_S = 8192 ) 	
       integer*4   GL_T
       parameter ( GL_T = 8193 ) 	
       integer*4   GL_R
       parameter ( GL_R = 8194 ) 	
       integer*4   GL_Q
       parameter ( GL_Q = 8195 ) 	

C  TextureEnvMode 
       integer*4   GL_MODULATE
       parameter ( GL_MODULATE = 8448 ) 	
       integer*4   GL_DECAL
       parameter ( GL_DECAL = 8449 ) 	
C       GL_BLEND 
C       GL_REPLACE_EXT 

C  TextureEnvParameter 
       integer*4   GL_TEXTURE_ENV_MODE
       parameter ( GL_TEXTURE_ENV_MODE = 8704 ) 	
       integer*4   GL_TEXTURE_ENV_COLOR
       parameter ( GL_TEXTURE_ENV_COLOR = 8705 ) 	

C  TextureEnvTarget 
       integer*4   GL_TEXTURE_ENV
       parameter ( GL_TEXTURE_ENV = 8960 ) 	

C  TextureGenMode 
       integer*4   GL_EYE_LINEAR
       parameter ( GL_EYE_LINEAR = 9216 ) 	
       integer*4   GL_OBJECT_LINEAR
       parameter ( GL_OBJECT_LINEAR = 9217 ) 	
       integer*4   GL_SPHERE_MAP
       parameter ( GL_SPHERE_MAP = 9218 ) 	

C  TextureGenParameter 
       integer*4   GL_TEXTURE_GEN_MODE
       parameter ( GL_TEXTURE_GEN_MODE = 9472 ) 	
       integer*4   GL_OBJECT_PLANE
       parameter ( GL_OBJECT_PLANE = 9473 ) 	
       integer*4   GL_EYE_PLANE
       parameter ( GL_EYE_PLANE = 9474 ) 	

C  TextureMagFilter 
       integer*4   GL_NEAREST
       parameter ( GL_NEAREST = 9728 ) 	
       integer*4   GL_LINEAR
       parameter ( GL_LINEAR = 9729 ) 	
C       GL_LINEAR_DETAIL_SGIS 
C       GL_LINEAR_DETAIL_ALPHA_SGIS 
C       GL_LINEAR_DETAIL_COLOR_SGIS 
C       GL_LINEAR_SHARPEN_SGIS 
C       GL_LINEAR_SHARPEN_ALPHA_SGIS 
C       GL_LINEAR_SHARPEN_COLOR_SGIS 

C  TextureMinFilter 
C       GL_NEAREST 
C       GL_LINEAR 
       integer*4   GL_NEAREST_MIPMAP_NEAREST
       parameter ( GL_NEAREST_MIPMAP_NEAREST = 9984 ) 	
       integer*4   GL_LINEAR_MIPMAP_NEAREST
       parameter ( GL_LINEAR_MIPMAP_NEAREST = 9985 ) 	
       integer*4   GL_NEAREST_MIPMAP_LINEAR
       parameter ( GL_NEAREST_MIPMAP_LINEAR = 9986 ) 	
       integer*4   GL_LINEAR_MIPMAP_LINEAR
       parameter ( GL_LINEAR_MIPMAP_LINEAR = 9987 ) 	

C  TextureParameterName 
       integer*4   GL_TEXTURE_MAG_FILTER
       parameter ( GL_TEXTURE_MAG_FILTER = 10240 ) 	
       integer*4   GL_TEXTURE_MIN_FILTER
       parameter ( GL_TEXTURE_MIN_FILTER = 10241 ) 	
       integer*4   GL_TEXTURE_WRAP_S
       parameter ( GL_TEXTURE_WRAP_S = 10242 ) 	
       integer*4   GL_TEXTURE_WRAP_T
       parameter ( GL_TEXTURE_WRAP_T = 10243 ) 	
C       GL_TEXTURE_BORDER_COLOR 
C       GL_TEXTURE_WRAP_R_EXT 
C       GL_DETAIL_TEXTURE_LEVEL_SGIS 
C       GL_DETAIL_TEXTURE_MODE_SGIS 

C  TextureTarget 
C       GL_TEXTURE_1D 
C       GL_TEXTURE_2D 
C       GL_PROXY_TEXTURE_1D_EXT 
C       GL_PROXY_TEXTURE_2D_EXT 
C       GL_TEXTURE_3D_EXT 
C       GL_PROXY_TEXTURE_3D_EXT 
C       GL_DETAIL_TEXTURE_2D_SGIS 

C  TextureWrapMode 
       integer*4   GL_CLAMP
       parameter ( GL_CLAMP = 10496 ) 	
       integer*4   GL_REPEAT
       parameter ( GL_REPEAT = 10497 ) 	

C  ClipPlaneName 
       integer*4   GL_CLIP_PLANE0
       parameter ( GL_CLIP_PLANE0 = 12288 ) 	
       integer*4   GL_CLIP_PLANE1
       parameter ( GL_CLIP_PLANE1 = 12289 ) 	
       integer*4   GL_CLIP_PLANE2
       parameter ( GL_CLIP_PLANE2 = 12290 ) 	
       integer*4   GL_CLIP_PLANE3
       parameter ( GL_CLIP_PLANE3 = 12291 ) 	
       integer*4   GL_CLIP_PLANE4
       parameter ( GL_CLIP_PLANE4 = 12292 ) 	
       integer*4   GL_CLIP_PLANE5
       parameter ( GL_CLIP_PLANE5 = 12293 ) 	

C  LightName 
       integer*4   GL_LIGHT0
       parameter ( GL_LIGHT0 = 16384 ) 	
       integer*4   GL_LIGHT1
       parameter ( GL_LIGHT1 = 16385 ) 	
       integer*4   GL_LIGHT2
       parameter ( GL_LIGHT2 = 16386 ) 	
       integer*4   GL_LIGHT3
       parameter ( GL_LIGHT3 = 16387 ) 	
       integer*4   GL_LIGHT4
       parameter ( GL_LIGHT4 = 16388 ) 	
       integer*4   GL_LIGHT5
       parameter ( GL_LIGHT5 = 16389 ) 	
       integer*4   GL_LIGHT6
       parameter ( GL_LIGHT6 = 16390 ) 	
       integer*4   GL_LIGHT7
       parameter ( GL_LIGHT7 = 16391 ) 	

C  Extensions 
       integer*4   GL_EXT_abgr
       parameter ( GL_EXT_abgr = 1 ) 	
       integer*4   GL_EXT_blend_color
       parameter ( GL_EXT_blend_color = 1 ) 	
       integer*4   GL_EXT_blend_logic_op
       parameter ( GL_EXT_blend_logic_op = 1 ) 	
       integer*4   GL_EXT_blend_minmax
       parameter ( GL_EXT_blend_minmax = 1 ) 	
       integer*4   GL_EXT_blend_subtract
       parameter ( GL_EXT_blend_subtract = 1 ) 	
       integer*4   GL_EXT_convolution
       parameter ( GL_EXT_convolution = 1 ) 	
       integer*4   GL_EXT_histogram
       parameter ( GL_EXT_histogram = 1 ) 	
       integer*4   GL_EXT_polygon_offset
       parameter ( GL_EXT_polygon_offset = 1 ) 	
       integer*4   GL_EXT_subtexture
       parameter ( GL_EXT_subtexture = 1 ) 	
       integer*4   GL_EXT_texture
       parameter ( GL_EXT_texture = 1 ) 	
       integer*4   GL_EXT_texture3D
       parameter ( GL_EXT_texture3D = 1 ) 	
       integer*4   GL_SGIS_detail_texture
       parameter ( GL_SGIS_detail_texture = 1 ) 	
       integer*4   GL_SGIS_multisample
       parameter ( GL_SGIS_multisample = 1 ) 	
       integer*4   GL_SGIS_sharpen_texture
       parameter ( GL_SGIS_sharpen_texture = 1 ) 	

C  EXT_abgr 
       integer*4   GL_ABGR_EXT
       parameter ( GL_ABGR_EXT = 32768 ) 	

C  EXT_blend_color 
       integer*4   GL_CONSTANT_COLOR_EXT
       parameter ( GL_CONSTANT_COLOR_EXT = 32769 ) 	
       integer*4   GL_ONE_MINUS_CONSTANT_COLOR_EXT
       parameter ( GL_ONE_MINUS_CONSTANT_COLOR_EXT = 32770 ) 	
       integer*4   GL_CONSTANT_ALPHA_EXT
       parameter ( GL_CONSTANT_ALPHA_EXT = 32771 ) 	
       integer*4   GL_ONE_MINUS_CONSTANT_ALPHA_EXT
       parameter ( GL_ONE_MINUS_CONSTANT_ALPHA_EXT = 32772 ) 	
       integer*4   GL_BLEND_COLOR_EXT
       parameter ( GL_BLEND_COLOR_EXT = 32773 ) 	

C  EXT_blend_minmax 
       integer*4   GL_FUNC_ADD_EXT
       parameter ( GL_FUNC_ADD_EXT = 32774 ) 	
       integer*4   GL_MIN_EXT
       parameter ( GL_MIN_EXT = 32775 ) 	
       integer*4   GL_MAX_EXT
       parameter ( GL_MAX_EXT = 32776 ) 	
       integer*4   GL_BLEND_EQUATION_EXT
       parameter ( GL_BLEND_EQUATION_EXT = 32777 ) 	

C  EXT_blend_subtract 
       integer*4   GL_FUNC_SUBTRACT_EXT
       parameter ( GL_FUNC_SUBTRACT_EXT = 32778 ) 	
       integer*4   GL_FUNC_REVERSE_SUBTRACT_EXT
       parameter ( GL_FUNC_REVERSE_SUBTRACT_EXT = 32779 ) 	

C  EXT_convolution 
       integer*4   GL_CONVOLUTION_1D_EXT
       parameter ( GL_CONVOLUTION_1D_EXT = 32784 ) 	
       integer*4   GL_CONVOLUTION_2D_EXT
       parameter ( GL_CONVOLUTION_2D_EXT = 32785 ) 	
       integer*4   GL_SEPARABLE_2D_EXT
       parameter ( GL_SEPARABLE_2D_EXT = 32786 ) 	
       integer*4   GL_CONVOLUTION_BORDER_MODE_EXT
       parameter ( GL_CONVOLUTION_BORDER_MODE_EXT = 32787 ) 	
       integer*4   GL_CONVOLUTION_FILTER_SCALE_EXT
       parameter ( GL_CONVOLUTION_FILTER_SCALE_EXT = 32788 ) 	
       integer*4   GL_CONVOLUTION_FILTER_BIAS_EXT
       parameter ( GL_CONVOLUTION_FILTER_BIAS_EXT = 32789 ) 	
       integer*4   GL_REDUCE_EXT
       parameter ( GL_REDUCE_EXT = 32790 ) 	
       integer*4   GL_CONVOLUTION_FORMAT_EXT
       parameter ( GL_CONVOLUTION_FORMAT_EXT = 32791 ) 	
       integer*4   GL_CONVOLUTION_WIDTH_EXT
       parameter ( GL_CONVOLUTION_WIDTH_EXT = 32792 ) 	
       integer*4   GL_CONVOLUTION_HEIGHT_EXT
       parameter ( GL_CONVOLUTION_HEIGHT_EXT = 32793 ) 	
       integer*4   GL_MAX_CONVOLUTION_WIDTH_EXT
       parameter ( GL_MAX_CONVOLUTION_WIDTH_EXT = 32794 ) 	
       integer*4   GL_MAX_CONVOLUTION_HEIGHT_EXT
       parameter ( GL_MAX_CONVOLUTION_HEIGHT_EXT = 32795 ) 	
       integer*4   GL_POST_CONVOLUTION_RED_SCALE_EX
       parameter ( GL_POST_CONVOLUTION_RED_SCALE_EX = 32796 ) 	
       integer*4   GL_POST_CONVOLUTION_GREEN_SCALE_
       parameter ( GL_POST_CONVOLUTION_GREEN_SCALE_ = 32797 ) 	
       integer*4   GL_POST_CONVOLUTION_BLUE_SCALE_E
       parameter ( GL_POST_CONVOLUTION_BLUE_SCALE_E = 32798 ) 	
       integer*4   GL_POST_CONVOLUTION_ALPHA_SCALE_
       parameter ( GL_POST_CONVOLUTION_ALPHA_SCALE_ = 32799 ) 	
       integer*4   GL_POST_CONVOLUTION_RED_BIAS_EXT
       parameter ( GL_POST_CONVOLUTION_RED_BIAS_EXT = 32800 ) 	
       integer*4   GL_POST_CONVOLUTION_GREEN_BIAS_E
       parameter ( GL_POST_CONVOLUTION_GREEN_BIAS_E = 32801 ) 	
       integer*4   GL_POST_CONVOLUTION_BLUE_BIAS_EX
       parameter ( GL_POST_CONVOLUTION_BLUE_BIAS_EX = 32802 ) 	
       integer*4   GL_POST_CONVOLUTION_ALPHA_BIAS_E
       parameter ( GL_POST_CONVOLUTION_ALPHA_BIAS_E = 32803 ) 	

C  EXT_histogram 
       integer*4   GL_HISTOGRAM_EXT
       parameter ( GL_HISTOGRAM_EXT = 32804 ) 	
       integer*4   GL_PROXY_HISTOGRAM_EXT
       parameter ( GL_PROXY_HISTOGRAM_EXT = 32805 ) 	
       integer*4   GL_HISTOGRAM_WIDTH_EXT
       parameter ( GL_HISTOGRAM_WIDTH_EXT = 32806 ) 	
       integer*4   GL_HISTOGRAM_FORMAT_EXT
       parameter ( GL_HISTOGRAM_FORMAT_EXT = 32807 ) 	
       integer*4   GL_HISTOGRAM_RED_SIZE_EXT
       parameter ( GL_HISTOGRAM_RED_SIZE_EXT = 32808 ) 	
       integer*4   GL_HISTOGRAM_GREEN_SIZE_EXT
       parameter ( GL_HISTOGRAM_GREEN_SIZE_EXT = 32809 ) 	
       integer*4   GL_HISTOGRAM_BLUE_SIZE_EXT
       parameter ( GL_HISTOGRAM_BLUE_SIZE_EXT = 32810 ) 	
       integer*4   GL_HISTOGRAM_ALPHA_SIZE_EXT
       parameter ( GL_HISTOGRAM_ALPHA_SIZE_EXT = 32811 ) 	
       integer*4   GL_HISTOGRAM_LUMINANCE_SIZE_EXT
       parameter ( GL_HISTOGRAM_LUMINANCE_SIZE_EXT = 32812 ) 	
       integer*4   GL_HISTOGRAM_SINK_EXT
       parameter ( GL_HISTOGRAM_SINK_EXT = 32813 ) 	
       integer*4   GL_MINMAX_EXT
       parameter ( GL_MINMAX_EXT = 32814 ) 	
       integer*4   GL_MINMAX_FORMAT_EXT
       parameter ( GL_MINMAX_FORMAT_EXT = 32815 ) 	
       integer*4   GL_MINMAX_SINK_EXT
       parameter ( GL_MINMAX_SINK_EXT = 32816 ) 	
       integer*4   GL_TABLE_TOO_LARGE_EXT
       parameter ( GL_TABLE_TOO_LARGE_EXT = 32817 ) 	

C  EXT_polygon_offset 
       integer*4   GL_POLYGON_OFFSET_EXT
       parameter ( GL_POLYGON_OFFSET_EXT = 32823 ) 	
       integer*4   GL_POLYGON_OFFSET_FACTOR_EXT
       parameter ( GL_POLYGON_OFFSET_FACTOR_EXT = 32824 ) 	
       integer*4   GL_POLYGON_OFFSET_BIAS_EXT
       parameter ( GL_POLYGON_OFFSET_BIAS_EXT = 32825 ) 	

C  EXT_texture 
       integer*4   GL_ALPHA4_EXT
       parameter ( GL_ALPHA4_EXT = 32827 ) 	
       integer*4   GL_ALPHA8_EXT
       parameter ( GL_ALPHA8_EXT = 32828 ) 	
       integer*4   GL_ALPHA12_EXT
       parameter ( GL_ALPHA12_EXT = 32829 ) 	
       integer*4   GL_ALPHA16_EXT
       parameter ( GL_ALPHA16_EXT = 32830 ) 	
       integer*4   GL_LUMINANCE4_EXT
       parameter ( GL_LUMINANCE4_EXT = 32831 ) 	
       integer*4   GL_LUMINANCE8_EXT
       parameter ( GL_LUMINANCE8_EXT = 32832 ) 	
       integer*4   GL_LUMINANCE12_EXT
       parameter ( GL_LUMINANCE12_EXT = 32833 ) 	
       integer*4   GL_LUMINANCE16_EXT
       parameter ( GL_LUMINANCE16_EXT = 32834 ) 	
       integer*4   GL_LUMINANCE4_ALPHA4_EXT
       parameter ( GL_LUMINANCE4_ALPHA4_EXT = 32835 ) 	
       integer*4   GL_LUMINANCE6_ALPHA2_EXT
       parameter ( GL_LUMINANCE6_ALPHA2_EXT = 32836 ) 	
       integer*4   GL_LUMINANCE8_ALPHA8_EXT
       parameter ( GL_LUMINANCE8_ALPHA8_EXT = 32837 ) 	
       integer*4   GL_LUMINANCE12_ALPHA4_EXT
       parameter ( GL_LUMINANCE12_ALPHA4_EXT = 32838 ) 	
       integer*4   GL_LUMINANCE12_ALPHA12_EXT
       parameter ( GL_LUMINANCE12_ALPHA12_EXT = 32839 ) 	
       integer*4   GL_LUMINANCE16_ALPHA16_EXT
       parameter ( GL_LUMINANCE16_ALPHA16_EXT = 32840 ) 	
       integer*4   GL_INTENSITY_EXT
       parameter ( GL_INTENSITY_EXT = 32841 ) 	
       integer*4   GL_INTENSITY4_EXT
       parameter ( GL_INTENSITY4_EXT = 32842 ) 	
       integer*4   GL_INTENSITY8_EXT
       parameter ( GL_INTENSITY8_EXT = 32843 ) 	
       integer*4   GL_INTENSITY12_EXT
       parameter ( GL_INTENSITY12_EXT = 32844 ) 	
       integer*4   GL_INTENSITY16_EXT
       parameter ( GL_INTENSITY16_EXT = 32845 ) 	
       integer*4   GL_RGB2_EXT
       parameter ( GL_RGB2_EXT = 32846 ) 	
       integer*4   GL_RGB4_EXT
       parameter ( GL_RGB4_EXT = 32847 ) 	
       integer*4   GL_RGB5_EXT
       parameter ( GL_RGB5_EXT = 32848 ) 	
       integer*4   GL_RGB8_EXT
       parameter ( GL_RGB8_EXT = 32849 ) 	
       integer*4   GL_RGB10_EXT
       parameter ( GL_RGB10_EXT = 32850 ) 	
       integer*4   GL_RGB12_EXT
       parameter ( GL_RGB12_EXT = 32851 ) 	
       integer*4   GL_RGB16_EXT
       parameter ( GL_RGB16_EXT = 32852 ) 	
       integer*4   GL_RGBA2_EXT
       parameter ( GL_RGBA2_EXT = 32853 ) 	
       integer*4   GL_RGBA4_EXT
       parameter ( GL_RGBA4_EXT = 32854 ) 	
       integer*4   GL_RGB5_A1_EXT
       parameter ( GL_RGB5_A1_EXT = 32855 ) 	
       integer*4   GL_RGBA8_EXT
       parameter ( GL_RGBA8_EXT = 32856 ) 	
       integer*4   GL_RGB10_A2_EXT
       parameter ( GL_RGB10_A2_EXT = 32857 ) 	
       integer*4   GL_RGBA12_EXT
       parameter ( GL_RGBA12_EXT = 32858 ) 	
       integer*4   GL_RGBA16_EXT
       parameter ( GL_RGBA16_EXT = 32859 ) 	
       integer*4   GL_TEXTURE_RED_SIZE_EXT
       parameter ( GL_TEXTURE_RED_SIZE_EXT = 32860 ) 	
       integer*4   GL_TEXTURE_GREEN_SIZE_EXT
       parameter ( GL_TEXTURE_GREEN_SIZE_EXT = 32861 ) 	
       integer*4   GL_TEXTURE_BLUE_SIZE_EXT
       parameter ( GL_TEXTURE_BLUE_SIZE_EXT = 32862 ) 	
       integer*4   GL_TEXTURE_ALPHA_SIZE_EXT
       parameter ( GL_TEXTURE_ALPHA_SIZE_EXT = 32863 ) 	
       integer*4   GL_TEXTURE_LUMINANCE_SIZE_EXT
       parameter ( GL_TEXTURE_LUMINANCE_SIZE_EXT = 32864 ) 	
       integer*4   GL_TEXTURE_INTENSITY_SIZE_EXT
       parameter ( GL_TEXTURE_INTENSITY_SIZE_EXT = 32865 ) 	
       integer*4   GL_REPLACE_EXT
       parameter ( GL_REPLACE_EXT = 32866 ) 	
       integer*4   GL_PROXY_TEXTURE_1D_EXT
       parameter ( GL_PROXY_TEXTURE_1D_EXT = 32867 ) 	
       integer*4   GL_PROXY_TEXTURE_2D_EXT
       parameter ( GL_PROXY_TEXTURE_2D_EXT = 32868 ) 	
       integer*4   GL_TEXTURE_TOO_LARGE_EXT
       parameter ( GL_TEXTURE_TOO_LARGE_EXT = 32869 ) 	

C  EXT_texture3D 
       integer*4   GL_PACK_SKIP_IMAGES_EXT
       parameter ( GL_PACK_SKIP_IMAGES_EXT = 32875 ) 	
       integer*4   GL_PACK_IMAGE_HEIGHT_EXT
       parameter ( GL_PACK_IMAGE_HEIGHT_EXT = 32876 ) 	
       integer*4   GL_UNPACK_SKIP_IMAGES_EXT
       parameter ( GL_UNPACK_SKIP_IMAGES_EXT = 32877 ) 	
       integer*4   GL_UNPACK_IMAGE_HEIGHT_EXT
       parameter ( GL_UNPACK_IMAGE_HEIGHT_EXT = 32878 ) 	
       integer*4   GL_TEXTURE_3D_EXT
       parameter ( GL_TEXTURE_3D_EXT = 32879 ) 	
       integer*4   GL_PROXY_TEXTURE_3D_EXT
       parameter ( GL_PROXY_TEXTURE_3D_EXT = 32880 ) 	
       integer*4   GL_TEXTURE_DEPTH_EXT
       parameter ( GL_TEXTURE_DEPTH_EXT = 32881 ) 	
       integer*4   GL_TEXTURE_WRAP_R_EXT
       parameter ( GL_TEXTURE_WRAP_R_EXT = 32882 ) 	
       integer*4   GL_MAX_3D_TEXTURE_SIZE_EXT
       parameter ( GL_MAX_3D_TEXTURE_SIZE_EXT = 32883 ) 	

C  SGIS_detail_texture 
       integer*4   GL_DETAIL_TEXTURE_2D_SGIS
       parameter ( GL_DETAIL_TEXTURE_2D_SGIS = 32917 ) 	
       integer*4   GL_DETAIL_TEXTURE_2D_BINDING_SGI
       parameter ( GL_DETAIL_TEXTURE_2D_BINDING_SGI = 32918 ) 	
       integer*4   GL_LINEAR_DETAIL_SGIS
       parameter ( GL_LINEAR_DETAIL_SGIS = 32919 ) 	
       integer*4   GL_LINEAR_DETAIL_ALPHA_SGIS
       parameter ( GL_LINEAR_DETAIL_ALPHA_SGIS = 32920 ) 	
       integer*4   GL_LINEAR_DETAIL_COLOR_SGIS
       parameter ( GL_LINEAR_DETAIL_COLOR_SGIS = 32921 ) 	
       integer*4   GL_DETAIL_TEXTURE_LEVEL_SGIS
       parameter ( GL_DETAIL_TEXTURE_LEVEL_SGIS = 32922 ) 	
       integer*4   GL_DETAIL_TEXTURE_MODE_SGIS
       parameter ( GL_DETAIL_TEXTURE_MODE_SGIS = 32923 ) 	
       integer*4   GL_DETAIL_TEXTURE_FUNC_POINTS_S
       parameter ( GL_DETAIL_TEXTURE_FUNC_POINTS_S = 32924 ) 	

C  SGIS_multisample 
       integer*4   GL_MULTISAMPLE_BIT_EXT
       parameter ( GL_MULTISAMPLE_BIT_EXT = 536870912 ) 	
       integer*4   GL_MULTISAMPLE_SGIS
       parameter ( GL_MULTISAMPLE_SGIS = 32925 ) 	
       integer*4   GL_SAMPLE_ALPHA_TO_MASK_SGIS
       parameter ( GL_SAMPLE_ALPHA_TO_MASK_SGIS = 32926 ) 	
       integer*4   GL_SAMPLE_ALPHA_TO_ONE_SGIS
       parameter ( GL_SAMPLE_ALPHA_TO_ONE_SGIS = 32927 ) 	
       integer*4   GL_SAMPLE_MASK_SGIS
       parameter ( GL_SAMPLE_MASK_SGIS = 32928 ) 	
       integer*4   GL_1PASS_SGIS
       parameter ( GL_1PASS_SGIS = 32929 ) 	
       integer*4   GL_2PASS_0_SGIS
       parameter ( GL_2PASS_0_SGIS = 32930 ) 	
       integer*4   GL_2PASS_1_SGIS
       parameter ( GL_2PASS_1_SGIS = 32931 ) 	
       integer*4   GL_4PASS_0_SGIS
       parameter ( GL_4PASS_0_SGIS = 32932 ) 	
       integer*4   GL_4PASS_1_SGIS
       parameter ( GL_4PASS_1_SGIS = 32933 ) 	
       integer*4   GL_4PASS_2_SGIS
       parameter ( GL_4PASS_2_SGIS = 32934 ) 	
       integer*4   GL_4PASS_3_SGIS
       parameter ( GL_4PASS_3_SGIS = 32935 ) 	
       integer*4   GL_SAMPLE_BUFFERS_SGIS
       parameter ( GL_SAMPLE_BUFFERS_SGIS = 32936 ) 	
       integer*4   GL_SAMPLES_SGIS
       parameter ( GL_SAMPLES_SGIS = 32937 ) 	
       integer*4   GL_SAMPLE_MASK_VALUE_SGIS
       parameter ( GL_SAMPLE_MASK_VALUE_SGIS = 32938 ) 	
       integer*4   GL_SAMPLE_MASK_INVERT_SGIS
       parameter ( GL_SAMPLE_MASK_INVERT_SGIS = 32939 ) 	
       integer*4   GL_SAMPLE_PATTERN_SGIS
       parameter ( GL_SAMPLE_PATTERN_SGIS = 32940 ) 	

C  SGIS_sharpen_texture 
       integer*4   GL_LINEAR_SHARPEN_SGIS
       parameter ( GL_LINEAR_SHARPEN_SGIS = 32941 ) 	
       integer*4   GL_LINEAR_SHARPEN_ALPHA_SGIS
       parameter ( GL_LINEAR_SHARPEN_ALPHA_SGIS = 32942 ) 	
       integer*4   GL_LINEAR_SHARPEN_COLOR_SGIS
       parameter ( GL_LINEAR_SHARPEN_COLOR_SGIS = 32943 ) 	
       integer*4   GL_SHARPEN_TEXTURE_FUNC_POINTS_S
       parameter ( GL_SHARPEN_TEXTURE_FUNC_POINTS_S = 32944 ) 	

C ***********************************************************


       character*128       fglGetString
       integer             fglGetError
       integer*4           fglRenderMode
       logical*1           fglAreTexturesResidentEXT
       logical*1           fglIsEnabled
       logical*1           fglIsList
       logical*1           fglIsTextureEXT
       logical*4           fglGenLists
       logical*4           fglGenTexturesEXT
