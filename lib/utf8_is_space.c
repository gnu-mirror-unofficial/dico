#include <config.h>
#include "dico.h"

static const unsigned start[] = {
     0,
     9,    32,  5760,  8192,  8200,  8232, 12288,
};

static int count[] = {
     0,
     5,     1,     1,     7,     4,     2,     1,
};

int
utf8_wc_is_space(unsigned wc)
{
    return utf8_table_check(wc, start, count, DICO_ARRAY_SIZE(start));
}
