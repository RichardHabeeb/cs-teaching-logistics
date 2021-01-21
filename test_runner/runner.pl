use Term::ANSIColor;

my $TimeOut = "";                               # Command to force timeout
my $TEST   = "tIJ";             # Name of test file (IJ is replaced by number)
my $ANSWER = "tIJ.t";           # Name of answer file (IJ is replaced by number)

# Blank-separated list of illegal files; wildcards permitted

my $PROGRAM = "./$NAME";        # Name of executable

my %WHICH;                      # Allow individual tests on command line
@WHICH{@ARGV}++
   if (@ARGV);

$SIG{HUP} = $SIG{INT} = $SIG{QUIT}
   = $SIG{TERM} = \&cleanup;
my @UNLINK;                                     # Files to delete on signal
my $TMPDIR = "/tmp/TEST.$NAME.$$";              # Name of temporary directory
sub cleanup {
   unlink (@UNLINK);                            # Delete files
   if (-e $TMPDIR) {                            # Delete temporary directory
      system ("/bin/chmod -R +w $TMPDIR");
      system ("/bin/rm -rf $TMPDIR");
   }
   exit;
}

$0 =~ s{^.*/}{};                                # Extract test type
my $TYPE = ($0 =~ m{^run_tests.pl}) ? "Public" : "Final";
print "$TYPE test script for $NAME\n";

&sectionHeader ("MAKING $NAME\n");

&makeProgram
   unless ($LANG eq "Perl");



$|++;
print "Each test is either passed or failed; there is no partial credit.\n\n"
    . "To execute the test labelled IJ, type the command:\n"
    . "     $TESTS_DIR/$TEST\n"
    . "The answer expected is in $TESTS_DIR/$ANSWER.\n";

&sectionHeader ("RUNNING TESTS\n");




sub cleanupTesting {
    if ($TYPE eq "Final") {
       print "\n";
       system ("rm -f $PROGRAM *.o")                # Cleanup if final script
          unless ($LANG eq "Perl");
    }

    if ($TYPE eq "Public") {                        # Reminder to students
       system ("$CLASS_BIN_DIR/checklog -noprint");
       system ("$CLASS_BIN_DIR/checkmake -noprint")
          unless ($LANG eq "Perl");
    }
}



##########
# Print section header for tests
sub sectionHeader {
   printf ("\n\n%10s %s\n", "==========", @_);
}


##########
# Print header for tests
sub header {
   printf ("\n%14s %s\n", "--------------", @_);
}


##########
# Print addition
sub addition {
   my ($points, $title) = @_;
   printf ("%3d point       $title\n", $points);
   return $points;
}


##########
# Print deduction
sub deduction {
   my ($points, $title) = @_;
   printf ("%3d point       $title\n", $points);
   return $points;
}


##########
# Print possible deduction
sub possibleDeduction {
   printf ("%18d %s\n", @_);
}


##########
# Run a test
sub runTest {
   my ($test, $title, $conds) = @_;
   my $where   = "$TESTS_DIR";                      # Where to find test files
   my $run     = "$CLASS_BIN_DIR/run";         # Utility to run program
   my $diff    = "/usr/bin/colordiff";          # Utility to compare files
   my $results = "/tmp/$NAME.$$";               # Results of test
   my $diffs   = "/tmp/diff.$$";                # Expected results vs. results
   my $errors  = "/tmp/errs.$$";                # Error messages generated
   my ($status, @conds, $points);

   my $testFile = "$where/$TEST";               # Name of test file
   $testFile    =~ s{IJ}{$test};
   my $answers  = "$where/$ANSWER";             # Name of answer file
   $answers     =~ s{IJ}{$test};

   return 0                                     # Either execute all tests or
      unless (keys %WHICH == 0                  #   only those on command line
	      || exists $WHICH{$test});

   (-r $testFile)
      || die ("$0: missing test file $testFile\n");

   push @UNLINK, $results, $errors, $diffs;     # Files to delete on signal

   if (-x $testFile) {
      $status = execute ($testFile, undef, $results, $errors);
   } elsif ($LANG eq "Perl") {
      $status = execute ($PROGRAM, $testFile, $results, $errors);
   } else {
      $status = execute ("$run $PROGRAM", $testFile, $results, $errors);
   }

   if (defined $conds && $conds eq "Graceful") {
      @conds = ('NORMAL', $status);
   } else {
      (-r $answers)
	 || die ("$0: missing answer file $answers\n");
      system ("$diff $answers $results  > $diffs  2>> $errors");
      @conds = ('NULL', $errors,  'NULL', $diffs);
   }

   if (defined $conds && $conds eq "Error message") {
      @conds = ('NONNULL', $errors,  'NULL', $diffs);
   }

   if (defined $conds && $conds =~ m{^Deduct=(\d+)$}) {
      @conds = ('DEDUCT', $1, @conds);
   }

   $points = &correct (@conds);
   printf ("%3d point  %3s. %s\n", $points, $test, $title);

   system ("rm -f $results $errors $diffs");
   pop @UNLINK;  pop @UNLINK;  pop @UNLINK;     # Remove added files

   return $points;
}


