/*

  Copyright (c) 2026 Schildkroet

  This file is part of cangaroo.

  cangaroo is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  cangaroo is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with cangaroo.  If not, see <http://www.gnu.org/licenses/>.

*/

// LIN Description File parser (src/db/ldf/ldf_parser.h).
//
// Header-only and free of Qt, but 1100 lines of hand-written recursive descent
// with unit-suffix handling (ms/us, kbps, k/M multipliers) and a std::variant for
// schedule commands -- plenty of room for quiet misparses.

#include <string>
#include <variant>

#include <QtTest>

#include "db/ldf/ldf_parser.h"

namespace
{

// A small but broad LDF: every section the parser claims to support, with
// deliberately varied number formats and comment styles.
const std::string k_sampleLdf = R"(LIN_description_file;
LIN_protocol_version = "2.1";
LIN_language_version = "2.1";
LIN_speed = 19.2 kbps;
Channel_name = "TestChannel";

Nodes {
    Master: MasterNode, 5 ms, 0.1 ms ;
    Slaves: SlaveA, SlaveB, SlaveC ;
}

Signals {
    // scalar init value
    MotorSpeed: 16, 0, MasterNode, SlaveA, SlaveB;
    Switch:      1, 1, SlaveA, MasterNode;
    HexInit:     8, 0x2A, SlaveA, MasterNode;
    /* array init value */
    Payload:    32, {0x11, 0x22, 0x33, 0x44}, SlaveB, MasterNode;
}

Frames {
    MasterFrame: 0x10, MasterNode, 4 {
        MotorSpeed, 0;
        Switch, 16;
    }
    SlaveFrame: 0x20, SlaveA, 8 {
        Payload, 0;
    }
}

Sporadic_frames {
    SporadicGroup: MasterFrame;
}

Event_triggered_frames {
    EventFrame: SchedTable, 0x30, SlaveFrame;
}

Diagnostic_frames {
    MasterReq: 0x3C {
        MotorSpeed, 0;
    }
    SlaveResp: 0x3D {
        Switch, 0;
    }
}

Schedule_tables {
    SchedTable {
        MasterFrame delay 10 ms;
        SlaveFrame delay 20 ms;
        MasterReq delay 10 ms;
        SlaveResp delay 10 ms;
        AssignNAD { SlaveA } delay 15 ms;
        AssignFrameId { SlaveA, SlaveFrame } delay 15 ms;
    }
    SecondTable {
        MasterFrame delay 5 ms;
    }
}

Signal_encoding_types {
    SwitchEncoding {
        logical_value, 0, "off";
        logical_value, 1, "on";
    }
    SpeedEncoding {
        physical_value, 0, 65535, 0.25, -100, "rpm";
    }
}

Signal_representation {
    SwitchEncoding: Switch;
    SpeedEncoding: MotorSpeed;
}
)";

} // namespace

class LdfParserTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void parsesHeader();
    void parsesNodes();
    void parsesSignals();
    void parsesArrayInitValue();
    void parsesFramesAndSignalOffsets();
    void parsesSporadicAndEventTriggeredFrames();
    void parsesDiagnosticFrames();
    void parsesScheduleTables();
    void parsesLogicalEncoding();
    void parsesPhysicalEncoding();
    void parsesSignalRepresentation();

    void findHelpersReturnNullForUnknownNames();

    void speedUnits_data();
    void speedUnits();

    void timeUnitsAreNormalisedToSeconds();
    void toleratesCommentsAndUnknownSections();

    void rejectsFileWithoutMagicHeader();
    void rejectsTruncatedFile();
    void reportsErrorInsteadOfThrowing();
};

void LdfParserTest::parsesHeader()
{
    const auto result = ldf::parse(k_sampleLdf);
    QVERIFY(result.has_value());

    QCOMPARE(result->lin_protocol_version, std::string("2.1"));
    QCOMPARE(result->lin_language_version, std::string("2.1"));
    QCOMPARE(result->channel_name, std::string("TestChannel"));
    QCOMPARE(result->lin_speed_bps, 19200.0);
}

