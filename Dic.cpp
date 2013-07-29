#include "Dic.h"

int Dic::load() { }
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
