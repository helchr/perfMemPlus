#ifndef ADDRESS2LINE_H
#define ADDRESS2LINE_H

#include <QString>

class Address2Line
{
public:
  struct LineInfo
  {
    QString function;
    QString file;
    int line = -1;
    QString inlinedBy;
  };
  static LineInfo getLineInfo(const QString& exe, const QString& address);

private:
  static LineInfo parseLineInfo(const QString &text);
  static QString addr2line(const QString &exe, const QString &address);
  static QString stringRefJoin(const QVector<QStringRef> &stringRefList);
};

#endif
