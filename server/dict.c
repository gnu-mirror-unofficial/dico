#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <gjdictd.h>
#include <server.h>

char *dictfilename = DICTFILENAME;
Data *dictfile;
char *textfilename = TEXTFILENAME;
Data *textfile;
int numdictentries;
Dictheader dictheader;
Ushort lowestkanji, highestkanji;

struct treeinfo {
    char *name;
    Index *file;
    int vital;
} tree[] = {
    { JIS_TREE, NULL, 1 },
    { UNICODE_TREE, NULL, 1 },
    { CORNER_TREE, NULL, 1 },
    { FREQ_TREE, NULL, 0 },
    { NELSON_TREE, NULL, 0 },
    { HALPERN_TREE, NULL, 0 },
    { GRADE_TREE, NULL, 0 },
    { BUSHU_TREE, NULL, 0 },
    { SKIP_TREE, NULL, 0 },
    { PINYIN_TREE, NULL, 0 },
    { D_ENGLISH_TREE, NULL, 0 },
    { KANJI_TREE, NULL, 1 },
    { XREF_TREE, NULL, 0 },
    { WORDS_TREE, NULL, 1 },
    { D_YOMI_TREE, NULL, 0 },
    { D_ROMAJI_TREE, NULL, 0 },
};

int num_dict_pages = 160000;
int num_text_pages = 320;

void
makenames()
{
    int i;

    dictfilename = mkfullname(dictpath, dictfilename);
    textfilename = mkfullname(dictpath, textfilename);
    for (i = 0; i < TreeLast; i++)
	tree[i].name = mkfullname(dictpath, tree[i].name);
}

void
initdict()
{
    int i;
    Index *iptr;

    logmsg(L_INFO, 0, "Initializing dictionaries...");
    logmsg(L_INFO, 0, "using %d dictionary pages", num_dict_pages);
    logmsg(L_INFO, 0, "using %d text storage pages", num_text_pages);
	 
    makenames();
    dictfile = dopen(dictfilename, FL_READ | _BUF_LRU,
		     num_dict_pages, NULL, NULL);
    if (!dictfile)
	die(1, L_CRIT, 0, "cannot open dictionary file `%s'", dictfilename);

    textfile = dopen(textfilename, FL_READ | _BUF_LRU,
		     num_text_pages, NULL, NULL);
    if (!textfile)
	die(1, L_CRIT, 0, "cannot open text storage `%s'", textfilename);

    for (i = 0; i < TreeLast; i++) {
	if (!(tree[i].file = iopen(tree[i].name, 0)) && tree[i].vital)
	    die(1, L_CRIT, 0, "cannot open B-tree `%s'", tree[i].name);
    }

    dreadpriv(dictfile, &dictheader, sizeof(dictheader));                
/*    init_dictentry_buf(dictheader.maxlen8);*/
		       
    iptr = tree[TreeJis].file;
    if (itop(iptr) != SUCCESS)
	die(1, L_CRIT, 0, "Unusable B-tree: `%s'?", iptr->name);
    lowestkanji = *(Ushort *) ivalue(iptr);
    if (ibottom(iptr) != SUCCESS)
	die(1, L_CRIT, 0, "Unusable B-tree: `%s'?", iptr->name);
    highestkanji = *(Ushort *) ivalue(iptr);
    logmsg(L_INFO, 0, "Done (%#x to %#x)", lowestkanji, highestkanji);
    numdictentries = dictfile->reccnt;

    dict_setcmpcut(TreeXref, 1); /*FIXME*/
}


int
have_tree(int n)
{
    return tree[n].file != NULL;
}

char *
tree_name(int n)
{
    return tree[n].name;
}

Index *
tree_ptr(int n)
{
    return tree[n].file;
}

Recno
tree_position(int n)
{
    if (!have_tree(n))
	return -1;
    return irecno(tree[n].file);
}

/* GetDictEntry(): Obtain a DictEntry structure by it's record number
 */
DictEntry *
dict_entry(DictEntry *entry, Recno recno)
{
    if (recno < 0 || recno >= dictfile->reccnt)
	return NULL;
    dseek(dictfile, recno, SEEK_SET);
    dread(dictfile, entry);
    return entry;
}

