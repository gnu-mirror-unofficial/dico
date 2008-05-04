/* This file is part of Gjdict.
   Copyright (C) 1998-2000, 2008 Sergey Poznyakoff

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>. */

#include <makedict.h>
#include <ctype.h>

#define __CAT2(a,b) a ## b

#define DCL_GET(field)                                                      \
  int                                                                       \
  __CAT2(get_,field)(DB *dbp, const DBT *pkey, const DBT *pdata, DBT *skey) \
  {                                                                         \
      DictEntry *dict = (DictEntry *)pdata->data;                           \
                                                                            \
      memset(skey, 0, sizeof(DBT));                                         \
      skey->data = &dict->field;                                            \
      skey->size = sizeof(dict->field);                                     \
      return 0;                                                             \
  }                                                             

#define DCL_GETS(field,inc)                                                 \
  int                                                                       \
  __CAT2(get_,field)(DB *dbp, const DBT *pkey, const DBT *pdata, DBT *skey) \
  {                                                                         \
      DictEntry *dict = (DictEntry *)pdata->data;                           \
                                                                            \
      memset(skey, 0, sizeof(DBT));                                         \
      skey->data = DICT_PTR(dict,field);                                    \
      skey->size = strlen(skey->data) + inc;                                \
      return 0;                                                             \
  }                                                             

#define DCL_CMP_NUM(type)                                                   \
  int                                                                       \
  __CAT2(cmp_,type)(DB *dbp, const DBT *a, const DBT *b)                    \
  {                                                                         \
      type an, bn;                                                          \
                                                                            \
      an = *(type*)a->data;                                                 \
      bn = *(type*)b->data;                                                 \
      if (an < bn)                                                          \
	  return -1;                                                        \
      else if (an > bn)                                                     \
	  return 1;                                                         \
      return 0;                                                             \
  }
    
DCL_GET(Jindex)
DCL_GET(Uindex)
DCL_GET(Qindex)
DCL_GET(frequency)
DCL_GET(Nindex)
DCL_GET(Hindex)
DCL_GET(grade_level)
DCL_GET(bushu) 
DCL_GET(skip)
DCL_GETS(english,1)
DCL_GETS(pinyin,1)
DCL_GETS(kanji,2)
DCL_GETS(yomi,2)
    
DCL_CMP_NUM(Ushort)
DCL_CMP_NUM(Uchar)    
DCL_CMP_NUM(unsigned)

int                                                                       
cmp_bushu(DB *dbp, const DBT *a, const DBT *b)                    
{                                                                         
    const Bushu *ap = a->data;
    const Bushu *bp = b->data;
                                                                          
    if (ap->bushu < bp->bushu)
	return -1;                                                      
    else if (ap->bushu > bp->bushu)
	return 1;

    if (ap->numstrokes < bp->numstrokes)
	return -1;
    else if (ap->numstrokes > bp->numstrokes)
	return 1;
    
    return 0;                                                             
}

/* FIXME: These two must ignore locale settings */
int                                                                       
cmp_c_string(DB *dbp, const DBT *a, const DBT *b)                    
{                                                                         
    const char *astr = a->data;
    const char *bstr = b->data;
    return strcmp(astr, bstr);
}

int                                                                       
cmp_ci_string(DB *dbp, const DBT *a, const DBT *b)                    
{                                                                         
    const char *astr = a->data;
    const char *bstr = b->data;
    return strcasecmp(astr, bstr);
}

/* FIXME: Not the same, but suffices for the time being. */
#define cmp_cw_string cmp_c_string

int
cmp_xref(DB *dbp, const DBT *a, const DBT *b)
{
    const Xref *xa = a->data;
    const Xref *xb = b->data;

    if (xa->kanji < xb->kanji)
	return -1;
    else if (xa->kanji > xb->kanji)
	return 1;

    if (xa->pos == USHRT_MAX || xb->pos == USHRT_MAX)
	return 0;
    else if (xa->pos < xb->pos)
	return -1;
    else if (xa->pos > xb->pos)
	return 1;
    return 0;
}


