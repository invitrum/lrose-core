# *=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=* 
# ** Copyright UCAR (c) 1990 - 2016                                         
# ** University Corporation for Atmospheric Research (UCAR)                 
# ** National Center for Atmospheric Research (NCAR)                        
# ** Boulder, Colorado, USA                                                 
# ** BSD licence applies - redistribution and use in source and binary      
# ** forms, with or without modification, are permitted provided that       
# ** the following conditions are met:                                      
# ** 1) If the software is modified to produce derivative works,            
# ** such modified software should be clearly marked, so as not             
# ** to confuse it with the version available from UCAR.                    
# ** 2) Redistributions of source code must retain the above copyright      
# ** notice, this list of conditions and the following disclaimer.          
# ** 3) Redistributions in binary form must reproduce the above copyright   
# ** notice, this list of conditions and the following disclaimer in the    
# ** documentation and/or other materials provided with the distribution.   
# ** 4) Neither the name of UCAR nor the names of its contributors,         
# ** if any, may be used to endorse or promote products derived from        
# ** this software without specific prior written permission.               
# ** DISCLAIMER: THIS SOFTWARE IS PROVIDED "AS IS" AND WITHOUT ANY EXPRESS  
# ** OR IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED      
# ** WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.    
# *=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=* 
###########################################################################
#
# Makefile for StormModel program
#
# Mike Dixon, RAP, NCAR, Boulder, Co, USA, 80307
#
# February 1998
#
###########################################################################

include $(RAP_MAKE_INC_DIR)/rap_make_macros

LOC_INCLUDES = $(NETCDF4_INCS)

LOC_LIBS = -lrapmath -lMdv -lRadx -lNcxx -lrapformats \
	-ltoolsa -ldataport -ltdrp $(NETCDF4_LIBS) \
	-lbz2 -lz -lm

LOC_LDFLAGS = $(NETCDF4_LDFLAGS)

LOC_CFLAGS =

MODULE_TYPE = progcpp

TARGET_FILE = StormModel

HDRS = \
	Area.h \
	Args.h \
	Duration.h \
	Generate.h \
	GenFromCdf.h \
	MidPoint.h \
	Params.h \
	StartTime.h \
	StormModel.h \
	Velocity.h

C_SRCS = $(TDRP_C)

CPPC_SRCS = \
	Area.cc \
	Args.cc \
	Duration.cc \
	Generate.cc \
	GenFromCdf.cc \
	Main.cc \
	MidPoint.cc \
	Params.cc \
	StartTime.cc \
	StormModel.cc \
	Velocity.cc

#
# tdrp support
#

include $(RAP_MAKE_INC_DIR)/rap_make_tdrp_macros

#
# general targets
#

include $(RAP_MAKE_INC_DIR)/rap_make_c++_targets

#
# local targets
#

depend: $(TDRP_C) depend_generic

include $(RAP_MAKE_INC_DIR)/rap_make_tdrp_targets

# DO NOT DELETE THIS LINE -- make depend depends on it.

