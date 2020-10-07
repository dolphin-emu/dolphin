#!/usr/bin/env perl
#############################################################################
##
## Copyright (C) 2016 The Qt Company Ltd.
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

#
# Synchronizes Qt header files - internal development tool.
#

# use packages -------------------------------------------------------
use File::Basename;
use File::Path;
use File::Spec;
use Cwd;
use Cwd 'abs_path';
use Config;
use strict;
use warnings;
use English qw(-no_match_vars );

my $normalizePath_fixDrive = ($^O eq "msys" ? 1 : 0);

######################################################################
# Syntax:  normalizePath(\$path)
# Params:  Reference to a path that's going to be normalized.
#
# Purpose: Converts the path into a form that can be used as include
#          path from C++ sources and qmake's .pro files.
#          Only relevant on Windows.
# Returns: -none-
######################################################################
sub normalizePath {
    my $s = shift;
    $$s =~ s=\\=/=g;
    if ($normalizePath_fixDrive && ($$s =~ m,^/([a-zA-Z])/(.*), || $$s =~ m,^([a-zA-Z]):/(.*),)) {
        $$s = lc($1) . ":/$2";
    }
}

# set output basedir to be where ever syncqt is run from
our $out_basedir = getcwd();
normalizePath(\$out_basedir);
our $build_basedir;
our $basedir;

# will be defined based on the modules sync.profile
our (%modules, %moduleheaders, @allmoduleheadersprivate, %classnames, %deprecatedheaders);
our (@qpa_headers, @private_headers);

# will be derived from sync.profile
our %reverse_classnames = ();
my %ignore_for_include_check = ();
my %ignore_for_qt_begin_namespace_check = ();

# global variables (modified by options)
my $isunix = 0;
my $module = 0;
my $showonly = 0;
my $verbose_level = 1;
my $remove_stale = 1;
my $force_win = 0;
my $check_includes = 0;
my $copy_headers = 0;
my $minimal = 0;
my $module_version = 0;
my @modules_to_sync ;


# functions ----------------------------------------------------------

######################################################################
# Syntax:  showUsage()
# Params:  -none-
#
# Purpose: Show the usage of the script.
# Returns: -none-
######################################################################
sub showUsage
{
    print "$0 usage:\n";
    print "  <module directory>    Specifies which module to sync header files for (required for shadow builds!)\n\n";

    print "  -copy                 Copy headers instead of include-fwd(default: " . ($copy_headers ? "yes" : "no") . ")\n";
    print "  -remove-stale         Removes stale headers              (default: " . ($remove_stale ? "yes" : "no") . ")\n";
    print "  -windows              Force platform to Windows          (default: " . ($force_win ? "yes" : "no") . ")\n";
    print "  -showonly             Show action but not perform        (default: " . ($showonly ? "yes" : "no") . ")\n";
    print "  -minimal              Do not create CamelCase headers    (default: " . ($minimal ? "yes" : "no") . ")\n";
    print "  -outdir <PATH>        Specify output directory for sync  (default: $out_basedir)\n";
    print "  -builddir <PATH>      Specify build directory for sync   (default: same as -outdir)\n";
    print "  -version <VERSION>    Specify the module's version       (default: detect from qglobal.h)\n";
    print "  -quiet                Only report problems, not activity (same as -verbose 0)\n";
    print "  -v, -verbose <level>  Sets the verbosity level (max. 4)  (default: $verbose_level)\n";
    print "                        The short form increases the level by +1\n";
    print "  -separate-module <NAME>:<PROFILEDIR>:<HEADERDIR>\n";
    print "                        Create headers for <NAME> with original headers in\n";
    print "                        <HEADERDIR> relative to <PROFILEDIR> \n";
    print "  -help                 This help\n";
    exit 0;
}

######################################################################
# Syntax:  checkUnix()
# Params:  -none-
#
# Purpose: Check if script runs on a Unix system or not. Cygwin
#          systems are _not_ detected as Unix systems.
# Returns: 1 if a unix system, else 0.
######################################################################
sub checkUnix {
    my ($r) = 0;
    if ( $force_win != 0) {
        return 0;
    } elsif ( -f "/bin/uname" ) {
        $r = 1;
        (-f "\\bin\\uname") && ($r = 0);
    } elsif ( -f "/usr/bin/uname" ) {
        $r = 1;
        (-f "\\usr\\bin\\uname") && ($r = 0);
    }
    if($r) {
        $_ = $Config{'osname'};
        $r = 0 if( /(ms)|(cyg)win/i );
    }
    return $r;
}

