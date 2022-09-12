TEMPLATE = app

TARGET = CQModelViewTest

DEPENDPATH += .

INCLUDEPATH += ../include .

QMAKE_CXXFLAGS += -std=c++17

CONFIG += debug

MOC_DIR = .moc

QT += widgets svg

# Input
SOURCES += \
CQModelViewTest.cpp \
\
CQJsonModel.cpp \
CQCsvModel.cpp \
CJson.cpp \

HEADERS += \
CQModelViewTest.h \
\
CQJsonModel.h \
CQCsvModel.h \
CJson.h \
CCsv.h \

DESTDIR     = ../bin
OBJECTS_DIR = ../obj
LIB_DIR     = ../lib

INCLUDEPATH += \
. \
../include \
../../CQBaseModel/include \
../../CArgs/include \
../../CMath/include \
../../CUtil/include \
../../CStrUtil/include \
../../COS/include \

unix:LIBS += \
-L$$LIB_DIR \
-L../../CQBaseModel/lib \
-L../../CQUtil/lib \
-L../../CArgs/lib \
-L../../CStrUtil/lib \
-L../../CRegExp/lib \
-L../../COS/lib \
-lCQModelView -lCQBaseModel -lCQUtil \
-lCArgs -lCRegExp -lCStrUtil -lCOS \
-ltre
