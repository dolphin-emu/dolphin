/**
 *	@file
 *	@brief Motion plus extension
 */

#ifndef MOTION_PLUS_H_INCLUDED
#define MOTION_PLUS_H_INCLUDED

#include "wiiuse_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

void motion_plus_disconnected(struct motion_plus_t* mp);

void motion_plus_event(struct motion_plus_t* mp, byte* msg);

#ifdef __cplusplus
}
#endif

#endif