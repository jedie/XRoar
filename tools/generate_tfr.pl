#!/usr/bin/perl

my %regs = (
	0 => 'reg_d',
	1 => 'reg_x',
	2 => 'reg_y',
	3 => 'reg_u',
	4 => 'reg_s',
	5 => 'reg_pc',
	8 => 'reg_a',
	9 => 'reg_b',
	10 => 'reg_cc',
	11 => 'reg_dp',
);

sub is_valid { my $r = shift; return (defined $regs{$r}); }
sub is_16bit { my $r = shift; return ($r >= 0 && $r <= 5); }
sub is_8bit { my $r = shift; return ($r >= 8 && $r <= 11); }

my %done = ();

print "/* 0x1E EXG immediate */\n";
print "case 0x1e: {\n";
print "\tunsigned int postbyte;\n";
print "\tunsigned int tmp;\n";
print "\tBYTE_IMMEDIATE(0,postbyte);\n";
print "\tswitch (postbyte & 0xff) {\n";
for my $s (0..15) {
	for my $d (0..15) {
		my $postbyte = sprintf("%x%x", $s, $d);
		my $revpost = sprintf("%x%x", $d, $s);
		next if ($s == $d);
		next unless (is_valid($s) || is_valid($d));
		next if ($done{$postbyte});
		$done{$postbyte} = $done{$revpost} = 1;
		print "\t\tcase 0x$postbyte: case 0x$revpost:";
		if (is_valid($s) && !is_valid($d)) {
			if (is_8bit($s)) { print " $regs{$s} = 0xff;"; }
			else { print " $regs{$s} = 0xffff;"; }
		} elsif (!is_valid($s) && is_valid($d)) {
			if (is_8bit($d)) { print " $regs{$d} = 0xff;"; }
			else { print " $regs{$d} = 0xffff;"; }
		} else {
			print " tmp = $regs{$d};";
			if (is_8bit($s) && is_8bit($d)) {
				print " $regs{$d} = $regs{$s}; $regs{$s} = tmp;";
			} elsif (is_16bit($s) && is_16bit($d)) {
				print " $regs{$d} = $regs{$s}; $regs{$s} = tmp;";
			} elsif (is_8bit($s) && is_16bit($d)) {
				print " $regs{$d} = 0xff00 | $regs{$s}; $regs{$s} = tmp & 0xff;";
			} else {
				print " $regs{$d} = $regs{$s} & 0xff; $regs{$s} = 0xff00 | tmp;";
			}
		}
		print " break;\n";
	}
}
print "\t\tdefault: break;\n";
print "\t}\n";
print "\tTAKEN_CYCLES(6);\n";
print "} break;\n";

print "/* 0x1F TFR immediate */\n";
print "case 0x1f: {\n";
print "\tunsigned int postbyte;\n";
print "\tBYTE_IMMEDIATE(0,postbyte);\n";
print "\tswitch (postbyte & 0xff) {\n";
for my $s (0..15) {
	for my $d (0..15) {
		my $postbyte = sprintf("%x%x", $s, $d);
		next if ($s == $d);
		next unless (is_valid($d));
		print "\t\tcase 0x$postbyte:";
		if (!is_valid($s)) {
			if (is_8bit($d)) {
				print " $regs{$d} = 0xff;";
			} else {
				print " $regs{$d} = 0xffff;";
			}
		} else {
			if (is_8bit($s) && is_8bit($d)) {
				print " $regs{$d} = $regs{$s};";
			} elsif (is_16bit($s) && is_16bit($d)) {
				print " $regs{$d} = $regs{$s};";
			} elsif (is_8bit($s) && is_16bit($d)) {
				print " $regs{$d} = 0xff00 | $regs{$s};";
			} else {
				print " $regs{$d} = $regs{$s} & 0xff;";
			}
		}
		print " break;\n";
	}
}
print "\t\tdefault: break;\n";
print "\t}\n";
print "\tTAKEN_CYCLES(4);\n";
print "} break;\n";

