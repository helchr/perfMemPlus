#ifndef AUTOANALYSIS_H
#define AUTOANALYSIS_H

#include <QString>
#include <QVariant>

class QSqlQuery;

struct BandwidthResult
  {
    QString memory;
    float latencySavings = 0;
    float latencySavingsPercent = 0;
    float latency = 0;
    bool problem = false;
    bool lowSampleCount = false;
  };

  struct Result
  {
    QString function = "";
    int allocationId = -1;
    int callPathId = -1;
    bool falseSharing = false;
    unsigned int hitmCount = 0;
    bool workingSetL1 = false;
    bool workingSetL2 = false;
    bool accessPattern = false;
    bool modifiedCachelines = false;
    bool trueSharing = false;
    float functionExectimePercent = 0;
    float objectLatencyPercent = 0;
    float hitmPercent = 0;
    float latencyOverLimitPercent = 0;
    float l1HitRate = 0;
    float l2HitRate = 0;
    float falseSharingLatencySavings = 0;
    float falseSharingLatencySavingsPercent = 0;
    float l1LatencySavings = 0;
    float l1LatencySavingsPercent = 0;
    float l2LatencySavings = 0;
    float l2LatencySavingsPercent = 0;
   float memoryStallCycles = 0;
   BandwidthResult l1Bandwidth;
   BandwidthResult l2Bandwidth;
   BandwidthResult l3Bandwidth;
   BandwidthResult dramBandwidth;
   BandwidthResult remoteDramBandwidth;
   BandwidthResult remoteCacheBandwidth;
  };


class AutoAnalysis
{
public:
  AutoAnalysis();
  ~AutoAnalysis();
  QList<Result> run(bool considerObjects);

private:
  unsigned int hitmLimit = 1;
  unsigned int lfbLatencyLimit = 290;
  unsigned int dramLatencyLimit = 466;
  unsigned int sampleCountLimit = 10;
  float cacheHitRateLimit = 80;
  float cacheHitRateMargin = 10;
  QHash<QString,float> latencyLimit \
  {{QStringLiteral("L1"),0}, {QStringLiteral("L2"),0}, {QStringLiteral("L3"),0}, \
  {QStringLiteral("Local DRAM"),0}, {QStringLiteral("Remote DRAM (1 hop)"),0}, \
  {QStringLiteral("Remote Cache (1 hops)"),0}};

  QList<int> getOffendingFunctions() const;
  QList<Result> analyzeFunctions(const QList<int>& symbols) const;
  Result analyzeFunction(const int symbolId) const;
  unsigned int getNumberOfHitmInFunction(const int symbolId, const int allocationId = -1) const;
  unsigned int getLatencyInLfb(const int symbolId, const int allocationId = -1) const;
  unsigned int getLatencyInDram(const int symbolId, const int allocationId = -1) const;
  QString symbolIdToFuctionName(const int symbolId) const;
  QList<int> getMostAccessedObjectsOfFunction(const int symbolId) const;

  //AutoAnalysis::Result analyzeFunctionAndObject(const int symbolId, const int allocationId = -1) const;
  Result analyzeFunctionAndObject(const int symbolId, const int allocation) const;
  QString allocationLimitString(const int allocationdId) const;
  QList<Result> analyzeFunctionsWithObjects(const QList<int> &functions) const;
  float getL1HitRate(const int symbolId, const int allocationdId = -1) const;
  float getL2HitRate(const int symbolId, const int allocationId = -1) const;
  float getL3HitRate(const int symbolId, const int allocationId = -1) const;

  void loadSettings(const QString& path = "settings.conf");
  void saveSettings(const QString& path = "settings.conf");
  QMap<int, QList<int> > groupObjectsByAllocationCallpath(const QList<int> &allocations) const;
  int getCallpathIdOfAllocation(const int symbolId) const;
  void createIndexes() const;
  unsigned int getNumberOfAccesses(const int symbolId, const int allocationId) const;
  unsigned int getDramSampleCount(const int symbolId, const int allocationId) const;
  float getObjectLatencyPercent(const int symboldId, const int allocation) const;
  float getExecTimePercent(const int symbolId) const;
  QString executeSingleStringQuery(QSqlQuery &q) const;
  float executeSingleFloatQuery(QSqlQuery &q) const;
  QVariant executeSingleResultQuery(QSqlQuery &q) const;
  unsigned int executeSingleUnsignedIntQuery(QSqlQuery &q) const;
  float getL1Latency(const int symbolId, const int allocationId) const;
  float getL2Latency(const int symbolId, const int allocationId) const;
  unsigned int getL2HitCount(const int symbolId, const int allocationId) const;
  unsigned int getL3HitCount(const int symbolId, const int allocationId) const;
  float getNonHimLatency(const int symbolId, const int allocationId, const QStringList &memories) const;
  float getHimLatency(const int symbolId, const int allocationId) const;
  float getL3Latency(const int symbolId, const int allocationId) const;
  unsigned int getDramHitCount(const int symbolId, const int allocationId) const;
  float getMemoryStallCycles(const int symbolId) const;
  QStringList getHitmMemoryLevels(const int symbolId, const int allocationId) const;
  unsigned int getSamplerate() const;
  bool areAddressesInObjectShared(const int allocationId) const;
  bool isObjectWrittenByMultipleThreads(const int allocationId) const;
  float getLatencyLimit(const QString &memory) const;
  unsigned int getSampleCountInMemory(const int symbolId, const int allocationId, const QString &memory) const;
  float getLatencyInMemory(const int symbolId, const int allocationId, const QString &memory) const;
  BandwidthResult checkBandwidth(const QString &memory, const int symbolId, const int allocationId) const;
};

#endif // AUTOANALYSIS_H
