
C  Copyright (c) Mark J. Kilgard, 1994.

C  This program is freely distributable without licensing fees
C  and is provided without guarantee or warrantee expressed or
C  implied.  This program is -not- in the public domain.

C  GLUT Fortran header file

C  display mode bit masks
	integer*4 GLUT_RGB
	parameter ( GLUT_RGB = 0 )
	integer*4 GLUT_RGBA
	parameter ( GLUT_RGBA = 0 )
	integer*4 GLUT_INDEX
	parameter ( GLUT_INDEX = 1 )
	integer*4 GLUT_SINGLE
	parameter ( GLUT_SINGLE = 0 )
	integer*4 GLUT_DOUBLE
	parameter ( GLUT_DOUBLE = 2 )
	integer*4 GLUT_ACCUM
	parameter ( GLUT_ACCUM = 4 )
	integer*4 GLUT_ALPHA
	parameter ( GLUT_ALPHA = 8 )
	integer*4 GLUT_DEPTH
	parameter ( GLUT_DEPTH = 16 )
	integer*4 GLUT_STENCIL
	parameter ( GLUT_STENCIL = 32 )
	integer*4 GLUT_MULTISAMPLE
	parameter ( GLUT_MULTISAMPLE = 128 )
	integer*4 GLUT_STEREO
	parameter ( GLUT_STEREO = 256 )

C  mouse buttons
	integer*4 GLUT_LEFT_BUTTON
	parameter ( GLUT_LEFT_BUTTON = 0 )
	integer*4 GLUT_MIDDLE_BUTTON
	parameter ( GLUT_MIDDLE_BUTTON = 1 )
	integer*4 GLUT_RIGHT_BUTTON
	parameter ( GLUT_RIGHT_BUTTON = 2 )

C  mouse button callback state
	integer*4 GLUT_DOWN
	parameter ( GLUT_DOWN = 0 )
	integer*4 GLUT_UP
	parameter ( GLUT_UP = 1 )

C  special key callback values
	integer*4 GLUT_KEY_F1
	parameter ( GLUT_KEY_F1 = 1 )
	integer*4 GLUT_KEY_F2
	parameter ( GLUT_KEY_F2 = 2 )
	integer*4 GLUT_KEY_F3
	parameter ( GLUT_KEY_F3 = 3 )
	integer*4 GLUT_KEY_F4
	parameter ( GLUT_KEY_F4 = 4 )
	integer*4 GLUT_KEY_F5
	parameter ( GLUT_KEY_F5 = 5 )
	integer*4 GLUT_KEY_F6
	parameter ( GLUT_KEY_F6 = 6 )
	integer*4 GLUT_KEY_F7
	parameter ( GLUT_KEY_F7 = 7 )
	integer*4 GLUT_KEY_F8
	parameter ( GLUT_KEY_F8 = 8 )
	integer*4 GLUT_KEY_F9
	parameter ( GLUT_KEY_F9 = 9 )
	integer*4 GLUT_KEY_F10
	parameter ( GLUT_KEY_F10 = 10 )
	integer*4 GLUT_KEY_F11
	parameter ( GLUT_KEY_F11 = 11 )
	integer*4 GLUT_KEY_F12
	parameter ( GLUT_KEY_F12 = 12 )
	integer*4 GLUT_KEY_LEFT
	parameter ( GLUT_KEY_LEFT = 100 )
	integer*4 GLUT_KEY_UP
	parameter ( GLUT_KEY_UP = 101 )
	integer*4 GLUT_KEY_RIGHT
	parameter ( GLUT_KEY_RIGHT = 102 )
	integer*4 GLUT_KEY_DOWN
	parameter ( GLUT_KEY_DOWN = 103 )
	integer*4 GLUT_KEY_PAGE_UP
	parameter ( GLUT_KEY_PAGE_UP = 104 )
	integer*4 GLUT_KEY_PAGE_DOWN
	parameter ( GLUT_KEY_PAGE_DOWN = 105 )
	integer*4 GLUT_KEY_HOME
	parameter ( GLUT_KEY_HOME = 106 )
	integer*4 GLUT_KEY_END
	parameter ( GLUT_KEY_END = 107 )
	integer*4 GLUT_KEY_INSERT
	parameter ( GLUT_KEY_INSERT = 108 )
	
C  entry/exit callback state
	integer*4 GLUT_LEFT
	parameter ( GLUT_LEFT = 0 )
	integer*4 GLUT_ENTERED
	parameter ( GLUT_ENTERED = 1 )

