#!/usr/bin/perl -w

use strict;
use XML::Simple;
use Getopt::Long;
use POSIX qw(ceil floor);

use Data::Dumper;

my $merge = 0;

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
	my $start = shift;
	my $end = shift;
	$end = 0xFFFF if ($end > 0xFFFF);

    $res .= join "\n", map {my $b = sprintf "\#0x%04X", $_; (my $a = $body) =~ s/\@SR\@/$b/g; $a} $start..$end;

	return $res;
}

sub calculateLines {
	my $file = shift;

	my @lines = `dsptool -s $file`;
	$lines[0] =~ /:\s*(.*)/;
	my $lnum = $1;
	
	return $lnum;
}

my ($cmds,$input,$output);
if (!GetOptions('cmds|c=s'   => \$cmds,
				'input|i=s'  => \$input,
				'merge|m'    => \$merge,
#                'output|o=s' => \$output,
   )) {
  usage();
  exit 1;
}

usage() if (! defined $input);

my $xtest = XMLin($input);
my $type = $xtest->{'type'};

my @cmdList = split(/,/, $cmds);

for(my $i = 0;$i < scalar(@cmdList);$i++) {
	my $cmd = $cmdList[$i];
	my $name = parseString($xtest->{'name'}, $cmd);
	$name =~ s/ /_/g;
	my $desc = parseString($xtest->{'description'}, $cmd);

	my $header = parseString($xtest->{'header'}, $cmd);
	open(OUTPUT, ">$name.tmp");
	print OUTPUT $header;
	close(OUTPUT);

	my $hsize = calculateLines("$name.tmp");

	my $body = parseString($xtest->{'body'}, $cmd);
	open(OUTPUT, ">$name.tmp");
	print OUTPUT generateSRFull($header, $body, 0, 0);
	close(OUTPUT);	

	my $bsize = calculateLines("$name.tmp") - $hsize;
	unlink("$name.tmp");
	
	# how many tests we can fit in a ucode.
	my $ucodes = POSIX::floor((0x1000 - $hsize) / $bsize);

	if ($merge) {
		open(NAMES, ">>all.lst");
	} else {
		open(NAMES, ">$name.lst");
	}

	my $numLines = POSIX::ceil(0xFFFF / $ucodes);
	for(my $j = 0; $j < $numLines; $j++) {
		open(OUTPUT, ">$name$j.tst");
		print OUTPUT generateSRFull($header, $body, $j*$ucodes, 
									($j+1)*$ucodes-1);
		print OUTPUT "jmp end_of_test";
		close(OUTPUT);	 
		print NAMES "$name$j.tst";
		
		# Don't end with a newline
		if ($j < $numLines - 1 || ($merge && $i != scalar(@cmdList)-1)) {
			print NAMES "\n";
		}
	}

	close(NAMES);

	if (! $merge) {
		print `dsptool -m $name.lst -h $name`;
		open(NAMES, "$name.lst");
		my @names = <NAMES>;
		chomp @names;
		unlink @names;
		close(NAMES);
		unlink("$name.lst");
	}
}

if ($merge) {
	print `dsptool -m all.lst -h unite_test`;
	open(NAMES, "all.lst");
	my @names = <NAMES>;
	chomp @names;
	unlink @names;
	close(NAMES);
	unlink "all.lst";
}
