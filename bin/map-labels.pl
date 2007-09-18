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

# Author: Darren Moore
# $Id: map-labels.pl,v 1.3 2005/10/20 01:19:22 moore Exp $

if ( @ARGV != 6 ) {
   
   print "\nusage: map-labels.pl <FSM1 (input)> <insyms for FSM1> <outsyms for FSM1> <FSM2 (output)> <insyms for FSM2> <outsyms for FSM2>\n\n";
   exit 1;

}

$FSM1 = $ARGV[0];
$INSYMS1 = $ARGV[1];
$OUTSYMS1 = $ARGV[2];
$FSM2 = $ARGV[3];
$INSYMS2 = $ARGV[4];
$OUTSYMS2 = $ARGV[5];

if ( -f $FSM2 ) {

   print "\nmap-labels.pl: output FSM file already exists - delete it then run this utility again\n\n";
   exit 1;
   
}


# Create a string-to-ID map for INSYMS1
#print "------ INPUT SYMBOLS MAPPING ------\n\n";
%INSYMS1WORDID = ();
open INF, $INSYMS1 or die "error opening $INSYMS1 for reading\n";
while ( $line = <INF> ) {

   next if $line =~ /^\s*$/;

   ( $word, $id ) = split /\s+/, $line;
   #print "$word $id\n";

   if ( exists $INSYMS1WORDID{$word} ) {
      print "duplicate entry detected in $INSYMS1 : $word\n";
      exit 1;
   }

   $INSYMS1WORDID{$word} = $id;
}
close INF;

# Create an ID1-to-ID2 map for INSYMS1 and INSYMS2
%INSYMSMAP = ();
$INSYMSMAP{0} = 0;
open INF, $INSYMS2 or die "error opening $INSYMS2 for reading\n";
while ( $line = <INF> ) {

   next if $line =~ /^\s*$/;

   ( $word, $id ) = split /\s+/, $line;

   next if ( $id == 0 );
   next if ( ! exists $INSYMS1WORDID{$word} );
   
   if ( exists $INSYMSMAP{$INSYMS1WORDID{$word}} ) {
      print "duplicate mapping detected for $word\n";
      exit 1;
   }

   $INSYMSMAP{$INSYMS1WORDID{$word}} = $id;

   #print "$word => $INSYMS1WORDID{$word} => $INSYMSMAP{$INSYMS1WORDID{$word}}\n";
}
close INF;

# Create a string-to-ID map for OUTSYMS1
#print "\n------ OUTPUT SYMBOLS MAPPING ------\n\n";
%OUTSYMS1WORDID = ();
open INF, $OUTSYMS1 or die "error opening $OUTSYMS1 for reading\n";
while ( $line = <INF> ) {

   next if $line =~ /^\s*$/;

   ( $word, $id ) = split /\s+/, $line;
   #print "$word $id\n";

   if ( exists $OUTSYMS1WORDID{$word} ) {
      print "duplicate entry detected in $OUTSYMS1 : $word\n";
      exit 1;
   }

   $OUTSYMS1WORDID{$word} = $id;
}
close INF;

# Create an ID1-to-ID2 map for OUTSYMS1 and OUTSYMS2
%OUTSYMSMAP = ();
$OUTSYMSMAP{0} = 0;
open INF, $OUTSYMS2 or die "error opening $OUTSYMS2 for reading\n";
while ( $line = <INF> ) {

   next if $line =~ /^\s*$/;

   ( $word, $id ) = split /\s+/, $line;

   next if ( $id == 0 );
   next if ( ! exists $OUTSYMS1WORDID{$word} );

   if ( exists $OUTSYMSMAP{$OUTSYMS1WORDID{$word}} ) {
      print "duplicate mapping detected for $word\n";
      exit 1;
   }

   $OUTSYMSMAP{$OUTSYMS1WORDID{$word}} = $id;

   #print "$word => $OUTSYMS1WORDID{$word} => $OUTSYMSMAP{$OUTSYMS1WORDID{$word}}\n";
}
close INF;


# Now translate the input FSM to the output FSM by applying the label mappings
open INF, $FSM1 or die "error opening $FSM1 for reading\n";
open OUTF, ">$FSM2" or die "error opening $FSM2 for writing\n";
$linecnt=0;
while ( $line = <INF> ) {
   
   $linecnt++;
   next if $line =~ /^\s*$/;

   @fields = split /\s+/ , $line;

   if ( (@fields == 1) || (@fields == 2) ) {
   
      # final state entry
      print OUTF "$line";
      
   } elsif ( (@fields == 4) || (@fields == 5) ) {

      if ( ! exists $INSYMSMAP{$fields[2]} ) {
         print "no mapping found for input symbol on line $linecnt of $FSM1\n\n";
         exit 1;
      }

      if ( ! exists $OUTSYMSMAP{$fields[3]} ) {
         print "no mapping found for output symbol on line $linecnt of $FSM1\n\n";
         exit 1;
      }

      print OUTF "$fields[0] $fields[1] $INSYMSMAP{$fields[2]} $OUTSYMSMAP{$fields[3]}";
      if ( @fields == 5 ) {
         print OUTF " $fields[4]\n";
      } else {
         print OUTF "\n";
      }
      
   } else {
   
      print "incorrect number of fields detected on line $linecnt of $FSM1\n\n";
      exit 1;

   }
}
close INF;
close OUTF;

