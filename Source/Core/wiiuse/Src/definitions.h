/*
 *	wiiuse
 *
 *	Written By:
 *		Michael Laforest	< para >
 *		Email: < thepara (--AT--) g m a i l [--DOT--] com >
 *
 *	Copyright 2006-2007
 *
 *	This file is part of wiiuse.
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 3 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *	$Header$
 *
 */

/**
 *	@file
 *	@brief General definitions.
 */

#ifndef DEFINITIONS_H_INCLUDED
#define DEFINITIONS_H_INCLUDED

#include <Common.h>

#ifndef _WIN32
#include <arpa/inet.h>					/* htons() */
#else
/* disable warnings I don't care about */
#pragma warning(disable:4244)           /* possible loss of data conversion     */
#pragma warning(disable:4273)           /* inconsistent dll linkage                     */
#pragma warning(disable:4217)

#endif // _WIN32

/* Convert to big endian */
#define BIG_ENDIAN_LONG(i)				(htonl(i))
#define BIG_ENDIAN_SHORT(i)				(htons(i))

#endif // DEFINITIONS_H_INCLUDED
