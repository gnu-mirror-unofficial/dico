/* This file is part of Dico.
   Copyright (C) 2008 Sergey Poznyakoff

   Dico is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   Dico is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Dico.  If not, see <http://www.gnu.org/licenses/>. */

#include <dictd.h>

size_t num_defines;
size_t num_matches;
size_t num_compares;

void
clear_stats()
{
    num_defines = num_matches = num_compares = 0;
}

void
begin_timing(const char *name)
{
    if (timing_option) {
	timer_start(name);
	clear_stats();
    }
}

void
report_timing(dico_stream_t stream, const char *name)
{
    if (timing_option) {
	xdico_timer_t t = timer_stop(name);
	stream_printf(stream, " [d/m/c = %lu/%lu/%lu ",
	              (unsigned long) num_defines,
		      (unsigned long) num_matches,
		      (unsigned long) num_compares);
	clear_stats();
	timer_format_time(stream, timer_get_real(t));
	dico_stream_write(stream, "r ", 2);
	timer_format_time(stream, timer_get_user(t));
	dico_stream_write(stream, "u ", 2);
	timer_format_time(stream, timer_get_system(t));
	dico_stream_write(stream, "s]", 2);
    }
}
