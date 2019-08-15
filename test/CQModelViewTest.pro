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

HEADERS += \
CQModelViewTest.h \

DESTDIR     = ../bin
OBJECTS_DIR = ../obj
LIB_DIR     = ../lib

INCLUDEPATH += \
. \
../include \
../../CArgs/include \

unix:LIBS += \
-L$$LIB_DIR \
-L../../CArgs/lib \
-L../../CStrUtil/lib \
-L../../COS/lib \
-lCQModelView -lCArgs -lCStrUtil -lCOS \
