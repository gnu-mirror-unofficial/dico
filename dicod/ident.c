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

#include <dicod.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include "des.h"

/* IDENT (AUTH, RFC1413) preauth */
#define USERNAME_C "USERID :"

/* If the reply matches sscanf expression
   
      "%*[^:]: USERID :%*[^:]:%s"

   return a pointer to the %s part.  Otherwise, return NULL. */

static char *
ident_extract_username(char *reply)
{
    char *p;
    
    p = strchr(reply, ':');
    if (!p)
	return NULL;
    if (p[1] != ' ' || strncmp(p + 2, USERNAME_C, sizeof(USERNAME_C) - 1))
	return NULL;
    p += 2 + sizeof(USERNAME_C) - 1;
    p = strchr(p, ':');
    if (!p)
	return NULL;
    do
	p++;
    while (*p == ' ');
    return p;
}

static int
is_des_p(const char *name)
{
    int len = strlen(name);
    return len > 1 && name[0] == '[' && name[len-1] == ']';
}

#define smask(step) ((1<<step)-1)
#define pstep(x,step) (((x)&smask(step))^(((x)>>step)&smask(step)))
#define parity_char(x) pstep(pstep(pstep((x),4),2),1)

static void
des_fixup_key_parity(unsigned char key[8])
{
    int i;
    for (i = 0; i < 8; i++) {
	key[i] &= 0xfe;
	key[i] |= 1 ^ parity_char(key[i]);
    }
}

static void
des_cbc_cksum(gl_des_ctx *ctx, unsigned char *buf, size_t bufsize,
	       unsigned char out[8], unsigned char key[8])
{
    while (bufsize > 0) {
	if (bufsize >= 8) {
	    unsigned char *p = key;
	    *p++ ^= *buf++;
	    *p++ ^= *buf++;
	    *p++ ^= *buf++;
	    *p++ ^= *buf++;
	    *p++ ^= *buf++;
	    *p++ ^= *buf++;
	    *p++ ^= *buf++;
	    *p++ ^= *buf++;
	    bufsize -= 8;
	} else {
	    unsigned char *p = key + bufsize;
	    buf += bufsize;
	    switch (bufsize) {
	    case 7:
		*--p ^= *--buf;
	    case 6:
		*--p ^= *--buf;
	    case 5:
		*--p ^= *--buf;
	    case 4:
		*--p ^= *--buf;
	    case 3:
		*--p ^= *--buf;
	    case 2:
		*--p ^= *--buf;
	    case 1:
		*--p ^= *--buf;
	    }
	    bufsize = 0;
	}
	gl_des_ecb_crypt(ctx, key, key, 0);
    }
}

static void
des_string_to_key(char *buf, size_t bufsize, unsigned char key[8])
{
    size_t i;
    int j;
    unsigned temp;
    unsigned char *p;
    char *p_char;
    char k_char[64];
    gl_des_ctx context;
    char *pstr;
    int forward = 1;
    
    p_char = k_char;
    memset(k_char, 0, sizeof(k_char));
  
    /* get next 8 bytes, strip parity, xor */
    pstr = buf;
    for (i = 1; i <= bufsize; i++) {
	/* get next input key byte */
	temp = (unsigned int) *pstr++;
	/* loop through bits within byte, ignore parity */
	for (j = 0; j <= 6; j++) {
	    if (forward)
		*p_char++ ^= (int) temp & 01;
	    else
		*--p_char ^= (int) temp & 01;
	    temp = temp >> 1;
	} while (--j > 0);

	/* check and flip direction */
	if ((i%8) == 0)
	    forward = !forward;
    }
  
    p_char = k_char;
    p = (unsigned char *) key;

    for (i = 0; i <= 7; i++) {
	temp = 0;
	for (j = 0; j <= 6; j++)
	    temp |= *p_char++ << (1 + j);
	*p++ = (unsigned char) temp;
    }

    des_fixup_key_parity(key);
    gl_des_setkey(&context, key);
    des_cbc_cksum(&context, buf, bufsize, key, key);
    memset(&context, 0, sizeof context);
    des_fixup_key_parity(key);
}

static int
decode64_buf(const char *name, unsigned char **pbuf, size_t *psize)
{
    size_t namelen;
    unsigned char *buf;
    size_t bufsize;
    size_t size;
  
    name++;
    namelen = strlen(name) - 1;

    bufsize = 3 * namelen + 1;
    buf = xmalloc(bufsize);
    
    dico_base64_decode(name, namelen,
		       buf, bufsize,
		       &size,
		       0, NULL);
    buf[size] = 0;
    *pbuf = realloc(buf, size + 1);
    *psize = size;
    return 0;
}

struct ident_info
{
    uint32_t checksum;
    uint16_t random;
    uint16_t uid;
    uint32_t date;
    uint32_t ip_local;
    uint32_t ip_remote;
    uint16_t port_local;
    uint16_t port_remote;
};

