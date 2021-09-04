#!/usr/bin/perl

if(@ARGV == 2){
  &CmpAny($ARGV[0],$ARGV[1]);
}else{
  for(my $i=2;$i<@ARGV;$i++){
    &CmpAny($ARGV[0] . "/" . $ARGV[$i],
	    $ARGV[1] . "/" . $ARGV[$i]);
  }
}

sub CmpAny{
  if(-d $_[0]){
    (-d $_[1]) or die "Comparing directory $_[0] with non-directory $_[1]\n";
    &CmpDirs($_[0],$_[1]);
  }else{
    (!(-d $_[1])) or die "Comparing non-directory $_[0] with directory $_[1]\n";
    &CmpFiles($_[0],$_[1]);
  }
}

sub CmpDirs{
  opendir Dir1, $_[0] or die "Could not open directory $_[0]";
  opendir Dir2, $_[1] or die "Could not open directory $_[1]";
  @List1=sort readdir(Dir1);
  @List2=sort readdir(Dir2);
  for(my $i=0;$i<@List1;$i++){
    if($List1[$i] ne $List2[$i]){
      print "Directories $_[0] and $_[1] have different files\n";
      exit(1);
    }
    (($List1[$i] ne ".")&&($List1[$i] ne "..")) or next;
    &CmpAny("$_[0]/$List1[$i]","$_[1]/$List2[$i]");
  }
  closedir(Dir1);
  closedir(Dir2);
}

sub CmpFiles{
  open InFile1, "< $_[0]"
    or die "Could not open input file $_[0]";
  open InFile2, "< $_[1]"
    or die "Could not open input file $_[1]";
  my $Line1;
  my $Line2;
  while(($Line1=<InFile1>)&&($Line2=<InFile2>)){
    my $Str1=$Line1;
    $Str1=~s/\s+$//;
    my $Str2=$Line2;
    $Str2=~s/\s+$//;
    while(($Str1 ne $Str2)&&($Str1 ne "")&&($Str2 ne "")){
      my $Match1;
      my $Rest1;
      $Str1=~/^(\s*)(.*)$/;
      my $Match1=$1;
      my $Rest1=$2;
      $Str2=~/^(\s*)(.*)$/;
      my $Match2=$1;
      my $Rest2=$2;
      if(($Match1 ne "")||($Match2 ne "")){
	# Do nothing, we stripped some white space in this iteration
      }elsif($Str1=~/^([\+\-]?((\d+(\.\d*)?)|(\d*\.\d+)))(([eEdD][\+\-]?\d+)?)/){
	my $Value1;
	my $Value2;
	$Match1=$&;
	$Rest1=$';
	($Str2=~/^([\+\-]?((\d+(\.\d*)?)|(\d*\.\d+)))(([eEdD][\+\-]?\d+)?)/) or last;
	$Match2=$&;
	$Rest2=$';
	if($Match1=~/^(.+)[eEdD](.+)$/){
	  $Value1=0.0+"$1e$2";
	}else{
	  $Value1=0.0+$Match1;
	}
	if($Match2=~/^(.+)[eEdD](.+)$/){
	  $Value2=0.0+"$1e$2";
	}else{
	  $Value2=0.0+$Match2;
	}
	my $Dif=abs(abs($Value1)-abs($Value2));
	my $Sum=abs($Value1)+abs($Value2);
	(($Sum<1e-6)||(($Dif/$Sum)<1e-3)) or last;
      }elsif($Str1=~/^(\S)(.*)$/){
	$Match1=$1;
	$Rest1=$2;
	($Str2=~/^(\S)(.*)$/) or last;
	$Match2=$1;
        $Rest2=$2;
	($Match1 eq $Match2) or last;
      }
      $Str1=$Rest1;
      $Str2=$Rest2;
    }
    if($Str1 ne $Str2){
      print "Files $_[0] and $_[1] differ:\n< $Line1> $Line2";
      exit(1);
    }
  }
  if((!eof(InFile1))||(!eof(InFile2))){
    print "Files $_[0] and $_[1] don't have the same number of lines\n";
    exit(1);
  }
  close(InFile1);
  close(InFile2);
}

sub CmpFilesOld{
  open InFile1, "< $_[0]"
    or die "Could not open input file $_[0]";
  open InFile2, "< $_[1]"
    or die "Could not open input file $_[1]";
  my $Line1;
  my $Line2;
  while(($Line1=<InFile1>)&&($Line2=<InFile2>)){
    my @List1=split(/\s+/,$Line1);
    my @List2=split(/\s+/,$Line2);
    while((@List1>0)&&(@List2>0)){
      my $Token1=$List1[0];
      my $Token2=$List2[0];
      if($Token1 ne $Token2){
	($Token1=~/^([\+\-]?((\d+(\.\d*)?)|(\d*\.\d+)))(([eEdD][\+\-]?\d+)?)/) or last;
	push(@List1,$');
	$Token1=$&;
	($Token2=~/^([\+\-]?((\d+(\.\d*)?)|(\d*\.\d+)))(([eEdD][\+\-]?\d+)?)/) or last;
	push(@List2,$');
	$Token2=$&;
	my $Value1;
	if($Token1=~/^(.+)[eEdD](.+)$/){
	  $Value1=0.0+"$1e$2";
	}else{
	  $Value1=0.0+$Token1;
	}
	my $Value2;
	if($Token2=~/^(.+)[eEdD](.+)$/){
	  $Value2=0.0+"$1e$2";
	}else{
	  $Value2=0.0+$Token2;
	}
	my $Dif=abs(abs($Value1)-abs($Value2));
	my $Sum=abs($Value1)+abs($Value2);
	(($Sum<1e-6)||(($Dif/$Sum)<1e-3)) or last;
      }
      shift @List1;
      shift @List2;
    }
    if((@List1!=0)&&(@List2!=0)){
      print "Files $_[0] and $_[1] differ:\n< $Line1> $Line2";
      exit(1);
    }
  }
  if((!eof(InFile1))||(!eof(InFile2))){
    print "Files $_[0] and $_[1] don't have the same number of lines\n";
    exit(1);
  }
  close(InFile1);
  close(InFile2);
}
