BEGIN {
    FS="\t"
}
{ print $1; print "    " $2 }
