
C  GLUT version of "GL/fgl.h"

C  Modifications from SGI IRIX 5.3 version:
C  1)  F prefix removed from GLU constants.
C  2)  Fix GLU_TRUE and GLU_FALSE.

C ***		Generic constants 		***

C  Errors: (return value 0 = no error) 
       integer*4   GLU_INVALID_ENUM
       parameter ( GLU_INVALID_ENUM = 100900 ) 	
       integer*4   GLU_INVALID_VALUE
       parameter ( GLU_INVALID_VALUE = 100901 ) 	
       integer*4   GLU_OUT_OF_MEMORY
       parameter ( GLU_OUT_OF_MEMORY = 100902 ) 	

C  For laughs: 
       integer*4   GLU_TRUE
       parameter ( GLU_TRUE = 1 ) 	
       integer*4   GLU_FALSE
       parameter ( GLU_FALSE = 0 ) 	


C *** 		Quadric constants 		***

C  Types of normals: 
       integer*4   GLU_SMOOTH
       parameter ( GLU_SMOOTH = 100000 ) 	
       integer*4   GLU_FLAT
       parameter ( GLU_FLAT = 100001 ) 	
       integer*4   GLU_NONE
       parameter ( GLU_NONE = 100002 ) 	

C  DrawStyle types: 
       integer*4   GLU_POINT
       parameter ( GLU_POINT = 100010 ) 	
       integer*4   GLU_LINE
       parameter ( GLU_LINE = 100011 ) 	
       integer*4   GLU_FILL
       parameter ( GLU_FILL = 100012 ) 	
       integer*4   GLU_SILHOUETTE
       parameter ( GLU_SILHOUETTE = 100013 ) 	

C  Orientation types: 
       integer*4   GLU_OUTSIDE
       parameter ( GLU_OUTSIDE = 100020 ) 	
       integer*4   GLU_INSIDE
       parameter ( GLU_INSIDE = 100021 ) 	

C  Callback types: 
C       GLU_ERROR		100103 


C ***	 	Tesselation constants 		***

C  Callback types: 
       integer*4   GLU_BEGIN
       parameter ( GLU_BEGIN = 100100 ) 	
       integer*4   GLU_VERTEX
       parameter ( GLU_VERTEX = 100101 ) 	
       integer*4   GLU_END
       parameter ( GLU_END = 100102 ) 	
       integer*4   GLU_ERROR
       parameter ( GLU_ERROR = 100103 ) 	
       integer*4   GLU_EDGE_FLAG
       parameter ( GLU_EDGE_FLAG = 100104 ) 	

C  Contours types: 
       integer*4   GLU_CW
       parameter ( GLU_CW = 100120 ) 	
       integer*4   GLU_CCW
       parameter ( GLU_CCW = 100121 ) 	
       integer*4   GLU_INTERIOR
       parameter ( GLU_INTERIOR = 100122 ) 	
       integer*4   GLU_EXTERIOR
       parameter ( GLU_EXTERIOR = 100123 ) 	
       integer*4   GLU_UNKNOWN
       parameter ( GLU_UNKNOWN = 100124 ) 	

       integer*4   GLU_TESS_ERROR1
       parameter ( GLU_TESS_ERROR1 = 100151 ) 	
       integer*4   GLU_TESS_ERROR2
       parameter ( GLU_TESS_ERROR2 = 100152 ) 	
       integer*4   GLU_TESS_ERROR3
       parameter ( GLU_TESS_ERROR3 = 100153 ) 	
       integer*4   GLU_TESS_ERROR4
       parameter ( GLU_TESS_ERROR4 = 100154 ) 	
       integer*4   GLU_TESS_ERROR5
       parameter ( GLU_TESS_ERROR5 = 100155 ) 	
       integer*4   GLU_TESS_ERROR6
       parameter ( GLU_TESS_ERROR6 = 100156 ) 	
       integer*4   GLU_TESS_ERROR7
       parameter ( GLU_TESS_ERROR7 = 100157 ) 	
       integer*4   GLU_TESS_ERROR8
       parameter ( GLU_TESS_ERROR8 = 100158 ) 	


C ***		NURBS constants			***

C  Properties: 
       integer*4   GLU_AUTO_LOAD_MATRIX
       parameter ( GLU_AUTO_LOAD_MATRIX = 100200 ) 	
       integer*4   GLU_CULLING
       parameter ( GLU_CULLING = 100201 ) 	
       integer*4   GLU_SAMPLING_TOLERANCE
       parameter ( GLU_SAMPLING_TOLERANCE = 100203 ) 	
       integer*4   GLU_DISPLAY_MODE
       parameter ( GLU_DISPLAY_MODE = 100204 ) 	

C  Trimming curve types 
       integer*4   GLU_MAP1_TRIM_2
       parameter ( GLU_MAP1_TRIM_2 = 100210 ) 	
       integer*4   GLU_MAP1_TRIM_3
       parameter ( GLU_MAP1_TRIM_3 = 100211 ) 	

C  Display modes: 
C       GLU_FILL 		100012 
       integer*4   GLU_OUTLINE_POLYGON
       parameter ( GLU_OUTLINE_POLYGON = 100240 ) 	
       integer*4   GLU_OUTLINE_PATCH
       parameter ( GLU_OUTLINE_PATCH = 100241 ) 	

