/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/ProtocolTypes.h"
#include "deskflow/ProtocolUtil.h"
#include "base/Log.h"
#include "io/IStream.h"

#include <QtTest>
#include <cstring>
#include <vector>

class ByteStream : public deskflow::IStream
{
public:
  void close() override { m_bytes.clear(); m_read = 0; }
  uint32_t read(void *destination, uint32_t count) override
  {
    const auto available = static_cast<uint32_t>(m_bytes.size() - m_read);
    const auto take = std::min(count, available);
    if (destination)
      std::memcpy(destination, m_bytes.data() + m_read, take);
    m_read += take;
    return take;
  }
  void write(const void *source, uint32_t count) override
  {
    const auto *bytes = static_cast<const uint8_t *>(source);
    m_bytes.insert(m_bytes.end(), bytes, bytes + count);
  }
  void flush() override {}
  void shutdownInput() override {}
  void shutdownOutput() override {}
  void *getEventTarget() const override { return const_cast<ByteStream *>(this); }
  bool isReady() const override { return m_read < m_bytes.size(); }
  uint32_t getSize() const override { return static_cast<uint32_t>(m_bytes.size() - m_read); }

private:
  std::vector<uint8_t> m_bytes;
  size_t m_read = 0;
};

class PhysicalEdgeProtocolTests : public QObject
{
  Q_OBJECT
  Log m_log;
private Q_SLOTS:
  void oldPeerVersionRemainsCompatible()
  {
    QCOMPARE(negotiatedProtocolMinor(8), int16_t(8));
    QCOMPARE(negotiatedProtocolMinor(9), int16_t(9));
  }

  void requestRoundTrip()
  {
    ByteStream stream;
    ProtocolUtil::writef(&stream, kMsgDPhysicalEdgeRequest, uint32_t(42), uint32_t(1), uint32_t(750000));
    char code[4];
    QCOMPARE(stream.read(code, 4), uint32_t(4));
    QCOMPARE(std::memcmp(code, "DERQ", 4), 0);
    uint32_t requestId = 0, fraction = 0;
    uint8_t side = 0;
    QVERIFY(ProtocolUtil::readf(&stream, kMsgDPhysicalEdgeRequest + 4, &requestId, &side, &fraction));
    QCOMPARE(requestId, uint32_t(42));
    QCOMPARE(side, uint8_t(1));
    QCOMPARE(fraction, uint32_t(750000));
  }

  void responsesRoundTrip()
  {
    ByteStream approved;
    ProtocolUtil::writef(&approved, kMsgCPhysicalEdgeApproved, uint32_t(77), int32_t(-25), int32_t(2400));
    char code[4];
    QCOMPARE(approved.read(code, 4), uint32_t(4));
    QCOMPARE(std::memcmp(code, "CERA", 4), 0);
    uint32_t id = 0;
    int32_t x = 0, y = 0;
    QVERIFY(ProtocolUtil::readf(&approved, kMsgCPhysicalEdgeApproved + 4, &id, &x, &y));
    QCOMPARE(id, uint32_t(77));
    QCOMPARE(x, -25);
    QCOMPARE(y, 2400);

    ByteStream rejected;
    ProtocolUtil::writef(&rejected, kMsgCPhysicalEdgeRejected, uint32_t(78), uint32_t(4));
    QCOMPARE(rejected.read(code, 4), uint32_t(4));
    QCOMPARE(std::memcmp(code, "CERE", 4), 0);
    uint8_t reason = 0;
    QVERIFY(ProtocolUtil::readf(&rejected, kMsgCPhysicalEdgeRejected + 4, &id, &reason));
    QCOMPARE(id, uint32_t(78));
    QCOMPARE(reason, uint8_t(4));
  }
};

QTEST_GUILESS_MAIN(PhysicalEdgeProtocolTests)
#include "PhysicalEdgeProtocolTests.moc"