sub checkRelative {
    my ($dir) = @_;
    return 0 if($dir =~ /^\//);
    return 0 if(!checkUnix() && $dir =~ /[a-zA-Z]:[\/\\]/);
    return 1;
}

######################################################################
# Syntax:  shouldMasterInclude(iheader)
# Params:  iheader, string, filename to verify inclusion
#
# Purpose: Determines if header should be in the master include file.
# Returns: 0 if file contains "#pragma qt_no_master_include" or not
#          able to open, else 1.
######################################################################
sub shouldMasterInclude {
    my ($iheader) = @_;
    return 0 if (basename($iheader) =~ /_/);
    return 0 if (basename($iheader) =~ /qconfig/);
    local $/ = "\x0a";
    if (open(F, "<$iheader")) {
        while (<F>) {
            s/\x0d?\x0a//;
            return 0 if (/^\#pragma qt_no_master_include$/);
        }
        close(F);
    } else {
        return 0;
    }
    return 1;
}

######################################################################
# Syntax:  classNames(iheader, clean, requires)
# Params:  iheader, string, filename to parse for classname "symlinks"
#          (out) clean, boolean, will be set to false if the header isn't clean
#          (out) requires, string, will be set to non-empty if the header
#                                  requires a feature
#
# Purpose: Scans through iheader to find all classnames that should be
#          synced into library's include structure.
# Returns: List of all class names in a file.
######################################################################
sub classNames {
    my @ret;
    my ($iheader, $clean, $requires) = @_;
    $$clean = 1;
    $$requires = "";

    my $suspended = 0;
    my $ihdrbase = basename($iheader);

    my $parsable = "";
    local $/ = "\x0a";
    if(open(F, "<$iheader")) {
        while(<F>) {
            s/\x0d?\x0a//;
            my $line = $_;
            if($line =~ /^\#/) {
                $$clean = 0 if ($line =~ m/^#pragma qt_sync_skip_header_check/);
                return @ret if($line =~ m/^#pragma qt_sync_stop_processing/);
                push(@ret, $1) if($line =~ m/^#pragma qt_class\(([^)]*)\)[\r\n]*$/);
                $suspended = 1 if ($line =~ m/^#pragma qt_sync_suspend_processing/);
                $suspended = 0 if ($line =~ m/^#pragma qt_sync_resume_processing/);
                $line = 0;
            }
            if ($line && !$suspended) {
                $line =~ s,//.*$,,; #remove c++ comments
                $line .= ";" if($line =~ m/^Q_[A-Z_0-9]*\(.*\)[\r\n]*$/); #qt macro
                $line .= ";" if($line =~ m/^QT_(BEGIN|END)_HEADER[\r\n]*$/); #qt macro
                $line .= ";" if($line =~ m/^QT_(BEGIN|END)_NAMESPACE(_[A-Z]+)*[\r\n]*$/); #qt macro
                $line .= ";" if($line =~ m/^QT_MODULE\(.*\)[\r\n]*$/); # QT_MODULE macro
                $line .= ";" if($line =~ m/^QT_WARNING_(PUSH|POP|DISABLE_\w+\(.*\))[\r\n]*$/); # qt macros
                $$requires = $1 if ($line =~ m/^QT_REQUIRE_CONFIG\((.*)\);[\r\n]*$/);
                $parsable .= " " . $line;
            }
        }
        close(F);
    }

    my $last_definition = 0;
    my @namespaces;
    for(my $i = 0; $i < length($parsable); $i++) {
        my $definition = 0;
        my $character = substr($parsable, $i, 1);
        if($character eq "/" && substr($parsable, $i+1, 1) eq "*") { #I parse like this for greedy reasons
            for($i+=2; $i < length($parsable); $i++) {
                my $end = substr($parsable, $i, 2);
                if($end eq "*/") {
                    $last_definition = $i+2;
                    $i++;
                    last;
                }
            }
        } elsif($character eq "{") {
            my $brace_depth = 1;
            my $block_start = $i + 1;
          BLOCK: for($i+=1; $i < length($parsable); $i++) {
              my $ignore = substr($parsable, $i, 1);
              if($ignore eq "{") {
                  $brace_depth++;
              } elsif($ignore eq "}") {
                  $brace_depth--;
                  unless($brace_depth) {
                      for(my $i2 = $i+1; $i2 < length($parsable); $i2++) {
                          my $end = substr($parsable, $i2, 1);
                          if($end eq ";" || $end ne " ") {
                              $definition = substr($parsable, $last_definition, $block_start - $last_definition) . "}";
                              $i = $i2 if($end eq ";");
                              $last_definition = $i + 1;
                              last BLOCK;
                          }
                      }
                  }
              }
          }
        } elsif($character eq ";") {
            $definition = substr($parsable, $last_definition, $i - $last_definition + 1);
            $last_definition = $i + 1;
        } elsif($character eq "}") {
            # a naked } must be a namespace ending
            # if it's not a namespace, it's eaten by the loop above
            pop @namespaces;
            $last_definition = $i + 1;
        }

        if (substr($parsable, $last_definition, $i - $last_definition + 1) =~ m/ namespace ([^ ]*) /
            && substr($parsable, $i+1, 1) eq "{") {
            push @namespaces, $1;

            # Eat the opening { so that the condensing loop above doesn't see it
            $i++;
            $last_definition = $i + 1;
        }

        if($definition) {
            $definition =~ s=[\n\r]==g;
            $definition =~ s/QT_DEPRECATED_X\s*\(\s*".*?"\s*\)//g;
            my @symbols;
            my $post_kw = qr/Q_DECL_FINAL|final|sealed/; # add here macros and keywords that go after the class-name of a class definition
            if($definition =~ m/^ *typedef *.*\(\*([^\)]*)\)\(.*\);$/) {
                push @symbols, $1;
            } elsif($definition =~ m/^ *typedef +(.*) +([^ ]*);$/) {
                push @symbols, $2;
            } elsif($definition =~ m/^ *(template *<.*> *)?(class|struct) +([^ <>]* +)?((?!$post_kw)[^<\s]+) ?(<[^>]*> ?)?\s*(?:$post_kw)?\s*((,|:)\s*(public|protected|private) *.*)? *\{\}$/o) {
                push @symbols, $4;
            } elsif($definition =~ m/^ *Q_DECLARE_.*ITERATOR\((.*)\);$/) {
                push @symbols, "Q" . $1 . "Iterator";
                push @symbols, "QMutable" . $1 . "Iterator";
            }

            our $publicclassregexp;
            foreach my $symbol (@symbols) {
                $symbol = (join("::", @namespaces) . "::" . $symbol) if (scalar @namespaces);

                my $revhdr = $reverse_classnames{$symbol};
                next if (defined($revhdr) and $revhdr ne $ihdrbase);
                if ($symbol =~ /^Q[^:]*$/) {           # no-namespace, starting with Q
                    push @ret, $symbol;
                } elsif (defined($publicclassregexp)) {
                    push @ret, $symbol if ($symbol =~ $publicclassregexp);
                }
            }
        }
    }
    return @ret;
}

sub check_header {
    my ($lib, $header, $iheader, $public_header, $private_header) = @_;
    my $header_skip_qt_begin_namespace_test = 0;

    return if ($ignore_for_include_check{$header});
    if ($public_header) {
        $header_skip_qt_begin_namespace_test = 1 if ($ignore_for_qt_begin_namespace_check{$header});
    }

    local $/ = "\x0a";
    open(F, "<$iheader") or return;
    my $qt_begin_namespace_found = 0;
    my $qt_end_namespace_found = 0;
    my $qt_namespace_suffix = "";
    my $line;
    my $stop_processing = 0;
    my $we_mean_it = 0;
    while ($line = <F>) {
        $line =~ s/\x0d?\x0a//;
        my $output_line = 1;
        if ($line =~ /^ *\# *pragma (qt_no_included_check|qt_sync_stop_processing)/) {
            $stop_processing = 1;
            last;
        }
        if ($line =~ /^ *\# *include/) {
            my $include = $line;
            if ($line =~ /<.*>/) {
                $include =~ s,.*<(.*)>.*,$1,;
            } elsif ($line =~ /".*"/) {
                $include =~ s,.*"(.*)".*,$1,;
            } else {
                $include = 0;
            }
            if ($include && $public_header) {
                print STDERR "$lib: ERROR: $iheader includes private header $include\n" if ($include =~ /_p\.h$/);
                for my $trylib (keys(%modules)) {
                    if (-e "$out_basedir/include/$trylib/$include") {
                        print STDERR "$lib: WARNING: $iheader includes $include when it should include $trylib/$include\n";
                    }
                }
            }
        } elsif (!$private_header) {
            if ($header_skip_qt_begin_namespace_test == 0 and $line =~ /^QT_BEGIN_NAMESPACE(_[A-Z_]+)?\s*$/) {
                $qt_namespace_suffix = defined($1) ? $1 : "";
                $qt_begin_namespace_found = 1;
            } elsif ($header_skip_qt_begin_namespace_test == 0 and $line =~ /^QT_END_NAMESPACE$qt_namespace_suffix\s*$/) {
                $qt_end_namespace_found = 1;
            }
        } elsif ($line =~ "^// We mean it.") {
            ++$we_mean_it;
        }
    }

    if ($public_header) {
        if ($header_skip_qt_begin_namespace_test == 0 and $stop_processing == 0) {
            if ($qt_begin_namespace_found == 0) {
                print STDERR "$lib: WARNING: $iheader does not include QT_BEGIN_NAMESPACE\n";
            }

            if ($qt_begin_namespace_found && $qt_end_namespace_found == 0) {
                print STDERR "$lib: WARNING: $iheader has QT_BEGIN_NAMESPACE$qt_namespace_suffix but no QT_END_NAMESPACE$qt_namespace_suffix\n";
            }
        }
    } elsif ($private_header) {
        print STDERR "$lib: WARNING: $iheader does not have the \"We mean it.\" warning\n" if (!$we_mean_it);
    }

    close(F);
}

sub make_path {
    my ($dir, $lib, $be_verbose) = @_;
    unless(-e $dir) {
        mkpath $dir;
        $dir = "<outbase>" . substr($dir, length($out_basedir)) if ($be_verbose < 3);
        print "$lib: mkpath $dir\n" if ($be_verbose > 1);
    }
}

######################################################################
# Syntax:  syncHeader(header, iheader, copy, timestamp)
# Params:  header, string, filename to create "symlink" for
#          iheader, string, destination name of symlink
#          copy, forces header to be a copy of iheader
#          timestamp, the requested modification time if copying
#
# Purpose: Syncronizes header to iheader
# Returns: 1 if successful, else 0.
######################################################################
sub syncHeader {
    my ($lib, $header, $iheader, $copy, $ts) = @_;
    normalizePath(\$iheader);
    normalizePath(\$header);
    return copyFile($lib, $iheader, $header) if($copy);

    unless(-e $header) {
        my $header_dir = dirname($header);
        make_path($header_dir, $lib, $verbose_level);

        #write it
        my $iheader_out = fixPaths($iheader, $header_dir);
        open(HEADER, ">$header") || die "Could not open $header for writing: $!\n";
        print HEADER "#include \"$iheader_out\"\n";
        close HEADER;
        if(defined($ts)) {
            utime(time, $ts, $header) or die "$iheader, $header";
        }
        return 1;
    }
    return 0;
}

######################################################################
# Syntax:  fixPaths(file, dir)
# Params:  file, string, filepath to be made relative to dir
#          dir, string, dirpath for point of origin
#
# Purpose: file is made relative (if possible) of dir.
# Returns: String with the above applied conversion.
######################################################################

sub cleanupPath {
    my ($file) = @_;
    normalizePath(\$file);
    while ($file =~ s,/[^/]+/\.\./,/,) {}
    return $file;
}

sub fixPaths {
    my ($file, $dir) = @_;

    my $out = File::Spec->abs2rel(cleanupPath($file), cleanupPath($dir));
    $out =~ s,\\,/,g;
    $out = "\"$out\"" if ($out =~ / /);
    return $out;
}

######################################################################
# Syntax:  fileContents(filename)
# Params:  filename, string, filename of file to return contents
#
# Purpose: Get the contents of a file.
# Returns: String with contents of the file, or empty string if file
#          doens't exist.
# Warning: Dies if it does exist but script cannot get read access.
######################################################################
sub fileContents {
    my ($filename) = @_;
    my $filecontents = "";
    if (-e $filename) {
        open(I, "< $filename") || die "Could not open $filename for reading, read block?";
        local $/;
        binmode I;
        $filecontents = <I>;
        close I;
    }
    return $filecontents;
}

######################################################################
# Syntax:  writeFile(filename, contents)
# Params:  filename, string, filename of file to write
#          contents, string, new contents for the file
#
# Purpose: Write file with given contents. If new contents match old
#          ones, do no change the file's timestamp.
# Returns: 1 if the file's contents changed.
######################################################################
sub writeFile {
    my ($filename, $contents, $lib, $what) = @_;
    my $oldcontents = fileContents($filename);
    $oldcontents =~ s/\r//g; # remove \r's , so comparison is ok on all platforms
    if ($oldcontents ne $contents) {
        open(O, "> " . $filename) || die "Could not open $filename for writing: $!\n";
        print O $contents;
        close O;
        if ($lib && $verbose_level) {
            my $action = ($oldcontents eq "") ? "created" : "updated";
            print "$lib: $action $what\n";
        }
        return 1;
    }
    return 0;
}

######################################################################
# Syntax:  fileCompare(file1, file2)
# Params:  file1, string, filename of first file
#          file2, string, filename of second file
#
# Purpose: Determines if files are equal, and which one is newer.
# Returns: 0 if files are equal no matter the timestamp, -1 if file1
#          is newer, 1 if file2 is newer.
######################################################################
sub fileCompare {
    my ($file1, $file2) = @_;
    my $file1contents = fileContents($file1);
    my $file2contents = fileContents($file2);
    if (! -e $file1) { return 1; }
    if (! -e $file2) { return -1; }
    return $file1contents ne $file2contents ? (stat($file2))[9] <=> (stat($file1))[9] : 0;
}

######################################################################
# Syntax:  copyFile(file, ifile)
# Params:  file, string, filename to create duplicate for
#          ifile, string, destination name of duplicate
#
# Purpose: Keeps files in sync so changes in the newer file will be
#          written to the other.
# Returns: 1 if files were synced, else 0.
# Warning: Dies if script cannot get write access.
######################################################################
sub copyFile
{
    my ($lib, $file,$ifile, $copy,$knowdiff,$filecontents,$ifilecontents) = @_;
    # Bi-directional synchronization
    open( I, "< " . $file ) || die "Could not open $file for reading";
    local $/;
    binmode I;
    $filecontents = <I>;
    close I;
    if ( open(I, "< " . $ifile) ) {
        local $/;
        binmode I;
        $ifilecontents = <I>;
        close I;
        $copy = fileCompare($file, $ifile);
        $knowdiff = 0,
    } else {
        $copy = -1;
        $knowdiff = 1;
    }

    if ( $knowdiff || ($filecontents ne $ifilecontents) ) {
        if ( $copy > 0 ) {
            my $file_dir = dirname($file);
            make_path($file_dir, $lib, $verbose_level);
            open(O, "> " . $file) || die "Could not open $file for writing (no write permission?)";
            local $/;
            binmode O;
            print O $ifilecontents;
            close O;
            utime time, (stat($ifile))[9], $file;
            return 1;
        } elsif ( $copy < 0 ) {
            my $ifile_dir = dirname($ifile);
            make_path($ifile_dir, $lib, $verbose_level);
            open(O, "> " . $ifile) || die "Could not open $ifile for writing (no write permission?)";
            local $/;
            binmode O;
            print O $filecontents;
            close O;
            utime time, (stat($file))[9], $ifile;
            return 1;
        }
    }
    return 0;
}

######################################################################
# Syntax:  findFiles(dir, match)
# Params:  dir, string, directory to search for name
#          match, string, regular expression to match in dir
#
# Purpose: Finds files matching a regular expression.
# Returns: List of matching files.
#
# Example:
#   findFiles("/tmp", "^#")      - finds #* files in /tmp
######################################################################
sub findFiles {
    my ($dir, $match) = @_;
    my ($file,$p,@files);
    local(*D);
    normalizePath(\$dir);
    ($dir eq "") && ($dir = ".");
    if ( opendir(D,$dir) ) {
        if ( $dir eq "." ) {
            $dir = "";
        } else {
            ($dir =~ /\/$/) || ($dir .= "/");
        }
        foreach $file ( sort readdir(D) ) {
            next if ( $file  =~ /^\.\.?$/ );
            $p = $file;
            ($file =~ /$match/) && (push @files, $p);
        }
        closedir(D);
    }
    return @files;
}

sub listSubdirs {
    my @subdirs = @_;
    foreach my $subdir (@subdirs) {
        opendir DIR, $subdir or die "Huh, directory ".$subdir." cannot be opened.";
        foreach my $t (sort readdir(DIR)) {
            push @subdirs, "$subdir/$t" if(-d "$subdir/$t" && !($t eq ".") &&
                                           !($t eq "..") && !($t eq ".obj") &&
                                           !($t eq ".moc") && !($t eq ".rcc") &&
                                           !($t eq ".uic") && !($t eq "build") &&
                                           !($t eq "doc"));
        }
        closedir DIR;
    }
    return @subdirs;
}

######################################################################
# Syntax:  loadSyncProfile()
#
# Purpose: Loads the sync.profile.
######################################################################
sub loadSyncProfile {
    if ($verbose_level) {
        print("<srcbase> = $basedir \n");
        print("<bldbase> = $build_basedir \n");
        print("<outbase> = $out_basedir \n");
    }

    my $syncprofile = "$basedir/sync.profile";
    my $result;
    unless ($result = do "$syncprofile") {
        die "syncqt couldn't parse $syncprofile: $@" if $@;
        die "syncqt couldn't execute $syncprofile: $!" unless defined $result;
    }

    for my $fn (keys %classnames) {
        for my $cn (split(/,/, $classnames{$fn})) {
            $reverse_classnames{$cn} = $fn;
        }
    }

    push @private_headers, qr/_p(ch)?\.h$/;
}

sub basePrettify {
    my ($arg) = @_;
    $$arg =~ s,^\Q$basedir\E,<srcbase>,;
    $$arg =~ s,^\Q$build_basedir\E,<bldbase>,;
    $$arg =~ s,^\Q$out_basedir\E,<outbase>,;
}

sub cleanPath {
    my ($arg) = @_;
    while ($arg =~ s,[^/]+/\.\.(/|$),,) {}
    return $arg;
}

######################################################################
# Syntax:  locateSyncProfile()
#
# Purpose: Locates the sync.profile.
######################################################################
sub locateSyncProfile
{
    my ($directory) = @_;
    $directory = abs_path($directory);
    while (1) {
        my $file = $directory."/sync.profile";
        return $file if (-e $file);
        my $odir = $directory;
        $directory = dirname($directory);
        return undef if ($directory eq $odir);
    }
}

sub isQpaHeader
{
    my ($header) = @_;
    foreach my $qpa_header (@qpa_headers) {
        return 1 if ($header =~ $qpa_header);
    }
    return 0;
}

sub isPrivateHeader
{
    my ($header) = @_;
    foreach my $private_header (@private_headers) {
        return 1 if ($header =~ $private_header);
    }
    return 0;
}

sub globosort($$) {
    my ($a, $b) = @_;
    if ($a =~ /^q(.*)global\.h$/) {
        my $sa = $1;
        if ($b =~ /^q(.*)global\.h$/) {
            my $sb = $1;
            # Compare stems so qglobal.h (empty stem) is first:
            return $sa cmp $sb;
        }
        return -1; # $a is global, so before $b
    }
    return +1 if $b =~ /^q.*global\.h$/; # $a not global, so after $b
    return $a cmp $b;
}

# check if this is an in-source build, and if so use that as the basedir too
$basedir = locateSyncProfile($out_basedir);
if ($basedir) {
    $basedir = dirname($basedir) ;
    normalizePath(\$basedir);
}

# --------------------------------------------------------------------
# "main" function
# --------------------------------------------------------------------

while ( @ARGV ) {
    my $var = 0;
    my $val = 0;

    #parse
    my $arg = shift @ARGV;
    if ($arg eq "-h" || $arg eq "-help" || $arg eq "-?" || $arg eq "?") {
        $var = "show_help";
        $val = "yes";
    } elsif($arg eq "-copy") {
        $var = "copy";
        $val = "yes";
    } elsif($arg eq "-o" || $arg eq "-outdir") {
        $var = "output";
        $val = shift @ARGV;
    } elsif($arg eq "-builddir") {
        $var = "build";
        $val = shift @ARGV;
    } elsif($arg eq "-showonly" || $arg eq "-remove-stale" || $arg eq "-windows" ||
            $arg eq "-relative" || $arg eq "-check-includes") {
        $var = substr($arg, 1);
        $val = "yes";
    } elsif($arg =~ /^-no-(.*)$/) {
        $var = $1;
        $val = "no";
        #these are for commandline compat
    } elsif($arg eq "-inc") {
        $var = "output";
        $val = shift @ARGV;
    } elsif($arg eq "-module") {
        $var = "module";
        $val = shift @ARGV;
    } elsif($arg eq "-separate-module") {
        $var = "separate-module";
        $val = shift @ARGV;
    } elsif($arg eq "-show") {
        $var = "showonly";
        $val = "yes";
    } elsif($arg eq "-quiet") {
        $var = "verbose";
        $val = "0";
    } elsif($arg eq "-v") {
        $var = "verbose";
        $val = "yes";
    } elsif($arg eq "-verbose") {
        $var = "verbose";
        $val = shift @ARGV;
    } elsif($arg eq "-minimal") {
        $var = "minimal";
        $val = "yes";
    } elsif($arg eq "-version") {
        $var = "version";
        $val = shift @ARGV;
    } elsif($arg =~/^-/) {
        print STDERR "ERROR: Unknown option: $arg\n\n" if (!$var);
        showUsage();
    } else {
        $basedir = locateSyncProfile($arg);
        die "Could not find a sync.profile for '$arg'\n" if (!$basedir);
        $basedir = dirname($basedir);
        normalizePath(\$basedir);
        $var = "ignore";
    }

    #do something
    if(!$var || $var eq "show_help") {
        print STDERR "ERROR: Unknown option: $arg\n\n" if (!$var);
        showUsage();
    } elsif ($var eq "copy") {
        if($val eq "yes") {
            $copy_headers++;
        } elsif($showonly) {
            $copy_headers--;
        }
    } elsif ($var eq "showonly") {
        if($val eq "yes") {
            $showonly++;
        } elsif($showonly) {
            $showonly--;
        }
    } elsif ($var eq "verbose") {
        if($val eq "yes") {
            $verbose_level++;
        } elsif($val eq "no" && $verbose_level) {
            $verbose_level--;
        } else {
            $verbose_level = int($val);
        }
    } elsif ($var eq "check-includes") {
        if($val eq "yes") {
            $check_includes++;
        } elsif($check_includes) {
            $check_includes--;
        }
    } elsif ($var eq "remove-stale") {
        if($val eq "yes") {
            $remove_stale++;
        } elsif($remove_stale) {
            $remove_stale--;
        }
    } elsif ($var eq "windows") {
        if($val eq "yes") {
            $force_win++;
        } elsif($force_win) {
            $force_win--;
        }
    } elsif ($var eq "minimal") {
        if($val eq "yes") {
            $minimal++;
        } elsif($minimal) {
            $minimal--;
        }
    } elsif ($var eq "module") {
        push @modules_to_sync, $val;
    } elsif ($var eq "separate-module") {
        my ($module, $prodir, $headerdir) = split(/:/, $val);
        $modules{$module} = $prodir;
        push @modules_to_sync, $module;
        $moduleheaders{$module} = $headerdir;
    } elsif ($var eq "version") {
        if($val) {
            $module_version = $val;
        } else {
            die "The -version option requires an argument";
        }
    } elsif ($var eq "output") {
        my $outdir = $val;
        if(checkRelative($outdir)) {
            $out_basedir = getcwd();
            chomp $out_basedir;
            $out_basedir .= "/" . $outdir;
        } else {
            $out_basedir = $outdir;
        }
        normalizePath(\$out_basedir);
    } elsif ($var eq "build") {
        my $outdir = $val;
        if (checkRelative($outdir)) {
            $build_basedir = getcwd();
            chomp $build_basedir;
            $build_basedir .= "/" . $outdir;
        } else {
            $build_basedir = $outdir;
        }
        normalizePath(\$build_basedir);
    }
}

# if we have no $basedir we cannot be sure which sources you want, so die
die "Could not find any sync.profile for your module!\nPass <module directory> to syncqt to sync your header files.\nsyncqt failed" if (!$basedir);
die "The -version argument is mandatory" if (!$module_version);

$build_basedir = $out_basedir if (!defined($build_basedir));

our @ignore_headers = ();
our @ignore_for_include_check = ();
our @ignore_for_qt_begin_namespace_check = ();
our @ignore_for_qt_module_check = ();
our %inject_headers = ();

# load the module's sync.profile here, before we can
loadSyncProfile();

@modules_to_sync = keys(%modules) if($#modules_to_sync == -1);

for my $p (keys %inject_headers) {
    push @ignore_for_include_check, @{$inject_headers{$p}};
}

my %allmoduleheadersprivate = map { $_ => 1 } @allmoduleheadersprivate;
%ignore_for_include_check = map { $_ => 1 } @ignore_for_include_check;
%ignore_for_qt_begin_namespace_check = map { $_ => 1 } @ignore_for_qt_begin_namespace_check;

$isunix = checkUnix; #cache checkUnix

foreach my $lib (@modules_to_sync) {
    die "No such module: $lib" unless(defined $modules{$lib});

    #iteration info
    my $module = $modules{$lib};
    my $is_qt = !($module =~ s/^!//);
    my @dirs = split(/;/, $module);
    my $dir = $dirs[0];
    shift @dirs if ($dir =~ s/^>//);

    my $pathtoheaders = "";
    $pathtoheaders = $moduleheaders{$lib} if ($moduleheaders{$lib});

    my $allheadersprivate = 0;
    $allheadersprivate = 1 if $allmoduleheadersprivate{$lib};

    #information used after the syncing
    my $pri_install_gfiles = "";
    my $pri_install_files = "";
    my $pri_install_pfiles = "";
    my $pri_install_qpafiles = "";
    my $pri_injections = "";
    my $pri_clean_files = "";

    my $libcapitals = uc($lib);
    my %master_contents = ();

    #remove the old files
    if ($remove_stale && !$minimal) {
        my %injections = ();
        for my $p (keys %inject_headers) {
            next unless ($p =~ /^\Q$dir\E(\/|$)/);
            my $sp = $p;
            $sp =~ s,^\Q$basedir\E/,$build_basedir/,;
            for my $n (@{$inject_headers{$p}}) {
                $injections{$sp."/".$n} = 1;
            }
        }
        my @subdirs = ("$out_basedir/include/$lib");
        foreach my $subdir (@subdirs) {
            if (opendir DIR, $subdir) {
                foreach my $t (sort { $b cmp $a } readdir(DIR)) {
                    next if ($t =~ /\.pri$/);
                    next if ($t =~ /^qt[a-z0-9]+-config(_p)?\.h$/);
                    my $file = "$subdir/$t";
                    if(-d $file) {
                        push @subdirs, $file unless($t eq "." || $t eq "..");
                    } else {
                        my @files = ($file);
                        #push @files, "$out_basedir/include/Qt/$t" if(-e "$out_basedir/include/Qt/$t");
                        foreach my $file (@files) {
                           my $remove_file = 0;
                           local $/ = "\x0a";
                           if(open(F, "<$file")) {
                                while(my $line = <F>) {
                                    $line =~ s/\x0d?\x0a//;
                                    if($line =~ /^\#include \"([^\"]*)\"$/) {
                                        my $include = $1;
                                        $include = $subdir . "/" . $include unless(substr($include, 0, 1) eq "/");
                                        $remove_file = 1 unless(-e $include or defined $injections{cleanPath($include)});
                                    } else {
                                        $remove_file = 0;
                                        last;
                                    }
                                }
                                close(F);
                                unlink $file if($remove_file);
                            }
                        }
                    }
                }
                closedir DIR;
            }

        }
    }

    #create the new ones
    foreach my $current_dir (@dirs) {
        my $thisprivate = 0;
        ($current_dir =~ s/^\^//) and $thisprivate = 1;
        my @headers_paths = split(/;/, $pathtoheaders);
        if (@headers_paths) {
            @headers_paths = map { "$current_dir/$_" } @headers_paths;
        } else {
            push @headers_paths, $current_dir;
        }

        foreach my $headers_dir (@headers_paths) {
            #calc subdirs
            my @subdirs = listSubdirs($headers_dir);

            #calc files and "copy" them
            foreach my $subdir (@subdirs) {
                my @headers = findFiles($subdir, "^[-a-z0-9_]*\\.h\$");
                @headers = grep(!/^qt[a-z0-9]+-config(_p)?\.h$/, @headers);
                if (defined $inject_headers{$subdir}) {
                    foreach my $if (@{$inject_headers{$subdir}}) {
                        my $cif = $if;
                        $cif =~ s/^\^//;
                        @headers = grep(!/^\Q$cif\E$/, @headers); #in case we configure'd previously
                        push @headers, "*".$if;
                    }
                }
                my $header_dirname = "";
                foreach my $header (@headers) {
                    my $shadow = ($header =~ s/^\*//);
                    my $no_stamp = $shadow && ($header =~ s/^\^//);
                    $header = 0 if ($header =~ /^ui_.*\.h$/);
                    foreach (@ignore_headers) {
                        $header = 0 if($header eq $_);
                    }
                    if($header) {
                        my $header_copies = 0;
                        #figure out if it is a public header
                        my $public_header = $header;
                        my $qpa_header = 0;
                        if(isQpaHeader($public_header)) {
                            $public_header = 0;
                            $qpa_header = 1;
                        } elsif ($allheadersprivate || $thisprivate || isPrivateHeader($public_header)) {
                            $public_header = 0;
                        }

                        my $clean_header;
                        my $requires;
                        my $iheader_src = $subdir . "/" . $header;
                        my $iheader = $iheader_src;
                        $iheader =~ s/^\Q$basedir\E/$build_basedir/ if ($shadow);
                        if ($check_includes) {
                            # We need both $public_header and $private_header because QPA headers count as neither
                            my $private_header = !$public_header && !$qpa_header
                                && $header =~ /_p\.h$/ && $subdir !~ /3rdparty/;
                            check_header($lib, $header, $iheader, $public_header, $private_header);
                        }
                        my @classes = ();
                        push @classes, classNames($iheader, \$clean_header, \$requires)
                            if (!$shadow && $public_header && !$minimal && $is_qt);
                        my $classname = $classnames{$header};
                        push @classes, split(/,/, $classname) if ($classname);
                        if($showonly) {
                            print "$header [$lib]\n";
                            foreach(@classes) {
                                print "SYMBOL: $_\n";
                            }
                        } else {
                            my $ts = $shadow ? 0 : (stat($iheader))[9];
                            #find out all the places it goes..
                            my $oheader;
                            if ($public_header) {
                                $oheader = "$out_basedir/include/$lib/$header";
                                foreach my $full_class (@classes) {
                                    my $header_base = basename($header);
                                    # Strip namespaces:
                                    my $class = $full_class;
                                    $class =~ s/^.*:://;
    #                               if ($class =~ m/::/) {
    #                                  class =~ s,::,/,g;
    #                               }

                                    $header_copies++ if (!$shadow && syncHeader($lib, "$out_basedir/include/$lib/$class", "$out_basedir/include/$lib/$header", 0, $ts));
                                }
                            } elsif (!$qpa_header) {
                                $oheader = "$out_basedir/include/$lib/$module_version/$lib/private/$header";
                            } else {
                                $oheader = "$out_basedir/include/$lib/$module_version/$lib/qpa/$header";
                            }
                            $header_copies++ if (!$shadow && syncHeader($lib, $oheader, $iheader, $copy_headers, $ts));

                            my $pri_install_iheader = fixPaths($iheader_src, $dir);
                            my $injection = "";
                            if ($public_header) {
                                foreach my $class (@classes) {
                                    # Strip namespaces:
                                    $class =~ s/^.*:://;
#                                   if ($class =~ m/::/) {
#                                       $class =~ s,::,/,g;
#                                   }
                                    my $class_header = "$class ";
                                    $pri_install_gfiles .= $class_header
                                        unless ($shadow || $pri_install_gfiles =~ m/\b$class_header/);
                                    $injection .= ":$class";
                                }

                                if (!$shadow) {
                                    # put it into the master file
                                    $master_contents{$public_header} = $requires if (shouldMasterInclude($iheader));

                                    # deal with the install directives
                                    $pri_install_files .= "$pri_install_iheader ";
                                    $pri_clean_files .= "$pri_install_iheader".($requires ? ":".$requires : "")." " if ($clean_header);
                                }
                            }
                            elsif ($qpa_header) {
                                $pri_install_qpafiles.= "$pri_install_iheader ";;
                            }
                            elsif (!$shadow) {
                                $pri_install_pfiles.= "$pri_install_iheader ";;
                            }
                            $pri_injections .= fixPaths($iheader, $build_basedir)
                                               .":".($no_stamp ? "^" : "").fixPaths($oheader, "$out_basedir/include/$lib")
                                               .$injection." " if ($shadow);
                        }

                        if ($verbose_level && $header_copies) {
                            my $new_header_dirname = dirname($iheader);
                            basePrettify(\$new_header_dirname) if ($new_header_dirname && $verbose_level < 2);
                            my $header_base = basename($iheader);
                            if ($verbose_level < 3) {
                                my $line_prefix = ",";
                                if ($new_header_dirname ne $header_dirname) {
                                    $line_prefix = "$lib: created fwd-include header(s) for $new_header_dirname/ {";
                                    $line_prefix = " }\n".$line_prefix if ($header_dirname);
                                    $header_dirname = $new_header_dirname;
                                } else {
                                    $line_prefix = ",";
                                }
                                print "$line_prefix $header_base ($header_copies)";
                            } else { # $verbose_level >= 3
                                basePrettify(\$iheader) if ($verbose_level == 3);
                                print "$lib: created $header_copies fwd-include headers for $iheader\n";
                            }
                        }
                    }
                }
                print " }\n" if ($header_dirname && $verbose_level > 0 && $verbose_level < 3);
            }
        }
    }

    # populate the master include:
    my $master_contents =
        "#ifndef QT_".$libcapitals."_MODULE_H\n" .
        "#define QT_".$libcapitals."_MODULE_H\n" .
        "#include <$lib/${lib}Depends>\n" .
        join("", map {
            my $rq = $master_contents{$_};
            ($rq ? "#if QT_CONFIG($rq)\n" : "") .
            "#include \"$_\"\n" .
            ($rq ? "#endif\n" : "")
        } sort globosort keys %master_contents) .
        "#include \"".lc($lib)."version.h\"\n" .
        "#endif\n";

    unless ($showonly || $minimal || !$is_qt) {
        # create deprecated headers
        my $first = 1;
        while (my ($header, $include) = each %{$deprecatedheaders{$lib}}) {
            die "Attempting unsupported aliasing of private header $header.\n"
                if ($allheadersprivate || ($header =~ /_p\.h$/));

            my $header_path = "$out_basedir/include/$lib/$header";
            unless (-e $header_path) {
                my $guard = "DEPRECATED_HEADER_" . $lib . "_" . $header;
                $guard =~ s/([^a-zA-Z0-9_])/_/g;

                my $header_dir = dirname($header_path);
                make_path($header_dir, $lib, $verbose_level);

                my $warning = "Header <$lib/$header> is deprecated. Please include <$include> instead.";
                my $hdrcont =
                    "#ifndef $guard\n" .
                    "#define $guard\n" .
                    "#if defined(__GNUC__)\n" .
                    "#  warning $warning\n" .
                    "#elif defined(_MSC_VER)\n" .
                    "#  pragma message (\"$warning\")\n" .
                    "#endif\n" .
                    "#include <$include>\n" .
                    "#if 0\n" .
                    "#pragma qt_no_master_include\n" .
                    "#endif\n" .
                    "#endif\n";
                if (writeFile($header_path, $hdrcont)) {
                    if ($verbose_level < 3) {
                        my $line_prefix = ",";
                        $line_prefix = "$lib: created deprecated header(s) {" if ($first);
                        print "$line_prefix $header";
                    } else {
                        print "$lib: created deprecated header $header => $include\n";
                    }
                    $first = 0;
                }
            }

            $pri_install_gfiles .= "$header ";
        }
        if ($verbose_level < 3) {
            print " }\n" unless ($first);
        }

        # module version header
        my $vheader = "$out_basedir/include/$lib/".lc($lib)."version.h";
        my $VHeader = "$out_basedir/include/$lib/${lib}Version";
        syncHeader($lib, $VHeader, $vheader, 0);
        $pri_install_gfiles .= lc($lib)."version.h ${lib}Version ";
        my @versions = split(/\./, $module_version);
        my $modulehexstring = sprintf("0x%02X%02X%02X", $versions[0], $versions[1], $versions[2]);
        my $vhdrcont =
            "/* This file was generated by syncqt. */\n".
            "#ifndef QT_".uc($lib)."_VERSION_H\n".
            "#define QT_".uc($lib)."_VERSION_H\n".
            "\n".
            "#define ".uc($lib)."_VERSION_STR \"".$module_version."\"\n".
            "\n".
            "#define ".uc($lib)."_VERSION ".$modulehexstring."\n".
            "\n".
            "#endif // QT_".uc($lib)."_VERSION_H\n";
        writeFile($vheader, $vhdrcont, $lib, "version header");

        my $master_include = "$out_basedir/include/$lib/$lib";
        $pri_install_gfiles .= "$lib ";
        writeFile($master_include, $master_contents, $lib, "master header");
    }

    unless ($showonly || $minimal) {
        #handle the headers.pri for each module
        my $headers_pri_contents = "";
        $headers_pri_contents .= "SYNCQT.HEADER_FILES = $pri_install_files\n";
        $headers_pri_contents .= "SYNCQT.GENERATED_HEADER_FILES = $pri_install_gfiles\n";
        $headers_pri_contents .= "SYNCQT.PRIVATE_HEADER_FILES = $pri_install_pfiles\n";
        $headers_pri_contents .= "SYNCQT.QPA_HEADER_FILES = $pri_install_qpafiles\n";
        $headers_pri_contents .= "SYNCQT.CLEAN_HEADER_FILES = $pri_clean_files\n";
        $headers_pri_contents .= "SYNCQT.INJECTIONS = $pri_injections\n";
        my $headers_pri_file = "$out_basedir/include/$lib/headers.pri";
        writeFile($headers_pri_file, $headers_pri_contents, $lib, "headers.pri file");
    }
}

exit 0;