C  Callbacks: 
C       GLU_ERROR		100103 

C  Errors: 
       integer*4   GLU_NURBS_ERROR1
       parameter ( GLU_NURBS_ERROR1 = 100251 ) 	
       integer*4   GLU_NURBS_ERROR2
       parameter ( GLU_NURBS_ERROR2 = 100252 ) 	
       integer*4   GLU_NURBS_ERROR3
       parameter ( GLU_NURBS_ERROR3 = 100253 ) 	
       integer*4   GLU_NURBS_ERROR4
       parameter ( GLU_NURBS_ERROR4 = 100254 ) 	
       integer*4   GLU_NURBS_ERROR5
       parameter ( GLU_NURBS_ERROR5 = 100255 ) 	
       integer*4   GLU_NURBS_ERROR6
       parameter ( GLU_NURBS_ERROR6 = 100256 ) 	
       integer*4   GLU_NURBS_ERROR7
       parameter ( GLU_NURBS_ERROR7 = 100257 ) 	
       integer*4   GLU_NURBS_ERROR8
       parameter ( GLU_NURBS_ERROR8 = 100258 ) 	
       integer*4   GLU_NURBS_ERROR9
       parameter ( GLU_NURBS_ERROR9 = 100259 ) 	
       integer*4   GLU_NURBS_ERROR10
       parameter ( GLU_NURBS_ERROR10 = 100260 ) 	
       integer*4   GLU_NURBS_ERROR11
       parameter ( GLU_NURBS_ERROR11 = 100261 ) 	
       integer*4   GLU_NURBS_ERROR12
       parameter ( GLU_NURBS_ERROR12 = 100262 ) 	
       integer*4   GLU_NURBS_ERROR13
       parameter ( GLU_NURBS_ERROR13 = 100263 ) 	
       integer*4   GLU_NURBS_ERROR14
       parameter ( GLU_NURBS_ERROR14 = 100264 ) 	
       integer*4   GLU_NURBS_ERROR15
       parameter ( GLU_NURBS_ERROR15 = 100265 ) 	
       integer*4   GLU_NURBS_ERROR16
       parameter ( GLU_NURBS_ERROR16 = 100266 ) 	
       integer*4   GLU_NURBS_ERROR17
       parameter ( GLU_NURBS_ERROR17 = 100267 ) 	
       integer*4   GLU_NURBS_ERROR18
       parameter ( GLU_NURBS_ERROR18 = 100268 ) 	
       integer*4   GLU_NURBS_ERROR19
       parameter ( GLU_NURBS_ERROR19 = 100269 ) 	
       integer*4   GLU_NURBS_ERROR20
       parameter ( GLU_NURBS_ERROR20 = 100270 ) 	
       integer*4   GLU_NURBS_ERROR21
       parameter ( GLU_NURBS_ERROR21 = 100271 ) 	
       integer*4   GLU_NURBS_ERROR22
       parameter ( GLU_NURBS_ERROR22 = 100272 ) 	
       integer*4   GLU_NURBS_ERROR23
       parameter ( GLU_NURBS_ERROR23 = 100273 ) 	
       integer*4   GLU_NURBS_ERROR24
       parameter ( GLU_NURBS_ERROR24 = 100274 ) 	
       integer*4   GLU_NURBS_ERROR25
       parameter ( GLU_NURBS_ERROR25 = 100275 ) 	
       integer*4   GLU_NURBS_ERROR26
       parameter ( GLU_NURBS_ERROR26 = 100276 ) 	
       integer*4   GLU_NURBS_ERROR27
       parameter ( GLU_NURBS_ERROR27 = 100277 ) 	
       integer*4   GLU_NURBS_ERROR28
       parameter ( GLU_NURBS_ERROR28 = 100278 ) 	
       integer*4   GLU_NURBS_ERROR29
       parameter ( GLU_NURBS_ERROR29 = 100279 ) 	
       integer*4   GLU_NURBS_ERROR30
       parameter ( GLU_NURBS_ERROR30 = 100280 ) 	
       integer*4   GLU_NURBS_ERROR31
       parameter ( GLU_NURBS_ERROR31 = 100281 ) 	
       integer*4   GLU_NURBS_ERROR32
       parameter ( GLU_NURBS_ERROR32 = 100282 ) 	
       integer*4   GLU_NURBS_ERROR33
       parameter ( GLU_NURBS_ERROR33 = 100283 ) 	
       integer*4   GLU_NURBS_ERROR34
       parameter ( GLU_NURBS_ERROR34 = 100284 ) 	
       integer*4   GLU_NURBS_ERROR35
       parameter ( GLU_NURBS_ERROR35 = 100285 ) 	
       integer*4   GLU_NURBS_ERROR36
       parameter ( GLU_NURBS_ERROR36 = 100286 ) 	
       integer*4   GLU_NURBS_ERROR37
       parameter ( GLU_NURBS_ERROR37 = 100287 ) 	


       character*128       fgluErrorString
       character*128       fgluGetString
       integer*4           fgluBuild1DMipmaps
       integer*4           fgluBuild2DMipmaps
       integer*4           fgluProject
       integer*4           fgluScaleImage
       integer*4           fgluUnProject
