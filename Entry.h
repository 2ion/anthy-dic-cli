#ifndef ENTRY_H
#define ENTRY_H

#include <QString>

class Entry {
    public:
        Entry(const QString &yomi,
                const QString &spelling,
                const QString &wordtype,
                int frequency)
            : m_yomi(yomi),
            m_spelling(spelling),
            m_wordtype(wordtype),
            m_frequency(frequency) {}
        ~Entry(){}
        QString* yomi(){ return &m_yomi; }
        QString* spelling(){ return &m_spelling; }
        QString* wordtype(){ return &m_wordtype; }
        int* frequency(){ return &m_frequency; }
    private:
        QString m_yomi;
        QString m_spelling;
        QString m_wordtype;
        int m_frequency;
};

#endif // ENTRY_H
