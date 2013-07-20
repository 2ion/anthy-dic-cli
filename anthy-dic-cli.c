#include <anthy/anthy.h>
#include <anthy/dicutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#ifndef BUFSIZE
#define BUFSIZE (512)
#endif

#ifndef DICCHUNK
#define DICCHUNK (16)
#endif

int g_anthy_version = 0;
const int g_minfreq = 1;
const int g_maxfreq = 1000;

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

void Entry_print(int index, const Entry *e) {
    printf("%04d: sound=%s @ wordtype=%s @ spelling=%s @ frequency=%04d\n",
            index, e->sound.p, e->wordtype.p, e->spelling.p, e->freq);
}

void Entry_allocate_strings(Entry *e) {
    assert(e != NULL);
#define ALLOCSTR(ptr) (ptr).p=(char*)malloc(sizeof(char)*BUFSIZE);assert((ptr).p!=NULL);(ptr).len=BUFSIZE
    ALLOCSTR(e->sound);
    ALLOCSTR(e->wordtype);
    ALLOCSTR(e->spelling);
#undef ALLOCSTR
}

void Entry_free(Entry *e) {
    assert(e != NULL);
#define FREESTR(ptr) if((ptr).p!=NULL){free((ptr).p);(ptr).len=0;}
    FREESTR(e->sound);
    FREESTR(e->wordtype);
    FREESTR(e->spelling);
#undef FREESTR
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
        fprintf(stderr, "Anthy could not access its private directory.\n");
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

    return EXIT_SUCCESS;
}

int main(int argc, char **argv) {
    Dictionary dic = { .p = NULL, .last = -1, .len = 0 };

    anthy_dic_util_init();
    anthy_dic_util_set_encoding(ANTHY_UTF8_ENCODING);
    g_anthy_version = atoi(anthy_get_version_string());
    assert(g_anthy_version != 0);

    readdic(&dic);

    anthy_dic_util_quit();
    Dictionary_free(&dic);
    return 0;
}
