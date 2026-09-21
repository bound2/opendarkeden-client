#!/usr/bin/env perl
use strict;
use warnings;
use File::Basename qw(dirname);

sub count_identifiers {
    my ($source) = @_;
    # Translation-phase line splicing happens before comments are recognized.
    $source =~ s/\\\r?\n//g;
    # Consume numeric tokens too: a digit separator must not start a character
    # literal that swallows the next formatter call up to a later apostrophe.
    $source =~ s{R"([^\s()\\]{0,16})\([\s\S]*?\)\1"|"(?:\\[\s\S]|[^"\\])*"|\b[0-9][A-Za-z0-9_.']*|'(?:\\[\s\S]|[^'\\])*'|//[^\r\n]*|/\*[\s\S]*?\*/}{ }g;
    return scalar(() = $source =~ /\bwsprintf(?:A|W)?\b/g);
}

# Keep the counter honest about comments, literals, aliases and line splices.
my @fixtures = (
    ['wsprintf(dst, "%d", n);', 1],
    ['::wsprintfA(dst, "x"); wsprintfW(wide, L"x");', 2],
    ['auto formatter = &wsprintfA;', 1],
    ['// wsprintf(dst, "x");', 0],
    ['/* wsprintf(dst, "x"); */ wsprintf(dst, "x");', 1],
    ['"http://example/wsprintf"; \'w\';', 0],
    [q{"escaped \"wsprintf\""; wsprintf(dst, "x");}, 1],
    ['R"tag(raw "wsprintf" // text)tag";', 0],
    ['u8R"(wsprintf("text"))"; wsprintf(dst, "x");', 1],
    ["w\\\nsprintf(dst, \"x\");", 1],
    ["// ignored \\\nwsprintf(dst, \"x\");\nwsprintf(dst, \"y\");", 1],
    ['my_wsprintf(dst); wsprintfExtra(dst);', 0],
    [q{int n = 1'000; wsprintf(dst, "%d", n); char c = 'x';}, 1],
    [q{auto n = 0x1'ff; wsprintfA(dst, "%x", n); char c = 'y';}, 1],
    ["\0wsprintf(dst, \"x\");", 1],
);
for my $fixture (@fixtures) {
    die "wsprintf counter self-test failed: $fixture->[0]\n"
        unless count_identifiers($fixture->[0]) == $fixture->[1];
}

my $root = dirname(__FILE__) . '/../..';
chdir $root or die "cannot enter repository: $!\n";
open my $git, '-|', 'git', 'ls-files', '-z', '--cached', '--others', '--exclude-standard'
    or die "cannot enumerate source files: $!\n";
my $listing = do { local $/; <$git> };
close $git or die "git source enumeration failed\n";
my %seen;
my @files = grep { /\.(?:c|cc|cpp|cxx|h|hpp|hxx)\z/i && !$seen{$_}++ && -f $_ }
    split /\0/, $listing;
die "wsprintf source inventory unexpectedly small\n" if @files < 2000;
my $total = 0;
for my $file (@files) {
    open my $in, '<:raw', $file or die "$file: $!\n";
    my $source = do { local $/; <$in> } // '';
    my $count = count_identifiers($source);
    print STDERR "$file: $count raw wsprintf identifier(s)\n" if $count;
    $total += $count;
}
print "$total\n";
