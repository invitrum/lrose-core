###########################################################################
#
# Makefile for RadiaFilt
#
###########################################################################

include $(RAP_MAKE_INC_DIR)/rap_make_macros

MODULE_TYPE=progcpp
TARGET_FILE=RadxModelQc
MY_CPPCHECKFLAGS = -E Params.cc -E Params.hh
LOC_INC_DIR = .

LOC_INCLUDES = $(NETCDF4_INCS) -I.
LOC_LIBS = -lNcxx -lradar -lRadx -lNcxx -lSpdb -ldsserver \
	-lrapformats -lrapmath -ldidss -ltoolsa \
	-ltdrp -ldataport \
	$(NETCDF4_LIBS) \
	-lbz2 -lz -lpthread
LOC_LDFLAGS = $(NETCDF4_LDFLAGS)

CPPC_SRCS = \
	Params.cc \
	Parms.cc \
	Main.cc \
	RadxModelQc.cc \
	AzGradientStateSpecialData.cc \
	AzGradientFilter.cc \
	Clutter2dQualFilter.cc \
	FIRFilter.cc \
	QscaleFilter.cc \
	RayData1.cc \
	Special0Filter.cc \
	Special1Filter.cc \
	Variance1dFilter.cc \
	Volume.cc

# RayUnaryFilter.cc \

include $(RAP_MAKE_INC_DIR)/rap_make_tdrp_macros

#
# Standard c++ targets
#
include $(RAP_MAKE_INC_DIR)/rap_make_c++_targets

#
# tdrp targets
#

include $(RAP_MAKE_INC_DIR)/rap_make_tdrp_c++_targets

#
# local targets
#

depend: depend_generic

# DO NOT DELETE THIS LINE -- make depend depends on it.

