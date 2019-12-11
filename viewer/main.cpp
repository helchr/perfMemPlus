#include "analysismain.h"
#include <QApplication>
#include "autoanalysis.h"
#include <QTextStream>

int main(int argc, char *argv[])
{

    QApplication a(argc, argv);

    QApplication::setApplicationName("PerfMemPlus Viewer");
    QApplication::setApplicationVersion("1.0");
    QCommandLineParser parser;
    parser.setApplicationDescription("View the results of a PerfMemPlus profiling session");
    parser.addHelpOption();
    parser.addPositionalArgument("dbPath","Path to database file");
    QCommandLineOption falseSharingAnalysisOpt("falseSharing","Runs the false sharing detection and prints the results without showing the GUI");
    parser.addOption(falseSharingAnalysisOpt);
    QCommandLineOption dramContetionAnalysisOpt("dramContention","Runs the DRAM contention detection and prints the results without showing the GUI");
    parser.addOption(dramContetionAnalysisOpt);
    QCommandLineOption configPathOpt("config","path to the configuration file","file");
    parser.addOption(configPathOpt);
    parser.process(a);
    AnalysisMain w;

    auto arguments = parser.positionalArguments();
    QString dbPath = "";
    auto configPath = parser.value(configPathOpt);
    auto headless = parser.isSet((falseSharingAnalysisOpt)) | parser.isSet((dramContetionAnalysisOpt));
    if(arguments.size() > 0)
    {
        dbPath = arguments.first();
        w.setDbPath(dbPath,headless);
    }

    if(headless && dbPath == "")
    {
        qDebug() << "No input file set in automatic mode";
        a.exit(1);
    }
    if(parser.isSet(falseSharingAnalysisOpt))
    {
        AutoAnalysis aa;
        if(configPath!="")
        {
            aa.loadSettings(configPath);
        }
        auto results = aa.runFalseSharingDetection();
        QTextStream out(stdout);
        out << "function,allocationId,intraObjectTrueSharing,interObjectTrueSharing,intraObjectFalseSharing,interObjectFalseSharing,hitmCount\n";
        for(auto r : results)
        {
            if(r.falseSharing == true || r.trueSharing == true)
            {
                auto calcDiff = [](const QPair<ClAccess,ClAccess>& p){return llabs(p.first.time - p.second.time);};
                out << r.function << ',' << r.callPathId << ',';
                out << !r.intraObjectTrueSharing.empty() << ',' << !r.interObjectTrueSharing.empty() << ',';
                out << !r.intraObjectFalseSharing.empty() << ',' << !r.interObjectFalseSharing.empty() << ',';
                out << r.hitmCount << '\n';
/*
                for (auto d : r.interObjectTrueSharing) {
                    out << "interObjectTrueSharing" << ',' << calcDiff(d) << '\n';
                }
                for (auto d : r.interObjectFalseSharing) {
                    out << "interObjectFalseSharing" << ',' << calcDiff(d) << '\n';
                }
                for (auto d : r.intraObjectTrueSharing) {
                    out << "intraObjectTrueSharing" << ',' << calcDiff(d) << '\n';
                }
                for (auto d : r.intraObjectFalseSharing) {
                    out << "intraObjectFalseSharing" << ',' << calcDiff(d) << ',' << d.first.ip << ',' << d.second.ip << ',' << d.first.adr << ',' << d.second.adr << ',' << d.first.allocationId << ',' << d.first.tid << '\n';
                    out << "intraObjectFalseSharing" << ',' << d.first.time << ',' << d.second.time << ',' << d.first.tid << ',' << d.second.tid << '\n';
                }

                */
            }
        }
    }
    if(parser.isSet(dramContetionAnalysisOpt))
    {
        AutoAnalysis aa;
        if(configPath!="")
        {
            aa.loadSettings(configPath);
        }
        auto results = aa.runDramContentionDetection();
        QTextStream out(stdout);
        out << "function,callPathId,";
        out << "localDramProblem,localDramLowSampleCount,localDramParallelReaders,localDramLatency,";
        out << "remoteDramProblem,remoteDramLowSampleCount,remoteDramParallelReaders,remoteDramLatency\n";
        for(auto r : results)
        {
            if(r.dramBandwidth.problem || r.remoteDramBandwidth.problem)
            {
                out << r.function << ',' << r.callPathId << ',';
                out << r.dramBandwidth.problem << ',' << r.dramBandwidth.lowSampleCount << ',' << r.dramBandwidth.parallelReaders << ',' << r.dramBandwidth.latency << ',';
                out << r.remoteDramBandwidth.problem << ',' << r.remoteDramBandwidth.lowSampleCount << ',' << r.remoteDramBandwidth.parallelReaders << ',' << r.remoteDramBandwidth.latency << '\n';
            }
        }
    }
    if(!headless) // GUI mode
    {
        w.show();
        if(dbPath != "")
        {
            w.setWindowTitle("PerfMemPlus Viewer - " + dbPath);
        }
        return a.exec();
    }
}