C  menu usage callback state
	integer*4 GLUT_MENU_NOT_IN_USE
	parameter ( GLUT_MENU_NOT_IN_USE = 0 )
	integer*4 GLUT_MENU_IN_USE
	parameter ( GLUT_MENU_IN_USE = 1 )

C  visibility callback state
	integer*4 GLUT_NOT_VISIBLE
	parameter ( GLUT_NOT_VISIBLE = 0 )
	integer*4 GLUT_VISIBLE
	parameter ( GLUT_VISIBLE = 1 )

C  color index component selection values
	integer*4 GLUT_RED
	parameter ( GLUT_RED = 0 )
	integer*4 GLUT_GREEN
	parameter ( GLUT_GREEN = 1 )
	integer*4 GLUT_BLUE
	parameter ( GLUT_BLUE = 2 )

C  XXX Unfortunately, SGI's Fortran compiler links with
C  EXTERNAL data even if it is not used.  This defeats
C  the purpose of GLUT naming fonts via opaque symbols.
C  This means GLUT Fortran programmers should explicitly
C  declared EXTERNAL GLUT fonts in subroutines where
C  the fonts are used.

C  stroke font opaque names
C       external GLUT_STROKE_ROMAN
C       external GLUT_STROKE_MONO_ROMAN

C  bitmap font opaque names
C       external GLUT_BITMAP_9_BY_15
C       external GLUT_BITMAP_8_BY_13
C       external GLUT_BITMAP_TIMES_ROMAN_10
C       external GLUT_BITMAP_TIMES_ROMAN_24
C       external GLUT_BITMAP_HELVETICA_10
C       external GLUT_BITMAP_HELVETICA_12
C       external GLUT_BITMAP_HELVETICA_18

C  glutGet parameters
	integer*4 GLUT_WINDOW_X
	parameter ( GLUT_WINDOW_X = 100 )
	integer*4 GLUT_WINDOW_Y
	parameter ( GLUT_WINDOW_Y = 101 )
	integer*4 GLUT_WINDOW_WIDTH
	parameter ( GLUT_WINDOW_WIDTH = 102 )
	integer*4 GLUT_WINDOW_HEIGHT
	parameter ( GLUT_WINDOW_HEIGHT = 103 )
	integer*4 GLUT_WINDOW_BUFFER_SIZE
	parameter ( GLUT_WINDOW_BUFFER_SIZE = 104 )
	integer*4 GLUT_WINDOW_STENCIL_SIZE
	parameter ( GLUT_WINDOW_STENCIL_SIZE = 105 )
	integer*4 GLUT_WINDOW_DEPTH_SIZE
	parameter ( GLUT_WINDOW_DEPTH_SIZE = 106 )
	integer*4 GLUT_WINDOW_RED_SIZE
	parameter ( GLUT_WINDOW_RED_SIZE = 107 )
	integer*4 GLUT_WINDOW_GREEN_SIZE
	parameter ( GLUT_WINDOW_GREEN_SIZE = 108 )
	integer*4 GLUT_WINDOW_BLUE_SIZE
	parameter ( GLUT_WINDOW_BLUE_SIZE = 109 )
	integer*4 GLUT_WINDOW_ALPHA_SIZE
	parameter ( GLUT_WINDOW_ALPHA_SIZE = 110 )
	integer*4 GLUT_WINDOW_ACCUM_RED_SIZE
	parameter ( GLUT_WINDOW_ACCUM_RED_SIZE = 111 )
	integer*4 GLUT_WINDOW_ACCUM_GREEN_SIZE
	parameter ( GLUT_WINDOW_ACCUM_GREEN_SIZE = 112 )
	integer*4 GLUT_WINDOW_ACCUM_BLUE_SIZE
	parameter ( GLUT_WINDOW_ACCUM_BLUE_SIZE = 113 )
	integer*4 GLUT_WINDOW_ACCUM_ALPHA_SIZE
	parameter ( GLUT_WINDOW_ACCUM_ALPHA_SIZE = 114 )
	integer*4 GLUT_WINDOW_DOUBLEBUFFER
	parameter ( GLUT_WINDOW_DOUBLEBUFFER = 115 )
	integer*4 GLUT_WINDOW_RGBA
	parameter ( GLUT_WINDOW_RGBA = 116 )
	integer*4 GLUT_WINDOW_PARENT
	parameter ( GLUT_WINDOW_PARENT = 117 )
	integer*4 GLUT_WINDOW_NUM_CHILDREN
	parameter ( GLUT_WINDOW_NUM_CHILDREN = 118 )
	integer*4 GLUT_WINDOW_COLORMAP_SIZE
	parameter ( GLUT_WINDOW_COLORMAP_SIZE = 119 )
	integer*4 GLUT_WINDOW_NUM_SAMPLES
	parameter ( GLUT_WINDOW_NUM_SAMPLES = 120 )
	integer*4 GLUT_WINDOW_STEREO
	parameter ( GLUT_WINDOW_STEREO = 121 )
	integer*4 GLUT_WINDOW_CURSOR
	parameter ( GLUT_WINDOW_CURSOR = 122 )
	integer*4 GLUT_SCREEN_WIDTH
	parameter ( GLUT_SCREEN_WIDTH = 200 )
	integer*4 GLUT_SCREEN_HEIGHT
	parameter ( GLUT_SCREEN_HEIGHT = 201 )
	integer*4 GLUT_SCREEN_WIDTH_MM
	parameter ( GLUT_SCREEN_WIDTH_MM = 202 )
	integer*4 GLUT_SCREEN_HEIGHT_MM
	parameter ( GLUT_SCREEN_HEIGHT_MM = 203 )
	integer*4 GLUT_MENU_NUM_ITEMS
	parameter ( GLUT_MENU_NUM_ITEMS = 300 )
	integer*4 GLUT_DISPLAY_MODE_POSSIBLE
	parameter ( GLUT_DISPLAY_MODE_POSSIBLE = 400 )
	integer*4 GLUT_INIT_WINDOW_X
	parameter ( GLUT_INIT_WINDOW_X = 500 )
	integer*4 GLUT_INIT_WINDOW_Y
	parameter ( GLUT_INIT_WINDOW_Y = 501 )
	integer*4 GLUT_INIT_WINDOW_WIDTH
	parameter ( GLUT_INIT_WINDOW_WIDTH = 502 )
	integer*4 GLUT_INIT_WINDOW_HEIGHT
	parameter ( GLUT_INIT_WINDOW_HEIGHT = 503 )
	integer*4 GLUT_INIT_DISPLAY_MODE
	parameter ( GLUT_INIT_DISPLAY_MODE = 504 )
	integer*4 GLUT_ELAPSED_TIME
	parameter ( GLUT_ELAPSED_TIME = 700 )

