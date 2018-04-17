#include "analysismain.h"
#include <QApplication>

int main(int argc, char *argv[])
{

  QApplication a(argc, argv);

  QApplication::setApplicationName("PerfMemPlus Viewer");
  QApplication::setApplicationVersion("1.0");
  QCommandLineParser parser;
  parser.setApplicationDescription("View the results of a PerfMemPlus profiling session");
  parser.addHelpOption();
  parser.addPositionalArgument("dbPath","Path to database file");
  parser.process(a);
  AnalysisMain w;

  auto arguments = parser.positionalArguments();
  QString dbPath = "";
  if(arguments.size() > 0)
  {
    dbPath = arguments.first();
    w.setDbPath(dbPath);
  }
  w.show();
  if(dbPath != "")
  {
    w.setWindowTitle("PerfMemPlus Viewer - " + dbPath);
  }

  return a.exec();
}
