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

// Regression tests for BusMessage signal packing (see issue #34).
//
// Every expected value below was produced by cantools (an independent DBC
// implementation), not by cangaroo itself. That matters: the big-endian bug in
// issue #34 survived for so long precisely because extractRawSignal and
// injectRawSignal were wrong in mutually cancelling ways, so any round-trip
// test written against cangaroo alone passed. Regenerate these vectors with
// cantools if you ever need to extend the table -- never from cangaroo output.

#include <cstdint>

#include <QtTest>

#include "core/BusMessage.h"

namespace
{

struct SignalCase
{
    const char *name;
    uint16_t dbcStartBit;   // start bit exactly as written in the DBC
    uint16_t length;
    bool isBigEndian;
    uint8_t dlc;
    const char *payloadHex; // frame contents for the extract direction
    uint64_t expectedRaw;   // value cantools decodes from payloadHex
    const char *injectedHex;// bytes cantools encodes into an all-zero frame
};

// clang-format off
constexpr SignalCase k_cases[] = {
    // Signals from the issue #34 reproducer DBC (reported by B-2U).
    { "RollingCounter", 11, 4, true, 32,
      "B399F42C41882B08BB90668F970F2C8A5491978077D15B12C4843FC14BF25C32",
      9ULL,
      "0009000000000000000000000000000000000000000000000000000000000000" },   // unaligned nibble, the reported symptom
    // More payloads for the reported signal: a single frame is not enough,
    // because for some payloads the old algorithm landed on the right value by
    // luck. Each of these three decoded wrongly before the fix.
    { "RollingCounter_b", 11, 4, true, 32,
      "676133992D3A3F22C21640BA6287AFB38916F09F7D336BB89C9637CAE6C95FD2",
      1ULL,
      "0001000000000000000000000000000000000000000000000000000000000000" },   // old code said 6
    { "RollingCounter_c", 11, 4, true, 32,
      "63DCAE362676A92DD9915615F2B787ADC6167850670A448E016C345CFDEE8814",
      12ULL,
      "000C000000000000000000000000000000000000000000000000000000000000" },   // old code said 13
    { "RollingCounter_d", 11, 4, true, 32,
      "7684E6329B76FA0D5B2137F3EBB729BFCA7198ECFB2DF180A92D091C052B79AF",
      4ULL,
      "0004000000000000000000000000000000000000000000000000000000000000" },   // old code said 8
    { "ChecksumByte", 7, 8, true, 32,
      "3967B4473E11B1107A61643A6AF8D4CF65ADDFB57C89A8710BC25819B31FF120",
      57ULL,
      "3900000000000000000000000000000000000000000000000000000000000000" },   // byte aligned
    { "StatusBits", 23, 2, true, 32,
      "453E425A957D1F2F45005A99A2B8ACA1127C5ECAD2A9B36E155A50BA7562D3F5",
      1ULL,
      "0000400000000000000000000000000000000000000000000000000000000000" },   // 2 bits at a byte end
    { "Field_18", 18, 3, true, 32,
      "5539D1949E87E9660BC29AC89BCDB0444764C5FE33D66580663999F5BDE3F2C7",
      1ULL,
      "0000010000000000000000000000000000000000000000000000000000000000" },   // 3 bits mid-byte
    { "Field_39", 39, 32, true, 32,
      "E1486FCFA0A1EE406317BF23CB0277A2F719D93A63EBF0F9D3811DC51CFE7AB1",
      2694966848ULL,
      "00000000A0A1EE40000000000000000000000000000000000000000000000000" },   // 32 bits, byte aligned
    { "Field_71", 71, 32, true, 32,
      "239B9779B90044719A46A71B61717452D0C931C9B151476E3BE0EFC4FA73609B",
      2588321563ULL,
      "00000000000000009A46A71B0000000000000000000000000000000000000000" },   // 32 bits, byte aligned, later in frame

    // Big-endian edge cases.
    { "be_1bit_msb", 7, 1, true, 8,
      "DCD64DBB1C825418",
      1ULL,
      "8000000000000000" },   // single MSB of byte 0
    { "be_1bit_lsb", 0, 1, true, 8,
      "D9E9198305137859",
      1ULL,
      "0100000000000000" },   // single LSB of byte 0
    { "be_cross2", 3, 10, true, 8,
      "4A5918251D5A65AC",
      662ULL,
      "0A58000000000000" },   // crosses a byte boundary unaligned
    { "be_cross3", 2, 20, true, 8,
      "550BE1F3531CBCB1",
      661443ULL,
      "050BE18000000000" },   // spans three bytes unaligned
    { "be_full64", 7, 64, true, 8,
      "EC2CECA9CFAC01BC",
      17018237306004046268ULL,
      "EC2CECA9CFAC01BC" },   // full 64-bit signal
    { "be_63", 6, 63, true, 8,
      "37AB0CB5FE845C88",
      4011313868902259848ULL,
      "37AB0CB5FE845C88" },   // 63 bits, unaligned start
    { "be_tail", 52, 9, true, 8,
      "ACDA971D732ABA55",
      421ULL,
      "0000000000001A50" },   // ends near the end of the frame

    // CAN FD: past the first 8 bytes, where the old 64-bit sliding window sat.
    { "be_fd_mid", 207, 24, true, 64,
      "60423BDE17B7294BB9B46D1F3A1798EB8C53D70E0E55640D5292B752EDFADF3C16F765D64A3168FEA58114A3AC4D7EDA81358F21522D7ED6499D7F9C3968F35F",
      9615186ULL,
      "0000000000000000000000000000000000000000000000000092B752000000000000000000000000000000000000000000000000000000000000000000000000" },   // starts in byte 25
    { "be_fd_unaligned", 250, 17, true, 64,
      "5C2BF2ED1C522225F8B32BD8853675BC3135269317155A968966336EB41E0081D712E8582A78219543B5EFC8E7CE9DA5686B4254BC59630306DB9B4D0011201F",
      30148ULL,
      "0000000000000000000000000000000000000000000000000000000000000001D710000000000000000000000000000000000000000000000000000000000000" },   // unaligned, crosses bytes 31-33
    { "be_fd_last", 499, 12, true, 64,
      "710F00360D781DEC24DFCFB5E20A23E1D4559A6BFFFE37058DD2B60067FCF38A615930944FD6D1917E89B83023B06993443FBE3C8DC26A91ADF2F09CE1FEDB00",
      2816ULL,
      "00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000B00" },   // ends on the frame's last bit

    // Little-endian guards: the issue #34 fix must not disturb the Intel path.
    { "le_nibble", 11, 4, false, 8,
      "1D21A08AC364AC61",
      4ULL,
      "0020000000000000" },   // Intel nibble
    { "le_16", 4, 16, false, 8,
      "AB15A19C687FE0AE",
      4442ULL,
      "A015010000000000" },   // Intel 16-bit unaligned
    { "le_full64", 0, 64, false, 8,
      "8216551662096CEB",
      16963944213283935874ULL,
      "8216551662096CEB" },   // Intel full 64-bit
    { "le_fd", 300, 24, false, 64,
      "E51D778B5A32665BDDE0B1C88679D8EE21834C99A8DB0E595E3E57F51D9CD659A715EA0D5B1778E018D35065B969E8B228C5C77BA431FE8C2DDA7C5D4609291D",
      9308033ULL,
      "000000000000000000000000000000000000000000000000000000000000000000000000001078E0080000000000000000000000000000000000000000000000" },   // Intel, FD, beyond byte 8
};
// clang-format on

// Mirrors the Motorola start-bit conversion in DbcParser::parseSignal
// (src/db/dbc/DbcParser.cpp). BusMessage receives the converted value, so
// the tests must apply the same conversion to stay faithful to the real path.
// Keep in sync if the parser convention ever changes.
[[nodiscard]] uint16_t toInternalStartBit(uint16_t dbcStartBit, bool isBigEndian) noexcept
{
    if (!isBigEndian) { return dbcStartBit; }

    const uint16_t row = dbcStartBit >> 3;
    const uint16_t column = dbcStartBit & 0b111;
    return static_cast<uint16_t>((row * 8) + (7 - column));
}

[[nodiscard]] QByteArray toBytes(const char *hex)
{
    return QByteArray::fromHex(QByteArray(hex));
}

[[nodiscard]] BusMessage makeMessage(const SignalCase &c, const QByteArray &data)
{
    BusMessage msg;
    msg.setLength(c.dlc);
    for (int i = 0; i < data.size(); i++)
    {
        msg.setByte(static_cast<uint8_t>(i), static_cast<uint8_t>(data.at(i)));
    }
    return msg;
}

} // namespace

