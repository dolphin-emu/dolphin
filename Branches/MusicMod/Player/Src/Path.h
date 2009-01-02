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


#ifndef PA_PATH_H
#define PA_PATH_H



#ifndef PLAINAMP_TESTING
# include "Global.h"
#else
# include "../Testing/GlobalTest.h"
#endif



bool ApplyRootToFilename( TCHAR * szRootpath, TCHAR * szFilename );
bool UnbloatFilename( TCHAR * szFullpath, bool bFixTooDeep );



#endif // PA_PATH_H
