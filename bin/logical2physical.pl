#!/usr/bin/perl -s
#
# Convert logical models in MMF file to physical models.
#
# Usage:
#        logical2physical.pl -mlist=xwrd.triphon.mlist MMF.ascii > MMF.ascii.new


while(<>) {
    last if (m/~h/);
    print;
}

do {
    if (m/~h "([a-z+-]+)"/) {
        $currentTriphone=$1;
    } else {
        $triphone{$currentTriphone} .= $_;
    }
} while(<>);

open( FID , $mlist);
while( <FID> ){
    chomp;
    @a = split;
    if ( $#a == 0 ) {
        print "~h \"$a[0]\"\n";
        print $triphone{$a[0]};
    } else {
        print "~h \"$a[0]\"\n";
        print $triphone{$a[1]};
    }
}
