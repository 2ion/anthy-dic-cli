#include <stdio.h>
#include <stdlib.h>
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

int g_anthy_version = 0;
const int g_minfreq = 1;
const int g_maxfreq = 1000;
const char *g_optstring = "y:s:t:f:amdg";

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
    size_t len;
} Dictionary;

enum { VERB_ADD, VERB_MOD, VERB_DEL, VERB_GREP, VERB_NULL };

typedef struct {
    unsigned int cmask;
    int verb;
    String *yomi;
    String *wordtype;
    String *spelling;
    int freq;
} CLIMap;

void Entry_print(int index, const Entry *e) {
    printf("%04d: sound=%s @ wordtype=%s @ spelling=%s @ frequency=%04d\n",
            index, e->sound.p, e->wordtype.p, e->spelling.p, e->freq);
}

void Entry_allocate_strings(Entry *e) {
    assert(e != NULL);
    ALLOCSTR(e->sound);
    ALLOCSTR(e->wordtype);
    ALLOCSTR(e->spelling);
}

void Entry_free(Entry *e) {
    assert(e != NULL);
    FREESTR(e->sound);
    FREESTR(e->wordtype);
    FREESTR(e->spelling);
}

Entry* Entry_new(void) {
    Entry *e = (Entry*) malloc(sizeof(Entry));
    assert(e != NULL);
    Entry_allocate_strings(e);
    return e;
}

int Dictionary_resize(Dictionary *d) {
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

int Dictionary_append(Dictionary *d, Entry *e) {
    assert(d != NULL && e != NULL);
    if( Dictionary_resize(d) != 0 ) {
        fprintf(stderr, "Dictionary_append(): memory allocation error\n");
        return -1;
    }
    d->p[++(d->last)] = e;
    return 0;
}

void Dictionary_free(Dictionary *d) {
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

int readdic(Dictionary *d) {
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
            e->freq = anthy_priv_dic_get_freq();
            if( e->freq < g_minfreq )
                e->freq = g_minfreq;
            else if( e->freq > g_maxfreq )
                e->freq = g_maxfreq;
            if( g_anthy_version < 7710 && e->spelling.p[0] == ' ' ) {
                // Handle anthy bug: returns entry with a leading whitespace
                if( memmove(e->spelling.p, e->spelling.p+1, e->spelling.len-1) == NULL ) {
                    Entry_free(e);
                    return -1;
                }
            }
            if( Dictionary_append(d, e) != 0 ) {
                fprintf(stderr, "readdic(): could not append to dictionary\n");
                Entry_free(e);
                return -1;
            }
            Entry_print(d->last, (const Entry*) e);
        }
    } while( anthy_priv_dic_select_next_entry() == 0 );

    return 0;
}

void usage(void) {
    printf( "Usage: anthy-dic-cli <verb> [<verb-options>]\n"
            "Verbs: -a -s <spelling> -y <yomi> [-f <frequency>] [-t <type>]\n"
            "       -m <add-like filter expression, '-+' denotes end of criteria>\n"
            "       -d <add-like filter expression>\n"
            "       -g <add-like filter expression>\n"
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
    int endofcritera = 0;
    int c;

#define VERB_NO_OVERWRITE if(cmap.verb!=VERB_NULL){\
    fprintf(stderr, "%s: error: option '%c' would override a previously specified verb.\n",\
            argv[0], c); return -1;}
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
            case '?':
                // not reached, because opterr!=0
                return -1;
            default:
                return -255;
        }
#undef VERB_NO_OVERWRITE

    anthy_dic_util_init();
    anthy_dic_util_set_encoding(ANTHY_UTF8_ENCODING);
    g_anthy_version = atoi(anthy_get_version_string());
    assert(g_anthy_version != 0);

    readdic(&dic);

    anthy_dic_util_quit();
    Dictionary_free(&dic);

    return 0;
}
