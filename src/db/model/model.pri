# CAN/LIN description database model: the in-memory object graph that the DBC
# and LDF parsers (../dbc, ../ldf) build and the GUI reads.
#
# These headers include core headers (BusMessage.h) by bare name, so core.pri's
# INCLUDEPATH must be in effect as well — src.pro includes both.

INCLUDEPATH += $$PWD

SOURCES += \
    $$PWD/CanDb.cpp \
    $$PWD/CanDbMessage.cpp \
    $$PWD/CanDbNode.cpp \
    $$PWD/CanDbSignal.cpp \
    $$PWD/LinDb.cpp \
    $$PWD/LinFrame.cpp \
    $$PWD/LinSignal.cpp

HEADERS += \
    $$PWD/CanDb.h \
    $$PWD/CanDbMessage.h \
    $$PWD/CanDbNode.h \
    $$PWD/CanDbSignal.h \
    $$PWD/LinDb.h \
    $$PWD/LinFrame.h \
    $$PWD/LinSignal.h
