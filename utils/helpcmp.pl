#!/usr/bin/perl
#$Id$
#

$page_offset = "2n";
$line_length  = "40n";
$indent = "5n";
    
# Collect command line args
while ($_ = $ARGV[0], /^-/) {
    shift;
    last if /^--$/;		
    /^-d(.*)/ && $debug++;
    /^-v/ && $verbose++;	
    /^-o(.*)/ && do {		
	$out_name=$1;
    };
    /^-ll(.*)/ && do {
	($line_length) = /^-ll([0-9]+[icpPumnv]?$)/;
    };
    /^-po(.*)/ && do {
	($page_offset) = /^-po([0-9]+[icpPumnv]?$)/;
    };
    /^-in(.*)/ && do {
	($indent) = /^-in([0-9]+[icpPumnv]?$)/;
    };
    /^-m(.*)/ && ($macrolib = $_);
}

# Set up various names
if ($ENV{"TEMP"}) {
    $tempdir = $ENV{"TEMP"};
} else {
    $tempdir = "/tmp";
}

if ($tempdir !~ /..*\//) {
    $tempdir=$tempdir."/";
}

$ttl_name = $tempdir."hcmp".$$.".title";
$txt_name = $tempdir."hcmp".$$.".text";
$ind_name = $tempdir."hcmp".$$.".ind";
$temp_name = $tempdir."hcmp".$$.".raw";

# Create output files
open(TITLES, ">".$ttl_name) or die "Can't open $ttl_name ($!)\n";
open(TEXT, ">".$txt_name) or die "Can't open $txt_name ($!)\n";
open(INDEX, ">".$ind_name) or die "Can't open $ind_name ($!)\n";
open_temp();


$count = -1;
$input = 100;	
OUTER: foreach $fn(@ARGV) {
    $filename = $fn;
    open($input, $filename) or die "Can't open $filename ($!)\n";
    $. = 1;
  INCLUDES:    while (1) {
    LOOP: while (<$input>) {
      SWITCH: {
	  /^%title.*/ && do {
	      if ($count>=0) {
		  $start_line = $.;
		  write_header_and_text();
		  open_temp();
	      }
	      $count++;
	      $written = 0;
	      $xlat = 0;
	      $xref_at = $xref_offset;
	      ($title) = /^%title[ \t]+(.*)$/;
	      last SWITCH;
	  };
	  /^%xref.*/ && do {
	      ($xref) = /^%xref[ \t]+(.*)$/;
	      if (!$refcount[$count]) {
		  $xref_offset++; # sizeof(long)
		  $total_refcount++;
	      }
	      $refcount[$count]++;
	      $xref_offset++;
	      $ref[$count] = $ref[$count]."%".$xref;
	      $reflines[$count] = $reflines[$count]."%".$filename.":".$.;
	      last SWITCH;
	  };
	  /^%include.*/ && do {
	      $includes[$itos++] = [$filename, $.];
	      ($filename) = /^%include[ \t]+(.*)/;
	      $input++;
	      warn "Can't open $filename ($!)\n" if (!open($input, $filename));
	      $. = 1;
	      debug("Switched to $filename\n");
	      last SWITCH;
	  };
	  /^%euc*/ && do {
	      $xlat = 1;
	      last SWITCH;
	  };
	  /^%%.*/ && last SWITCH;
	  /^%eof/ && last LOOP;
	  /^%.*/ && warn "$filename:$.:unknown directive: $_";
	  print TEMP &replace_upper_ascii($_);
      }
    }				
      if ($itos) {
	  --$itos;
	  --$input;
	  tell $input;
	  $filename = $includes[$itos][0];
	  $. = $includes[$itos][1];  
	  debug("Restored $filename:$.\n");
      } else {			 
	  last INCLUDES;	
      }
  }
}				
	
do {write_header_and_text(); $count++} unless ($written);

info("Collected $count help topics\n");

$titles_start = 6*4 + tell(INDEX);
$titles_length = tell(TITLES);
$xref_start = $titles_start + $titles_length;
$text_offset = $xref_start + 4*$xref_offset;
$total_size = $text_offset + tell(TEXT);
write_output();
close TITLES;
close TEXT;
close INDEX;
# close TEMP; Should be closed already
unlink $ttl_name, $txt_name, $ind_name, $temp_name;    

# ******************************************************
# Subroutines
#

sub write_header_and_text {
    close TEMP;
    $start = tell TEXT;
    print TEXT `tbl $temp_name | nroff $macrolib -Teuc-jp`;
    write_header($start, tell(TEXT)-$start+1, $title);
}
	
sub write_header {
    ($start, $length, $text) = @_;

    $topic{$text} = $count+1;
    $title_offset = tell TITLES;
    print TITLES pack("a*x", $text);
    $b = pack("LLLLL", $start, $length, $title_offset,
	      $refcount[$count] ? $xref_at : -1, $xlat);
    print INDEX $b;
    $written = 1;
}

sub write_output {
    if ($out_name) {
	warn "Can't open $out_name ($!)\n" if (!open(OUTPUT, ">".$out_name));
	info("Writing output file $out_name\n");
	select OUTPUT;
    } else {
	$out_name = "STDOUT";
    }
    $a = pack("LLLLLL",
	      $count, $text_offset, $xref_start, $xref_offset,
	      $titles_start, $titles_length);
    print $a;
    cat($ind_name,$ttl_name);
    
    info("Generating cross-references...\n");
#    print "xrbeg";
    for ($i=0; $i<$count; $i++) {
	if ($refcount[$i]) {
	    @array = split(/%/, $ref[$i]);
	    @lines = split(/%/, $reflines[$i]);
	    $rc = 0;
	    for ($j=1; $j<=$#array; $j++) {
		if ($topic{$array[$j]}) {
		    $refindex[$rc++] = $topic{$array[$j]}-1;
		} else {
		    warn "$lines[$j]:undefined xref $array[$j]\n";
		}
	    }
	    $v = pack("L", $rc);
	    print $v;
	    for ($j=0; $j<$refcount[$i]; $j++) {
		if ($j < $rc) {
#		    debug("$refindex[$j]\n");
		    $v = pack("L", $refindex[$j]);
		} else {
		    $v = pack("L", 0);
		}
		print $v;
	    }
	}
    }
#    print "xrend";
    info("Done\n");
    cat($txt_name);
    
    info("Wrote $total_size bytes to $out_name\n");
}

sub cat {
    print `cat @_`;
}

sub info {
    if ($verbose) {
	print STDERR @_;
    }
}

sub debug {
    if ($debug) {
	print STDERR "debug: @_";
    }
}
    
sub open_temp {
    open(TEMP, ">".$temp_name) or die "Can't open $temp_name ($!)\n";
    print TEMP ".po $page_offset\n";
    print TEMP ".in $indent\n";
    print TEMP ".ll $line_length\n";
    $start_line += 3;
    print TEMP ".lf $start_line $filename\n";
}

sub convert {
    my(@input) = @_;
    $rc = "";
    for ($i=0; $i < $#input; $i++) {
	$input[$i] =~ s/([\000-\177])\255$/$1-/;
	$rc = $rc.$input[$i];
    }
    return $rc;
}

sub replace_upper_ascii {
    my @input = split(//,$_);
    foreach my $c (@input) {
	if (ord($c) > 127) {
	    $c = "\\N'".ord($c)."'";
	}
    }
    return join('',@input);
}
    
