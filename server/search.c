#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <gjdictd.h>
#include <bushu.h>
#include <server.h>

struct searchctl_t search_ctl;
int encoding = ENC_JIS;

char * answer_str[] = {
    "FULL", "SEQUENTIAL", "CLOSEST",
};

char * match_str[] = {
    "EXACT", "INEXACT"
};

char * dir_str[] = {
    "FWD", "BWD"
};

char * tree_str[] = {
    "Jis",
    "Unicode",
    "Corner",
    "Freq",
    "Nelson",
    "Halpern",
    "Grade",
    "Bushu",
    "Skip",
    "Pinyin",
    "English",
    "Kanji",
    "Xref",
    "Words",
    "Yomi",
    "Romaji",
};

unsigned entry_count = 0;
DictEntry *entry_buf;

void
alloc_entry_buf()
{
    unsigned count;
    
    if (search_ctl.mode == QUERY_SEQ)
	count = search_ctl.count;
    else
	count = 1;

    if (count > entry_count) {
	DictEntry *p;
	
	if (!entry_buf)
	    p = malloc(count * sizeof(entry_buf[0]));
	else
	    p = realloc(entry_buf, count * sizeof(entry_buf[0]));
	if (p == 0) {
	    logmsg(L_ERR, 0, "not enough core to allocate %d entries", count);
	    if (!entry_buf) 
		die(1, L_ERR, 0, "fatally low on core, exiting");
	    search_ctl.count = entry_count;
	} else {
	    entry_buf = p;
	    entry_count = search_ctl.count;
	}
    }
}

void
initsearch()
{
}

void
xlat_string(char *p, int len, char *s)
{
    char *start = p;
    int n;
    
    while (*s) {
	if (p >= start + len)
	    break;
	if (*s == '\\') {
	    switch (*++s) {
	    case 'e':
	    case 'E':
		*p++ = '\033';
		s++;
		break;
	    case 't':
		*p++ = '\t';
		s++;
		break;
	    case '0':
		n = strtol(s, &s, 8);
		*p++ = n;
		break;
	    default:
		*p++ = *s++;
	    }
	} else
	    *p++ = *s++;
    }
    *p = 0;
}

char *string_buffer;
int string_buffer_size;

char *
realloc_buffer(int len)
{
    if (!string_buffer) {
	string_buffer = malloc(len);
	if (!string_buffer)
	    die(1, L_ERR, 0, "not enough core to allocate string buffer");
	else
	    string_buffer_size = len;
	return string_buffer ;
    } else if (len > string_buffer_size) {
	char *p = realloc(string_buffer, len);
	if (p) {
	    string_buffer_size = len;
	    string_buffer = p;
	} else 
	    die(1, L_ERR, 0, "not enough core to reallocate string buffer");
	return p;
    }
    return string_buffer;
}

char *
enc(char *s)
{
    char *p, *start;
    
    switch (encoding) {
    case ENC_EUC:
	if ((p = realloc_buffer(strlen(s))) == NULL)
	    return s;
	start = p;
	while (*s)
	    *p++ = *s++ | 0x80;
	*p = 0;
	break;
    default:
	start = s;
    }
    return start;
}

void
set_mode(int match_mode, Matchdir dir, int cnt)
{
    search_ctl.mode = match_mode;
    if (match_mode == QUERY_SEQ) {
	search_ctl.dir = dir;
	search_ctl.count = cnt;
    }
}

void
getrecord(Recno recno)
{
    DictEntry entry;
    
    if (dict_entry(&entry, recno) == NULL) {
	msg(RC_ERR, "no such record");
    } else {
	msg(RC_OK, "OK");
	send_entries(&entry, 1);
    }
}
    
int
simple_query(void *data, int size)
{
    int i;
    
    status.query++;
    status.match = 0;

    alloc_entry_buf();
    switch (search_ctl.mode) {
    case QUERY_CLOSEST:
	if (read_closest_entry(entry_buf, search_ctl.tree, data))
	    status.match++;
	break;

    case QUERY_EXACT:
	if (read_exact_match_entry(entry_buf, search_ctl.tree, data))
	    status.match++;
	break;

    case QUERY_SEQ:
	for (i = 0; i < search_ctl.count; i++)
	    if (NULL == read_match_entry(entry_buf + i,
					 search_ctl.tree,
					 data,
					 search_ctl.dir))
		break;

	status.match = i;
	break;
    }

    status.total_match += status.match;
    return status.match;
}

