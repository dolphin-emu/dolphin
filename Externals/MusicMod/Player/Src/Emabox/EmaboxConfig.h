/*//////////////////////////////////////////////////////////////////////////////
// ExtraMessageBox
// 
// Copyright © 2006  Sebastian Pipping <webmaster@hartwork.org>
// 
// -->  http://www.hartwork.org
// 
// This source code is released under the GNU General Public License (GPL).
// See GPL.txt for details. Any non-GPL usage is strictly forbidden.
//////////////////////////////////////////////////////////////////////////////*/


#ifndef EXTRA_MESSAGE_BOX_CONFIG_H
#define EXTRA_MESSAGE_BOX_CONFIG_H 1



/* Allow laziness */
#define EMA_AUTOINIT

/* Allow overwriting message text */
#ifndef EMA_TEXT_NEVER_AGAIN
# define EMA_TEXT_NEVER_AGAIN szNeverAgain
#endif

#ifndef EMA_TEXT_REMEMBER_CHOICE
# define EMA_TEXT_REMEMBER_CHOICE szRememberChoice
#endif



#endif /* EXTRA_MESSAGE_BOX_CONFIG_H */
