#!/usr/bin/perl
#-----------------------------------------------------------------------------
# uitext_apply.pl - write the English overrides of the packed UI text
#-----------------------------------------------------------------------------
# Reads the unpacked archives (see uitext_extract.pl) and the translation
# table, and writes every member that contains Korean as a loose file with
# its Korean lines replaced:
#
#	<out>/Data/Ui/txt/<member>      for the Data/Ui/txt archives
#	<out>/Data/Info/<member>        for infodata.rpk
#
# CRarFile prefers a loose file beside the archive over the packed member, so
# copying the output over a game directory (or into the browser asset pack,
# tools/web/package-assets.py --overlay) replaces the text without touching
# the archives. The files are ASCII, which is what the client's CP949 text
# decoding and the UTF-8 renderer both accept unchanged; line endings are the
# member's own.
#
# The translation table is keyed by the Korean line, so a line that recurs in
# several members (the three race variants of a mail, say) is translated
# once. A member is written only when every Korean line it holds is
# translated: the missing lines are reported and the member is skipped, so a
# half-translated file never ships.
#
#	perl uitext_apply.pl <unpacked dir> <uitext.en.tsv> <out dir>
#-----------------------------------------------------------------------------

use strict;
use warnings;
use Encode ();
use File::Basename ();
use File::Find ();
use File::Path ();

my ($root, $enPath, $outRoot) = @ARGV;
die "usage: $0 <unpacked dir> <uitext.en.tsv> <out dir>\n" unless defined $root && defined $enPath && defined $outRoot;
$root =~ s{[/\\]\z}{};

my %target = (
	book        => 'Data/Ui/txt',
	help        => 'Data/Ui/txt',
	item        => 'Data/Ui/txt',
	progress    => 'Data/Ui/txt',
	skill       => 'Data/Ui/txt',
	title       => 'Data/Ui/txt',
	tutorialetc => 'Data/Ui/txt',
	infodata    => 'Data/Info',
);

#-----------------------------------------------------------------------------
# Translation table
#-----------------------------------------------------------------------------
sub unescape {
	my ($s) = @_;
	$s =~ s/\\(.)/$1 eq 't' ? "\t" : $1/ge;
	return $s;
}

my %english;
open my $en, '<:raw:encoding(UTF-8)', $enPath or die "$enPath: $!\n";
my $enLine = 0;
while (my $line = <$en>) {
	$enLine++;
	$line =~ s/\r?\n\z//;
	# A line without a tab is a comment (a Korean line may itself start with '#').
	next if $line !~ /\t/;
	my ($korean, $text) = split /\t/, $line, 2;
	die "$enPath:$enLine: duplicate entry\n" if exists $english{$korean};
	die "$enPath:$enLine: translation is not ASCII\n" if $text =~ /[^\x00-\x7F]/;
	$english{ unescape($korean) } = unescape($text);
}
close $en;

#-----------------------------------------------------------------------------
# Members
#-----------------------------------------------------------------------------
my @files;
File::Find::find({ wanted => sub { push @files, $File::Find::name if -f $_ }, no_chdir => 1 }, $root);

my (%written, %missing, $members, $translated, $skipped);
for my $path (sort @files) {
	(my $rel = $path) =~ s{^\Q$root\E[/\\]}{};
	$rel =~ s{\\}{/}g;
	my ($archive, $member) = split m{/}, $rel, 2;
	next unless defined $member;
	my $dir = $target{ lc $archive } or die "$rel: unknown archive directory '$archive'\n";

	open my $in, '<:raw', $path or die "$path: $!\n";
	local $/;
	my $bytes = <$in>;
	close $in;
	next unless $bytes =~ /[\x80-\xFF]/;
	next if $bytes =~ /[\x00-\x08\x0E-\x1F]/;    # a binary member (SlayerPortal.inf)
	my $text = Encode::decode('cp949', $bytes, sub { sprintf '\\x%02X', shift });
	$members++;

	my ($out, $incomplete) = ('', 0);
	my $eol = $text =~ /\r\n/ ? "\r\n" : "\n";
	my $trailing = $text =~ /\n\z/ ? 1 : 0;
	my @lines = split /\r?\n/, $text, -1;
	pop @lines if $trailing;
	for my $line (@lines) {
		if ($line =~ /[^\x00-\x7F]/) {
			if (exists $english{$line}) {
				$line = $english{$line};
			}
			else {
				$missing{$line} = $rel;
				$incomplete++;
			}
		}
		$out .= $line . $eol;
	}
	$out =~ s/\Q$eol\E\z// unless $trailing;

	if ($incomplete) {
		$skipped++;
		next;
	}
	if ($out =~ /[^\x00-\x7F]/) {
		warn "$rel: leaves non-ASCII text behind, skipped\n";
		$skipped++;
		next;
	}
	# An XML member now holds ASCII, which is UTF-8; say so in its
	# declaration. The shipped "iso-8859-1" is not an encoding the client's
	# XML reader accepts (TextEncoding::Parse), and "euc-kr" would only
	# describe bytes the file no longer has.
	$out =~ s/^(<\?xml[^>]*\bencoding=)(["'])[^"']*\2/$1$2UTF-8$2/ if $member =~ /\.xml\z/i;

	my $target = "$outRoot/$dir/$member";
	if (exists $written{ lc $target } && $written{ lc $target } ne $out) {
		warn "$rel: '$member' was already written from another archive with different text, keeping the first\n";
		next;
	}
	$written{ lc $target } = $out;
	File::Path::make_path(File::Basename::dirname($target));
	open my $o, '>:raw', $target or die "$target: $!\n";
	print $o $out;
	close $o;
	$translated++;
}

printf STDERR "%d members with non-ASCII text: %d written, %d skipped, %d distinct lines untranslated\n",
	$members // 0, $translated // 0, $skipped // 0, scalar keys %missing;
for my $line (sort { $missing{$a} cmp $missing{$b} } keys %missing) {
	(my $shown = $line) =~ s/\t/\\t/g;
	print STDERR Encode::encode('UTF-8', "  $missing{$line}: $shown\n");
}
exit(keys %missing ? 1 : 0);