/* dict_string(): Get a text string with offset off.
 */
void *
dict_string(Offset off)
{
    long recno;
    int pos;
    char *ptr;

    if (off == 0) 
	return NULL;
    recno = off / textfile->pagesize;
    pos = off % textfile->pagesize;
    if (recno < textfile->reccnt) {
	dseek(textfile, recno, SEEK_SET);
	dread(textfile, NULL);
	ptr = textfile->head + pos;
    } else {
	ptr = NULL;
    }
    return ptr;
}

/* setcmplen(): Set B-tree key compare length.
 */
void
dict_setcmplen(int tree_no, int len)
{
    Index *iptr = tree[tree_no].file;

    if (!iptr)
	return;
    icmplen(iptr, 0, len);
    imode(iptr, IF_ALLOWINEXACT, 1);
}

/* setcmpcut(): Set B-tree compound key compare cut
 */
void
dict_setcmpcut(int tree_no, int num)
{
    Index *iptr = tree[tree_no].file;

    if (!iptr)
	return;
    if (num == 0)
	iptr->numcompkeys = iptr->numsubkeys;
    else if (num <= iptr->numsubkeys)
	iptr->numcompkeys = num;
    imode(iptr, IF_ALLOWINEXACT, 1);
}

/* Read (eventually next or prev) DictEntry matching
 * key `value' in the tree # tree_no
 */
DictEntry *
read_match_entry(DictEntry *entry, int tree_no, void *value, Matchdir dir)
{
    Index *iptr = tree[tree_no].file;
    int rc;

    if (!iptr)
	return NULL;
    if (iseq_matchkey(iptr, value)) 
	rc = iseq_begin(iptr, value);
    else if (dir == MatchNext)
	rc = iseq_next(iptr);
    else /* if (dir == MatchPrev) */
	rc = iseq_prev(iptr);
    
    switch (rc) {
    case SUCCESS:
	return dict_entry(entry, iptr->seq.recno);
    default:
	return NULL;
    }
    /*NOTREACHED*/
}

/* ReadClosestDictEntry(): Read DictEntry that most closely corresponds
 * to a key
 */
DictEntry *
read_closest_entry(DictEntry *entry, int tree_no, void *value)
{
    Index *iptr = tree[tree_no].file;

    if (!iptr)
	return NULL;
    iseek(iptr, value);
    return dict_entry(entry, irecno(iptr));
}

/* ReadExactMatchDictEntry(): Read DictEntry that exactly matches the key
 */
DictEntry *
read_exact_match_entry(DictEntry *entry, int tree_no, void *value)
{
    int rc;
    Index *iptr = tree[tree_no].file;

    if (!iptr)
	return NULL;
    rc = iseek(iptr, value);
    if (rc == SUCCESS)
	return dict_entry(entry, irecno(iptr));
    return NULL;
}

Recno
dict_index(int tree_no, void *value)
{
    Index *iptr = tree[tree_no].file;

    if (!iptr)
	return 0;
    if (iseek(iptr, value) == SUCCESS)
	return irecno(iptr);
    return 0;
}

Recno
next_dict_index(int tree_no, void *value)
{
    Index *iptr = tree[tree_no].file;
    int rc;

    if (!iptr)
	return 0;
    if (iseq_matchkey(iptr, value)) 
	rc = iseq_begin(iptr, value);
    else 
	rc = iseq_next(iptr);
    
    switch (rc) {
    case SUCCESS:
	return iptr->seq.recno;
    default:
	return 0;
    }
    /*NOTREACHED*/
}

void
dict_enum(int tree_no, int (*fun) ())
{
    int rc;
    Index *iptr = tree[tree_no].file;
    DictEntry entry;
    
    if (!iptr)
	return;

    if (itop(iptr) != SUCCESS)
	die(1, L_CRIT, 0, "itop!");
    do {
	rc = fun(dict_entry(&entry, irecno(iptr)));
    } while (rc == 0 && iskip(iptr, 1L) == 1L);
}




