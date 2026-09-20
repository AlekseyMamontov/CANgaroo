TARGET = can_db_test
include(../common.pri)

# CanDb::saveXML takes a QDomDocument, hence QtXml. It ignores its Backend
# argument, so no Backend symbols are pulled in.
QT += xml

SOURCES += \
    CanDbTest.cpp \
    ../support/LogStub.cpp \
    $$SRC_DIR/db/dbc/DbcParser.cpp \
    $$SRC_DIR/db/dbc/DbcTokens.cpp \
    $$SRC_DIR/core/BusMessage.cpp \
    $$SRC_DIR/db/model/CanDb.cpp \
    $$SRC_DIR/db/model/CanDbMessage.cpp \
    $$SRC_DIR/db/model/CanDbNode.cpp \
    $$SRC_DIR/db/model/CanDbSignal.cpp
