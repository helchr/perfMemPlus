#ifndef AUTOANALYSIS_H
#define AUTOANALYSIS_H

#include <QString>
#include <QVariant>

class QSqlQuery;

class AutoAnalysis
{
public:
  struct Result
  {
    QString function = "";
    int allocationId = -1;
    int callPathId = -1;
    bool falseSharing = false;
    unsigned int hitmCount = 0;
    bool bandwidth = false;
    float bandwidthLimitPercent = 0;
    bool workingSetL1 = false;
    bool workingSetL2 = false;
    bool accessPattern = false;
    float functionExectimePercent = 0;
    float objectLatencyPercent = 0;
    float hitmPercent = 0;
    float latencyOverLimitPercent = 0;
    float l1HitRate = 0;
    float l2HitRate = 0;
  };

  AutoAnalysis();
  ~AutoAnalysis();
  QList<AutoAnalysis::Result> run(bool considerObjects);

private:
  unsigned int hitmLimit = 5;
  unsigned int lfbLatencyLimit = 290;
  unsigned int dramLatencyLimit = 466;
  unsigned int dramSampleCountLimit = 10;
  float cacheHitRateLimit = 80;
  float cacheHitRateMargin = 10;

  QList<int> getOffendingFunctions() const;
  QList<AutoAnalysis::Result> analyzeFunctions(const QList<int>& symbols) const;
  AutoAnalysis::Result analyzeFunction(const int symbolId) const;
  unsigned int getNumberOfHitmInFunction(const int symbolId, const int allocationId = -1) const;
  unsigned int getLatencyInLfb(const int symbolId, const int allocationId = -1) const;
  unsigned int getLatencyInDram(const int symbolId, const int allocationId = -1) const;
  QString symbolIdToFuctionName(const int symbolId) const;
  QList<int> getMostAccessedObjectsOfFunction(const int symbolId) const;

  //AutoAnalysis::Result analyzeFunctionAndObject(const int symbolId, const int allocationId = -1) const;
  AutoAnalysis::Result analyzeFunctionAndObject(const int symbolId, const int allocation) const;
  QString allocationLimitString(const int allocationdId) const;
  QList<AutoAnalysis::Result> analyzeFunctionsWithObjects(const QList<int> &functions) const;
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
};

#endif // AUTOANALYSIS_H
