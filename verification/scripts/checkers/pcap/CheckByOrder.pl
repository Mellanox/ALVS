#!/usr/bin/perl -w
# This script receives a result pcap file and an expected pcap file,
# and returns true if the files are equal, and false otherwise.
# This means that both files contain the same packets, in the same order.
#
# Usage: CheckByOrder.pl <result.pcap> <expected.pcap>
# Example: CheckByOrder.pl res_0_10GE_0.pcap exp_0_10GE_0.pcap

use strict;
use warnings;
use FindBin;                # where was script installed?
use lib $FindBin::Bin;      # use that dir for libs, too
use PcapIterator;

my $MAX_DIFFERENT_PACKETS		 = 10;

if (not defined $ARGV[1]) {
	print "Missing arguments!\nUsage:   $0 <result.pcap> <expected.pcap>\n";
	print "Example: $0 res_0_10GE_0.pcap exp_0_10GE_0.pcap\n";
	exit (-1);
}
if (not -e $ARGV[0]) {
	print "File $ARGV[0] does not exist!\n";
	exit(-1);
}
if (not -e $ARGV[1]) {
	print "File $ARGV[1] does not exist!\n";
	exit(-1);
}

my $resultPcapIterator = new PcapIterator($ARGV[0]);
my $expectedPcapIterator = new PcapIterator($ARGV[1]);

my $pass = 1;
my $orderInFile = 0;
my $differentPackets = 0;

if (not $resultPcapIterator->{_pcapReader}->getGlobalHeader()->isEqual($expectedPcapIterator->{_pcapReader}->getGlobalHeader())) {
	$pass = 0;
	print "Global headers differ!\nExpected:\n";
	$expectedPcapIterator->{_pcapReader}->getGlobalHeader()->toString();
	print "Result:\n";
	$resultPcapIterator->{_pcapReader}->getGlobalHeader()->toString();
}

while ($expectedPcapIterator->hasNext()) {
	my $expectedPacket = $expectedPcapIterator->next();
	my $resultPacket;
	if ($resultPcapIterator->hasNext()) {
		$resultPacket = $resultPcapIterator->next();
	}
	else {
		print "The file $ARGV[1] contains more expected frames than given in $ARGV[0]\n";
		$pass = 0;
		last;
	}
	
	if (not $expectedPacket->isEqual($resultPacket)) {
		$expectedPacket->printDifference($resultPacket);
		$differentPackets++;
		$pass = 0;
	}
	if ($differentPackets == $MAX_DIFFERENT_PACKETS) {
		print "\n.\n.\n.\n";
		last;
	}
	$orderInFile++;
}

if ($resultPcapIterator->hasNext() && (not ($differentPackets == $MAX_DIFFERENT_PACKETS))) {
	print "The file $ARGV[0] contains more frames than expected in $ARGV[1]\n";
	$pass = 0;
}

if ($pass) {
	print "\nPASS!\n";
	exit(0);
}
else {
	print "\nFAIL!\n";
	exit(-1);
}

