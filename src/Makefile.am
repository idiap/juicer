#
# Copyright 2007 by IDIAP Research Institute
#                   http://www.idiap.ch
#
# Copyright 2008 by The University of Sheffield
#
# See the file COPYING for the licence associated with this software.
#

## Process this file with automake to produce Makefile.in

OPT = # add flags as you wish
OPT += -DOPTIMISE_FLATMODEL
OPT += -DOPTIMISE_ALIGN4
OPT += -DNO_BEST_END
OPT += -DNO_BEST_START

lib_LTLIBRARIES = libjuicer.la

JUICER_SOURCES = \
	WFSTDecoder.cpp \
	WFSTNetwork.cpp \
	WFSTModel.cpp \
	WFSTLattice.cpp \
	WFSTGramGen.cpp \
	WFSTCDGen.cpp \
	WFSTLexGen.cpp \
	WFSTHMMGen.cpp \
	MonophoneLookup.cpp \
	DecVocabulary.cpp \
	DecLexInfo.cpp \
	DecPhoneInfo.cpp \
	HTKModels.cpp \
	HTKFlatModels.cpp \
	DecoderBatchTest.cpp \
	DecoderSingleTest.cpp \
	DecHypHistPool.cpp \
	Histogram.cpp \
	BlockMemPool.cpp \
	LogFile.cpp \
	WordPairLM.cpp \
	ARPALM.cpp \
	string_stuff.cpp

# Ignore until somebody wants the on the fly decoder
#	WFSTModelOnTheFly.cpp
#	WFSTOnTheFlyDecoder.cpp
#	WFSTLatticeOnTheFly.cpp


# These sources have no corresponding header file
libjuicer_la_SOURCES = $(JUICER_SOURCES) \
	FrontEnd.h \
	htkparse.y.ypp \
	htkparse.l.lpp

# Whilst the interface is in flux
#	juicer_wrap.cxx

pkginclude_HEADERS = $(JUICER_SOURCES:.cpp=.h) \
	htkparse.h \
	WFSTGeneral.h

BUILT_SOURCES = htkparse.y.h
AM_YFLAGS = -d -p htk
AM_LFLAGS = -Phtk -L
LEX_OUTPUT_ROOT = lex.htk

bin_PROGRAMS = juicer gramgen cdgen lexgen genwfstseqs

libjuicer_la_CPPFLAGS = $(OPT) @TORCH_CFLAGS@ @TRACTER_CFLAGS@ @HTK_INCLUDES@
libjuicer_la_LIBADD = @TRACTER_LIBS@

LDADD = libjuicer.la @TORCH_LIBS@

if WITH_HTK
JUICER_SOURCES += HModels.cpp
endif

juicer_SOURCES = juicer.cpp
juicer_CPPFLAGS = $(OPT) @TORCH_CFLAGS@ @TRACTER_CFLAGS@ @HTK_INCLUDES@

# juicer_htk_SOURCES = juicer-htk.cpp
# juicer_htk_CPPFLAGS = @TORCH_CFLAGS@ @TRACTER_CFLAGS@ @HTK_INCLUDES@

gramgen_SOURCES = gramgen.cpp
gramgen_CPPFLAGS = $(OPT) @TORCH_CFLAGS@

cdgen_SOURCES = cdgen.cpp
cdgen_CPPFLAGS = $(OPT) @TORCH_CFLAGS@

lexgen_SOURCES = lexgen.cpp
lexgen_CPPFLAGS = $(OPT) @TORCH_CFLAGS@

genwfstseqs_SOURCES = genwfstseqs.cpp
genwfstseqs_CPPFLAGS = $(OPT) @TORCH_CFLAGS@
