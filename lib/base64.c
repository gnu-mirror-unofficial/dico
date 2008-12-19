/* This file is part of GNU Dico
   Copyright (C) 1999, 2000, 2001, 2004, 2005,
   2007, 2008 Free Software Foundation, Inc.
  
   GNU Dico is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   GNU Dico is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GNU Dico.  If not, see <http://www.gnu.org/licenses/>. */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <dico.h>

const char b64_table[64] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int
dico_base64_input(char c)
{
    int i;

    for (i = 0; i < 64; i++) {
	if (b64_table[i] == c)
	    return i;
    }
    return -1;
}

int
dico_base64_decode(const char *iptr, size_t isize, char *optr, size_t osize,
		   size_t *pnbytes)
{
    int i = 0, tmp = 0, pad = 0;
    size_t consumed = 0;
    unsigned char data[4];
    size_t nbytes = 0;
    
    while (consumed < isize && nbytes + 3 < osize) {
	while (i < 4 && consumed < isize) {
	    tmp = dico_base64_input(*iptr++);
	    consumed++;
	    if (tmp != -1)
		data[i++] = tmp;
	    else if (*(iptr-1) == '=') {
		data[i++] = 0;
		pad++;
	    }
	}

	/* I have a entire block of data 32 bits get the output data.  */
	if (i == 4) {
	    *optr++ = (data[0] << 2) | ((data[1] & 0x30) >> 4);
	    *optr++ = ((data[1] & 0xf) << 4) | ((data[2] & 0x3c) >> 2);
	    *optr++ = ((data[2] & 0x3) << 6) | data[3];
	    nbytes += 3 - pad;
	} else {
	    /* I did not get all the data.  */
	    consumed -= i;
	    break;
	}
	i = 0;
    }
    *pnbytes = nbytes;
    return consumed;
}

int
dico_base64_encode (const char *iptr, size_t isize,
		    char *optr, size_t osize,
		    size_t *pnbytes)
{
    size_t consumed = 0;
    int pad = 0;
    const unsigned char* ptr = (const unsigned char*) iptr;
    size_t nbytes = 0;
  
    if (isize <= 3)
	pad = 1;
    while ((consumed + 3 <= isize && nbytes + 4 <= osize) || pad) {
	unsigned char c1 = 0, c2 = 0, x = '=', y = '=';
	
        *optr++ = b64_table[ptr[0] >> 2];
	consumed++;
	switch (isize - consumed) {
	default:
	    consumed++;
	    y = b64_table[ptr[2] & 0x3f];
	    c2 = ptr[2] >> 6;
	case 1:
	    consumed++;
	    x = b64_table[((ptr[1] << 2) + c2) & 0x3f];
	    c1 = (ptr[1] >> 4);
	case 0:
	    *optr++ = b64_table[((ptr[0] << 4) + c1) & 0x3f];
	    *optr++ = x;
	    *optr++ = y;
	}
	
	ptr += 3;
	nbytes += 4;
	pad = 0;
    }

    *pnbytes = nbytes;
    return consumed;
}

dico_stream_t
dico_base64_stream_create(dico_stream_t str, int mode)
{
    return filter_stream_create(str, 3, 76,
				mode == FILTER_ENCODE ?
				  dico_base64_encode : dico_base64_decode,
				mode);
}