int
simple_list_iter(struct dbidx *idx, unsigned num, char *p)
{
    DBT key, content;
    size_t len;
    int stop = 0;
    
    memset(&content, 0, sizeof content);
    content.data = &num;
    content.size = sizeof(num);
    
    while (!stop) {
	int rc;
	
	while (*p && isspace(*p))
	    p++;
	if (!*p)
	    break;
	for (len = 0; p[len] && p[len] != '|'; len++)
	    ;
	if (p[len] == 0)
	    stop = 1;
	else
	    p[len] = 0;
	
	memset(&key, 0, sizeof key);
	key.data = p;
	key.size = len + 1;
	p += len + 1;

	/*printf("%s/%u\n", key.data,num);*/
	rc = idx->dbp->put(idx->dbp, NULL, &key, &content, 0);
	if (rc && rc != DB_KEYEXIST) {
	    logmsg(L_ERR, 0, "%s: failed to insert entry %s/%u: %s",
		   idx->name, (char*)key.data, num, db_strerror(rc));
	}
    }
    return 0;
}


int
pinyin_iter(struct dbidx *idx, unsigned num, DictEntry *ep)
{
    return simple_list_iter(idx, num, DICT_PINYIN_PTR(ep));
}


int
english_iter(struct dbidx *idx, unsigned num, DictEntry *ep)
{
    return simple_list_iter(idx, num, DICT_ENGLISH_PTR(ep));
}


#define WORDDELIM " \t(){}[]&.,;:?!'`\"+-*/^%$#@~|\\<>"

#define isdelim(c) strchr(WORDDELIM, c)

char *auxiliary_words[] = {
    "a", "an", "the",
    "this", "that", "these", "those",
    "in", "to", "into",
    "from",
    "on", "up", "upon",
    "through", "thru",
    "with", "within", "without",
    "out", "of", "by",
    "for",
    "one", "one's",
    "and", "or",
    "as", "at",
    "before", "after"
};

int
auxword(char *str, int len)
{
    int i;

    for (i = 0; i < NUMITEMS(auxiliary_words); i++)
	if (strncasecmp(auxiliary_words[i], str, len) == 0)
	    return 1;
    return 0;
}

int 
single_word(char *text)
{
    while (*text && !isdelim(*text)) 
        ++text;
    return *text == 0;
} 
	
int
simplify_text(char *buf, char *text, int size)
{
    int cnt = 0;
    int len;
    char *word;

    if (!text)
        return 0;
    
    if (single_word(text)) {
        strncpy(buf, text, size);
        buf[size-1] = 0;
        return 1;
    }	
    while (size > 0) {
	/* Skip delimiters */
	while (*text && isdelim(*text)) {
	    if (*text == '(')
		break;
	    ++text;
	}
	if (!*text)
	    break;

	/* Eliminate parenthesized words */
	if (*text == '(') {
	    while (*text && *text++ != ')') ;
	    if (!*text)
		break;
	    continue;
	}
	    
	/* Determine the word boundary */
	word = text;
	for (len = 0; word[len] && !isdelim(word[len]); len++) ;
	text += len;

	/* If aux. word -- throw it away */
	if (auxword(word, len))
	    continue;

	/* Copy word to buffer */
	cnt++;
	if (len > size)
	    len = size;
	size -= len;
	
	while (len--)
	    *buf++ = *word++;
	if (size) {
	    *buf++ = ' ';
	    size--;
	}
    }
    *buf = 0;
    return cnt;
}

int
words_iter(struct dbidx *idx, unsigned num, DictEntry *ep)
{
    DBT key, content;
    char *p;
    char *buf;
    size_t len;
    int stop = 0;
    
    memset(&content, 0, sizeof content);
    content.data = &num;
    content.size = sizeof(num);
    
    p = DICT_ENGLISH_PTR(ep);
    if (*p == 0)
	return 0;
    len = strlen(p);
    buf = malloc(len + 1);
    simplify_text(buf, p, len);
    p = buf;
    while (!stop) {
	int rc;
	
	while (*p && isspace(*p))
	    p++;
	if (!*p)
	    break;
	for (len = 0; p[len] && !isspace(p[len]); len++)
	    ;
	if (p[len] == 0)
	    stop = 1;
	else
	    p[len] = 0;
	
	memset(&key, 0, sizeof key);
	key.data = p;
	key.size = len + 1;
	p += len + 1;

	/*printf("%s/%u\n", key.data,num);*/
	rc = idx->dbp->put(idx->dbp, NULL, &key, &content, 0);
	if (rc && rc != DB_KEYEXIST) {
	    logmsg(L_ERR, 0, "%s: failed to insert entry %s/%u: %s",
		   idx->name, (char*)key.data, num, db_strerror(rc));
	}
    }
    free(buf);
    return 0;
}


