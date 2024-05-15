// HAYA'S EXPANSION PACK 2
//-----------------------------------------------------------------------------
// Copyright (C) 2024 by haya3218.

// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  hep2/h_general.h
/// \brief HEP2 global header

#ifndef __H_GENERAL_H__
#define __H_GENERAL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "../byteptr.h"
#include "../doomdef.h"
#include "../r_main.h"

void add_element(void **array, UINT8 *capacity, UINT8 typeSize, void *new_elem);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // __H_GENERAL_H__