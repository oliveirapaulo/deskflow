/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "platform/PhysicalEdgeGeometry.h"

#include <QtTest>

using namespace deskflow::physical_edge;

class PhysicalEdgeGeometryTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void oneMonitor()
  {
    Geometry g;
    QVERIFY(g.reset({{0, 0, 100, 80}}, {0, 0, 100, 80}));
    QCOMPARE(g.segments().size(), size_t(4));
    QCOMPARE(g.segments()[0].side, Side::Left);
    QCOMPARE(g.segments()[1].side, Side::Right);
    QCOMPARE(*g.fraction(2, 100, 40), 40.5 / 80.0);
    QVERIFY(!g.fraction(0, 100, 40));
    QVERIFY(!g.fraction(2, 100, 90));
  }

  void internalSeamAndOffset()
  {
    Geometry g;
    QVERIFY(g.reset({{0, 0, 100, 100}, {100, 25, 100, 50}}, {0, 0, 200, 100}));
    // Left monitor's right side is exposed only above and below the seam.
    int exposed = 0;
    for (const auto &s : g.segments()) {
      if (s.zoneIndex == 0 && s.side == Side::Right) {
        ++exposed;
        QVERIFY((s.begin == 0 && s.end == 25) || (s.begin == 75 && s.end == 100));
      }
      if (s.zoneIndex == 1 && s.side == Side::Left)
        QFAIL("internal seam was armed");
    }
    QCOMPARE(exposed, 2);
  }

  void coordinateTranslationAndMismatch()
  {
    Geometry g;
    QVERIFY(g.reset({{-200, -50, 100, 100}, {-100, -25, 100, 50}}, {30, 70, 200, 100}));
    const auto *left = g.find(1);
    QVERIFY(left);
    QCOMPARE(left->side, Side::Left);
    QCOMPARE(*g.fraction(left->barrierId, -200, 0), 50.5 / 100.0);
    QVERIFY(!g.reset({{-200, -50, 100, 100}, {-100, -25, 100, 50}}, {30, 70, 220, 100}));
    QVERIFY(!g.valid());
    QVERIFY(!g.fraction(1, -200, 0));
  }

  void generationReplacement()
  {
    Geometry g;
    QVERIFY(g.reset({{0, 0, 100, 100}}, {0, 0, 100, 100}));
    QVERIFY(g.find(4));
    QVERIFY(g.reset({{0, 0, 100, 100}, {100, 0, 100, 100}}, {0, 0, 200, 100}, 10));
    QVERIFY(!g.find(4));
    QVERIFY(g.find(10));
    QCOMPARE(g.segments().size(), size_t(6));
    QVERIFY(!g.find(999));
  }

  void perOutputCorrespondence()
  {
    Geometry g;
    const std::vector<Rect> portal{{-200, -40, 100, 100}, {-100, -15, 100, 50}};
    const std::vector<Rect> matchingEi{{30, 95, 100, 50}, {-70, 70, 100, 100}};
    QVERIFY(g.reset(portal, {-70, 70, 200, 100}, 20, matchingEi));
    QVERIFY(g.find(20));
    // Identical bounding dimensions are insufficient when monitor regions
    // differ: the crossing coordinate could map to the wrong screen segment.
    const std::vector<Rect> mismatchedEi{{-70, 70, 100, 50}, {30, 70, 100, 100}};
    QVERIFY(!g.reset(portal, {-70, 70, 200, 100}, 30, mismatchedEi));
    QVERIFY(!g.valid());
  }

  void distantOriginsDoNotOverflowTranslation()
  {
    Geometry g;
    QVERIFY(g.reset({{-2000000000, -2000000000, 100, 100}},
                    {2000000000, 2000000000, 100, 100}, 1,
                    {{2000000000, 2000000000, 100, 100}}));
    QCOMPARE(*g.fraction(2, -1999999900, -1999999950), 50.5 / 100.0);
  }
};

QTEST_GUILESS_MAIN(PhysicalEdgeGeometryTests)
#include "PhysicalEdgeGeometryTests.moc"
