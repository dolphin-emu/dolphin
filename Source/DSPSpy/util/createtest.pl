#!/usr/bin/perl -w

use strict;
use XML::Simple;
use Getopt::Long;

use Data::Dumper;

sub usage() {
	die("createtest -i <test template> -c <command> \n");
}

sub parseString {
	my $string = shift;
	my $cmd = shift;
	$string =~ s/\@CMD\@/$cmd/gi;
	return $string;
}

sub generateSRFull {
	my $res = shift;
	my $body = shift;
	
    $res .= join "\n", map {my $b = sprintf "\#0x%04X", $_; (my $a = $body) =~ s/\@SR\@/$b/g; $a} 1..65535;

	return $res;
}

sub generateTest {
	my $type = shift;
	my $header = shift;
	my $body = shift;

	if ($type eq "srfull") {
		return generateSRFull($header, $body);
	}
}

sub calculateLines {
	my $text = shift;
	my $lines = 0;
	my $incdir = "./";
	
	foreach my $line (split /\n/, $text) {
	    next if ($line =~ /^\S*$/);
		next if ($line =~ /^\S*;/);
		next if ($line =~ /:/);

		if ($line =~ /incdir\s*\"(.*)\"/) {
			$incdir = $1;
		} elsif ($line =~ /include\s*\"(.*)\"/) {
			my $incname = "$incdir/$1";
			open(INCLUDE, "<$incname") ||
				die("Can't open include file $incname: $!\n");
   
			my $include = join "", <INCLUDE>;
			$lines += calculateLines($include);
		} else {
			$lines++;
		}
	}

	return $lines;
}

my ($cmds,$input,$output);
if (!GetOptions('cmds|c=s'   => \$cmds,
				'input|i=s'  => \$input,
#                'output|o=s' => \$output,
   )) {
  usage();
  exit 1;
}

usage() if (! defined $input);

my $xtest = XMLin($input);
my $type = $xtest->{'type'};

foreach my $cmd (split(/,/, $cmds)) {
	my $name = parseString($xtest->{'name'}, $cmd);
	$name =~ s/ /_/g;
	my $desc = parseString($xtest->{'description'}, $cmd);

	my $header = parseString($xtest->{'header'}, $cmd);
	my $hsize = calculateLines($header);

	my $body = parseString($xtest->{'body'}, $cmd);
	my $bsize = calculateLines($body);

	open(OUTPUT, ">$name.ds") ||
		die("Can't open file $name for writing: $!\n");
	
	print OUTPUT "; $name\n";
	print OUTPUT "; $desc\n";
	my $test = generateTest($type, $header, $body);
	print OUTPUT $test . "\n";
	
}

