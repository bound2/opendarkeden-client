#!/usr/bin/env perl
#----------------------------------------------------------------------
# count_identifier.pl - ratchet R13's counter
#----------------------------------------------------------------------
#
# Prints how many times an identifier matching the regular expression
# given as the first argument occurs, as a whole token, in the C++ files
# named on stdin, one per line. Comments and string literals are removed
# by the same tokenizer count_register.pl uses, one file at a time, and
# for the same reason: the ratchet script's joined-stream pipeline reads
# the blitters' `//*pDest = ...` line comments as block-comment openers
# and swallows code until the next `*/`, which can only hide a hit -
# acceptable for a count at zero only if nothing can hide there. An
# unreadable file is an error, not a skipped file, likewise.
#
# The pattern is anchored on both sides with identifier boundaries here,
# so `PLATFORM_MACOS` does not match `PLATFORM_MACOSX` and the caller
# does not have to remember to.
#
#   find ... -name '*.cpp' | perl count_identifier.pl '__LINUX__|_LINUX'
#----------------------------------------------------------------------
use strict;
use warnings;

my $pattern = shift @ARGV;
die "usage: count_identifier.pl PATTERN < files\n" unless defined $pattern;
my $re = qr/(?<![A-Za-z0-9_])(?:$pattern)(?![A-Za-z0-9_])/;

my @files;
while (my $line = <STDIN>) {
	$line =~ s/\r?\n\z//;
	push @files, $line if length $line;
}

my $n = 0;
for my $file (@files) {
	open my $in, '<:raw', $file or die "$file: $!\n";
	local $/;
	my $src = <$in>;
	close $in;
	my $code = '';
	while ($src =~ m{\G(?:
		(/\*.*?\*/)
	  | (//[^\n]*)
	  | ("(?:[^"\\\n]|\\.)*")
	  | ('(?:[^'\\\n]|\\.)*')
	  | ([^/"']+ | .)
	)}gsx) {
		$code .= defined $5 ? $5 : ' ';
	}
	$n++ while $code =~ /$re/g;
}
print "$n\n";
