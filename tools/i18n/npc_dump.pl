#!/usr/bin/perl
#-----------------------------------------------------------------------------
# npc_dump.pl - dump Data/Info/NPC.inf to a reviewable TSV
#-----------------------------------------------------------------------------
# NPC.inf is a CTypeMap<NPC_INFO> (MNPCTable.h): the name the client shows
# over an NPC and the one-line description of its role, in CP949.
#
#	int   count
#	{
#		int    npcID
#		int    nameLength;         char name[]
#		int    numberOfShopTemplate
#		{ int shopTemplateID }
#		int    descriptionLength;  char description[]
#		int    spriteID
#	}
#
# Output: npcID, spriteID, name, description - one NPC per line, the text
# decoded to UTF-8 with tabs and backslashes escaped.
#
#	perl npc_dump.pl <NPC.inf> <npc.ko.tsv>
#-----------------------------------------------------------------------------

use strict;
use warnings;
use Encode ();

my ($inPath, $outPath) = @ARGV;
die "usage: $0 <NPC.inf> <npc.ko.tsv>\n" unless defined $inPath && defined $outPath;

open my $in, '<:raw', $inPath or die "$inPath: $!\n";
local $/;
my $buf = <$in>;
close $in;

my $len = length $buf;
my $pos = 0;

sub u32 {
	die "truncated integer at offset $pos\n" if $pos + 4 > $len;
	my $v = unpack 'V', substr($buf, $pos, 4);
	$pos += 4;
	return $v;
}

sub bstr {
	my $n = u32();
	die "string length $n at offset $pos runs past end of file\n" if $pos + $n > $len;
	my $s = substr($buf, $pos, $n);
	$pos += $n;
	return $s;
}

sub column {
	my ($bytes) = @_;
	my $s = Encode::decode('cp949', $bytes, sub { sprintf '\\x%02X', shift });
	$s =~ s/\\/\\\\/g;
	$s =~ s/\t/\\t/g;
	$s =~ s/\r/\\r/g;
	$s =~ s/\n/\\n/g;
	return $s;
}

open my $out, '>:raw:encoding(UTF-8)', $outPath or die "$outPath: $!\n";
my $count = u32();
for my $i (1 .. $count) {
	my $id = u32();
	my $name = bstr();
	my $shops = u32();
	u32() for 1 .. $shops;
	my $description = bstr();
	my $sprite = u32();
	print $out join("\t", $id, $sprite, column($name), column($description)), "\n";
}
close $out;
die "trailing bytes after the last record\n" if $pos != $len;
print STDERR "$count NPCs\n";
