TEMPLATE = lib

TARGET = CQModelView

DEPENDPATH += .

QT += widgets svg

CONFIG += staticlib

QMAKE_CXXFLAGS += -std=c++14

MOC_DIR = .moc

SOURCES += \
CQModelView.cpp \
CQItemDelegate.cpp \

HEADERS += \
../include/CQModelView.h \
../include/CQItemDelegate.h \
../include/CQBaseModelTypes.h \

OBJECTS_DIR = ../obj

DESTDIR = ../lib

INCLUDEPATH += \
. \
../include \