void LdfParserTest::parsesNodes()
{
    const auto result = ldf::parse(k_sampleLdf);
    QVERIFY(result.has_value());

    QCOMPARE(result->nodes.master, std::string("MasterNode"));
    QCOMPARE(result->nodes.slaves.size(), size_t(3));
    QCOMPARE(result->nodes.slaves[0], std::string("SlaveA"));
    QCOMPARE(result->nodes.slaves[2], std::string("SlaveC"));

    // "5 ms" and "0.1 ms" are stored in seconds.
    QCOMPARE(result->nodes.master_time_base_s, 0.005);
    QVERIFY(qFuzzyCompare(result->nodes.master_jitter_s, 0.0001));
}

void LdfParserTest::parsesSignals()
{
    const auto result = ldf::parse(k_sampleLdf);
    QVERIFY(result.has_value());
    QCOMPARE(result->signals.size(), size_t(4));

    const ldf::Signal *speed = result->find_signal("MotorSpeed");
    QVERIFY(speed != nullptr);
    QCOMPARE(speed->bit_length, 16u);
    QCOMPARE(speed->init_value, 0ULL);
    QCOMPARE(speed->publisher, std::string("MasterNode"));
    QCOMPARE(speed->subscribers.size(), size_t(2));

    // Hexadecimal init values must be read as hex, not decimal.
    const ldf::Signal *hexInit = result->find_signal("HexInit");
    QVERIFY(hexInit != nullptr);
    QCOMPARE(hexInit->init_value, 42ULL);
}

void LdfParserTest::parsesArrayInitValue()
{
    const auto result = ldf::parse(k_sampleLdf);
    QVERIFY(result.has_value());

    const ldf::Signal *payload = result->find_signal("Payload");
    QVERIFY(payload != nullptr);
    QCOMPARE(payload->bit_length, 32u);
    QCOMPARE(payload->init_array.size(), size_t(4));
    QCOMPARE(payload->init_array[0], uint8_t(0x11));
    QCOMPARE(payload->init_array[3], uint8_t(0x44));
    QCOMPARE(payload->publisher, std::string("SlaveB"));
}

void LdfParserTest::parsesFramesAndSignalOffsets()
{
    const auto result = ldf::parse(k_sampleLdf);
    QVERIFY(result.has_value());
    QCOMPARE(result->frames.size(), size_t(2));

    const ldf::Frame *master = result->find_frame("MasterFrame");
    QVERIFY(master != nullptr);
    QCOMPARE(master->id, uint8_t(0x10));
    QCOMPARE(master->publisher, std::string("MasterNode"));
    QCOMPARE(master->length, uint8_t(4));
    QCOMPARE(master->signals.size(), size_t(2));
    QCOMPARE(master->signals[0].signal_name, std::string("MotorSpeed"));
    QCOMPARE(master->signals[0].bit_offset, 0u);
    QCOMPARE(master->signals[1].signal_name, std::string("Switch"));
    QCOMPARE(master->signals[1].bit_offset, 16u);

    const ldf::Frame *slave = result->find_frame("SlaveFrame");
    QVERIFY(slave != nullptr);
    QCOMPARE(slave->id, uint8_t(0x20));
    QCOMPARE(slave->length, uint8_t(8));
}

void LdfParserTest::parsesSporadicAndEventTriggeredFrames()
{
    const auto result = ldf::parse(k_sampleLdf);
    QVERIFY(result.has_value());

    QCOMPARE(result->sporadic_frames.size(), size_t(1));
    QCOMPARE(result->sporadic_frames[0].name, std::string("SporadicGroup"));
    QCOMPARE(result->sporadic_frames[0].frames.size(), size_t(1));

    QCOMPARE(result->event_triggered_frames.size(), size_t(1));
    QCOMPARE(result->event_triggered_frames[0].name, std::string("EventFrame"));
    QCOMPARE(result->event_triggered_frames[0].frame_id, uint8_t(0x30));
    QCOMPARE(result->event_triggered_frames[0].schedule_table, std::string("SchedTable"));
}

