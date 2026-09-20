# Description-database subsystem: the shared model plus the parsers that fill it.
#   model/ — CanDb* / Lin* object graph
#   dbc/   — DBC (CAN) text parser
#   ldf/   — LDF (LIN) parser, header-only

include($$PWD/model/model.pri)
include($$PWD/dbc/dbc.pri)
include($$PWD/ldf/ldf.pri)