C  glutDeviceGet parameters
	integer*4 GLUT_HAS_KEYBOARD
	parameter ( GLUT_HAS_KEYBOARD = 600 )
	integer*4 GLUT_HAS_MOUSE
	parameter ( GLUT_HAS_MOUSE = 601 )
	integer*4 GLUT_HAS_SPACEBALL
	parameter ( GLUT_HAS_SPACEBALL = 602 )
	integer*4 GLUT_HAS_DIAL_AND_BUTTON_BOX
	parameter ( GLUT_HAS_DIAL_AND_BUTTON_BOX = 603 )
	integer*4 GLUT_HAS_TABLET
	parameter ( GLUT_HAS_TABLET = 604 )
	integer*4 GLUT_NUM_MOUSE_BUTTONS
	parameter ( GLUT_NUM_MOUSE_BUTTONS = 605 )
	integer*4 GLUT_NUM_SPACEBALL_BUTTONS
	parameter ( GLUT_NUM_SPACEBALL_BUTTONS = 606 )
	integer*4 GLUT_NUM_BUTTON_BOX_BUTTONS
	parameter ( GLUT_NUM_BUTTON_BOX_BUTTONS = 607 )
	integer*4 GLUT_NUM_DIALS
	parameter ( GLUT_NUM_DIALS = 608 )
	integer*4 GLUT_NUM_TABLET_BUTTONS
	parameter ( GLUT_NUM_TABLET_BUTTONS = 609 )

C  glutLayerGet parameters
	integer*4 GLUT_OVERLAY_POSSIBLE
	parameter ( GLUT_OVERLAY_POSSIBLE = 800 )
	integer*4 GLUT_LAYER_IN_USE
	parameter ( GLUT_LAYER_IN_USE = 801 )
	integer*4 GLUT_HAS_OVERLAY
	parameter ( GLUT_HAS_OVERLAY = 802 )
	integer*4 GLUT_TRANSPARENT_INDEX
	parameter ( GLUT_TRANSPARENT_INDEX = 803 )
	integer*4 GLUT_NORMAL_DAMAGED
	parameter ( GLUT_NORMAL_DAMAGED = 804 )
	integer*4 GLUT_OVERLAY_DAMAGED
	parameter ( GLUT_OVERLAY_DAMAGED = 805 )

