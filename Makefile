#
# Copyright 2007 by IDIAP Research Institute
#                   http://www.idiap.ch
#
# See the file COPYING for the licence associated with this software.
#

TORCHDIR = $(HOME)/cvs/Torch3

BISON = /usr/bin/bison
FLEX = /usr/bin/flex

INC = -Isrc -I$(TORCHDIR)/core -I$(TORCHDIR)/speech
OPT = -O2 -g
DEF = -DMIRROR_SCORE0 -DLOCAL_HYPS
# MON = -pg

CC = g++
CPPFLAGS = $(INC) $(OPT) $(DEF) $(MON) -MD -Wall
CFLAGS = $(CPPFLAGS)

LDFLAGS = $(MON)
LDLIBS = -L$(TORCHDIR)/lib/Linux_OPT_FLOAT -ltorch

VPATH = ./src

OBJ = \
	WFSTDecoder.o \
	WFSTOnTheFlyDecoder.o \
	WFSTNetwork.o \
	WFSTModel.o \
	WFSTModelOnTheFly.o \
	WFSTLattice.o \
	WFSTLatticeOnTheFly.o \
	WFSTGramGen.o \
	WFSTCDGen.o \
	WFSTLexGen.o \
	WFSTHMMGen.o \
	MonophoneLookup.o \
	DecVocabulary.o \
	DecLexInfo.o \
	DecPhoneInfo.o \
	Models.o \
	DecoderBatchTest.o \
	DecoderSingleTest.o \
	DecHypHistPool.o \
	Histogram.o \
	BlockMemPool.o \
	DecodingHMM.o \
	htkparse.l.o \
	htkparse.y.o \
	DiskFile.o \
	LogFile.o \
	WordPairLM.o \
	ARPALM.o \
	string_stuff.o \

DEP = $(OBJ:.o=.d) juicer.d gramgen.d cdgen.d lexgen.d genwfstseqs.d

all: bin/juicer bin/gramgen bin/cdgen bin/lexgen bin/genwfstseqs

bin/juicer: juicer
	cp juicer bin

bin/gramgen: gramgen
	cp gramgen bin

bin/cdgen: cdgen
	cp cdgen bin

bin/lexgen: lexgen
	cp lexgen bin

bin/genwfstseqs: genwfstseqs
	cp genwfstseqs bin

clean:
	rm -f *.o *.d juicer gramgen cdgen lexgen hmmgen genwfstseqs

htkparse.y.cpp htkparse.y.h: htkparse.y
	${BISON} -o htkparse.y.cpp -p htk -d -l $<
	mv htkparse.y.hpp htkparse.y.h

htkparse.l.cpp: htkparse.l htkparse.y.h
	${FLEX} -ohtkparse.l.cpp -Phtk -L $<

# libjuicer.a: libjuicer.a($(OBJ))

juicer: $(OBJ) juicer.o

gramgen: $(OBJ) gramgen.o

cdgen: $(OBJ) cdgen.o

lexgen: $(OBJ) lexgen.o

hmmgen: $(OBJ) hmmgen.o

genwfstseqs: $(OBJ) genwfstseqs.o

-include $(DEP)
