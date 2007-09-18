#!/usr/bin/perl

# Copyright (c) 2005, Darren Moore (IDIAP)
#
# This file is part of the Juicer decoding package.
#
# All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer. 
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution. 
# 3. Neither the name of the author nor the names of its contributors may be
#    used to endorse or promote products derived from this software without
#    specific prior written permission. 
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

if ( @ARGV != 1 )
{
   print "\nusage: do-star-closure.pl <FSM file>\n\n" ;
   print "  Outputs new FSM file to stdout.\n\n" ;
   print "  Does the *-closure by changing all arcs that go to final states to go to\n";
   print "    the initial state instead, removing the final state entries for the old\n";
   print "    final states and adding a final state entry for the initial state.\n\n";
   print "  Note that any final state weights present in the input FSM will be ignored,\n";
   print "    and will not be present in the output FSM\n\n";
   exit 1 ;
}

$FSM = $ARGV[0] ;

# 1st pass: note all final states in FSM file
%FINALSTATES = ();
open INF , $FSM or die "error opening $FSM for reading\n" ;
while ( $line = <INF> )
{
   next if $line =~ /^\s*$/ ;
   chomp $line ;

   @fields = split /\s+/ , $line ;
   if ( (@fields == 1) || (@fields == 2) )
   {
      if ( exists $FINALSTATES{$fields[0]} ) {
         print "duplicate final state found = $fields[0]\n\n";
         exit 1;
      }

      $FINALSTATES{$fields[0]} = 1;
   }
}
close INF ;

# 2nd pass: all arcs that go to a final state now go to initial state and
#           initial state becomes the final state as well.
open INF , $FSM or die "error opening $FSM for reading\n" ;
$initial = -1;
while ( $line = <INF> )
{
   next if $line =~ /^\s*$/ ;
   chomp $line ;

   @fields = split /\s+/ , $line ;
   if ( @fields >= 4 )
   {
      if ( $initial < 0 ) {
         # the initial state is the first 'from' state in the FSM file.
         $initial = $fields[0];
      }
      
      if ( exists $FINALSTATES{$fields[1]} ) {
         $fields[1] = $initial ;
      }

      $out = join ' ' , @fields ;
      print "$out\n" ;
   }
}
close INF ;

# print a final state entry for the initial state
if ( $initial < 0 ) {
   print "at end of FSM file, but initial state not determined\n\n";
   exit 1;
}
print "$initial\n";