void LdfParserTest::parsesDiagnosticFrames()
{
    const auto result = ldf::parse(k_sampleLdf);
    QVERIFY(result.has_value());

    QCOMPARE(result->diagnostic_frames.size(), size_t(2));
    QCOMPARE(result->diagnostic_frames[0].name, std::string("MasterReq"));
    QCOMPARE(result->diagnostic_frames[0].id, uint8_t(0x3C));
    QCOMPARE(result->diagnostic_frames[1].id, uint8_t(0x3D));
}

// Schedule entries are a std::variant; the command type must survive parsing,
// not just the delay.
void LdfParserTest::parsesScheduleTables()
{
    const auto result = ldf::parse(k_sampleLdf);
    QVERIFY(result.has_value());
    QCOMPARE(result->schedule_tables.size(), size_t(2));

    const ldf::ScheduleTable &table = result->schedule_tables[0];
    QCOMPARE(table.name, std::string("SchedTable"));
    QCOMPARE(table.entries.size(), size_t(6));

    QVERIFY(std::holds_alternative<ldf::UnconditionalCmd>(table.entries[0].command));
    QCOMPARE(std::get<ldf::UnconditionalCmd>(table.entries[0].command).frame_name,
             std::string("MasterFrame"));
    QCOMPARE(table.entries[0].delay_s, 0.010);
    QCOMPARE(table.entries[1].delay_s, 0.020);

    QVERIFY(std::holds_alternative<ldf::MasterReqCmd>(table.entries[2].command));
    QVERIFY(std::holds_alternative<ldf::SlaveRespCmd>(table.entries[3].command));

    QVERIFY(std::holds_alternative<ldf::AssignNadCmd>(table.entries[4].command));
    QCOMPARE(std::get<ldf::AssignNadCmd>(table.entries[4].command).node_name,
             std::string("SlaveA"));

    QVERIFY(std::holds_alternative<ldf::AssignFrameIdCmd>(table.entries[5].command));
    const auto &assign = std::get<ldf::AssignFrameIdCmd>(table.entries[5].command);
    QCOMPARE(assign.node_name, std::string("SlaveA"));
    QCOMPARE(assign.frame_name, std::string("SlaveFrame"));

    QCOMPARE(result->schedule_tables[1].name, std::string("SecondTable"));
    QCOMPARE(result->schedule_tables[1].entries.size(), size_t(1));
}

void LdfParserTest::parsesLogicalEncoding()
{
    const auto result = ldf::parse(k_sampleLdf);
    QVERIFY(result.has_value());
    QCOMPARE(result->signal_encoding_types.size(), size_t(2));

    const ldf::SignalEncodingType &enc = result->signal_encoding_types[0];
    QCOMPARE(enc.name, std::string("SwitchEncoding"));
    QCOMPARE(enc.values.size(), size_t(2));

    QVERIFY(std::holds_alternative<ldf::LogicalValue>(enc.values[0]));
    const auto &off = std::get<ldf::LogicalValue>(enc.values[0]);
    QCOMPARE(off.signal_value, 0u);
    QCOMPARE(off.text, std::string("off"));

    const auto &on = std::get<ldf::LogicalValue>(enc.values[1]);
    QCOMPARE(on.signal_value, 1u);
    QCOMPARE(on.text, std::string("on"));
}

void LdfParserTest::parsesPhysicalEncoding()
{
    const auto result = ldf::parse(k_sampleLdf);
    QVERIFY(result.has_value());

    const ldf::SignalEncodingType &enc = result->signal_encoding_types[1];
    QCOMPARE(enc.name, std::string("SpeedEncoding"));
    QCOMPARE(enc.values.size(), size_t(1));

    QVERIFY(std::holds_alternative<ldf::PhysicalRange>(enc.values[0]));
    const auto &range = std::get<ldf::PhysicalRange>(enc.values[0]);
    QCOMPARE(range.min_value, 0.0);
    QCOMPARE(range.max_value, 65535.0);
    QCOMPARE(range.scale, 0.25);
    QCOMPARE(range.offset, -100.0);
    QCOMPARE(range.unit, std::string("rpm"));
}

