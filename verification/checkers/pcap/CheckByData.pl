#!/usr/bin/perl -w
# This script receives a result pcap file and an expected pcap file,
# and returns true if the files contain the same packet data, and false otherwise.
# This means that the result and the expected pcap files contain the same data,
# not necessarily in the same order.
#
# Usage: CheckByData.pl <result.pcap> <expected.pcap>
# Example: CheckByData.pl res_0_10GE_0.pcap exp_0_10GE_0.pcap

use strict;
use warnings;
use FindBin;                # where was script installed?
use lib $FindBin::Bin;      # use that dir for libs, too
use PacketContainer;
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

my $resultPacketContainer = new PacketContainer($ARGV[0]);
my $expectedPcapIterator = new PcapIterator($ARGV[1]);

my $pass = 1;
my $differentPackets = 0;
print "\nComparing $ARGV[0] to expected $ARGV[1]\n";

if (not $resultPacketContainer->{_pcapReader}->getGlobalHeader()->isEqual($expectedPcapIterator->{_pcapReader}->getGlobalHeader())) {
	$pass = 0;
	print "Global headers differ!\nExpected:\n";
	$expectedPcapIterator->{_pcapReader}->getGlobalHeader()->toString();
	print "Result:\n";
	$resultPacketContainer->{_pcapReader}->getGlobalHeader()->toString();
}

while ($expectedPcapIterator->hasNext()) {
	my $packet = $expectedPcapIterator->next();
	my @results = $resultPacketContainer->getPacketsOffsetByData($packet);
	my $result = 0;
	my $index = 0;
	while ($index < @results) {
		my $resultPacket = $resultPacketContainer->getPacketByOffset($results[$index]);
		if ($resultPacket->isEqual($packet)) {
			$result = $resultPacket;
			$resultPacketContainer->removePacketByIndexFromData($resultPacket, $index);
			last;
		}
		$index++;
	}
	if ( $result == 0 ) {
		print "\nPacket not found - \n";
		$packet->printPacket();
		$pass = 0;
		$differentPackets++;
		if ($differentPackets == $MAX_DIFFERENT_PACKETS) {
			print "\n.\n.\n.\n";
			last;
		}
		next;
	}
}

if (!$resultPacketContainer->isEmpty()){
	print "\nResult pcap file contains more packets than expected.\n";
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

