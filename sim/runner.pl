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

  #XXX: Could avoid re-compilation by comparing timestamps of $input and $test.so.
  print "  Compiling test simulation ... ";
  if (system("clang++ -x c++ -std=c++14 -shared -fPIC -O0 -g3 -Wall -Werror -I. $input -o $test.so")) {
    print "failed\n";
    next;
  }
  print "success\n";

  print "  Compiling test firmware   ... ";
  if (system("clang --target=riscv32 -march=rv32ic -mno-relax -std=c99 -DCOMPILE_FIRMWARE=1 -Wall -Werror -O3 $input -c -o $test.o -I../sw > $test.comp.log 2>&1") ||
      system("llvm-mc --arch=riscv32 -mcpu=generic-rv32 -mattr=+c -assemble ../sw/start.S --filetype=obj -o start.o") ||
      system("ld.lld -T ../sw/system.ld start.o $test.o -o $test.elf") ||
      system("llvm-objcopy --output-target=binary $test.elf $test.bin")) {
    print "failed\n";
    next;
  }
  print "success\n";

  print "  Running simulation        ... ";
  $simres = system("./usb-sim ./$test.so $test.bin > $test.sim.log 2>&1");
  system("mv dump.vcd $test.sim.vcd");
  system("sigrok-cli -i dump.csv -I csv:samplerate=48000000 -o $test.sim.sr");

  if ($simres) {
    print "failed\n";
    next;
  }
  print "success\n";

  $passed = $passed + 1;
}

print "\n---\nPassed ($passed/$total)\n";