int
bushu_query(TempBushu *tmp)
{
    int i;
    Bushu bushu;
    DictEntry *found = NULL;
    
    status.query++;
    status.match = 0;

    alloc_entry_buf();
    debug(DEBUG_SEARCH, 10)
	("searching for bushu: type %d, rad %d, nstr %d, off %d",
	 tmp->type, tmp->bushu, tmp->numstrokes, tmp->off);

    switch (search_ctl.mode) {
    case QUERY_CLOSEST:
	debug(DEBUG_SEARCH, 10)("mode closest");
	while (decode_bushu(search_ctl.dir, tmp, &bushu) == 0) {
	    found = read_closest_entry(entry_buf, TreeBushu, &bushu);
	    if (!found && tmp->type == BushuFuzzy) {
		if (search_ctl.dir == MatchNext)
		    tmp->off++;
		else
		    tmp->off--;
	    } else
		break;
	}
	if (found) {
	    status.match++;
	    debug(DEBUG_SEARCH, 10)
		("found: %u:%u", bushu.bushu, bushu.numstrokes);
	}
	break;

    case QUERY_EXACT:
	debug(DEBUG_SEARCH, 10)("mode exact");
	while (decode_bushu(search_ctl.dir, tmp, &bushu) == 0) {
	    found = read_exact_match_entry(entry_buf, TreeBushu, &bushu);
	    if (!found && tmp->type == BushuFuzzy) {
		if (search_ctl.dir == MatchNext)
		    tmp->off++;
		else
		    tmp->off--;
	    } else
		break;
	}
	if (found) {
	    status.match++;
	    debug(DEBUG_SEARCH, 10)
		("found: %u:%u", bushu.bushu, bushu.numstrokes);
	}
	break;

    case QUERY_SEQ:
	debug(DEBUG_SEARCH, 10)("mode sequential");
	i = 0;
	while (decode_bushu(search_ctl.dir, tmp, &bushu) == 0) {
	    for ( ; i < search_ctl.count; i++) {
		if (!read_match_entry(entry_buf + i,
					 TreeBushu,
					 &bushu,
					 search_ctl.dir))
		    break;
		debug(DEBUG_SEARCH, 15)
		    ("found: %u:%u", bushu.bushu, bushu.numstrokes);
	    }
	    if (i < search_ctl.count && tmp->type == BushuFuzzy) {
		if (search_ctl.dir == MatchNext)
		    tmp->off++;
		else
		    tmp->off--;
	    } else
		break;
	}
	debug(DEBUG_SEARCH, 10)
	    ("found %d matches", i);
	status.match = i;
	break;
    }

    status.total_match += status.match;
    return status.match;
}

void
respond(void *data, int size)
{
    int cnt;

    cnt = simple_query(data, size);
    if (cnt) {
	if (cnt == 1)
	    msg(RC_OK, "OK");
	else
	    msg(RC_OK, "%d matches", cnt);
	send_entries(entry_buf, cnt);
    } else
	msg(RC_NOMATCH, "no (more) matches found");
}

void
xref(int code)
{
    Recno recno, *recbuf;
    Ushort jis = (Ushort)code;
    Xref xref;
    int refcnt;
    int i, rc;
    Index *iptr = tree_ptr(TreeXref);
    DictEntry entry;

    recno = dict_index(TreeJis, &jis); /*FIXME:this disturbes the tree pos */
    if (recno == 0) {
	msg(RC_ERR, "cannot find master record");
	return;
    }

    if (!dict_entry(&entry, recno)) {
	msg(RC_ERR, "unexpected error");
	return;
    }

    refcnt = entry.refcnt;

    xref.kanji = jis; /* JIS code of the kanji */
    rc = iseek(iptr, &xref);
    if (rc != SUCCESS) {
	logmsg(L_ERR, 0, "%s:%d:iseek(%#x) returned %d",
	        __FILE__, __LINE__, jis, rc);
	msg(RC_ERR, "cannot find cross-references");
    }
    
    recbuf = calloc(refcnt+1, sizeof(*recbuf));
    if (!recbuf) {
	logmsg(L_ERR, 0, "low core");
	msg(RC_ERR, "low core");
	return;
    }


    msg(RC_OK, "OK %d", refcnt);
    for (i = 1; i <= refcnt; i++) {
	recbuf[i] = irecno(iptr);
	if (iskip(iptr, 1) != 1) {
	    logmsg(L_ERR, 0, "%s:%d: unexpected end of index", 
	           __FILE__, __LINE__);
	    break;
	}
    }
    recbuf[0] = i-1;
    rc = send_recno(refcnt+1, recbuf);
    free(recbuf);
}

