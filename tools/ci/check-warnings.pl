#!/usr/bin/env perl
# Count distinct diagnostic locations, not repeated includes or MSBuild summaries.
# Run on the complete --clean-first build log, never an incremental build.
use strict;
use warnings;
use Getopt::Long qw(GetOptions);
use JSON::PP;
use Encode qw(decode FB_CROAK);

my ($log, $profile, $baseline, $report, $record);
GetOptions('log=s' => \$log, 'profile=s' => \$profile,
    'baseline=s' => \$baseline, 'report=s' => \$report, 'record' => \$record)
    or die "invalid arguments\n";
die "required: --log --profile --baseline --report [--record]\n"
    unless defined($log) && defined($profile) && defined($baseline) && defined($report);
my $json = JSON::PP->new->utf8->canonical->pretty;
sub read_bytes {
    my ($path) = @_;
    open my $in, '<:raw', $path or die "$path: $!\n";
    return do { local $/; <$in> } // '';
}
sub write_json {
    my ($path, $value) = @_;
    open my $out, '>:raw', $path or die "$path: $!\n";
    print {$out} $json->encode($value);
    close $out or die "$path: $!\n";
}
my $bytes = read_bytes($log);
die "empty build log: $log\n" unless length $bytes;
my $encoding = $bytes =~ s/^\xFF\xFE// ? 'UTF-16LE' : 'UTF-8';
$bytes =~ s/^\xEF\xBB\xBF// if $encoding eq 'UTF-8';
my $text = decode($encoding, $bytes, FB_CROAK);
$text =~ s/\e\[[0-9;]*m//g;
die "no compilation evidence; use a complete --clean-first build log\n"
    unless $text =~ /Building (?:C|CXX) object|MSBuild version/;
die "failed build log cannot establish a warning budget\n"
    if $text =~ /(?:^FAILED:|\b(?:fatal )?error(?:\s+[A-Z]+\d+)?\s*:|Build FAILED\.|ninja: build stopped)/m;

my (%seen, %counts, @unknown);
my $warning_lines = 0;
for my $line (split /\r?\n/, $text) {
    next unless $line =~ /\bwarning(?:\s+[A-Z]+\d+)?\s*:/i;
    ++$warning_lines;
    my ($location, $code, $message);
    # Both compiler Cxxxx and linker LNKxxxx, including command-line warnings.
    if ($line =~ /^\s*(.*?)\s*:\s*warning\s+([A-Z]+\d+):\s*(.*?)(?:\s+\[[^\]]+\.vcxproj\])?\s*$/) {
        ($location, $code, $message) = ($1, $2, $3);
    } elsif ($line =~ /^\s*(.*?)\bwarning:\s*(.*?)\s+\[(-W[^\]]+)\]\s*$/) {
        ($location, $message, $code) = ($1, $2, $3);
    } else {
        push @unknown, $line;
        next;
    }
    $location =~ s{\\}{/}g;
    $location =~ s/\s+$//;
    # A source coordinate identifies the diagnostic even if a template is
    # instantiated by several translation units. A linker/driver diagnostic
    # has no coordinate, so keep its message to distinguish affected symbols.
    my $key = "$location\0$code";
    $key .= "\0$message" unless $location =~ /(?:\(\d+(?:,\d+)?\)|:\d+(?::\d+)?:)\z/;
    ++$counts{$code} unless $seen{$key}++;
}
my $total = 0;
$total += $_ for values %counts;
write_json($report, {profile => $profile, counts => \%counts,
    unique_warnings => $total, warning_lines => $warning_lines,
    unparsed => \@unknown});
print "$profile: $total distinct warnings ($warning_lines emitted lines); $report\n";
die "unparsed warning diagnostics; inspect $report\n" if @unknown;

my $budgets = $json->decode(read_bytes($baseline));
die "warning baseline must be an object\n" unless ref($budgets) eq 'HASH';
if ($record) {
    # This is a deliberate maintenance command, never called by CI.
    $budgets->{$profile} = \%counts;
    write_json($baseline, $budgets);
    print "recorded $profile in $baseline\n";
    exit 0;
}
die "missing warning baseline for $profile; record a complete build explicitly\n"
    unless ref($budgets->{$profile}) eq 'HASH';
my %codes = map { $_ => 1 } (keys %counts, keys %{$budgets->{$profile}});
my $failed = 0;
for my $code (sort keys %codes) {
    my $expected = $budgets->{$profile}{$code} // 0;
    die "invalid budget for $profile/$code\n" unless $expected =~ /^\d+$/;
    my $actual = $counts{$code} // 0;
    next if $actual == $expected;
    print "$code: $expected -> $actual; ",
        ($actual > $expected ? 'warning growth' : 'tighten the recorded baseline'), "\n";
    $failed = 1;
}
exit $failed;
