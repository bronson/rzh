#!/usr/bin/perl -w

# Scott Bronson
# 3 Nov 2004

# quick 'n dirty script to generate a random sequence of bytes.
# can also check a sequence to ensure it's correct.
# gad, it's not so quick but it's still dirty.  This should be
# rewritten in C.

# Usage:
# --seed=12     sets seed for random number generator
# --size=47     sets the amount of data (either expected or generated)
# --check       will check the data on stdin instead of generating it
# If you don't supply --check, it will generate random data on stdout.


use strict;

use Getopt::Long ();

my $check = 0;
my $size = 1024;

# we don't actually want random numbers -- we want them to be
# totally reproducible.
srand(1);

exit(1) unless Getopt::Long::GetOptions(
	"check"		=> \$check,
	"size|s=i"	=> \$size,
	"seed|r=i"	=> sub { srand($_[1]); }
	);

if($check) {
	my($buf, $cnt);
	my $total = 0;

	binmode(STDIN);
	while($total <= $size && ($cnt = read(STDIN, $buf, 1))) {
		die "Could not read: $!\n" unless defined $cnt;

		if($cnt) {
			die "Error: byte $total doesn't match.\n"
				if unpack("C", $buf) != int rand(256);
		} else {
			last;
		}
		$total += $cnt;
	}

	die "Too much input: expected only $size bytes.\n" if $total > $size;
	die "Too little input: expected $size but only got $total bytes.\n" if $total < $size;

} else {

	binmode(STDOUT);
	for(my $i=0; $i < $size; $i++) {
		print pack("C", rand(256));
	}
}
