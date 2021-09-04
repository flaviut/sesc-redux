#!/usr/bin/env perl

my %BadCallCount;
my %GoodCallCount;

my $BadFileName=shift(@ARGV);
&ParseFile($BadFileName,\%BadCallCount);
foreach $GoodFileName (@ARGV){
  &ParseFile($GoodFileName,\%GoodCallCount);
}

foreach $FuncName (keys %BadCallCount){
  if(!exists $GoodCallCount{$FuncName}){
    printf("  None    vs %9d: %s\n",$BadCallCount{$FuncName},$FuncName);
  }elsif(($GoodCallCount{$FuncName}==0)&&($BadCallCount{$FuncName}!=0)){
    printf("%9d vs %9d: %s\n",$GoodCallCount{$FuncName},$BadCallCount{$FuncName},$FuncName);
  }
}

sub ParseFile{
  my $FileName=$_[0];
  my $CallCountRef=$_[1];
  printf "File: $FileName\n";
  open File, "< $FileName"
    or die "Unable to open input file $FileName\n";
  while(<File>){
    my $cnt=-1;
    my $fnc;
    if(/^\s+[\d\.]+\s+[\d\.]+\s+[\d\.]+\s+([\d]+)\s+[\d\.]+\s+[\d\.]+\s+([^\d\s].*)$/){
      $cnt=$1;
      $fnc=$2;
    }elsif(/^\s+[\d\.]+\s+[\d\.]+\s+[\d\.]+\s+([^\d\s].*)$/){
      $cnt=0;
      $fnc=$1;
    }else{
      next;
    }
    if(exists $$CallCountRef{$fnc}){
      $$CallCountRef{$fnc}+=$cnt;
    }else{
      $$CallCountRef{$fnc}=$cnt;
    }
  }
  close(File);
}
