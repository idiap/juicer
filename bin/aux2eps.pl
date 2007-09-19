#!/usr/bin/perl
#
# Copyright 2005 by IDIAP Research Institute
#                   http://www.idiap.ch
#
# See the file COPYING for the licence associated with this software.
#

if ( @ARGV != 2 )
{
   print "\nusage: aux2eps.pl <FSM file> <input symbols file>\n\n" ;
   print "  Outputs new FSM file to stdout\n\n" ;
   exit 1 ;
}

$FSM = $ARGV[0] ;
$INSYMS = $ARGV[1] ;

# read in the input symbols file and note the auxiliary symbols and the epsilon symbol
$EPSID = -1 ;
%AUXSYMS = () ;
open INF , $INSYMS or die "error opening $INSYMS for reading" ;
while ( $line = <INF> )
{
   next if $line =~ /^\s*$/ ;
   chomp $line ;
   
   ( $str , $num ) = split /\s+/ , $line ;
   $tmp = substr $str , 0 , 1 ;
   if ( $str eq "<eps>" )
   {
      $EPSID = $num ;
   }   
   elsif ( $tmp eq "#" )
   {
      #print "$str $num\n" ;
      if ( exists $AUXSYMS{$num} )
      {
         print "duplicate auxiliary symbol found, $num\n" ;
         exit 1 ;
      }
      $AUXSYMS{$num} = $str ;
   }
}
close INF ;

if ( $EPSID < 0 )
{
   print "epsilon symbol not found in symbols file\n" ;
   exit 1 ;
}
if ( ! %AUXSYMS )
{
   print "no aux syms in symbol file - nothing to do\n" ;
   exit 0 ;
}

# now process the input FSM file - changing all input aux syms to epsilons
open INF , $FSM or die "error opening $FSM for reading\n" ;
while ( $line = <INF> )
{
   next if $line =~ /^\s*$/ ;
   chomp $line ;

   @fields = split /\s+/ , $line ;
   if ( @fields >= 4 )
   {
      if ( exists $AUXSYMS{$fields[2]} )
      {
         $fields[2] = $EPSID ;
      }
   }

   $out = join ' ' , @fields ;
   print "$out\n" ;
}
close INF ;