class BusMessageSignalTest : public QObject
{
    Q_OBJECT

private slots:
    void extractMatchesReference_data();
    void extractMatchesReference();

    void injectMatchesReference_data();
    void injectMatchesReference();

    void injectThenExtractRoundTrips_data();
    void injectThenExtractRoundTrips();

    void injectLeavesOtherBitsUntouched_data();
    void injectLeavesOtherBitsUntouched();

    void rejectsInvalidLength();
    void injectClipsAtDlc();
};

void BusMessageSignalTest::extractMatchesReference_data()
{
    QTest::addColumn<int>("caseIndex");
    for (size_t i = 0; i < std::size(k_cases); i++)
    {
        QTest::newRow(k_cases[i].name) << static_cast<int>(i);
    }
}

// Decoding a known frame must produce the value cantools decodes.
void BusMessageSignalTest::extractMatchesReference()
{
    QFETCH(int, caseIndex);
    const SignalCase &c = k_cases[caseIndex];

    const BusMessage msg = makeMessage(c, toBytes(c.payloadHex));
    const uint64_t raw = msg.extractRawSignal(
        toInternalStartBit(c.dbcStartBit, c.isBigEndian), c.length, c.isBigEndian);

    QCOMPARE(raw, c.expectedRaw);
}

void BusMessageSignalTest::injectMatchesReference_data()
{
    extractMatchesReference_data();
}

