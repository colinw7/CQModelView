TEMPLATE = app

TARGET = CQModelViewTest

DEPENDPATH += .

INCLUDEPATH += ../include .

QMAKE_CXXFLAGS += -std=c++14

CONFIG += debug

MOC_DIR = .moc

QT += widgets svg

# Input
SOURCES += \
CQModelViewTest.cpp \
\
CQCsvModel.cpp \
CQDataModel.cpp \
CQBaseModel.cpp \
CQModelDetails.cpp \
CQModelUtil.cpp \
CQModelVisitor.cpp \
CQSortModel.cpp \
CQValueSet.cpp \
CQTrie.cpp \

HEADERS += \
CQModelViewTest.h \
\
CQCsvModel.h \
CCsv.h \
CQDataModel.h \
CQBaseModel.h \
CQModelDetails.h \
CQModelUtil.h \
CQModelVisitor.h \
CQSortModel.h \
CQValueSet.h \
CQStatData.h \
CQTrie.h \

DESTDIR     = ../bin
OBJECTS_DIR = ../obj
LIB_DIR     = ../lib

INCLUDEPATH += \
. \
../include \
../../CArgs/include \
../../CMath/include \
../../COS/include \

unix:LIBS += \
-L$$LIB_DIR \
-L../../CArgs/lib \
-L../../CStrUtil/lib \
-L../../COS/lib \
-lCQModelView -lCArgs -lCStrUtil -lCOS \
