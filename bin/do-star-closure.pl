#!/usr/bin/perl
#
# Copyright 2005 by IDIAP Research Institute
#                   http://www.idiap.ch
#
# See the file COPYING for the licence associated with this software.
#

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
