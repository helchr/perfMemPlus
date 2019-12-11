#ifndef AUTOANALYSIS_H
#define AUTOANALYSIS_H

#include <QString>
#include <QVariant>
#include <QVector>

class QSqlQuery;

struct BandwidthResult
  {
    QString memory;
    float latency = 0;
    bool problem = false;
    bool lowSampleCount = false;
    float parallelReaders = 0;
  };

  struct ClAccess {
          long long time;
          long long adr;
          int tid;
          int allocationId;
          long long ip;
          bool operator==(const ClAccess& other) const
          {
              return this->time == other.time && this->adr == other.adr && this->tid == other.tid && this->allocationId == other.allocationId && this->ip == other.ip;
          }
  };

  struct Result
  {
    QString function = "";
    int allocationId = -1;
    int callPathId = -1;
    bool falseSharing = false;
    unsigned int hitmCount = 0;
    bool trueSharing = false;
    float functionExectimePercent = 0;
    float objectLatencyPercent = 0;
    float hitmPercent = 0;
   QVector<QPair<ClAccess,ClAccess>> interObjectFalseSharing;
   QVector<QPair<ClAccess,ClAccess>> interObjectTrueSharing;
   QVector<QPair<ClAccess,ClAccess>> intraObjectFalseSharing;
   QVector<QPair<ClAccess,ClAccess>> intraObjectTrueSharing;
   BandwidthResult dramBandwidth;
   BandwidthResult remoteDramBandwidth;
  };


class AutoAnalysis
{
public:
  AutoAnalysis();
  ~AutoAnalysis();
  QList<Result> run();
  void loadSettings(const QString& path = "settings.conf");
  QList<Result> runFalseSharingDetection();
  QList<Result> runDramContentionDetection();

private:
  unsigned int sampleCountLimit = 0;
  unsigned int hitmLimit = 0;
  QHash<QString,float> latencyLimit \
  {{QStringLiteral("L1"),0}, {QStringLiteral("L2"),0}, {QStringLiteral("L3"),0}, \
  {QStringLiteral("Local DRAM"),0}, {QStringLiteral("Remote DRAM (1 hop)"),0}, \
  {QStringLiteral("Remote Cache (1 hops)"),0}};


  QList<Result> runDetection(std::function<Result (const int, const int)> analyze);
  QList<int> getOffendingFunctions() const;
  QList<int> getMostAccessedObjectsOfFunction(const int symbolId) const;
  void createIndexes() const;
  unsigned int getNumberOfHitmInFunction(const int symbolId, const int allocationId = -1) const;
  QString symbolIdToFuctionName(const int symbolId) const;
  QString allocationLimitString(const int allocationdId) const;
  bool isObjectWrittenByMultipleThreads(const int allocationId) const;
  float getLatencyLimit(const QString &memory) const;
  unsigned int getSampleCountInMemory(const int symbolId, const int allocationId, const QString &memory) const;
  float getLatencyInMemory(const int symbolId, const int allocationId, const QString &memory) const;
  BandwidthResult checkBandwidth(const QString &memory, const int symbolId, const int allocationId) const;
  float getAvgTimeBetweenMemoryReadSamples(const int symbolId, const int allocationId) const;
  float getNumberOfParallelReaders(const int symbolId, const int allocationId) const;
  QList<Result> runDetection(std::function<Result (const int, const int, const Result &)> analyze);
  Result detectDramContention(const int symbolId, const int allocation, const Result &rIn) const;
  Result detectFalseSharing(const int symbolId, const int allocation, const Result &rIn) const;
  unsigned int getSampleCount(const int symbolId, const int allocationId) const;
};

#endif // AUTOANALYSIS_H
