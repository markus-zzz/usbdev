#!/usr/bin/perl -w

$total = 0;
$passed = 0;

if ($ENV{'PREFIX'}) {
  $prefix = $ENV{'PREFIX'};
} else {
  $prefix = '';
}

if (@ARGV) {
  @inputs = @ARGV;
} else {
  @inputs = glob("tests/test_[0-9]*\.c");
}

foreach $input (@inputs) {

  $total = $total + 1;

  $input =~ m/(test_[0-9]*)/;
  $test = $1;
  print "$test:\n";

  print "  Compiling firmware ... ";
  if (system("clang --target=riscv32 -DCOMPILE_FIRMWARE=1 -Wall -Werror -O3 $input -c -o $test.o -I../sw > $test.comp.log 2>&1") ||
      system("clang --target=riscv32 -Wall -Werror -O3 ../sw/start.c -c") ||
      system("ld.lld -T ../sw/system.ld start.o $test.o -o system.elf") ||
      system("llvm-objcopy --output-target=binary system.elf system.bin") ||
      system("hexdump -v -e '4/4 \"%08x \" \"\n\"' system.bin > rom.vh")) {
    print "failed\n";
    next;
  }
  print "success\n";

  print "  Running simulation ... ";
  if (system("./usb-sim $test > $test.sim.log 2>&1")) {
    print "failed\n";
    next;
  }
  print "success\n";
  system("sigrok-cli -i dump.csv -I csv:samplerate=48000000 -o $test.sim.sr");

  $passed = $passed + 1;
}

print "\n---\nPassed ($passed/$total)\n";
