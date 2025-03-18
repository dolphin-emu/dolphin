/* $Id: upnperrors.h,v 1.8 2025/02/08 23:15:17 nanard Exp $ */
/* (c) 2007-2025 Thomas Bernard
 * All rights reserved.
 * MiniUPnP Project.
 * http://miniupnp.free.fr/ or https://miniupnp.tuxfamily.org/
 * This software is subjet to the conditions detailed in the
 * provided LICENCE file. */
#ifndef UPNPERRORS_H_INCLUDED
#define UPNPERRORS_H_INCLUDED

/*! \file upnperrors.h
 * \brief code to string API for errors
 */
#include "miniupnpc_declspec.h"

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * \brief convert error code to string
 *
 * Work for both MiniUPnPc specific errors and UPnP standard defined
 * errors.
 *
 * \param[in] err numerical error code
 * \return a string description of the error code
 *         or NULL for undefinded errors
 */
MINIUPNP_LIBSPEC const char * strupnperror(int err);

#ifdef __cplusplus
}
#endif

#endif
