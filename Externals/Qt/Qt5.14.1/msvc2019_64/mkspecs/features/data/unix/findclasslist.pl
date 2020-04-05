#!/usr/bin/env perl
#############################################################################
##
## Copyright (C) 2016 Intel Corporation.
## Contact: https://www.qt.io/licensing/
##
## This file is part of the build configuration tools of the Qt Toolkit.
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

use strict;
my $syntax = "findclasslist.pl\n" .
             "Replaces each \@FILE:filename\@ in stdin with the classes found in that file\n";

$\ = $/;
while (<STDIN>) {
    chomp;
    unless (/\@FILE:(.*)\@/) {
        print;
        next;
    }

    # Replace this line with the class list
    open HDR, "<$1" or die("Could not open header $1: $!");
    my $comment = "    /* $1 */";
    while (my $line = <HDR>) {
        # Match a struct or class declaration, but not a forward declaration
        $line =~ /^(?:struct|class|namespace) (?:Q_.*_EXPORT)? (\w+)(?!;)/ or next;
        print $comment if $comment;
        printf "    *%d%s*;\n", length $1, $1;
        $comment = 0;
    }
    close HDR;
}
