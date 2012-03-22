#ifndef RTags_h
#define RTags_h

#include <QByteArray>
#include <Path.h>
#include <Log.h>
#include <stdio.h>
#include <assert.h>

namespace RTags {
enum { DatabaseVersion = 2 };
enum UnitType { CompileC, CompileCPlusPlus, PchC, PchCPlusPlus };

static inline int digits(int len)
{
    int ret = 1;
    while (len >= 10) {
        len /= 10;
        ++ret;
    }
    return ret;
}

struct Location {
    Location(const Path &p = Path(), int off = 0)
        : path(p), offset(off)
    {}

    Path path;
    int offset;

    bool isNull() const
    {
        return !offset;
    }

    inline bool operator==(const Location &other) const
    {
        return path == other.path && offset == other.offset;
    }
    inline bool operator<(const Location &other) const
    {
        const int cmp = strcmp(path.constData(), other.path.constData());
        if (cmp < 0) {
            return true;
        } else if (cmp > 0) {
            return false;
        }
        return offset < other.offset;
    }

    QByteArray key() const
    {
        if (!offset)
            return QByteArray();
        const int extra = RTags::digits(offset) + 1;
        QByteArray ret(path.size() + extra, '0');
        memcpy(ret.data(), path.constData(), path.size());
        snprintf(ret.data(), ret.size() + extra + 1, "%s,%d", path.constData(), offset);
        return ret;
    }

    static RTags::Location fromKey(const QByteArray &key)
    {
        if (key.isEmpty())
            return Location();
        const int lastComma = key.lastIndexOf(',');
        Q_ASSERT(lastComma > 0 && lastComma + 1 < key.size());
        Location ret;
        ret.offset = atoi(key.constData() + lastComma + 1);
        Q_ASSERT(ret.offset);
        ret.path = key.left(lastComma);
        return ret;
    }
};

static inline QDataStream &operator<<(QDataStream &ds, const Location &loc)
{
    ds << loc.path << loc.offset;
    return ds;
}

static inline QDataStream &operator>>(QDataStream &ds, Location &loc)
{
    ds >> loc.path >> loc.offset;
    return ds;
}

static inline QDebug operator<<(QDebug dbg, const Location &loc)
{
    const QByteArray out = "Location(" + loc.key() + ")";
    return (dbg << out);
}

static inline uint qHash(const Location &l)
{
    // ### this should be done better
    return qHash(l.path) + l.offset;
}

int canonicalizePath(char *path, int len);
QByteArray unescape(QByteArray command);
QByteArray join(const QList<QByteArray> &list, const QByteArray &sep = QByteArray());
int readLine(FILE *f, char *buf, int max);
QByteArray context(const Path &path, unsigned offset);

bool makeLocation(const QByteArray &arg, Location *loc,
                  QByteArray *resolvedLocation = 0, const Path &cwd = Path());
QByteArray makeLocation(const QByteArray &encodedLocation);
}

#endif