C  glutUseLayer parameters
	integer*4 GLUT_NORMAL
	parameter ( GLUT_NORMAL = 0 )
	integer*4 GLUT_OVERLAY
	parameter ( GLUT_OVERLAY = 1 )

C  glutGetModifiers return mask
	integer*4 GLUT_ACTIVE_SHIFT
	parameter ( GLUT_ACTIVE_SHIFT = 1 )
	integer*4 GLUT_ACTIVE_CTRL
	parameter ( GLUT_ACTIVE_CTRL = 2 )
	integer*4 GLUT_ACTIVE_ALT
	parameter ( GLUT_ACTIVE_ALT = 4 )

C  glutSetCursor parameters
	integer*4 GLUT_CURSOR_RIGHT_ARROW
	parameter ( GLUT_CURSOR_RIGHT_ARROW = 0 )
	integer*4 GLUT_CURSOR_LEFT_ARROW
	parameter ( GLUT_CURSOR_LEFT_ARROW = 1 )
	integer*4 GLUT_CURSOR_INFO
	parameter ( GLUT_CURSOR_INFO = 2 )
	integer*4 GLUT_CURSOR_DESTROY
	parameter ( GLUT_CURSOR_DESTROY = 3 )
	integer*4 GLUT_CURSOR_HELP
	parameter ( GLUT_CURSOR_HELP = 4 )
	integer*4 GLUT_CURSOR_CYCLE
	parameter ( GLUT_CURSOR_CYCLE = 5 )
	integer*4 GLUT_CURSOR_SPRAY
	parameter ( GLUT_CURSOR_SPRAY = 6 )
	integer*4 GLUT_CURSOR_WAIT
	parameter ( GLUT_CURSOR_WAIT = 7 )
	integer*4 GLUT_CURSOR_TEXT
	parameter ( GLUT_CURSOR_TEXT = 8 )
	integer*4 GLUT_CURSOR_CROSSHAIR
	parameter ( GLUT_CURSOR_CROSSHAIR = 9 )
	integer*4 GLUT_CURSOR_UP_DOWN
	parameter ( GLUT_CURSOR_UP_DOWN = 10 )
	integer*4 GLUT_CURSOR_LEFT_RIGHT
	parameter ( GLUT_CURSOR_LEFT_RIGHT = 11 )
	integer*4 GLUT_CURSOR_TOP_SIDE
	parameter ( GLUT_CURSOR_TOP_SIDE = 12 )
	integer*4 GLUT_CURSOR_BOTTOM_SIDE
	parameter ( GLUT_CURSOR_BOTTOM_SIDE = 13 )
	integer*4 GLUT_CURSOR_LEFT_SIDE
	parameter ( GLUT_CURSOR_LEFT_SIDE = 14 )
	integer*4 GLUT_CURSOR_RIGHT_SIDE
	parameter ( GLUT_CURSOR_RIGHT_SIDE = 15 )
	integer*4 GLUT_CURSOR_TOP_LEFT_CORNER
	parameter ( GLUT_CURSOR_TOP_LEFT_CORNER = 16 )
	integer*4 GLUT_CURSOR_TOP_RIGHT_CORNER
	parameter ( GLUT_CURSOR_TOP_RIGHT_CORNER = 17 )
	integer*4 GLUT_CURSOR_BOTTOM_RIGHT_CORNER
	parameter ( GLUT_CURSOR_BOTTOM_RIGHT_CORNER = 18 )
	integer*4 GLUT_CURSOR_BOTTOM_LEFT_CORNER
	parameter ( GLUT_CURSOR_BOTTOM_LEFT_CORNER = 19 )
	integer*4 GLUT_CURSOR_INHERIT
	parameter ( GLUT_CURSOR_INHERIT = 100 )
	integer*4 GLUT_CURSOR_NONE
	parameter ( GLUT_CURSOR_NONE = 101 )
	integer*4 GLUT_CURSOR_FULL_CROSSHAIR
	parameter ( GLUT_CURSOR_FULL_CROSSHAIR = 102 )

C  GLUT functions
	integer*4 glutcreatewindow
	integer*4 glutcreatesubwindow
	integer*4 glutgetwindow
	integer*4 glutcreatemenu
	integer*4 glutgetmenu
	real glutgetcolor
	integer*4 glutget
	integer*4 glutdeviceget
	integer*4 glutextensionsupported

C  GLUT NULL name
	external glutnull

