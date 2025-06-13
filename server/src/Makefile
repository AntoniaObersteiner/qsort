PKGDIR		?= ../..
L4DIR		?= $(PKGDIR)/../..

TARGET		= qsort
SRC_CC		= main.cc
REQUIRES_LIBS = l4util libstdc++ cxx_libc_io cxx_io

CXXFLAGS	= -O0 -fno-inline-small-functions -fno-inline -fno-optimize-sibling-calls

#LIBS     += -ldl

include $(L4DIR)/mk/prog.mk
