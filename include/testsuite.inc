# This file is part of GNU Dico
# Copyright (C) 2012 Sergey Poznyakoff
#
# GNU Dico is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3, or (at your option)
# any later version.
#
# GNU Dico is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with GNU Dico.  If not, see <http://www.gnu.org/licenses/>.

m4_define([dico_quote], [m4_if([$#], [0], [], [[$*]])])

dnl AT_DICOD([TEXT],[OUTPUT],[ERROR])
m4_define([AT_DICOD],[
AT_DATA([input],dico_quote($1))
AT_CHECK([dnl
dicod --config $abs_builddir/dicod.conf --stderr -i < input | dnl
    tr -d '\r' | sed 's/^\(2[[25][0-9]]\) .*/\1/;s/ *$//'],
[0],
[$2],
[$3])])