// Encoding the same value into a zeroed frame must produce the bytes cantools
// encodes -- this is what pins down bit order independently of extraction.
void BusMessageSignalTest::injectMatchesReference()
{
    QFETCH(int, caseIndex);
    const SignalCase &c = k_cases[caseIndex];

    BusMessage msg = makeMessage(c, QByteArray(c.dlc, '\0'));
    msg.injectRawSignal(toInternalStartBit(c.dbcStartBit, c.isBigEndian),
                        c.length, c.isBigEndian, c.expectedRaw);

    const QByteArray expected = toBytes(c.injectedHex);
    QByteArray actual(c.dlc, '\0');
    for (int i = 0; i < actual.size(); i++)
    {
        actual[i] = static_cast<char>(msg.getByte(static_cast<uint8_t>(i)));
    }

    QCOMPARE(actual.toHex().toUpper(), expected.toHex().toUpper());
}

void BusMessageSignalTest::injectThenExtractRoundTrips_data()
{
    extractMatchesReference_data();
}

// Weaker than the two tests above (it passed even with the issue #34 bug), but
// it still catches asymmetry between the two directions.
void BusMessageSignalTest::injectThenExtractRoundTrips()
{
    QFETCH(int, caseIndex);
    const SignalCase &c = k_cases[caseIndex];
    const uint16_t startBit = toInternalStartBit(c.dbcStartBit, c.isBigEndian);

    BusMessage msg = makeMessage(c, toBytes(c.payloadHex));
    msg.injectRawSignal(startBit, c.length, c.isBigEndian, c.expectedRaw);

    QCOMPARE(msg.extractRawSignal(startBit, c.length, c.isBigEndian), c.expectedRaw);
}

void BusMessageSignalTest::injectLeavesOtherBitsUntouched_data()
{
    extractMatchesReference_data();
}

