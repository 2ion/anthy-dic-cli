#include "Dic.h"

Dic::Dic() {
    anthy_dic_util_init();
    anthy_dic_util_set_encoding(ANTHY_UTF8_ENCODING);
    this->anthy_version = atoi(anthy_get_version_string());
}

int Dic::load() {
    Entry *e;
    int v;
    if( (v = anthy_priv_dic_select_first_entry()) == -1) {
        qWarning() << "Dic::load(): Dictionary is empty.";
        return 0;
    } else if( v == -3 && this->anthy_version >= 7716 ) {
        qFatal() << "Dic::load(): Error: Could not access the anthy dictionary!";
        return -1;
    }
    do {
        e = new Entry(

}
int Dic::save() { }
QList<Entry*> Dic::entries(){}
QList<Entry*> Dic::entries(const QRegExp &yomi,
        const QRegExp &spelling,
        const QRegExp &wordtype,
        const QPair<int,int> &freqrange) {}
int Dic::add_entry(const QString &yomi,
        const QString &spelling,
        const QString &wordtype,
        int freq) {}
int Dic::delete_entry(const QRegExp &yomi,
        const QRegExp &spelling,
        const QRegExp &wordtype,
        const QPair<int,int> &freqrange) {}
Entry* Dic::modify_entry(const Entry &mod,
        const QRegExp &yomi,
        const QRegExp &spelling,
        const QRegExp &wordtype,
        const QPair<int,int> &freqrange) {}

int Dic::normalize_freq(int freq) {}
