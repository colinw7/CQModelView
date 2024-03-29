TEMPLATE = lib

TARGET = CQModelView

DEPENDPATH += .

QT += widgets svg

CONFIG += staticlib

QMAKE_CXXFLAGS += -std=c++17

MOC_DIR = .moc

SOURCES += \
CQModelView.cpp \
CQModelViewHeader.cpp \
CQItemDelegate.cpp \

HEADERS += \
../include/CQModelView.h \
../include/CQModelViewHeader.h \
../include/CQItemDelegate.h \

OBJECTS_DIR = ../obj

DESTDIR = ../lib

INCLUDEPATH += \
. \
../include \
../../CQBaseModel/include \
