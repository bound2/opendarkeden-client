#!/usr/bin/env perl
use strict;
use warnings;
use Encode qw(decode FB_CROAK LEAVE_SRC);
use File::Basename qw(dirname);

# Match the compiler's UTF-8 source contract. Enumerate tracked and new source
# files through Git so generated build trees stay out; never pass an empty scan.
my $root = dirname(__FILE__) . '/../..';
chdir $root or die "cannot chdir to repo root: $!\n";
open my $git, '-|', 'git', 'ls-files', '-z', '--cached', '--others', '--exclude-standard'
    or die "cannot enumerate sources: $!\n";
my $listing = do { local $/; <$git> };
close $git or die "git source enumeration failed\n";
my %seen;
my @files = grep { /\.(?:c|cc|cpp|cxx|h|hpp|hxx)\z/i && !$seen{$_}++ && -f $_ }
    split /\0/, $listing;
die "source encoding scan unexpectedly small: " . scalar(@files) . " files\n"
    if @files < 2000;

my $failed = 0;
for my $file (sort @files) {
    open my $in, '<:raw', $file or die "$file: $!\n";
    my $bytes = do { local $/; <$in> } // '';
    close $in or die "$file: $!\n";
    eval { decode('UTF-8', $bytes, FB_CROAK | LEAVE_SRC); 1 } or do {
        print STDERR "$file: invalid UTF-8 source: $@";
        ++$failed;
    };
}
print scalar(@files) . " source files checked; $failed invalid UTF-8 files\n";
exit($failed ? 1 : 0);
