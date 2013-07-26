/*
    anthy-dic-cli - A CLI for editing the private dictionary used by Anthy
    Copyright (C) 2013 Jens Oliver John <asterisk >swirly< 2ion >dot< de>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdlib.h>
#include <values.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <anthy/anthy.h>
#include <anthy/dicutil.h>
#ifndef BUFSIZE
#define BUFSIZE (512) // buffer length for dictionary fields
#endif
#ifndef DICCHUNK
#define DICCHUNK (16) // allocate X dictionary entries at a time
#endif
#define ALLOCSTR(ptr) (ptr).p=(char*)malloc(sizeof(char)*BUFSIZE);assert((ptr).p!=NULL);(ptr).len=BUFSIZE
#define FREESTR(ptr) if((ptr).p!=NULL){free((ptr).p);(ptr).len=0;}
#define FREESTRING(ptr) if((ptr)!=NULL){free((ptr)->p);(ptr)->len=0;}
#define FREESTR_IFNOTNULL(ptr) if((ptr).p!=NULL){free((ptr).p);(ptr).len=0;}
#define STRISNULL(ptr) ((ptr).p==NULL ? 1 : 0)
#define STRINGISNULL(ptr) ((ptr)->p==NULL ? 1 : 0)
#define CSTR(ptr) ((ptr) != NULL ? (ptr)->p : NULL)

/* globals */

int g_anthy_version = 0;
const int g_minfreq = 1;
const int g_maxfreq = 1000;
const int g_deffreq = 500;
const char *g_optstring = "y:s:t:f:amdgl+";

/* types */

typedef struct {
    char *p;
    size_t len;
} String;
    
typedef struct {
    String sound;
    String wordtype;
    String spelling;
    int freq;
} Entry;

typedef struct {
    Entry **p;
    int last;
    int oldlast; // last entry loaded from Anthy
    size_t len;
} Dictionary;

typedef struct {
    unsigned int cmask;
    int verb;
    String *yomi;
    String *wordtype;
    String *spelling;
    int frequency;
} CLIMap;

enum { VERB_ADD, VERB_MOD, VERB_DEL, VERB_GREP, VERB_DICLEN, VERB_NULL };
enum { C_YOMI = 1<<0, C_WT = 1<<1, C_SPELLING = 1<<2, C_FREQ = 1<<3 }; // CLIMAP::cmask

/* prototypes */

static inline int normalize_freq(int freq);
static void CLIMap_free(CLIMap*);
static void Entry_print(int index, const Entry*);
static void Entry_allocate_strings(Entry*);
static void Entry_free(Entry*);
static Entry* Entry_new(void);
static String* String_copy(String*, const String*);
static String* String_fromChar(const char*);
static int Dictionary_resize(Dictionary*);
static int Dictionary_append(Dictionary*, Entry*);
static void Dictionary_free(Dictionary*);
static int Dictionary_load(Dictionary*);
static int Dictionary_save(Dictionary*);
static int verb_add(const CLIMap*, Dictionary*);
static int verb_diclen(const Dictionary*);
static void usage(void);

/* implementation */

static inline int normalize_freq(int f) {
    if( f < g_minfreq )
        return g_minfreq;
    else if( f > g_maxfreq)
        return g_maxfreq;
    return f;
}

static void CLIMap_free(CLIMap *map) {
    assert(map != NULL);
    FREESTRING(map->yomi);
    FREESTRING(map->spelling);
    FREESTRING(map->wordtype);
}

static void Entry_print(int index, const Entry *e) {
    printf("%04d: sound=%s @ wordtype=%s @ spelling=%s @ frequency=%04d\n",
            index, e->sound.p, e->wordtype.p, e->spelling.p, e->freq);
}

static void Entry_allocate_strings(Entry *e) {
    assert(e != NULL);
    ALLOCSTR(e->sound);
    ALLOCSTR(e->wordtype);
    ALLOCSTR(e->spelling);
}

static void Entry_free(Entry *e) {
    assert(e != NULL);
    FREESTR(e->sound);
    FREESTR(e->wordtype);
    FREESTR(e->spelling);
}

static Entry* Entry_new(void) {
    Entry *e = (Entry*) malloc(sizeof(Entry));
    assert(e != NULL);
    Entry_allocate_strings(e);
    return e;
}

static String* String_copy(String *dest, const String *src) {
    assert(dest != NULL && src != NULL);
    if( dest->len < src->len ) {
        dest->p = (char*) realloc(dest->p, src->len);
        assert(dest->p != NULL);
        dest->len = src->len;
    }
    strncpy(dest->p, (const char*) src->p, src->len);
    return dest;
}

