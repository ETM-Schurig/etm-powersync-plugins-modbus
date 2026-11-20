// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026, ETM-Schurig SARL

#include <QtTest>

#include "netenergyintegrator.h"

// 1 Wh, in kWh: tolerance of the drift checks.
static const double k_oneWh = 0.001;

class TestNetEnergyIntegrator : public QObject
{
    Q_OBJECT

private slots:
    void notSeededCountsNothing();
    void seedBreaksTheChain();

    // Observed case: phase A exports its PV while
    // B and C draw, total power positive. Register 30075 climbed; the net export
    // index must not move.
    void lowPvHouseDrawing_exportIndexDoesNotMove();
    void netExport_exportIndexMoves();

    void signChange_splitAtZeroCrossing();
    void difference_isTheSignedTrapezoid();

    void gapBeyondCap_notIntegrated();
    void gapAtCap_integrated();
    void nonMonotonicTimestamp_notIntegrated();
    void breakChain_skipsOneInterval();

    void clear_returnsToNotSeeded();
    void unsavedFlag();
};

void TestNetEnergyIntegrator::notSeededCountsNothing()
{
    NetEnergyIntegrator integrator;
    QCOMPARE(integrator.addPowerSample(1000, 0), NetEnergyIntegrator::SampleNotSeeded);
    QCOMPARE(integrator.addPowerSample(1000, 2000), NetEnergyIntegrator::SampleNotSeeded);
    QCOMPARE(integrator.importKWh(), 0.0);
    QVERIFY(!integrator.hasUnsavedChanges());
}

void TestNetEnergyIntegrator::seedBreaksTheChain()
{
    NetEnergyIntegrator integrator;
    integrator.seed(100, 3111.2271);
    QCOMPARE(integrator.addPowerSample(3600, 0), NetEnergyIntegrator::SampleChainStarted);
    integrator.seed(200, 3111.2271);
    // The interval straddling the seed is not counted.
    QCOMPARE(integrator.addPowerSample(3600, 2000), NetEnergyIntegrator::SampleChainStarted);
    QCOMPARE(integrator.importKWh(), 200.0);
    QCOMPARE(integrator.addPowerSample(3600, 3000), NetEnergyIntegrator::SampleIntegrated);
    QCOMPARE(integrator.importKWh(), 200.0 + 0.001);
}

void TestNetEnergyIntegrator::lowPvHouseDrawing_exportIndexDoesNotMove()
{
    NetEnergyIntegrator integrator;
    integrator.seed(1000, 3111.2271);

    // 30 minutes at 2 s, net acquisition between 300 and 1900 W.
    double expectedImport = 0;
    double previous = 0;
    for (int i = 0; i <= 900; ++i) {
        const double powerW = 300 + (i * 37) % 1600;
        integrator.addPowerSample(powerW, i * 2000LL);
        if (i > 0)
            expectedImport += (previous + powerW) / 2.0 * 2.0 / 3600.0 / 1000.0;
        previous = powerW;
    }

    QCOMPARE(integrator.exportKWh(), 3111.2271);
    QVERIFY(qAbs(integrator.importKWh() - 1000 - expectedImport) < 1e-9);
    QVERIFY(integrator.importKWh() - 1000 > 0.1);
}

void TestNetEnergyIntegrator::netExport_exportIndexMoves()
{
    NetEnergyIntegrator integrator;
    integrator.seed(1000, 3000);
    integrator.addPowerSample(-1800, 0);
    for (int i = 1; i <= 1000; ++i)
        integrator.addPowerSample(-1800, i * 2000LL);

    // 1800 W for 2000 s = 1 kWh exported, nothing imported.
    QVERIFY(qAbs(integrator.exportKWh() - 3001) < 1e-9);
    QCOMPARE(integrator.importKWh(), 1000.0);
}

void TestNetEnergyIntegrator::signChange_splitAtZeroCrossing()
{
    double importKWh = -1;
    double exportKWh = -1;

    // +3600 W -> -3600 W over 2 s: crossing at 1 s, two triangles of 1800 W.s.
    NetEnergyIntegrator::splitInterval(3600, -3600, 2000, &importKWh, &exportKWh);
    QVERIFY(qAbs(importKWh - 0.0005) < 1e-12);
    QVERIFY(qAbs(exportKWh - 0.0005) < 1e-12);

    // -1000 W -> +3000 W over 4 s: crossing at 1 s.
    NetEnergyIntegrator::splitInterval(-1000, 3000, 4000, &importKWh, &exportKWh);
    QVERIFY(qAbs(exportKWh - 1000.0 * 1.0 / 2.0 / 3600.0 / 1000.0) < 1e-12);
    QVERIFY(qAbs(importKWh - 3000.0 * 3.0 / 2.0 / 3600.0 / 1000.0) < 1e-12);

    // Zero at one end: everything on one side.
    NetEnergyIntegrator::splitInterval(0, -720, 10000, &importKWh, &exportKWh);
    QCOMPARE(importKWh, 0.0);
    QVERIFY(qAbs(exportKWh - 0.001) < 1e-12);
}

