/* revbits: reads the hex numbers (in ascii) from the stdin, reverses the
 *          bit order and prints the result to stdout.
 */
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdlib.h>
#include <stdio.h>

int
xtoi(char *s)
{
    register int out = 0;

    if (*s == '0' && (s[1] == 'x' || s[1] == 'X'))
	s += 2;
    for (; *s; s++) {
	if (*s >= 'a' && *s <= 'f')
	    out = (out << 4) + *s - 'a' + 10;
	else if (*s >= 'A' && *s <= 'F')
	    out = (out << 4) + *s - 'A' + 10;
	else if (*s >= '0' && *s <= '9')
	    out = (out << 4) + *s - '0';
	else 
	    break;
    }
    return out;
}

int
main(int argc, char **argv)
{
    unsigned short num, out, i;
    char buf[128], *s;

    while (s = fgets(buf, sizeof(buf), stdin)) {
	num = xtoi(s);
	out = 0;
	for (i = 0; i < 8*sizeof(num); i++) {
	    out <<= 1;
	    if (num & 1)
		out |= 1;
	    num >>= 1;
	}
	printf("%04x\n", out);
    }
    return 0;
}