// Injecting must be a surgical bit operation: writing a signal must not disturb
// bits outside it, whatever the neighbouring payload looks like.
void BusMessageSignalTest::injectLeavesOtherBitsUntouched()
{
    QFETCH(int, caseIndex);
    const SignalCase &c = k_cases[caseIndex];
    const uint16_t startBit = toInternalStartBit(c.dbcStartBit, c.isBigEndian);

    // Write the same value twice: once over the original payload and once over
    // its bitwise complement. Bits belonging to the signal must agree, so the
    // two results may only differ outside the signal -- and each result must
    // still preserve its own surroundings.
    const QByteArray payload = toBytes(c.payloadHex);
    QByteArray inverted = payload;
    for (int i = 0; i < inverted.size(); i++)
    {
        inverted[i] = static_cast<char>(~static_cast<uint8_t>(payload.at(i)));
    }

    for (const QByteArray &background : { payload, inverted })
    {
        BusMessage msg = makeMessage(c, background);
        msg.injectRawSignal(startBit, c.length, c.isBigEndian, c.expectedRaw);

        // Every bit outside the signal must be unchanged. Locate the signal's
        // bits by injecting into two frames that differ only in fill value.
        BusMessage allZero = makeMessage(c, QByteArray(c.dlc, '\0'));
        BusMessage allOnes = makeMessage(c, QByteArray(c.dlc, '\xFF'));
        allZero.injectRawSignal(startBit, c.length, c.isBigEndian, c.expectedRaw);
        allOnes.injectRawSignal(startBit, c.length, c.isBigEndian, c.expectedRaw);

        for (uint8_t i = 0; i < c.dlc; i++)
        {
            // A bit is part of the signal iff it ended up identical no matter
            // what it started as.
            const uint8_t signalBits = static_cast<uint8_t>(~(allZero.getByte(i) ^ allOnes.getByte(i)));
            const uint8_t outside = static_cast<uint8_t>(~signalBits);
            const uint8_t before = static_cast<uint8_t>(background.at(i)) & outside;
            const uint8_t after = msg.getByte(i) & outside;

            QCOMPARE(after, before);
        }
    }
}

// A malformed DBC must not push the bit loops out of range.
void BusMessageSignalTest::rejectsInvalidLength()
{
    BusMessage msg;
    msg.setLength(8);
    for (uint8_t i = 0; i < 8; i++) { msg.setByte(i, 0xA5); }

    for (const uint16_t length : { uint16_t(0), uint16_t(65), uint16_t(1000) })
    {
        QCOMPARE(msg.extractRawSignal(0, length, true), 0ULL);
        QCOMPARE(msg.extractRawSignal(0, length, false), 0ULL);

        BusMessage copy = msg;
        copy.injectRawSignal(0, length, true, 0xFFFFFFFFFFFFFFFFULL);
        copy.injectRawSignal(0, length, false, 0xFFFFFFFFFFFFFFFFULL);
        for (uint8_t i = 0; i < 8; i++)
        {
            QCOMPARE(copy.getByte(i), uint8_t(0xA5));
        }
    }

    // Out-of-range start bits are rejected too.
    QCOMPARE(msg.extractRawSignal(512, 8, true), 0ULL);
    QCOMPARE(msg.extractRawSignal(512, 8, false), 0ULL);
}

// injectRawSignal must not write outside the frame's declared length.
void BusMessageSignalTest::injectClipsAtDlc()
{
    BusMessage msg;
    msg.setLength(2);
    msg.setByte(0, 0x00);
    msg.setByte(1, 0x00);
    msg.setByte(2, 0x11);   // beyond the DLC

    // A 32-bit big-endian signal starting at DBC bit 7 spans bytes 0..3.
    msg.injectRawSignal(toInternalStartBit(7, true), 32, true, 0xFFFFFFFFULL);

    QCOMPARE(msg.getByte(0), uint8_t(0xFF));
    QCOMPARE(msg.getByte(1), uint8_t(0xFF));
    QCOMPARE(msg.getByte(2), uint8_t(0x11));   // untouched
}

QTEST_APPLESS_MAIN(BusMessageSignalTest)

#include "BusMessageSignalTest.moc"
