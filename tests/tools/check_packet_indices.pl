#!/usr/bin/perl
#----------------------------------------------------------------------
# check_packet_indices.pl - server-supplied values used as array
# subscripts in the packet layer (ctest `packet_indices`).
#----------------------------------------------------------------------
#
# Code-health priority 1: "Client/Packet/Gpackets/ passes server-supplied
# lengths, indices and item classes straight into array subscripts". The
# lengths half is ratchet R3, now at 0. This is the indices half.
#
# IT WALKS THE VALUE, NOT THE SPELLING - and the first version of this
# file did not, which is the reason the header says so at this length.
# `array[pPacket->getSlotID()]` is one spelling with two live instances;
# the population is `int slot = pPacket->getSlotID();` twenty lines above
# `array[slot]`, and there are a hundred of those. Grepping the first
# shape and concluding anything about the second is the mistake ratchet
# R7 made about format strings, twice.
#
# The first version then made the same mistake one level up: it hardcoded
# the receiver name `pPacket`, and 19 handlers call their parameter
# something else - pGCGQuestStatusModify, pGCAddNickname, pGCGetDamage
# and so on. An unguarded subscript in any of those 19 files would have
# moved nothing. The receiver is taken from each file's own execute()
# signature now, and between that and the three shapes below the count
# went 102 -> 114 with nothing in the tree changed.
#
# What it follows, all of it earned from that review round:
#
#   - the receiver from `execute ( SomePacket * name , Player * ... )`,
#     per file, rather than an assumed name;
#   - locals assigned from `name->getX()`, and locals assigned from
#     those locals (one hop), because `int a = p->getX(); int b = a;`
#     is the same value;
#   - the direct form `container[name->getX()]`, which the first version
#     did not count at all while claiming to supersede it;
#   - subscripts spanning more than one line, which a line-based matcher
#     cannot see.
#
# What it still cannot follow, written down rather than left to be
# found: a value passed through a function call or stored in a member;
# `.at()` and pointer arithmetic, which are not `[]`; and more than one
# hop between locals.
#
#----------------------------------------------------------------------
# Most of the population subscripts a CTypeTable, whose operator[]
# range-checks in EVERY build (e65ab7a), so an out-of-range index yields
# a default-constructed element rather than a wild access. Those are
# counted and excluded - BY NAME, from the list below, not by pattern.
#
# The first version excluded anything matching `(*g_p<word>)`, which is
# a claim about spelling again: `(*g_pGameMessage)[i]` would have been
# excluded too, and CMessageArray::operator[] does truncating arithmetic
# rather than a range test. An unknown global therefore counts as RAW
# here - the check fails closed, and adding a name to this list is a
# deliberate act that says someone read its operator[].
#----------------------------------------------------------------------
use strict;
use warnings;
use File::Find;

my $ROOT = shift || '.';

# Verified CTypeTable subclasses: operator[] range-checks unconditionally
# (Client/CTypeTable.h), and none of them overrides it.
my %RANGE_CHECKED = map { $_ => 1 } qw(
	g_pActionInfoTable
	g_pSkillInfoTable
	g_pEffectStatusTable
	g_pSkillManager
	g_pCreatureTable
	g_pItemClassTable
	g_pGameStringTable
	g_pNickNameStringTable
	g_pZoneTable
	g_pRankBonusTable
);

# A rise means a new raw-container subscript. Read it, guard it, then
# raise this line in the same commit.
# 12: the vampire addon lookup now switches on named gear enums.
my $MAXIMUM_RAW = 12;

my @sources;
for my $dir ("$ROOT/Client/PacketHandler", "$ROOT/Client/Packet")
{
	find(sub { push @sources, $File::Find::name if /\.cpp$/i }, $dir) if -d $dir;
}

die "check_packet_indices: found no sources under $ROOT/Client\n" unless @sources;

my $checked = 0;
my $raw     = 0;
my @raw_hits;

for my $path (sort @sources)
{
	open my $fh, '<:raw', $path or die "$path: $!\n";
	my $text = do { local $/; <$fh> };
	close $fh;

	# Comments out, newlines kept so reported line numbers stay true.
	$text =~ s{/\*.*?\*/}{ my $m = $&; $m =~ s/[^\r\n]//g; $m }gse;
	$text =~ s{//[^\r\n]*}{}g;

	# The receiver this file's handler calls its packet. More than one
	# execute() in a file is fine - every name found is followed.
	my %receiver;
	while ($text =~ /::execute\s*\(\s*\w+\s*\*\s*(\w+)\s*,/g)
	{
		$receiver{$1} = 1;
	}

	next unless %receiver;

	my $recv = join '|', map { quotemeta } sort keys %receiver;

	# Locals taking a packet value, with an optional cast...
	my %tainted;
	while ($text =~ /(\w+)\s*=\s*(?:\([^()\r\n]*\)\s*)?(?:$recv)\s*->\s*(get\w+)\s*\(/g)
	{
		$tainted{$1} = $2;
	}

	# ...and locals taking one of those, one hop. Repeated to a fixed
	# point would need real scoping; one hop is what the tree has.
	for my $name (sort keys %tainted)
	{
		my $src = $tainted{$name};
		while ($text =~ /(\w+)\s*=\s*(?:\([^()\r\n]*\)\s*)?\Q$name\E\s*[;,)]/g)
		{
			$tainted{$1} = $src unless exists $tainted{$1};
		}
	}

	# Whole-text scan, so a subscript split across lines is still seen.
	# The index class forbids a quote and a brace, so neither a string
	# literal nor a block can masquerade as one.
	while ($text =~ /\[([^\[\]"{}]{1,80})\]/g)
	{
		my $index = $1;
		my $end   = pos($text);

		my $source;

		if ($index =~ /(?:$recv)\s*->\s*(get\w+)\s*\(/)
		{
			# The direct form.
			$source = $1;
		}
		else
		{
			my ($name) = grep { $index =~ /\b\Q$_\E\b/ } sort keys %tainted;
			next unless defined $name;
			$source = $tainted{$name};
		}

		$checked++;

		# Whatever is being subscripted: the token run before the '['.
		# The class excludes '(' so it cannot run back into a preceding
		# call's arguments, which is why a dereferenced global arrives
		# here as "*g_pFoo)" rather than "(*g_pFoo)".
		my $before    = substr($text, 0, $end - length($index) - 2);
		my $container = ($before =~ /([\w\]\)>.:*\-]{1,50})$/) ? $1 : '?';

		if ($container =~ /^\*(g_p\w+)\)$/ && $RANGE_CHECKED{$1})
		{
			next;
		}

		my $line = 1 + (() = substr($text, 0, $end) =~ /\n/g);

		$raw++;
		push @raw_hits, sprintf("  %s:%d  %s[%s]  <- %s()",
			$path, $line, $container, $index, $source);
	}
}

print "$_\n" for @raw_hits;

printf "check_packet_indices: %d packet-indexed subscript(s), %d into a container that is not range-checked\n",
	$checked, $raw;

if ($raw != $MAXIMUM_RAW)
{
	printf "FAIL: %d such subscripts against the %d recorded here.\n", $raw, $MAXIMUM_RAW;
	print  "      Up: a server-supplied index reaches memory nothing range-checks.\n";
	print  "          Read the new site and guard it.\n";
	print  "      Down: progress - record it.\n";
	print  "      Either way, change \$MAXIMUM_RAW in this file in the same commit.\n";
	exit 1;
}

print "OK\n";
exit 0;
