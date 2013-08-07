#ifndef DIC_H
#define DIC_H

#include <QtDebug>
#include <QList>
#include <QString>
#include <QPair>
#include <QRegExp>
#include <anthy/anthy.h>
#include <anthy/dicutil.h>
#include <Entry.h>

class Dic {
    public:
        Dic();
        ~Dic();
        int load();
        int save();
        QList<Entry*> entries();
        QList<Entry*> entries(const QRegExp &yomi,
                const QRegExp &spelling,
                const QRegExp &wordtype,
                const QPair<int,int> &freqrange);
        int add_entry(const QString &yomi,
                const QString &spelling,
                const QString &wordtype,
                int freq);
        int delete_entry(const QRegExp &yomi,
                const QRegExp &spelling,
                const QRegExp &wordtype,
                const QPair<int,int> &freqrange);
        Entry* modify_entry(const Entry &mod,
                const QRegExp &yomi,
                const QRegExp &spelling,
                const QRegExp &wordtype,
                const QPair<int,int> &freqrange);
    private:
        int normalize_freq(int freq);
        QList<Entry*> data;
        const int g_maxfreq = 1000;
        const int g_minfreq = 0;
        const int g_deffreq = 0;
        int old_head;
        int head;
        int anthy_version;
};

#endif
