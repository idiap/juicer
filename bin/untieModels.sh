#!/bin/bash
#
# Converts tied (logical) models in MMF to untied (physical) models.
#
# Usage:
#       untieModels.sh <MMF file> <tied triphone list> <output directory>
#
#
# Copyright 2009 by The University of Sheffield
#
# See the file COPYING for the licence associated with this software.
#

HHED=HHEd

INMMF=$1
INMLIST=$2
OUTDIR=$3

OUTMMF=${OUTDIR}/${INMMF##*/}
OUTMLIST=${OUTDIR}/${INMLIST##*/}

## Convert logical triphones to physical triphones by duplicating the appropriate ~h macros in the MMF file.
logical2physical.pl -mlist=${INMLIST} ${INMMF} > ${OUTMMF}

## Sort the models into the correct order.
## The ~h macros in the MMF file must be consistent with the insyms ordering of an existing context dependency FSM.
## To do this we sort the complete list of triphone names alphabetically.
## Then use HTK's HHEd to sort the macros in the MMF.  
## We use HHEd so that the resulting MMF is compatible with HModels.
export LC_ALL=C
cut -f 1 -d ' ' ${INMLIST} | sort > ${OUTMLIST}
$HHED -H ${OUTMMF} -w ${OUTMMF}.tmp /dev/null ${OUTMLIST} 

## Generate new tied list with trihpones in the same order as the newly generated MMF file
## Then use HTK's HHEd to sort the macros in the MMF again.  
grep ~h ${OUTMMF}.tmp | perl -pe 's:~h\s+::;s:"::g;' > ${OUTMLIST}
$HHED -H ${OUTMMF}.tmp -w ${OUTMMF} /dev/null ${OUTMLIST} 

rm ${OUTMMF}.tmp
