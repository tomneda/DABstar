#
TEMPLATE    = app
CONFIG      += console
QT          += widgets charts

INCLUDEPATH += . \
               ../../scopes \
               ../../main

HEADERS     = ./dump_viewer.h \
              ../../scopes/plot_widget.h
SOURCES     = ./dump_viewer.cpp main.cpp \
              ../../scopes/plot_widget.cpp
TARGET      = dumpViewer
FORMS		+= ./dumpwidget.ui

unix{
DESTDIR     = ./linux-bin
}

