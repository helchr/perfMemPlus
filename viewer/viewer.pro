#-------------------------------------------------
#
# Project created by QtCreator 2017-11-13T13:54:33
#
#-------------------------------------------------

QT       += sql core gui concurrent
QT       += charts

greaterThan(QT_MAJOR_VERSION, 5): QT += widgets

TARGET = viewer
TEMPLATE = app
CONFIG += c++14

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
        main.cpp \
        analysismain.cpp \
    memorylevelwindow.cpp \
    percentdelegate.cpp \
    objectaccessedbyfunctionwindow.cpp \
    allocationcallpathresolver.cpp \
    pdfwriter.cpp \
    timelinewidget.cpp \
    timelinewindow.cpp \
    memorycoherencywindow.cpp \
    graphwindow.cpp \
    sqlutils.cpp \
    guiutils.cpp \
    timelineaxiswidget.cpp \
    abstracttimelinewidget.cpp \
    autoanalysis.cpp \
    treeitem.cpp \
    treemodel.cpp

HEADERS += \
        analysismain.h \
    initdb.h \
    memorylevelwindow.h \
    percentdelegate.h \
    objectaccessedbyfunctionwindow.h \
    allocationcallpathresolver.h \
    pdfwriter.h \
    timelinewidget.h \
    timelinewindow.h \
    memorycoherencywindow.h \
    graphwindow.h \
    sqlutils.h \
    guiutils.h \
    timelineaxiswidget.h \
    abstracttimelinewidget.h \
    autoanalysis.h \
    treeitem.h \
    treemodel.h

FORMS += \
        analysismain.ui \
    memorylevelwindow.ui \
    objectaccessedbyfunctionwindow.ui \
    timelinewindow.ui \
    memorycoherencywindow.ui \
    graphwindow.ui
