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
CQJsonModel.cpp \
CQCsvModel.cpp \
CQDataModel.cpp \
CQBaseModel.cpp \
CQModelDetails.cpp \
CQModelUtil.cpp \
CQModelVisitor.cpp \
CQSortModel.cpp \
CQValueSet.cpp \
CQModelNameValues.cpp \
CQStrParse.cpp \
CQTrie.cpp \
CJson.cpp \

HEADERS += \
CQModelViewTest.h \
\
CQJsonModel.h \
CQCsvModel.h \
CQDataModel.h \
CQBaseModel.h \
CQModelDetails.h \
CQModelUtil.h \
CQModelVisitor.h \
CQSortModel.h \
CQValueSet.h \
CQStatData.h \
CQModelNameValues.h \
CQStrParse.h \
CQTrie.h \
CJson.h \
CCsv.h \

DESTDIR     = ../bin
OBJECTS_DIR = ../obj
LIB_DIR     = ../lib

INCLUDEPATH += \
. \
../include \
../../CArgs/include \
../../CMath/include \
../../CUtil/include \
../../CStrUtil/include \
../../COS/include \

unix:LIBS += \
-L$$LIB_DIR \
-L../../CArgs/lib \
-L../../CStrUtil/lib \
-L../../COS/lib \
-lCQModelView -lCArgs -lCStrUtil -lCOS \