union ident_data
{
    struct ident_info fields;
    unsigned long longs[6];
    unsigned char chars[24];
};

int
ident_decrypt(const char *file, const char *name, struct ident_info *info)
{
    unsigned char *buf = NULL;
    size_t size = 0;
    int fd;
    char keybuf[1024];
    union ident_data id;

    if (decode64_buf(name, &buf, &size))
	return 1;

    if (size != 24) {
	dico_log(L_ERR, 0,
		 _("Incorrect length of IDENT DES packet"));
	free(buf);
	return 1;
    }

    fd = open(file, O_RDONLY);
    if (fd < 0) {
	dico_log(L_ERR, errno, _("Cannot open file %s"), file);
	return 1;
    }

    while (read(fd, keybuf, sizeof(keybuf)) == sizeof(keybuf)) {
	int i;
	unsigned char key[8];
	gl_des_ctx ctx;
      
	des_string_to_key(keybuf, sizeof(keybuf), key);
	gl_des_setkey(&ctx, key);
      
	memcpy(id.chars, buf, size);
      
	gl_des_ecb_decrypt(&ctx, (char *)&id.longs[4], (char *)&id.longs[4]);
	id.longs[4] ^= id.longs[2];
	id.longs[5] ^= id.longs[3];

	gl_des_ecb_decrypt(&ctx, (char *)&id.longs[2], (char *)&id.longs[2]);
	id.longs[2] ^= id.longs[0];
	id.longs[3] ^= id.longs[1];
	
	gl_des_ecb_decrypt(&ctx, (char *)&id.longs[0], (char *)&id.longs[0]);

	for (i = 1; i < 6; i++)
	    id.longs[0] ^= id.longs[i];

	if (id.fields.checksum == 0)
	    break;
    }
    close(fd);
    free(buf);
    if (id.fields.checksum == 0) {
	memmove(info, &id, sizeof(*info));
	return 0;
    }
    return 1;
}

char *
query_ident_name(struct sockaddr_in *srv_addr, struct sockaddr_in *clt_addr)
{
    int fd;
    char buf[UINTMAX_STRSIZE_BOUND];
    struct sockaddr_in s;
    ssize_t bytes;
    FILE *fp;
    char *bufp;
    size_t size;
    char *name;
    RETSIGTYPE (*sighan) (int);
    
    fd = socket(PF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
	dico_log(L_ERR, errno,
		 _("cannot create socket for AUTH identification"));
	return NULL;
    }
    /* FIXME: Switch socket to non-blocked mode */

    s.sin_family = AF_INET;
    s.sin_addr.s_addr = srv_addr->sin_addr.s_addr;
    s.sin_port = 0;
    if (bind(fd, (struct sockaddr*) &s, sizeof(s)) < 0) {
	dico_log(L_ERR, errno,
		 _("cannot bind AUTH socket"));
    }

    s = *clt_addr;
    s.sin_port = htons(113);
    if (connect(fd, (struct sockaddr*) &s, sizeof(s)) == -1) {
	dico_log(L_ERR, errno,
		 _("cannot connect to AUTH server %s"),
		 inet_ntoa(s.sin_addr));
	close(fd);
	return NULL;
    }

    sighan = signal(SIGPIPE, SIG_IGN);

    fp = fdopen(fd, "r+");
    if (!fp) {
	close(fd);
	signal(SIGPIPE, sighan);
	return NULL;
    }
    
    fprintf(fp, "%u , %u\r\n",
	    ntohs(clt_addr->sin_port),
	    ntohs(srv_addr->sin_port));

    bufp = NULL;
    size = 0;
    bytes = getline(&bufp, &size, fp);
    fclose(fp);
    signal(SIGPIPE, sighan);
    
    if (bytes <= 0) {
	dico_log(L_ERR, errno, _("no reply from AUTH server %s"),
		 inet_ntoa(s.sin_addr));
	return NULL;
    }

    trimnl(bufp, bytes);
    name = ident_extract_username(bufp);
    if (!name) {
	dico_log(L_ERR, 0,
		 _("Malformed IDENT response: `%s', from %s"),
		 bufp, inet_ntoa(s.sin_addr));
	free(bufp);
	return NULL;
    } else if (is_des_p(name)) {
	if (!ident_keyfile) {
	    dico_log (L_ERR, 0,
		      _("Keyfile for AUTH responses not specified in config; "
			"use `ident-keyfile FILE'"));
	    name = NULL;
	} else {
	    struct ident_info info;
	    if (ident_decrypt(ident_keyfile, name, &info) == 0)
		name = xstrdup(umaxtostr(ntohs(info.uid), buf));
	    else
		name = NULL;
	}
    } else
	name = xstrdup(name);
    free(bufp);
    return name;
}