##########
#  correct ({[UNOP FILE] | ['NORMAL' STATUS]}*)
#
#  Return 1 if the conjunction of the specified tests is true, else 0, where:
#
#    UNOP FILE (where UNOP is either 'NULL' or 'NONNULL'):
#      Is the size of the file FILE as specified?
#
#    'NORMAL' STATUS:
#      Did the process terminate normally?
#
#    'DEDUCT' POINTS:
#      Change the point values to 0 for success, -POINTS for failure
#
sub correct {
   my $op;
   my ($success, $failure) = (1, 0);
   my $head    = "$CLASS_BIN_DIR/Head -c";     # Utility to output files

   while ($op = shift) {
      if ($op eq 'NULL') {
	 my $arg = shift;
	 print STDERR "$op $arg\n" if $debug;
	 if (-s $arg) {
	    if ($arg =~ m{/diff\.}) {
	       system ("$head $arg");
	       error ("STDOUT differs from expected");
	    } elsif ($arg =~ m{/errs\.}) {
	       system ("$head $arg");
	       error ("STDERR should be empty)");
	    } else {
	       error ("File $arg is nonempty");
	    }
	    $success = $failure;
	 }

      } elsif ($op eq 'NONNULL') {
	 my $arg = shift;
	 print STDERR "$op $arg\n" if $debug;
	 if (!-s $arg) {
	    if ($arg =~ m{/errs\.}) {
	       system ("$head $arg");
	       error ("STDERR should be nonempty");
	    } else {
	       error ("File $arg is empty");
	    }
	    $success = $failure;
	 }

      } elsif ($op eq 'NORMAL') {
	 my $arg = 127 & shift;
	 print STDERR "$op $arg\n" if $debug;
	 if ($arg != 0) {
	    error ("Status = $arg is nonzero");
	    $success = $failure;
	 }

      } elsif ($op eq 'DEDUCT') {
	 my $arg = shift;
	 ($success, $failure) = (0, -$arg);
      }
   }
   return $success;
}


##########
# Create program to test
sub makeProgram {
#  system ("rm -f $PROGRAM");                   # Delete program & object files
#  system ("rm -f *.o")
#     if ($TYPE eq "Final");

   (-f "Makefile" || -f "makefile")             # Give warning if no Makefile
      || warn ("$0: no makefile found\n");

   system ("$CLASS_BIN_DIR/makewarn $PROGRAM");
   ($? == 0)
      || die ("$0: cannot compile $PROGRAM\n");

   for my $p (split (" ", $PROGRAM)) {
      -e $p
	 || die ("$0: could not make $p\n");
   }
}


##########
# Limit CPU-time, wall-clock-time, file-size, and/or heap-size
use BSD::Resource;

sub limitCpuTime { # (time in seconds)
   my ($soft, $hard) = @_;
   $hard = $soft
      if (! defined($hard));
   setrlimit (RLIMIT_CPU, $soft, $hard);
}

sub limitWallClock { # (time in seconds)
   my ($wall) = @_;                             # TERM after WALL seconds;
   $TimeOut = "timeout -k $wall $wall";         #   KILL after 2*WALL seconds
}

sub limitFileSize { # (size in kilobytes)
   my ($size) = @_;
   $size *= 1024;
   setrlimit (RLIMIT_FSIZE, $size, $size);
}

sub limitHeapSize { # (size in kilobytes        # Bug: Has no effect
   my ($size) = @_;
   $size *= 1024;
   setrlimit (RLIMIT_VMEM, $size, $size);
}

sub limitProcesses { # (#processes)             # Bug: Has no effect
   my ($nproc) = @_;
   setrlimit (RLIMIT_NPROC, $nproc, $nproc);
}


##########
# Execute program after redirecting stdin, stdout, & stderr and return status
sub execute {
   my ($program, $stdin, $stdout, $stderr) = @_;
   my ($pid, $status);

   (defined ($pid = fork))                      # Create child process
      || die ("$0: fork failed\n");

   if ($pid == 0) {                             # Child process
      open (STDIN, "<$stdin")                   #  Redirect stdin
	 if (defined $stdin);
      open (STDOUT, ">$stdout")                 #  Redirect stdout
	 if (defined $stdout);
      open (STDERR, ">$stderr")                 #  Redirect stderr
	 if (defined $stderr);
      mkdir ($TMPDIR)                           #  Create a temporary directory
	 || die ("$0: mkdir $TMPDIR failed\n");
      system ("/bin/cp $PROGRAM $TMPDIR");      #    With a copy of the program
      chdir ("$TMPDIR")                         #    And cd there
	 || die ("$0: chdir $TMPDIR failed\n");
      (exec "$TimeOut $program")                #  Execute command
	 ||  die ("$0: exec failed\n");
   }

   waitpid ($pid, 0);                           # Wait for child to die,
   $status = $?;

   system ("/bin/chmod -R +w $TMPDIR");         # Delete temporary directory
   system ("/bin/rm -rf $TMPDIR");
   (! -e $TMPDIR)
      || die ("$0: cannot delete $TMPDIR\n");

   return $status;                              # ... and return exit status
}


##########
# $ILLEGAL_FILES is a blank-separated list of filenames which may include wildcards.
# If any of these files exist in the current working directory, print their
# names and return -1; else return 0.
sub localCopies { # ($files)
   my ($ILLEGAL_FILES) = @_;
   open (LS, "ls -l $ILLEGAL_FILES 2>/dev/null |")
       || die ("$0: cannot ls -l $ILLEGAL_FILES\n");
   my @ls = <LS>;
   close (LS);
   print @ls;
   return (@ls > 0) ? -2 : 0;
}


##########
# Print error message in color.
sub error { # ($message)
   my ($msg) = @_;
   print colored ("Error: $msg", "blue") . "\n";
}
