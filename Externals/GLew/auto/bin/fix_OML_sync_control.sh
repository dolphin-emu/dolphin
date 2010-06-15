#!/bin/sh
##
## Copyright (C) 2002-2008, Marcelo E. Magallon <mmagallo[]debian org>
## Copyright (C) 2002-2008, Milan Ikits <milan ikits[]ieee org>
##
## This program is distributed under the terms and conditions of the GNU
## General Public License Version 2 as published by the Free Software
## Foundation or, at your option, any later version.

perl -e 's/#ifndef GLX_OML_sync_control/#if !defined(GLX_OML_sync_control) \&\& defined(__STDC_VERSION__) \&\& (__STDC_VERSION__ >= 199901L)\n#include <inttypes.h>/;' -pi $1
perl -e 's/#ifdef GLX_OML_sync_control/#if defined(GLX_OML_sync_control) \&\& defined(__STDC_VERSION__) \&\& (__STDC_VERSION__ >= 199901L)\n#include <inttypes.h>/;' -pi $1
perl -e 's/(extern PFNGLXGETMSCRATEOMLPROC __glewXGetMscRateOML;)/#ifdef GLX_OML_sync_control\n\1/' -pi $1
perl -e 's/(extern PFNGLXWAITFORSBCOMLPROC __glewXWaitForSbcOML;)/\1\n#endif/' -pi $1
perl -e 's/(extern GLboolean __GLXEW_OML_sync_control;)/#ifdef GLX_OML_sync_control\n\1\n#endif/' -pi $1
perl -e 's/(PFNGLXGETMSCRATEOMLPROC __glewXGetMscRateOML = NULL;)/#ifdef GLX_OML_sync_control\n\1/' -pi $1
perl -e 's/(PFNGLXWAITFORSBCOMLPROC __glewXWaitForSbcOML = NULL;)/\1\n#endif/' -pi $1
perl -e 's/(GLboolean __GLXEW_OML_sync_control = GL_FALSE;)/#ifdef GLX_OML_sync_control\n\1\n#endif/' -pi $1
rm -f $1.bak
