QT += charts

# QT += network
SUBDIRS += src

# Unit tests are opt-in so the normal build needs no Qt Test dependency:
#   qmake6 CONFIG+=tests && make && ./build/tests/bus_message_signal_test
tests: SUBDIRS += tests

TEMPLATE = subdirs
CONFIG += ordered warn_on qt debug_and_release
CONFIG += c++20
LIBS += -lbsd
