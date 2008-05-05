# This file is part of Dico.
# Copyright (C) 1998, 2008 Sergey Poznyakoff
#
# Dico is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3, or (at your option)
# any later version.
#
# Dico is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Dico.  If not, see <http://www.gnu.org/licenses/>.

BEGIN {
    SWIDTH="96 0"
    DWIDTH="16 0"
    BBX="16 16 0 -2"
    while ((getline < HEADER) > 0)
	print
    print "CHARS " CHARS+1
    count = 0
}

function endchar() {
    if (count) {
	print "ENDCHAR"
    }
    count++	
}    

/static unsigned char .*_bits/ {
    endchar()
    num = substr($4, 1, length($4)-7); # +2 for "[]"
    print "STARTCHAR " num
    print "ENCODING " num
    print "SWIDTH " SWIDTH
    print "DWIDTH " DWIDTH
    print "BBX " BBX
    print "BITMAP"
}

/0x[0-9a-fA-F][0-9a-fA-F],/ {
    for (i=1; i <= NF; i++) 
       bits[i] = substr($i, 3, 2)
    for (i = 1; i <= NF; i += 2) 
       print bits[i+1] bits[i] | "revbits" # LSB first
    close("revbits")	   
}

END {
    endchar()
    while ((getline < FOOTER) > 0)
	print
    print "ENDFONT"
}

