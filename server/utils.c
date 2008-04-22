#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <gjdictd.h>
#include <server.h>

char *
mkfullname(char *dir, char *name)
{
    int dirlen = strlen(dir);
    int namelen = strlen(name);
    char *rp;

    rp = malloc(dirlen + namelen + (dir[dirlen - 1] != '/') + 1);
    if (!rp) 
	die(1, L_EMERG, 0, "not enough core");
    strcpy(rp, dir);
    if (dir[dirlen - 1] != '/')
	rp[dirlen++] = '/';
    strcpy(rp + dirlen, name);
    return rp;
}

void
format_skipcode(char *buf, unsigned code)
{
    sprintf(buf, "%d-%d-%d", code >> 16, (code >> 8) & 0xf, code & 0xf);
}




