// RINGRACERS-NOIRE
//-----------------------------------------------------------------------------
// Copyright (C) 2024 by hayaUnderscore/haya
// Copyright (C) 2024 by NepDisk
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------

#ifndef __N_CVAR__
#define __N_CVAR__

#ifdef __cplusplus
extern "C" {
#endif

//Player
extern consvar_t cv_colorizedhud;
extern consvar_t cv_colorizeditembox;
extern consvar_t cv_darkitembox;
extern consvar_t cv_colorizedhudcolor;
extern consvar_t cv_oldinputdisplay;
extern consvar_t cv_oldpositiondisplay;

void ColorHUD_OnChange(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