int
xref_iter(struct dbidx *idx, unsigned num, DictEntry *ep)
{
    DBT key, content;
    XChar2b *text;
    Ushort hi = highestkanji << 8;
    Ushort lo = lowestkanji >> 8;
    Xref xref;
    int rc;
	
    memset(&content, 0, sizeof content);
    content.data = &num;
    content.size = sizeof(num);
    
    text = (XChar2b*)DICT_KANJI_PTR(ep);
    xref.pos = 0;
    while (text->byte1) {
	while (text->byte1
	       && !(text->byte1 >= lo && text->byte1 <= hi)) {
	    xref.pos++;
	    text++;
	}

	xref.kanji = (Ushort)(text->byte1<<8) + text->byte2;
	if (xref.kanji == 0)
	    break;

	memset(&key, 0, sizeof key);
	key.data = &xref;
	key.size = sizeof xref;
	
	/*printf("%s/%u\n", key.data,num);*/
	rc = idx->dbp->put(idx->dbp, NULL, &key, &content, 0);
	if (rc) {
	    logmsg(L_ERR, 0, "%s: failed to insert entry %x-%u/%u: %s",
		   idx->name, xref.kanji, xref.pos, num, db_strerror(rc));
	}
	xref.pos++;
	text++;
    }
    return 0;
}


const XChar2b *
strchr16(const XChar2b *str, unsigned c)
{
    XChar2b v;
    
    v.byte1 = (c & 0xff00) >> 8;
    v.byte2 = (c & 0xff);

    for (; str->byte1; str++)
	if (str->byte1 == v.byte1 && str->byte2 == v.byte2)
	    return str;
    return NULL;
}

const XChar2b *
strseg16(const XChar2b *str, const XChar2b *delim)
{
    unsigned c;

    for (; str->byte1; str++) {
	c = ((unsigned) str->byte1 << 8) + str->byte2; 
	if (strchr16(delim, c))
	    return str;
    }
    return NULL;
}

int
yomi_generic_iter(struct dbidx *idx, unsigned num, DictEntry *ep,
		  void (*dfun)(const XChar2b *, size_t, DBT *),
		  void (*dfree)(DBT *))
{
    const XChar2b *text, *end;
    size_t length;
    static XChar2b delim[] = {
	{ 0x21, 0x21 }, /* blank space */
	{ 0x21, 0x4a }, /* open paren */
	{ 0x21, 0x4b }, /* close paren */
	{ 0 }
    };
    DBT key, content;
    int rc;
    
    memset(&content, 0, sizeof content);
    content.data = &num;
    content.size = sizeof(num);

    text = (XChar2b*)DICT_YOMI_PTR(ep);
    while (text->byte1) {
	while (text->byte1 &&
	       text->byte1 != HIRAGANA_BYTE && text->byte1 != KATAKANA_BYTE)
	    text++;
	if (text->byte1 == 0)
	    return 0;
	end = strseg16(text, delim);
	if (end) 
	    length = (end - text) * 2;
	else
	    length = strlen((char*)text);
	
	memset(&key, 0, sizeof key);
	dfun(text, length, &key);
	rc = idx->dbp->put(idx->dbp, NULL, &key, &content, 0);
	if (dfree)
	    dfree(&key);
	
	if (rc && rc != DB_KEYEXIST) {
	    logmsg(L_ERR, 0, "%s: failed to insert entry `%s': %s",
		   idx->name, (char*)key.data, db_strerror(rc));
	}

	if (end) {
	    if (end->byte1 == 0x21 && end->byte2 == 0x4a) {
		/* Okurigana: remove parens surrounding the flection and
		   append it to the root. */
		const XChar2b *s;
		char *buf;
		size_t olen = 0;
		for (s = end + 1;
		     s->byte1 &&
			 !(s->byte1 == 0x21 && s->byte2 == 0x4b);
		     s++)
		    olen++;

		buf = xmalloc(length + 2*olen);
		memcpy(buf, text, length);
		memcpy(buf + length, end + 1, 2*olen);
		end = s + 1;
		
		memset(&key, 0, sizeof key);
		dfun((const XChar2b*)buf, length + 2*olen, &key);
		rc = idx->dbp->put(idx->dbp, NULL, &key, &content, 0);
		if (dfree)
		    dfree(&key);
		free(buf);
		if (rc && rc != DB_KEYEXIST) {
		    logmsg(L_ERR, 0, "%s: failed to insert entry `%s': %s",
			   idx->name, (char*)key.data, db_strerror(rc));
		}
	    } 
	    text = end;
	} else
	    break;
    }
    return 0;
}

