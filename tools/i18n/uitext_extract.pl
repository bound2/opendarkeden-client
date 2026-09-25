#!/usr/bin/perl
#-----------------------------------------------------------------------------
# uitext_extract.pl - list the Korean lines of the packed UI text
#-----------------------------------------------------------------------------
# The client's item, skill, help, book, tutorial, progress and title text
# ships in CP949 inside password-protected archives under Data/Ui/txt
# (Item.rpk, Skill.rpk, Help.rpk, Book.rpk, TutorialEtc.rpk, progress.rpk,
# title.rpk). Unpack them first, one directory per archive:
#
#	unrar x -pdarkeden Data/Ui/txt/Help.rpk unpacked/Help/
#
# and point this script at the parent directory. It writes one row per
# distinct line that is not ASCII (Korean text, and a few symbol-only lines):
#
#	archive/member <TAB> line
#
# Tabs and backslashes inside a line are written as \t and \\ so a row stays
# one line. uitext_apply.pl rebuilds the members from this dump and the
# translation beside it. The one line the client parses as markup and that
# carries Korean, the "[==Level 조건표==]" marker of the help format, is
# translated to "[==Level Table==]", which MHelpMessageManager accepts too.
#
#	perl uitext_extract.pl <unpacked dir> <uitext.ko.tsv>
#-----------------------------------------------------------------------------

use strict;
use warnings;
use Encode ();
use File::Find ();

my ($root, $outPath) = @ARGV;
die "usage: $0 <unpacked dir> <uitext.ko.tsv>\n" unless defined $root && defined $outPath;
$root =~ s{[/\\]\z}{};

my @files;
File::Find::find({ wanted => sub { push @files, $File::Find::name if -f $_ }, no_chdir => 1 }, $root);

my (%seen, @rows, $undecodable);
for my $path (sort @files) {
	(my $rel = $path) =~ s{^\Q$root\E[/\\]}{};
	$rel =~ s{\\}{/}g;
	open my $in, '<:raw', $path or die "$path: $!\n";
	local $/;
	my $bytes = <$in>;
	close $in;
	next if $bytes =~ /[\x00-\x08\x0E-\x1F]/;    # a binary member (SlayerPortal.inf)
	my $text = Encode::decode('cp949', $bytes, sub { $undecodable++; sprintf '\\x%02X', shift });
	for my $line (split /\r?\n/, $text) {
		# Every line that is not ASCII: Korean, and the few symbol-only lines.
		next unless $line =~ /[^\x00-\x7F]/;
		next if $seen{$line}++;
		(my $escaped = $line) =~ s/\\/\\\\/g;
		$escaped =~ s/\t/\\t/g;
		push @rows, "$rel\t$escaped";
	}
}

open my $out, '>:raw:encoding(UTF-8)', $outPath or die "$outPath: $!\n";
print $out "$_\n" for @rows;
close $out;
printf STDERR "%d lines from %d files (%d undecodable bytes)\n", scalar @rows, scalar @files, $undecodable // 0;
