#!/usr/bin/env perl
#############################################################################
##
## Copyright (C) 2016 The Qt Company Ltd.
## Contact: https://www.qt.io/licensing/
##
## This file is part of the porting tools of the Qt Toolkit.
##
## $QT_BEGIN_LICENSE:LGPL$
## Commercial License Usage
## Licensees holding valid commercial Qt licenses may use this file in
## accordance with the commercial license agreement provided with the
## Software or, alternatively, in accordance with the terms contained in
## a written agreement between you and The Qt Company. For licensing terms
## and conditions see https://www.qt.io/terms-conditions. For further
## information use the contact form at https://www.qt.io/contact-us.
##
## GNU Lesser General Public License Usage
## Alternatively, this file may be used under the terms of the GNU Lesser
## General Public License version 3 as published by the Free Software
## Foundation and appearing in the file LICENSE.LGPL3 included in the
## packaging of this file. Please review the following information to
## ensure the GNU Lesser General Public License version 3 requirements
## will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
##
## GNU General Public License Usage
## Alternatively, this file may be used under the terms of the GNU
## General Public License version 2.0 or (at your option) the GNU General
## Public license version 3 or any later version approved by the KDE Free
## Qt Foundation. The licenses are as published by the Free Software
## Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
## included in the packaging of this file. Please review the following
## information to ensure the GNU General Public License requirements will
## be met: https://www.gnu.org/licenses/gpl-2.0.html and
## https://www.gnu.org/licenses/gpl-3.0.html.
##
## $QT_END_LICENSE$
##
#############################################################################


use Cwd;
use File::Find;
use File::Spec;
use IO::File;
use Getopt::Long;
use strict;
use warnings;

my $dry_run = 0;
my $help = 0;
my $stripModule = 0;
my $fixedFileCount = 0;
my $fileCount = 0;
my $verbose = 0;
my $qtdir;
my $qtIncludeDir;

my $USAGE=<<EOF;
This script replaces all Qt 4 style includes with Qt 5 includes.

Usage: $0 [options]

Options:
   --dry-run           : Do not replace anything, just print what would be replaced
   --strip-modules     : Strip the module headers for writing portable code
   --verbose           : Verbose
   --qtdir <directory> : Point to Qt 5's qtbase directory
EOF

if (!GetOptions('dry-run' => \$dry_run, 'help' => \$help,
     'strip-modules' => \$stripModule, 'verbose' => \$verbose, 'qtdir:s' => \$qtdir)
    || $help) {
    print $USAGE;
    exit (1);
}

my %headerSubst = ();
my $cwd = getcwd();

sub fixHeaders
{
    my $fileName = $File::Find::name;
    my $relFileName = File::Spec->abs2rel($fileName, $cwd);

    # only check sources, also ignore symbolic links and directories
    return unless -f $fileName && $fileName =~ /(\.h|\.cpp|\/C|\.cc|\.CC)$/;

    my $inFile = new IO::File('<' . $fileName) or die ('Unable to open ' . $fileName . ': ' . $!);
    $fileCount++;
    my @affectedClasses;
    my @outLines;

    while (my $line = <$inFile>) {
        if ($line =~ /^#(\s*)include(\s*)<.*?\/(.*?)>(.*)/) {
            my $newHeader = $headerSubst{$3};
            if ($newHeader) {
                $line = '#' . $1 . 'include' . $2 . '<' . $newHeader . '>' . $4 . "\n";
                push(@affectedClasses, $3);
            }
        } elsif ($line =~ /^#(\s*)include(\s*)<QtGui>(.*)/) {
            $line = '#' . $1 . 'include' . $2 . '<QtWidgets>' . $3 . "\n";
            push(@affectedClasses, 'QtGui');
        }
        push(@outLines, $line);
    }
    $inFile->close();

    if (scalar(@affectedClasses)) {
        $fixedFileCount++;
        print $relFileName, ': ', join(', ', @affectedClasses), "\n" if ($verbose || $dry_run);
        if (!$dry_run) {
            my $outFile = new IO::File('>' . $fileName) or die ('Unable to open ' . $fileName . ': ' . $!);
            map { print $outFile $_; } @outLines;
            $outFile->close();
        }
    } else {
        print $relFileName, ": no modification.\n" if ($verbose || $dry_run);
    }
}

sub findQtHeaders
{
    my ($dirName,$includeDir) = @_;

    local (*DIR);

    my $moduleIncludeDir = $includeDir . '/' . $dirName;
    opendir(DIR, $moduleIncludeDir) || die ('Unable to open ' . $moduleIncludeDir . ': ' . $!);
    my @headers = readdir(DIR);
    closedir(DIR);

    foreach my $header (@headers) {
        next if (-d ($moduleIncludeDir . '/' . $header) || $header =~ /\.pri$/);
        $headerSubst{$header} = $stripModule ?  $header : ($dirName . '/' . $header);
    }
}

# -------- MAIN

if ($qtdir) {
    $qtIncludeDir = $qtdir . '/include';
} else {
    $qtIncludeDir = `qmake -query QT_INSTALL_HEADERS`;
    chop($qtIncludeDir);
}

die "The location of the Qt 5 include files could not be determined.\n"
        ."Please ensure qmake can be found in PATH or pass the command line option --qtdir.\n"
    unless -d $qtIncludeDir;

findQtHeaders('QtCore', $qtIncludeDir);
findQtHeaders('QtConcurrent', $qtIncludeDir);
findQtHeaders('QtWidgets', $qtIncludeDir);
findQtHeaders('QtPrintSupport', $qtIncludeDir);

if (-d $qtIncludeDir . '/include/QtMultimedia') {
    findQtHeaders('QtMultimedia', $qtIncludeDir);
    findQtHeaders('QtMultimediaWidgets', $qtIncludeDir);
} elsif (-d $qtIncludeDir . '/../qtmultimedia' ) {
    # This is the case if QTDIR points to a source tree instead of an installed Qt
    findQtHeaders('QtMultimedia', $qtIncludeDir . '/../qtmultimedia');
    findQtHeaders('QtMultimediaWidgets', $qtIncludeDir . '/../qtmultimedia');
}

# Support porting from "Qt 4.99" QtDeclarative to QtQuick (QQuickItem et al)
if (-d $qtIncludeDir . '/include/QtQuick') {
    findQtHeaders('QtQuick', $qtIncludeDir);
} elsif (-d $qtIncludeDir . '/../qtdeclarative' ) {
    # This is the case if QTDIR points to a source tree instead of an installed Qt
    findQtHeaders('QtQuick', $qtIncludeDir . '/../qtdeclarative');
}

# special case
$headerSubst{'QtGui'} = 'QtWidgets/QtWidgets';

find({ wanted => \&fixHeaders, no_chdir => 1}, $cwd);

print 'Done. ', ($dry_run ? 'Checked' : 'Modified'), ' ', $fixedFileCount, ' of ', $fileCount, " file(s).\n";
