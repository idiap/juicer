#
# Copyright 2007 by IDIAP Research Institute
#                   http://www.idiap.ch
#
# See the file COPYING for the licence associated with this software.
#

## Process this file with automake to produce Makefile.in

lib_LIBRARIES = libjuicer.a

libjuicer_a_SOURCES = \
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

BUILT_SOURCES = htkparse.y.h
AM_YFLAGS = -d -p htk
AM_LFLAGS = -Phtk -L
LEX_OUTPUT_ROOT = lex.htk

bin_PROGRAMS = juicer gramgen cdgen lexgen genwfstseqs

LDADD = libjuicer.a

juicer_SOURCES = juicer.cpp
gramgen_SOURCES = gramgen.cpp
cdgen_SOURCES = cdgen.cpp
lexgen_SOURCES = lexgen.cpp
genwfstseqs_SOURCES = genwfstseqs.cpp