static void
yomi_fill_key(const XChar2b *input, size_t length, DBT *key)
{
    key->data = (void*) input;
    key->size = length + 2;    
}

int
yomi_iter(struct dbidx *idx, unsigned num, DictEntry *ep)
{
    return yomi_generic_iter(idx, num, ep, yomi_fill_key, NULL);
}

static void
romaji_fill_key(const XChar2b *input, size_t length, DBT *key)
{
    key->size = kana_to_romaji_str(input, length, (char**)&key->data);
    if (key->size)
	key->size++;
}

static void
romaji_free_key(DBT *key)
{
    free(key->data);
}

int
romaji_iter(struct dbidx *idx, unsigned num, DictEntry *ep)
{
    return yomi_generic_iter(idx, num, ep, romaji_fill_key, romaji_free_key);
}



struct dbidx dbidx[] = {
    /* JIS codes */
    { JIS_INDEX_NAME, get_Jindex, cmp_Ushort },

    /* Unicode */
    { UNICODE_INDEX_NAME, get_Uindex, cmp_Ushort  },

    /* Four-corner index */
    { CORNER_INDEX_NAME, get_Qindex, cmp_Ushort },

    /* Frequency */
    { FREQ_INDEX_NAME, get_frequency, cmp_Ushort },

    /* Nelson dictionary index */
    { NELSON_INDEX_NAME, get_Nindex, cmp_Ushort },

    /* Halpern dictionary index */
    { HALPERN_INDEX_NAME, get_Hindex, cmp_Ushort },

    /* Jouyou grade level */
    { GRADE_INDEX_NAME, get_grade_level, cmp_Uchar },

    /* Radical */
    { BUSHU_INDEX_NAME, get_bushu, cmp_bushu }, 
    
    /* SKIP code */
    { SKIP_INDEX_NAME, get_skip, cmp_unsigned },

    /* Pinyin */
    { PINYIN_INDEX_NAME, get_pinyin, cmp_ci_string, pinyin_iter },

    /* English translation index */
    { ENGLISH_INDEX_NAME,  get_english, cmp_ci_string, english_iter },

    /* Kanji/kana text index */
    { KANJI_INDEX_NAME, get_kanji, cmp_cw_string  },

    /* Cross-reference of kanji */
    { XREF_INDEX_NAME, get_kanji, cmp_xref, xref_iter },
    
    /* Cross-reference of the words in translations */
    { WORDS_INDEX_NAME,  get_english, cmp_ci_string, words_iter },

    /* Readings */
    { YOMI_INDEX_NAME, get_yomi, cmp_cw_string, yomi_iter },

    /* Readings in romaji */
    { ROMAJI_INDEX_NAME, get_yomi, cmp_ci_string, romaji_iter },

    { NULL },
};

int dbmode = 0644;

void
open_secondary(DB *master, struct dbidx *idx)
{
    DB *sdbp;
    int ret;
    
    if ((ret = db_create(&sdbp, NULL, 0)) != 0)
	die(1, L_CRIT, 0, "cannot create secondary index DB: %s",
	    db_strerror(ret));
    if ((ret = sdbp->set_flags(sdbp, DB_DUP|DB_DUPSORT)) != 0)
	die(1, L_CRIT, 0, "cannot set flags on secondary index DB %s: %s",
	    idx->name, db_strerror(ret));

    if (idx->cmp_key && (ret = sdbp->set_bt_compare(sdbp, idx->cmp_key)))
	die(1, L_CRIT, 0, "cannot set comparator on secondary index DB %s: %s",
	    idx->name, db_strerror(ret));
    
    if ((ret = sdbp->open(sdbp, NULL, idx->name, NULL, 
			  DB_BTREE, DB_CREATE|DB_TRUNCATE, dbmode)) != 0)
	die(1, L_CRIT, 0, "cannot open secondary index DB %s: %s",
	    idx->name, db_strerror(ret));
    
    /* Associate the secondary with the primary. */
    if (!idx->iter &&
	(ret = master->associate(master, NULL, sdbp, idx->get_key, 0)) != 0)
	die(1, L_CRIT, 0, "cannot associate secondary index DB %s: %s",
	    idx->name, db_strerror(ret));

    idx->dbp = sdbp;
}

