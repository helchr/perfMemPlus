#include "address2Line.h"
#include <QProcess>
#include <QStringRef>
#include <QVector>
#include <QMap>
#include <QStringBuilder>

QString Address2Line::addr2line(const QString& exe, const QString& address)
{
  static QMap<QString,QProcess*> addr2lineProcesses;
  auto it = addr2lineProcesses.find(exe);
  if(it != addr2lineProcesses.end())
  {
    auto process = *it;
    QString addr = QLatin1String("0x") % address % QLatin1Char('\n');
    process->write(addr.toLatin1());
    process->waitForReadyRead(100);
    auto result = process->readAllStandardOutput();
    return QString(result);
  }
  else
  {
  QProcess* process = new QProcess;
  QString file = QString("sh -c \"addr2line -p -f -C -i -e  ") % exe % "\"";
  process->start(file);
  process->waitForStarted(100);
  addr2lineProcesses.insert(exe,process);
  return addr2line(exe,address);
  }
}

Address2Line::LineInfo Address2Line::getLineInfo(const QString &exe, const QString &address)
{
  return parseLineInfo(addr2line(exe,address));
}

QString Address2Line::stringRefJoin(const QVector<QStringRef>& stringRefList)
{
  QString mergedString = QStringLiteral("");
  if(stringRefList.empty())
  {
    return mergedString;
  }
  for(auto ref : stringRefList)
  {
    mergedString += ref.toString() + QStringLiteral("\n");
  }
  mergedString.chop(1);
  return mergedString;
}

Address2Line::LineInfo Address2Line::parseLineInfo(const QString &text)
{
  LineInfo linfo;
  QVector<QStringRef> addr2lineLines = text.splitRef('\n',QString::SkipEmptyParts);
  if(!addr2lineLines.empty())
  {
    QVector<QStringRef> addr2lineParts = addr2lineLines.first().split(QStringLiteral(" at "),QString::SkipEmptyParts);
    if(!addr2lineParts.empty() && addr2lineParts.first() != QStringLiteral("") && addr2lineParts.first() != QStringLiteral("?? ??:0"))
    {
      //addr2line can get useful data
      linfo.function = addr2lineParts.first().toString();
      addr2lineParts.removeFirst();

      auto fileAndLine = addr2lineParts.first().split(QLatin1String(":"));
      linfo.file = fileAndLine.first().toString();
      linfo.line = fileAndLine.at(1).toInt();
      addr2lineLines.remove(0);
      linfo.inlinedBy = stringRefJoin(addr2lineLines);
      return linfo;
    }
  }
  linfo.file = QLatin1String("");
  linfo.function = QLatin1String("");
  return linfo;
}