void LdfParserTest::parsesSignalRepresentation()
{
    const auto result = ldf::parse(k_sampleLdf);
    QVERIFY(result.has_value());

    QCOMPARE(result->signal_representation.size(), size_t(2));
    QVERIFY(result->signal_representation.count("SwitchEncoding") == 1);
    QCOMPARE(result->signal_representation.at("SwitchEncoding").size(), size_t(1));
    QCOMPARE(result->signal_representation.at("SwitchEncoding")[0], std::string("Switch"));
}

void LdfParserTest::findHelpersReturnNullForUnknownNames()
{
    const auto result = ldf::parse(k_sampleLdf);
    QVERIFY(result.has_value());

    QVERIFY(result->find_signal("NoSuchSignal") == nullptr);
    QVERIFY(result->find_frame("NoSuchFrame") == nullptr);
}

void LdfParserTest::speedUnits_data()
{
    QTest::addColumn<QString>("speedLiteral");
    QTest::addColumn<double>("expectedBps");

    QTest::newRow("kbps decimal") << "19.2 kbps" << 19200.0;
    QTest::newRow("kbps integer") << "20 kbps"   << 20000.0;
    QTest::newRow("bps")          << "9600 bps"  << 9600.0;
}

void LdfParserTest::speedUnits()
{
    QFETCH(QString, speedLiteral);
    QFETCH(double, expectedBps);

    const std::string source =
        "LIN_description_file;\nLIN_protocol_version = \"2.1\";\nLIN_speed = "
        + speedLiteral.toStdString() + ";\n";

    const auto result = ldf::parse(source);
    QVERIFY(result.has_value());
    QVERIFY(qFuzzyCompare(result->lin_speed_bps, expectedBps));
}

// Times are stored in seconds regardless of the suffix used in the file.
void LdfParserTest::timeUnitsAreNormalisedToSeconds()
{
    const std::string source = R"(LIN_description_file;
LIN_protocol_version = "2.1";
Nodes {
    Master: M, 1000 us, 500 us ;
    Slaves: S ;
}
)";

    const auto result = ldf::parse(source);
    QVERIFY(result.has_value());
    QVERIFY(qFuzzyCompare(result->nodes.master_time_base_s, 0.001));
    QVERIFY(qFuzzyCompare(result->nodes.master_jitter_s, 0.0005));
}

void LdfParserTest::toleratesCommentsAndUnknownSections()
{
    const std::string source = R"(LIN_description_file;
// line comment
/* block
   comment */
LIN_protocol_version = "2.1";
Some_future_section {
    whatever { nested }
}
Signals {
    A: 8, 0, M, S;
}
)";

    const auto result = ldf::parse(source);
    QVERIFY(result.has_value());
    QCOMPARE(result->signals.size(), size_t(1));
    QCOMPARE(result->signals[0].name, std::string("A"));
}

void LdfParserTest::rejectsFileWithoutMagicHeader()
{
    const auto result = ldf::parse("Signals { A: 8, 0, M, S; }\n");
    QVERIFY(!result.has_value());
    QVERIFY(!result.error().empty());
}

void LdfParserTest::rejectsTruncatedFile()
{
    const auto result = ldf::parse(R"(LIN_description_file;
Signals {
    A: 8, 0, M
)");
    QVERIFY(!result.has_value());
}

// The parser must report failures through std::expected rather than letting an
// exception escape into the caller.
void LdfParserTest::reportsErrorInsteadOfThrowing()
{
    bool threw = false;
    try
    {
        const auto result = ldf::parse("LIN_description_file;\nNodes { Master: ");
        QVERIFY(!result.has_value());
    }
    catch (...)
    {
        threw = true;
    }

    QVERIFY(!threw);
}

QTEST_APPLESS_MAIN(LdfParserTest)

#include "LdfParserTest.moc"