DB *
open_db()
{
    DB *dbp;
    int ret;
    int i;
    
    /* Open/create primary */
    if ((ret = db_create(&dbp, NULL, 0)) != 0)
	die(1, L_CRIT, 0, "cannot create DB: %s", db_strerror(ret));
    if ((ret = dbp->open(dbp, NULL,
			 dictname, NULL, DB_BTREE, DB_CREATE|DB_TRUNCATE,
			 dbmode)) != 0)
	die(1, L_CRIT, 0, "cannot open DB %s: %s", dictname, db_strerror(ret));

    /* Open/create secondaries */
    for (i = 0; dbidx[i].name; i++) 
	open_secondary(dbp, &dbidx[i]);
    return dbp;
}

void
close_db(DB *dbp)
{
    int i;
    
    dbp->close(dbp, 0);
    for (i = 0; dbidx[i].dbp; i++) {
	dbidx[i].dbp->close(dbidx[i].dbp, 0);
	dbidx[i].dbp = NULL;
    }
}

int
insert_dict_entry(DB *dbp, unsigned num, DictEntry *entry, size_t size)
{
    DBT key, content;

    memset(&key, 0, sizeof key);
    key.data = &num;
    key.size = sizeof(num);

    memset(&content, 0, sizeof content);
    content.data = entry;
    content.size = size;
	
    return dbp->put(dbp, NULL, &key, &content, 0);
}

void
iterate_db(DB *dbp)
{
    int rc;
    DBC *cursor;
    DBT key;
    DBT content;

    if (verbose)
	printf("creating additional indexes\n");
    if (rc = dbp->cursor(dbp, NULL, &cursor, 0))
	die(1, L_CRIT, 0, "cannot create cursor: %s", db_strerror(rc));

    memset(&key, 0, sizeof key);
    memset(&content, 0, sizeof content);
    for (rc = cursor->c_get(cursor, &key, &content, DB_FIRST);
	 rc == 0;
	 rc = cursor->c_get(cursor, &key, &content, DB_NEXT)) {
	int i;
	unsigned num = *(unsigned*)key.data;
	DictEntry *ep = content.data;

	for (i = 0; dbidx[i].dbp; i++) {
	    if (dbidx[i].iter)
		dbidx[i].iter(&dbidx[i], num, ep);
	}
/*	printf("%s\n", DICT_ENGLISH_PTR(ep)); */
    }
    if (rc != DB_NOTFOUND)
	die(1, L_CRIT, 0, "cannot iterate over the database: %s",
	    db_strerror(rc));
    cursor->c_close(cursor);
}

static void
update_kanji_ref_count(DB *dbp, Ushort kanji, unsigned count)
{
    int rc;
    DictEntry *ep;
    DB *jisdb = dbidx[index_jis].dbp;
    DBT key, pkey;
    DBT content;

    memset(&key, 0, sizeof key);
    key.data = &kanji;
    key.size = sizeof kanji;
    memset(&pkey, 0, sizeof pkey);
    memset(&content, 0, sizeof content);

    rc = jisdb->pget(jisdb, NULL, &key, &pkey, &content, 0);
    if (rc) {
	die(1, L_CRIT, 0, "cannot get dictionary entry for %x: %s",
	    kanji, db_strerror(rc));
    }
		
    ep = content.data;
    ep->refcnt = count;
    rc = dbp->put(dbp, NULL, &pkey, &content, 0);
    if (rc)
	die(1, L_CRIT, 0, "cannot update item: %s", db_strerror(rc));
}

void
count_xref(DB *dbp)
{
    int rc;
    struct dbidx *idx = dbidx + index_xref;
    DB *idbp = idx->dbp;
    DBC *cursor;
    DBT key;
    DBT pkey;
    
    if (verbose)
	printf("Counting cross-references\n");

    if (rc = idbp->cursor(idbp, NULL, &cursor, 0))
	die(1, L_CRIT, 0, "cannot create cursor: %s", db_strerror(rc));

    memset(&key, 0, sizeof key);
    memset(&pkey, 0, sizeof pkey);
    
    rc = cursor->c_get(cursor, &key, &pkey, DB_FIRST);
    if (rc == 0) {
	Ushort last = ((Xref*) key.data)->kanji;
	unsigned count = 0;
	
	for (; rc == 0;
	     rc = cursor->c_get(cursor, &key, &pkey, DB_NEXT)) {
	    Xref *xref = key.data;

	    if (xref->kanji == last)
		count++;
	    else {
		update_kanji_ref_count(dbp, last, count);
		last = xref->kanji;
		count = 1;
	    }
	}
	update_kanji_ref_count(dbp, last, count);
    }
    if (rc != DB_NOTFOUND)
	die(1, L_CRIT, 0, "cannot iterate over the database: %s",
	    db_strerror(rc));
    cursor->c_close(cursor);
}