void
search_string16(char *str)
{
    int len = strlen(str);
    dict_setcmplen(search_ctl.tree, len);
    debug(DEBUG_SEARCH, 1)
	("tree %d, data str16 `%s', length %d",
	 search_ctl.tree, str, len);
    respond(str, len+1);
}

void
search_string32(char *str)
{
    int len = strlen(str);
    dict_setcmplen(search_ctl.tree, len);
    debug(DEBUG_SEARCH, 1)
	("tree %d, data str32 `%s', length %d",
	          search_ctl.tree, str, len);
    respond(str, len);
}

void
search_number32(char *str)
{
    unsigned num;
    char *endp;
    
    num = strtol(str, &endp, 0);
    if (*endp) {
	msg(RC_BADINPUT, "bad number (stopped at %s)", endp);
	return;
    }

    debug(DEBUG_SEARCH, 1)
	("tree %d, data ulong %ul", search_ctl.tree, num);
    respond(&num, sizeof(num));
}

void
search_number16(char *str)
{
    int num;
    Ushort snum;
    char *endp;
    
    num = strtol(str, &endp, 0);
    if (*endp) {
	msg(RC_BADINPUT, "bad number (stopped at %s)", endp);
	return;
    }

    snum = (Ushort) num;
    debug(DEBUG_SEARCH, 1)
	("tree %d, data Ushort %4x", search_ctl.tree, snum);
    respond(&snum, sizeof(snum));
}

void
search_number8(char *str)
{
    int num;
    unsigned char snum;
    char *endp;
    
    num = strtol(str, &endp, 0);
    if (*endp) {
	msg(RC_BADINPUT, "bad number (stopped at %s)", endp);
	return;
    }

    snum = (unsigned char) num;
    debug(DEBUG_SEARCH, 1)
	("tree %d, data Ushort %4x", search_ctl.tree, snum);
    respond(&snum, sizeof(snum));
}

void
search_corner(char *str)
{
    int n;
    Ushort snum;
    
    for (n = 0; n < 4; n++)
	if (!isdigit(str[n])) {
	    msg(RC_BADINPUT, "malformed corner code (at %s)", str+n);
	    return;
	}

    if (str[n] == '.') {
	msg(RC_NOTSUPPORT, "qualified corner codes not supported yet");
	str[n] = 0;
    }
    
    snum = atoi(str);
    debug(DEBUG_SEARCH, 1)
	("tree %d, data Ushort %4x", search_ctl.tree, snum);
    respond(&snum, sizeof(snum));
}

static char *
matchstr(int n)
{
    static char buf[128];
    
    sprintf(buf, "%d match%s", n, n > 1 ? "es" : "");
    return buf;
}

void
search_bushu(char *str)
{
    int found;
    TempBushu tmp;

    if (parse_bushu(str, &tmp) == BushuError) {
	return;
    }

    if (found = bushu_query(&tmp)) {
	if (tmp.type == BushuFuzzy)
	    msg(RC_OK, "%s, next fuzzy offset (%d)",
		matchstr(found), tmp.off);
	else
	    msg(RC_OK, "%s", matchstr(found));
	send_entries(entry_buf, found);
    } else
	msg(RC_NOMATCH, "no (more) matches found");
}

void
search_skip(char *str)
{
    unsigned int num;
    
    num = strtol(str, (char**)&str, 10);
    if (*str != '-') {
	msg(RC_BADINPUT, "Invalid SKIP pattern: '-' expected");
	return;
    }
    num <<= 8;
    num |= strtol(str+1, (char**)&str, 10);
    if (*str != '-') {
	msg(RC_BADINPUT, "Invalid SKIP pattern: '-' expected");
	return;
    }
    num <<= 8;
    num |= strtol(str+1, (char**)&str, 10);
    while (*str && isspace(*str))
	str++;
    if (*str) {
	msg(RC_BADINPUT, "Invalid SKIP pattern: junk at the end");
	return;
    }
    debug(DEBUG_SEARCH, 1)
	("tree %d, data Ushort %4x", search_ctl.tree, num);
    respond(&num, sizeof(num));
}











