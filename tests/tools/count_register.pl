#!/usr/bin/env perl
#----------------------------------------------------------------------
# count_register.pl - ratchet R11's counter
#----------------------------------------------------------------------
#
# Prints the number of `register` storage-class specifiers in the C++
# files named on the command line, or one per line on stdin when there
# are none (a list of every C++ file in the tree is longer than xargs
# will pass to one process, and a count split across two prints two
# numbers): the keyword followed by a type or an identifier, or written
# after the type as `int register i`.
#
# Comments and string literals are removed by a tokenizer, one file at a
# time, rather than by the ratchet script's joined-stream pipeline. That
# pipeline strips /* */ before //, so a line comment such as
# `//*pDest = ...` - the blitters have dozens - opens a block comment
# that swallows code until the next `*/`, across files if need be; over
# the 23 files on master it saw 538 of the 627 declarations R11 was
# recorded against, and the figure moves with the order the files are
# joined in. It does not strip string literals either, so the NPC script
# lines that say "register as a couple" would count. A swallowed region
# can only hide a hit, never invent one, which is only acceptable for a
# count at zero if nothing can hide there. An unreadable file is an
# error, not a skipped file, for the same reason.
#----------------------------------------------------------------------
use strict;
use warnings;

my @files = @ARGV;
if (!@files) {
	while (my $line = <STDIN>) {
		$line =~ s/\r?\n\z//;
		push @files, $line if length $line;
	}
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
	$n++ while $code =~ /
		(?<![A-Za-z0-9_]) register \s+ (?:(?:const|unsigned|signed)\s+)? (?:::)?[A-Za-z_][A-Za-z0-9_:]* \s* [*&]* \s* [A-Za-z_*]
	  | (?<![A-Za-z0-9_]) [A-Za-z_][A-Za-z0-9_:]* \s+ register \s+ [A-Za-z_]
	/gx;
}
print "$n\n";
