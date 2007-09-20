#!/usr/bin/perl -w
#
# Copyright 2007 by IDIAP Research Institute
#                   http://www.idiap.ch
#
# See the file COPYING for the licence associated with this software.
#

use strict;

my $file1 = shift;
my $file2 = shift;

open(F1, "$file1") || die "$file1: $!";
open(F2, ">$file2") || die "$file2: $!";

##
# Loop over the input file, replace auxiliaries with epsilon
#
while (<F1>)
{
    # Require space before word to avoid first line (#FSTBasic)
    s/\s+\#\w+/ ,/og;
    print F2;
}