void TestNetEnergyIntegrator::difference_isTheSignedTrapezoid()
{
    // Drift check of the acceptance test, on the logic side: over a day with
    // frequent sign changes, import - export equals the integral of the signed
    // power, i.e. what 30073 - 30075 (≡ 30397) counts.
    NetEnergyIntegrator integrator;
    integrator.seed(0, 0);

    double signedTrapezoidKWh = 0;
    double previous = 0;
    quint32 state = 12345;
    for (int i = 0; i <= 43200; ++i) {
        state = state * 1103515245u + 12345u;
        const double powerW = static_cast<double>(state % 6001) - 3000.0;
        integrator.addPowerSample(powerW, i * 2000LL);
        if (i > 0)
            signedTrapezoidKWh += (previous + powerW) / 2.0 * 2.0 / 3600.0 / 1000.0;
        previous = powerW;
    }

    QVERIFY(integrator.importKWh() > 1);
    QVERIFY(integrator.exportKWh() > 1);
    QVERIFY(qAbs(integrator.importKWh() - integrator.exportKWh() - signedTrapezoidKWh) < k_oneWh);
}

void TestNetEnergyIntegrator::gapBeyondCap_notIntegrated()
{
    NetEnergyIntegrator integrator;
    integrator.seed(0, 0);
    integrator.addPowerSample(3600, 0);
    QCOMPARE(integrator.addPowerSample(3600, NetEnergyIntegrator::k_maxGapMs + 1),
             NetEnergyIntegrator::SampleGapSkipped);
    QCOMPARE(integrator.importKWh(), 0.0);

    // The chain resumes from the sample after the hole.
    QCOMPARE(integrator.addPowerSample(3600, NetEnergyIntegrator::k_maxGapMs + 1 + 1000),
             NetEnergyIntegrator::SampleIntegrated);
    QVERIFY(qAbs(integrator.importKWh() - 0.001) < 1e-12);
}

void TestNetEnergyIntegrator::gapAtCap_integrated()
{
    NetEnergyIntegrator integrator;
    integrator.seed(0, 0);
    integrator.addPowerSample(360, 0);
    QCOMPARE(integrator.addPowerSample(360, NetEnergyIntegrator::k_maxGapMs),
             NetEnergyIntegrator::SampleIntegrated);
    QVERIFY(qAbs(integrator.importKWh() - 0.001) < 1e-12);
}

void TestNetEnergyIntegrator::nonMonotonicTimestamp_notIntegrated()
{
    NetEnergyIntegrator integrator;
    integrator.seed(0, 0);
    integrator.addPowerSample(1000, 5000);
    QCOMPARE(integrator.addPowerSample(1000, 5000), NetEnergyIntegrator::SampleGapSkipped);
    QCOMPARE(integrator.addPowerSample(1000, 4000), NetEnergyIntegrator::SampleGapSkipped);
    QCOMPARE(integrator.importKWh(), 0.0);
}

void TestNetEnergyIntegrator::breakChain_skipsOneInterval()
{
    NetEnergyIntegrator integrator;
    integrator.seed(0, 0);
    integrator.addPowerSample(3600, 0);
    integrator.breakChain();
    QCOMPARE(integrator.addPowerSample(3600, 2000), NetEnergyIntegrator::SampleChainStarted);
    QCOMPARE(integrator.importKWh(), 0.0);
    QCOMPARE(integrator.addPowerSample(3600, 3000), NetEnergyIntegrator::SampleIntegrated);
    QVERIFY(qAbs(integrator.importKWh() - 0.001) < 1e-12);
}

void TestNetEnergyIntegrator::clear_returnsToNotSeeded()
{
    NetEnergyIntegrator integrator;
    integrator.seed(10, 20);
    integrator.clear();
    QVERIFY(!integrator.isSeeded());
    QCOMPARE(integrator.addPowerSample(1000, 0), NetEnergyIntegrator::SampleNotSeeded);
}

void TestNetEnergyIntegrator::unsavedFlag()
{
    NetEnergyIntegrator integrator;
    integrator.seed(10, 20);
    QVERIFY(integrator.hasUnsavedChanges());
    integrator.markSaved();

    // Zero power: nothing moved, nothing to write to flash.
    integrator.addPowerSample(0, 0);
    integrator.addPowerSample(0, 2000);
    QVERIFY(!integrator.hasUnsavedChanges());

    integrator.addPowerSample(500, 4000);
    QVERIFY(integrator.hasUnsavedChanges());
}

QTEST_GUILESS_MAIN(TestNetEnergyIntegrator)

#include "tst_netenergyintegrator.moc"
