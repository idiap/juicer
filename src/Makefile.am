#
# Copyright 2007 by IDIAP Research Institute
#                   http://www.idiap.ch
#
# See the file COPYING for the licence associated with this software.
#

## Process this file with automake to produce Makefile.in

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

bin_PROGRAMS = juicer gramgen cdgen lexgen genwfstseqs #juicer-htk

libjuicer_la_CPPFLAGS = @TORCH_CFLAGS@ @TRACTER_CFLAGS@ @HTK_INCLUDES@ -DMIRROR_SCORE0 -DOPTIMISE_FLATMODEL -DOPTIMISE_ALIGN4 -DOPTIMISE_TRANSBUFPOOL -DOPTIMISE_GETNEWMODEL -DOPTIMISE_INLINE_RESET_DECHYP -DOPTIMISE_TEEMODEL
libjuicer_la_LIBADD = @TRACTER_LIBS@

LDADD = libjuicer.la @TORCH_LIBS@

if WITH_HTK
JUICER_SOURCES += HModels.cpp
endif

juicer_SOURCES = juicer.cpp
juicer_CPPFLAGS = @TORCH_CFLAGS@ @TRACTER_CFLAGS@ @HTK_INCLUDES@

# juicer_htk_SOURCES = juicer-htk.cpp
# juicer_htk_CPPFLAGS = @TORCH_CFLAGS@ @TRACTER_CFLAGS@ @HTK_INCLUDES@

gramgen_SOURCES = gramgen.cpp
gramgen_CPPFLAGS = @TORCH_CFLAGS@

cdgen_SOURCES = cdgen.cpp
cdgen_CPPFLAGS = @TORCH_CFLAGS@

lexgen_SOURCES = lexgen.cpp
lexgen_CPPFLAGS = @TORCH_CFLAGS@

genwfstseqs_SOURCES = genwfstseqs.cpp
genwfstseqs_CPPFLAGS = @TORCH_CFLAGS@
