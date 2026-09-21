#!/usr/bin/env perl
use strict;
use warnings;
use Test::More;
use File::Temp qw(tempdir);
use File::Basename qw(dirname);
use JSON::PP;
use Encode qw(encode);

my $dir = tempdir(CLEANUP => 1);
my $checker = dirname(__FILE__) . '/../../tools/ci/check-warnings.pl';
my $json = JSON::PP->new->canonical;
sub write_file {
    my ($name, $data) = @_;
    open my $out, '>:raw', "$dir/$name" or die $!;
    print {$out} $data;
    close $out or die $!;
}
sub run_check {
    my (@extra) = @_;
    open my $pipe, '-|', $^X, $checker, '--log', "$dir/build.log",
        '--profile', 'fixture', '--baseline', "$dir/baseline.json",
        '--report', "$dir/report.json", @extra or die $!;
    my $output = do { local $/; <$pipe> };
    close $pipe;
    return ($? >> 8, $output);
}
sub read_report {
    open my $in, '<:raw', "$dir/report.json" or die $!;
    return $json->decode(do { local $/; <$in> });
}

write_file('baseline.json', $json->encode({fixture => {'C4311' => 1, 'LNK4217' => 1}}));
my $msvc = "MSBuild version 17.14\n" .
    "C:\\repo\\Client\\a.cpp(12,4): warning C4311: pointer truncation [C:\\build\\x.vcxproj]\n" .
    "C:\\repo\\Client\\a.cpp(12,4): warning C4311: pointer truncation [C:\\build\\y.vcxproj]\n" .
    "a.obj : warning LNK4217: locally defined symbol imported [C:\\build\\x.vcxproj]\n";
write_file('build.log', "\xFF\xFE" . encode('UTF-16LE', $msvc));
is((run_check())[0], 0, 'PowerShell UTF-16 MSVC log accepted');
is_deeply(read_report()->{counts}, {C4311 => 1, LNK4217 => 1}, 'duplicate header/summary diagnostics count once');
is(read_report()->{warning_lines}, 3, 'raw warning lines are reported too');

write_file('baseline.json', $json->encode({fixture => {'-Wundef' => 2, '-Wunused-variable' => 1}}));
my $clang = "[1/2] Building CXX object a.cpp.o\n" .
    "\e[1m/work/a.h:2:3: \e[0mwarning: 'X' is not defined [-Wundef]\n" .
    "/work/a.h:2:3: warning: 'X' is not defined [-Wundef]\n" .
    "/work/b.cpp:9:7: warning: unused variable 'x' [-Wunused-variable]\n" .
    "/work/a.h:5:3: warning: 'Y' is not defined [-Wundef]\n" .
    "3 warnings generated.\n";
write_file('build.log', $clang);
is((run_check())[0], 0, 'Clang colour and summary lines parsed');
is_deeply(read_report()->{counts}, {'-Wundef' => 2, '-Wunused-variable' => 1}, 'locations distinguish warnings of the same kind');
write_file('build.log', $clang . "/work/b.cpp:10:7: warning: unused variable 'y' [-Wunused-variable]\n");
isnt((run_check())[0], 0, 'warning growth fails');
write_file('build.log', $clang . "/work/c.cpp:1:1: warning: new problem [-Wconversion]\n");
isnt((run_check())[0], 0, 'new warning class fails');
write_file('build.log', "[1/1] Building CXX object a.cpp.o\n/work/a.cpp:1:1: warning: macro [-Wundef]\n");
isnt((run_check())[0], 0, 'unrecorded decrease fails');
isnt((run_check('--profile', 'missing'))[0], 0, 'missing toolchain profile fails');
write_file('build.log', $clang . "/work/z.cpp:7: warning: unfamiliar diagnostic syntax\n");
isnt((run_check())[0], 0, 'unrecognised warning cannot disappear from count');
write_file('build.log', '');
isnt((run_check())[0], 0, 'empty log fails');
write_file('build.log', "ninja: no work to do.\n");
isnt((run_check())[0], 0, 'incremental log cannot silently lower baseline');
write_file('build.log', $clang . "FAILED: a.cpp.o\n/work/a.cpp:2:3: error: failed\n");
isnt((run_check('--record'))[0], 0, 'failed compilation cannot establish a baseline');
write_file('build.log', $clang);
is((run_check('--record'))[0], 0, 'explicit recording writes measured counts');
is((run_check())[0], 0, 'recorded counts pass');
write_file('baseline.json', '{invalid');
isnt((run_check())[0], 0, 'malformed baseline fails');
done_testing();
