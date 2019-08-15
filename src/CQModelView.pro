TEMPLATE = lib

TARGET = CQModelView

DEPENDPATH += .

QT += widgets svg

CONFIG += staticlib

QMAKE_CXXFLAGS += -std=c++14

MOC_DIR = .moc

SOURCES += \
CQBaseModel.cpp \
CQCsvModel.cpp \
CQDataModel.cpp \
CQItemDelegate.cpp \
CQModelDetails.cpp \
CQModelUtil.cpp \
CQModelView.cpp \
CQModelVisitor.cpp \
CQSortModel.cpp \
CQValueSet.cpp \
CQTrie.cpp \

HEADERS += \
../include/CCsv.h \
../include/CQBaseModel.h \
../include/CQBaseModelTypes.h \
../include/CQCsvModel.h \
../include/CQDataModel.h \
../include/CQItemDelegate.h \
../include/CQModelDetails.h \
../include/CQModelUtil.h \
../include/CQModelView.h \
../include/CQModelVisitor.h \
../include/CQSortModel.h \
../include/CQStatData.h \
../include/CQValueSet.h \
../include/CQTrie.h \

OBJECTS_DIR = ../obj

DESTDIR = ../lib

INCLUDEPATH += \
. \
../include \
../../CMath/include \
../../COS/include \
