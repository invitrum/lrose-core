/* *=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=* */
/* ** Copyright UCAR (c) 1990 - 2016                                         */
/* ** University Corporation for Atmospheric Research (UCAR)                 */
/* ** National Center for Atmospheric Research (NCAR)                        */
/* ** Boulder, Colorado, USA                                                 */
/* ** BSD licence applies - redistribution and use in source and binary      */
/* ** forms, with or without modification, are permitted provided that       */
/* ** the following conditions are met:                                      */
/* ** 1) If the software is modified to produce derivative works,            */
/* ** such modified software should be clearly marked, so as not             */
/* ** to confuse it with the version available from UCAR.                    */
/* ** 2) Redistributions of source code must retain the above copyright      */
/* ** notice, this list of conditions and the following disclaimer.          */
/* ** 3) Redistributions in binary form must reproduce the above copyright   */
/* ** notice, this list of conditions and the following disclaimer in the    */
/* ** documentation and/or other materials provided with the distribution.   */
/* ** 4) Neither the name of UCAR nor the names of its contributors,         */
/* ** if any, may be used to endorse or promote products derived from        */
/* ** this software without specific prior written permission.               */
/* ** DISCLAIMER: THIS SOFTWARE IS PROVIDED "AS IS" AND WITHOUT ANY EXPRESS  */
/* ** OR IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED      */
/* ** WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.    */
/* *=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=* */
#ifndef	zoom_pu_HEADER
#define	zoom_pu_HEADER

/*
 * zoom_pu_ui.h - User interface object and function declarations.
 * This file was generated by `gxv' from `zoom_pu.G'.
 * DO NOT EDIT BY HAND.
 */

extern Attr_attribute	INSTANCE;


typedef struct {
	Xv_opaque	zoom_pu;
	Xv_opaque	controls1;
	Xv_opaque	domain_st;
} zoom_pu_zoom_pu_objects;

extern zoom_pu_zoom_pu_objects	*zoom_pu_zoom_pu_objects_initialize(zoom_pu_zoom_pu_objects *, Xv_opaque);

extern Xv_opaque	zoom_pu_zoom_pu_zoom_pu_create(zoom_pu_zoom_pu_objects *, Xv_opaque);
extern Xv_opaque	zoom_pu_zoom_pu_controls1_create(zoom_pu_zoom_pu_objects *, Xv_opaque);
extern Xv_opaque	zoom_pu_zoom_pu_domain_st_create(zoom_pu_zoom_pu_objects *, Xv_opaque);

typedef struct {
	Xv_opaque	bookmk_pu;
	Xv_opaque	controls2;
	Xv_opaque	bookmk_st;
} zoom_pu_bookmk_pu_objects;

extern zoom_pu_bookmk_pu_objects	*zoom_pu_bookmk_pu_objects_initialize(zoom_pu_bookmk_pu_objects *, Xv_opaque);

extern Xv_opaque	zoom_pu_bookmk_pu_bookmk_pu_create(zoom_pu_bookmk_pu_objects *, Xv_opaque);
extern Xv_opaque	zoom_pu_bookmk_pu_controls2_create(zoom_pu_bookmk_pu_objects *, Xv_opaque);
extern Xv_opaque	zoom_pu_bookmk_pu_bookmk_st_create(zoom_pu_bookmk_pu_objects *, Xv_opaque);
#endif
