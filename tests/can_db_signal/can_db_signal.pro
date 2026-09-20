TARGET = can_db_signal_test
include(../common.pri)

SOURCES += \
    CanDbSignalTest.cpp \
    $$SRC_DIR/core/BusMessage.cpp \
    $$SRC_DIR/db/model/CanDbSignal.cpp \
    $$SRC_DIR/db/model/CanDbMessage.cpp
