////////////////////////////////////////////////////////////////////////////////
// Plainamp, Open source Winamp core
// 
// Copyright © 2005  Sebastian Pipping <webmaster@hartwork.org>
// 
// -->  http://www.hartwork.org
// 
// This source code is released under the GNU General Public License (GPL).
// See GPL.txt for details. Any non-GPL usage is strictly forbidden.
////////////////////////////////////////////////////////////////////////////////


#include "Util.h"



#ifndef ICC_STANDARD_CLASSES
# define ICC_STANDARD_CLASSES 0x00004000
#endif



bool bLoaded     = false;
bool bAvailable  = false;



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool LoadCommonControls()
{
	if( bLoaded ) return bAvailable;
	
	INITCOMMONCONTROLSEX icce = {
		sizeof( INITCOMMONCONTROLSEX ),
		ICC_BAR_CLASSES |           // Statusbar, trackbar, toolbar
			ICC_COOL_CLASSES |      // Rebar 
			ICC_LISTVIEW_CLASSES |  // Listview
			ICC_STANDARD_CLASSES |  //
			ICC_TREEVIEW_CLASSES    // Treeview 
	};

	bLoaded     = true;
	bAvailable  = ( InitCommonControlsEx( &icce ) == TRUE );
	
	return bAvailable;
}
