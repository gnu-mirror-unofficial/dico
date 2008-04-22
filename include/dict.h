#ifndef __dict_h
#define __dict_h

#ifndef INPUTDICTPATH
# define INPUTDICTPATH "/usr/local/lib"
#endif
#ifndef DICTPATH
# define DICTPATH "/usr/local/lib/jdict"
#endif

#define DICT_DB "dict.db"
#define JIS_TREE "jis.db"
#define UNICODE_TREE "unicode.db"
#define CORNER_TREE "corner.db"
#define FREQ_TREE "freq.db"
#define NELSON_TREE "nelson.db"
#define HALPERN_TREE "halpern.db"
#define GRADE_TREE "grade.db"
#define PINYIN_TREE "pinyin.db"
#define D_ENGLISH_TREE "english.db"
#define KANJI_TREE "kanji.db"
#define WORDS_TREE "words.db"
#define BUSHU_TREE "bushu.db"
#define SKIP_TREE "skip.db"
#define XREF_TREE "xref.db"
#define D_YOMI_TREE "yomi.db"
#define D_ROMAJI_TREE "romaji.db"

enum {
    TreeJis,
    TreeUnicode,
    TreeCorner,
    TreeFreq,
    TreeNelson,
    TreeHalpern,
    TreeGrade,
    TreeBushu,
    TreeSkip,
    TreePinyin,
    TreeEnglish,
    TreeKanji,
    TreeXref,
    TreeWords,
    TreeYomi,
    TreeRomaji,
    TreeLast          
};

typedef struct {
    Ushort bushu;        /* The radical number */
    Ushort numstrokes;   /* The number of strokes */
} Bushu;
    
typedef struct {
    Ushort kanji;
    Ushort pos;
} Xref;

typedef long Offset;

/* Dictionary entry structure.
 * Offsets are binary offsets to the strings in text datafile.
 * Offset 0 means there is no string.
 * All strings are zero-terminated (16-bit terminate with two 
 * zeroes.
 */
 
typedef struct translation {
    Bushu  bushu;
    Ushort Qindex;		/* for the "four corner" lookup method */
    unsigned skip;              /* SKIP code */
    Ushort Jindex;              /* jis index */
    Ushort Uindex;		/* "Unicode" index */
    Ushort Nindex;		/* Nelson dictionary */
    Ushort Hindex;		/* Halpern dictionary */

    Ushort frequency;		/* frequency that kanji is used */
    Uchar grade_level;		/* akin to  school class level */

    int refcnt;                 /* Number of references to this kanji */ 
    
    size_t english;		/* english translation string. */
    size_t yomi;	        /* kana, actually */
    size_t kanji;		/* can be pointer to pronunciation */
    size_t pinyin;              /* Pinyin pronunciation */
} DictEntry;

#define DICT_PTR(e, f) ((char*)((e) + 1) + (e)->f)
#define DICT_ENGLISH_PTR(e) DICT_PTR(e, english)
#define DICT_PINYIN_PTR(e)  DICT_PTR(e, pinyin)
#define DICT_KANJI_PTR(e)   DICT_PTR(e, kanji)
#define DICT_YOMI_PTR(e)    DICT_PTR(e, yomi)

typedef struct {
    int numkanji;
    int numentries;
    Ushort lowestkanji;
    Ushort highestkanji;
    int maxlen8;
    int maxlen16;
} Dictheader;

#endif