static int Dictionary_resize(Dictionary *d) {
    assert(d != NULL);
    if( d->len == 0 ) {
        d->p = (Entry**) malloc(DICCHUNK * sizeof(Entry));
        if( d->p == NULL )
            return -1;
        d->len = DICCHUNK;
    }
    if( (d->last+1) == d->len ) {
        d->len += DICCHUNK * sizeof(Entry);
        d->p = (Entry**) realloc(d->p, d->len);
        if( d->p == NULL ) {
            d->len -= DICCHUNK * sizeof(Entry);
            return -1;
        }
    }
    return 0;
}

static int Dictionary_append(Dictionary *d, Entry *e) {
    assert(d != NULL && e != NULL);
    if( Dictionary_resize(d) != 0 ) {
        fprintf(stderr, "Dictionary_append(): memory allocation error\n");
        return -1;
    }
    d->p[++(d->last)] = e;
    return 0;
}

static void Dictionary_free(Dictionary *d) {
    int i;
    assert(d != NULL);
    if( d->len == 0 )
        return;
    for( i = 0; i <= d->last; ++i ) {
        if( d->p[i] == NULL )
            continue;
        Entry_free(d->p[i]);
        free(d->p[i]);
    }
}

static int Dictionary_load(Dictionary *d) {
    int v;
    Entry *e;
    if( (v = anthy_priv_dic_select_first_entry()) == -1 ) {
        fprintf(stderr, "Dictionary is empty.\n");
        return 0;
    } else if( v == -3 && g_anthy_version >= 7716 ) {
        fprintf(stderr, "Anthy could not access the private dictionary!\n");
        return -1;
    }
    do {
        e = Entry_new();
        if( anthy_priv_dic_get_index(e->sound.p, e->sound.len) &&
                 anthy_priv_dic_get_wtype(e->wordtype.p, e->wordtype.len) && 
                 anthy_priv_dic_get_word(e->spelling.p, e->spelling.len) ) {
            e->freq = normalize_freq(anthy_priv_dic_get_freq());
            if( g_anthy_version < 7710 && e->spelling.p[0] == ' ' ) {
                // Handle anthy bug: returns entry with a leading whitespace
                if( memmove(e->spelling.p, e->spelling.p+1, e->spelling.len-1) == NULL ) {
                    Entry_free(e);
                    return -1;
                }
            }
            if( Dictionary_append(d, e) != 0 ) {
                fprintf(stderr, "Dictionary_load(*): could not append to dictionary\n");
                Entry_free(e);
                return -1;
            }
            //Entry_print(d->last, (const Entry*) e);
        }
    } while( anthy_priv_dic_select_next_entry() == 0 );
    d->oldlast = d->last; 
    return 0;
}

static String* String_fromChar(const char *str) {
    String *s = (String*) malloc(sizeof(String));
    assert(s != NULL);
    s->len = strlen(str) + 1;
    s->p = (char*) malloc(s->len);
    assert(s->p != NULL);
    strncpy(s->p, str, s->len);
    return s;
}

static int verb_add(const CLIMap *map, Dictionary *dic) {
    if( STRINGISNULL(map->spelling) || STRINGISNULL(map->yomi) )
        return -2;
    Entry *e = Entry_new();
    int err = 0;
    e->freq = map->frequency == -1 ? g_deffreq : map->frequency;
    String_copy(&e->spelling, map->spelling);
    String_copy(&e->sound, map->yomi);
    String_copy(&e->wordtype, map->wordtype);
    e->freq = map->frequency;
    err = Dictionary_append(dic, e);
    if( err != 0 ) {
        Entry_free(e);
        return err;
    }
    return 0;
}

static int verb_diclen(const Dictionary *d) {
<<<<<<< HEAD
    if( printf("%d\n", d->last) < 0 )
=======
    if( printf("%d\n", (int) d->len) < 0 )
>>>>>>> d2dc431618c88041e6603b200d8acc7ac8070e5a
        return -1;
    return 0;
}

static void usage(void) {
    printf( "Usage: anthy-dic-cli <verb> [<verb-options>]\n"
            "Verbs: [add]           -a -s <spelling> -y <yomi> [-f <frequency>] [-t <type>]\n"
            "       [modify]        -m <add-like filter expression, '-+' denotes end of criteria>\n"
            "       [delete]        -d <add-like filter expression>\n"
            "       [grep]          -g <add-like filter expression>\n"
            "       [dict length]   -l\n"
            "       -h\n");
}

