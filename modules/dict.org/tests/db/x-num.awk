BEGIN {
    FS="\t"
}
{ print $2
  print "    " $1 }
