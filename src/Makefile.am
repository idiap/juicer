#
# Copyright 2007 by IDIAP Research Institute
#                   http://www.idiap.ch
#
# See the file COPYING for the licence associated with this software.
#

## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = -DMIRROR_SCORE0 -DLOCAL_HYPS

lib_LTLIBRARIES = libjuicer.la

libjuicer_la_SOURCES = \
	htkparse.y.ypp \
	htkparse.l.lpp \
	WFSTDecoder.cpp \
	WFSTOnTheFlyDecoder.cpp \
	WFSTNetwork.cpp \
	WFSTModel.cpp \
	WFSTModelOnTheFly.cpp \
	WFSTLattice.cpp \
	WFSTLatticeOnTheFly.cpp \
	WFSTGramGen.cpp \
	WFSTCDGen.cpp \
	WFSTLexGen.cpp \
	WFSTHMMGen.cpp \
	MonophoneLookup.cpp \
	DecVocabulary.cpp \
	DecLexInfo.cpp \
	DecPhoneInfo.cpp \
	Models.cpp \
	DecoderBatchTest.cpp \
	DecoderSingleTest.cpp \
	DecHypHistPool.cpp \
	Histogram.cpp \
	BlockMemPool.cpp \
	DecodingHMM.cpp \
	DiskFile.cpp \
	LogFile.cpp \
	WordPairLM.cpp \
	ARPALM.cpp \
	string_stuff.cpp

# For SWIG
libjuicer_la_SOURCES += juicer_wrap.cxx

include_HEADERS = $(libjuicer_la_SOURCES:.cpp=.h) \
	htkparse.h \
	WFSTGeneral.h

BUILT_SOURCES = htkparse.y.h
AM_YFLAGS = -d -p htk
AM_LFLAGS = -Phtk -L
LEX_OUTPUT_ROOT = lex.htk

bin_PROGRAMS = juicer gramgen cdgen lexgen genwfstseqs

libjuicer_la_CPPFLAGS = @TORCH_CFLAGS@ @TRACTER_CFLAGS@
libjuicer_la_LIBADD = @TRACTER_LIBS@

LDADD = libjuicer.la @TORCH_LIBS@

juicer_SOURCES = juicer.cpp
juicer_CPPFLAGS = @TORCH_CFLAGS@ @TRACTER_CFLAGS@

gramgen_SOURCES = gramgen.cpp
gramgen_CPPFLAGS = @TORCH_CFLAGS@

cdgen_SOURCES = cdgen.cpp
cdgen_CPPFLAGS = @TORCH_CFLAGS@

lexgen_SOURCES = lexgen.cpp
lexgen_CPPFLAGS = @TORCH_CFLAGS@

genwfstseqs_SOURCES = genwfstseqs.cpp
genwfstseqs_CPPFLAGS = @TORCH_CFLAGS@