int main(int argc, char **argv) {
    if( argc < 2 ) {
        puts("Too few arguments.");
        usage();
        return -1;
    }
    if( strcmp(argv[1], "-h") == 0 ) {
        usage();
        return 0;
    }

    CLIMap cmap = { 0, VERB_NULL, NULL, NULL, NULL, -1 };
    Dictionary dic = { .p = NULL, .last = -1, .len = 0 };
    int endofcriteria = 0;
    int c;
    long int freqtmp;
    int err = 0;

#define VERB_NO_OVERWRITE if(cmap.verb!=VERB_NULL){\
    fprintf(stderr, "%s: error: option '%c' would override a previously specified verb.\n",\
            argv[0], c); return -1;}
#define CHECK_VERB_NOT_SET if(cmap.verb==VERB_NULL){\
    fprintf(stderr, "%s: error: a verb must be specified before '%c'.\n",\
            argv[0], c); return -1;}
#define MAYBE_CRITERIUM(cval) if(cmap.verb==VERB_MOD && endofcriteria==0){cmap.cmask|=(cval);}
    while( (c = getopt(argc, argv, g_optstring)) != -1 )
        switch( c ) {
            case 'a':
                VERB_NO_OVERWRITE;
                cmap.verb = VERB_ADD;
                break;
            case 'm':
                VERB_NO_OVERWRITE;
                cmap.verb = VERB_MOD;
                break;
            case 'd':
                VERB_NO_OVERWRITE;
                cmap.verb = VERB_DEL;
                break;
            case 'g':
                VERB_NO_OVERWRITE;
                cmap.verb = VERB_GREP;
                break;
            case 'l':
                VERB_NO_OVERWRITE;
                cmap.verb = VERB_DICLEN;
                break;
            case '+':
                CHECK_VERB_NOT_SET;
                if( cmap.verb == VERB_MOD )
                    endofcriteria = 1;
                else
                    fprintf(stderr, "Ignored option without effect: %c\n", c);
                break;
            case 'y':
                CHECK_VERB_NOT_SET;
                MAYBE_CRITERIUM(C_YOMI);
                if( STRINGISNULL(cmap.yomi) == 0 ) {
                    fprintf(stderr, "Warning: overriding previous argument of -y with '%s'\n",
                            optarg);
                    FREESTRING(cmap.yomi);
                }
                cmap.yomi = String_fromChar((const char*) optarg);
                break;
            case 's':
                CHECK_VERB_NOT_SET;
                MAYBE_CRITERIUM(C_SPELLING);
                if( STRINGISNULL(cmap.spelling) == 0 ) {
                    fprintf(stderr, "Warning: overriding previous argument of -s with '%s'\n",
                            optarg);
                    FREESTRING(cmap.spelling);
                }
                cmap.spelling = String_fromChar((const char*) optarg);
                break;
            case 't':
                CHECK_VERB_NOT_SET;
                MAYBE_CRITERIUM(C_WT);
                if( STRINGISNULL(cmap.wordtype) == 0 ) {
                    fprintf(stderr, "Warning: overriding previous argument of -t with '%s'\n",
                            optarg);
                    FREESTRING(cmap.wordtype);
                }
                cmap.wordtype = String_fromChar((const char*) optarg);
                break;
            case 'f':
                CHECK_VERB_NOT_SET;
                MAYBE_CRITERIUM(C_FREQ);
                freqtmp = strtol((const char*) optarg, NULL, 10);
                if( freqtmp == LONG_MIN || freqtmp == LONG_MAX ) {
                    // over-|underflow
                    fprintf(stderr, "Error: conversion of frequency value to string failed: %s\n", optarg);
                    err = -1;
                    goto MAIN_cleanup_cmap;
                }
                cmap.frequency = (int) freqtmp;
                cmap.frequency = normalize_freq((int) freqtmp);
                break;
            case '?':
                // not reached, because opterr!=0
                goto MAIN_cleanup_cmap;
            default:
                goto MAIN_cleanup_cmap;
        }
#undef VERB_NO_OVERWRITE
#undef CHECK_VERB_NOT_SET
#undef MAYBE_CRITERIUM

    anthy_dic_util_init();
    anthy_dic_util_set_encoding(ANTHY_UTF8_ENCODING);
    g_anthy_version = atoi(anthy_get_version_string());
    assert(g_anthy_version != 0);
    Dictionary_load(&dic);

    switch( cmap.verb ) {
        case VERB_ADD:
            err = verb_add(&cmap, &dic);
            if( err != 0 )
                fprintf(stderr,
                        "Error: failed to add entry to the dictionary: %s--%s\n",
                        CSTR(cmap.yomi), CSTR(cmap.spelling));
            break;
        case VERB_DICLEN:
            err = verb_diclen(&dic);
            if( err != 0 )
                fprintf(stderr,
                        "Warning: call to printf() returned an error\n");
            break;
    }

    anthy_dic_util_quit();
    Dictionary_free(&dic);
MAIN_cleanup_cmap:
    CLIMap_free(&cmap);

    return err;
}
