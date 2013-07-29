TEMPLATE = app
TARGET = anthycli
QT = core
CONFIG *= warn_on
MOC_DIR = .build.tmp
OBJECTS_DIR = .build.tmp
DESTDIR=.
HEADERS*=Dic.h Entry.h
SOURCES*=anthycli.cpp Dic.cpp
LIBS*= -lkdecore
QMAKE_CXXFLAGS += -std=gnu++11
