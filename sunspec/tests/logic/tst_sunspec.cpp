// SPDX-License-Identifier: GPL-3.0-or-later

/* Tests of the acceptance criteria of sunspec/FRONIUS.md.
 *
 * Each test carries the number of the AC it validates: test_AC_<n>_<what>.
 *
 * Two mechanisms:
 *
 *  - the criteria concerning the plugin *definition* (value ranges,
 *    thingClasses, default values) are verified by reading back
 *    integrationpluginsunspec.json;
 *  - the criteria concerning *behaviour* are verified either on a
 *    register dump passed to the real libnymea-sunspec model, or on the
 *    pure functions of sunspecstoragelogic / froniusrules.
 *
 * What cannot be verified here (Modbus frames actually emitted, expiry
 * of the timer, auto-scale of the SFs during operation) is listed explicitly in
 * sunspec/FRONIUS.md.
 */

#include <QTest>
#include <QObject>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include <models/sunspecstoragemodel.h>

#include "sunspecstoragelogic.h"
#include "froniusrules.h"
#include "sunspecduplicates.h"

using namespace SunSpecStorageLogic;

/* Gives access to the model's internals to inject a register dump into it
 * without a Modbus connection. Reproduces what SunSpecModel::handleNewBlockData()
 * does after a real read: fill the data points from the block, then
 * call processBlockData(). */
class StorageModelFixture : public SunSpecStorageModel
{
public:
    explicit StorageModelFixture(const QVector<quint16> &blockData)
        : SunSpecStorageModel(nullptr, 40000, 24, SunSpecDataPoint::ByteOrderLittleEndian)
    {
        m_blockData = blockData;
        foreach (const QString &name, m_dataPoints.keys()) {
            m_dataPoints[name].setRawData(m_blockData.mid(m_dataPoints[name].addressOffset(),
                                                          m_dataPoints[name].size()));
        }
        processBlockData();
    }
};

/* Dump of model 124, L = 24: ID + L + 24 registers. The offsets are those of the
 * generated model (ID at 0), not absolute addresses: no hard-coded address
 * is introduced here (INV-1). */
struct StorageBlock
{
    quint16 wChaMax = 10000;
    qint16 wChaGra = 300;         // gradient, % WChaMax/s
    qint16 wDisChaGra = 400;      // gradient, % WChaMax/s
    quint16 storCtlMod = 0;       // bitfield: bit 0 charge, bit 1 discharge
    quint16 vaChaMax = 0;
    quint16 minRsvPct = 500;
    quint16 chaState = 5000;      // 50.00 % with SF -2
    quint16 storAval = 0xFFFF;    // not supported -> sentinel
    quint16 inBatV = 0xFFFF;      // not supported -> sentinel
    quint16 chaSt = 4;            // CHARGING
    qint16 outWRte = 1000;        // 10.00 % with SF -2
    qint16 inWRte = -2500;        // -25.00 % with SF -2
    quint16 winTms = 0;
    quint16 rvrtTms = 0;
    quint16 rmpTms = 0;
    quint16 chaGriSet = 0;        // 0 = PV, 1 = GRID
    qint16 wChaMaxSf = 0;
    qint16 wChaDisChaGraSf = -2;
    qint16 vaChaMaxSf = 0;
    qint16 minRsvPctSf = -2;
    qint16 chaStateSf = -2;
    qint16 storAvalSf = 0;
    qint16 inBatVSf = 0;
    qint16 inOutWRteSf = -2;

    QVector<quint16> toRegisters() const
    {
        QVector<quint16> r(26, 0);
        r[0] = 124;                             // ID
        r[1] = 24;                              // L
        r[2] = wChaMax;
        r[3] = static_cast<quint16>(wChaGra);
        r[4] = static_cast<quint16>(wDisChaGra);
        r[5] = storCtlMod;
        r[6] = vaChaMax;
        r[7] = minRsvPct;
        r[8] = chaState;
        r[9] = storAval;
        r[10] = inBatV;
        r[11] = chaSt;
        r[12] = static_cast<quint16>(outWRte);
        r[13] = static_cast<quint16>(inWRte);
        r[14] = winTms;
        r[15] = rvrtTms;
        r[16] = rmpTms;
        r[17] = chaGriSet;
        r[18] = static_cast<quint16>(wChaMaxSf);
        r[19] = static_cast<quint16>(wChaDisChaGraSf);
        r[20] = static_cast<quint16>(vaChaMaxSf);
        r[21] = static_cast<quint16>(minRsvPctSf);
        r[22] = static_cast<quint16>(chaStateSf);
        r[23] = static_cast<quint16>(storAvalSf);
        r[24] = static_cast<quint16>(inBatVSf);
        r[25] = static_cast<quint16>(inOutWRteSf);
        return r;
    }
};

class SunSpecFroniusTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    // Block 0 - upstream fixes
    void test_AC_1_charging_rate_range_allows_forced_charge();
    void test_AC_1_discharging_rate_range_allows_negative();
    void test_AC_2_charging_rate_is_read_from_in_w_rte();
    void test_AC_2_discharging_rate_is_read_from_out_w_rte();
    void test_AC_2_gradients_exposed_under_distinct_names();
    void test_AC_3_enable_discharging_reconciled_from_stor_ctl_mod();
    void test_AC_3_enable_charging_still_reconciled();
    void test_AC_3_enable_discharging_is_named_after_the_limit();
    void test_AC_4_setpoint_plan_writes_revert_time_last();
    void test_AC_4_revert_time_is_written_even_for_a_single_setpoint();
    void test_AC_4_generic_vendor_defaults_revert_time_to_zero();

    // Block 1 - fronius vendor
    void test_AC_5_manufacturer_filter_matches_fronius_case_insensitively();
    void test_AC_5_manufacturer_filter_rejects_other_manufacturers();
    void test_AC_6_fronius_uses_sunspec_default_byte_order();
    void test_AC_7_fronius_connection_declares_expected_parameters();
    void test_AC_7_fronius_connection_does_not_expose_endianness();
    void test_AC_8_update_interval_floor_is_seven_seconds();
    void test_AC_8_update_interval_above_floor_is_untouched();
    void test_AC_8_fronius_polling_defaults();
    void test_AC_9_forbidden_registers_are_refused();
    void test_AC_9_allowed_registers_are_not_refused();
    void test_AC_10_fronius_revert_time_default_is_300_seconds();
    void test_AC_10_limit_without_revert_timer_is_refused_data();
    void test_AC_10_limit_without_revert_timer_is_refused();
    void test_AC_10_release_without_revert_timer_is_accepted_data();
    void test_AC_10_release_without_revert_timer_is_accepted();
    void test_AC_10_armed_revert_timer_refuses_nothing();
    void test_AC_10_plan_without_timer_write_counts_as_disabled();
    void test_AC_3_enable_charging_is_named_after_the_limit();

    // Block 2 - battery control
    void test_AC_11_mode_produces_contract_registers_data();
    void test_AC_11_mode_produces_contract_registers();
    void test_AC_11_write_order_follows_contract();
    void test_AC_11_discharge_block_is_one_plan_data();
    void test_AC_11_discharge_block_is_one_plan();
    void test_AC_11_discharge_block_action_is_declared();
    void test_AC_12_plan_rereads_scale_factor_before_writing();
    void test_AC_13_plan_verifies_written_registers_by_readback();

    // Block 3 - model 160
    void test_AC_14_blocks_are_classified_by_id_string();
    void test_AC_14_st_dis_cha_is_not_confused_with_st_cha();
    void test_AC_14_unknown_id_string_is_not_classified();
    void test_AC_15_pv_power_sums_mppt_blocks_only();
    void test_AC_15_battery_power_is_positive_when_discharging();
    void test_AC_15_battery_power_is_negative_when_charging();

    // D-9 - energystorage interface of sunspecStorage
    void test_D9_storage_declares_energystorage_without_losing_battery();
    void test_D9_storage_exposes_current_power_in_watt();
    void test_D9_storage_exposes_capacity_in_kilowatthour();
    void test_D9_current_power_is_positive_when_charging();
    void test_D9_current_power_is_negative_when_discharging();
    void test_D9_current_power_is_the_opposite_of_battery_power();
    void test_D9_current_power_ignores_the_pv_strings();
    void test_D9_battery_blocks_are_detected_by_id_string();
    void test_D9_an_inverter_without_battery_has_no_battery_block();
    void test_D9_unread_blocks_are_not_mistaken_for_a_missing_battery();
    void test_D9_read_blocks_are_identified();
    void test_D9_capacity_converts_watthours_to_kilowatthours();
    void test_D9_a_nameplate_without_capacity_is_refused();

    void test_AC_16_fronius_meter_keeps_the_device_sign();
    void test_AC_16_generic_meter_keeps_the_historical_negation();
    void test_AC_16_field_case_export_is_reported_negative();

    void test_AC_16_three_phase_meter_export_on_every_phase();

    void test_AC_18_swapped_words_yield_denormals();

    void test_AC_17_no_battery_means_no_storage_child();
    void test_AC_17_an_installed_battery_is_still_detected();
    void test_AC_17_presence_does_not_depend_on_state_of_charge();

    // F3 - multiple unit IDs
    void test_F3_meter_slave_id_zero_disables_the_meter_connection();
    void test_F3_meter_slave_id_out_of_range_is_refused();
    void test_F3_meter_slave_id_equal_to_the_inverter_is_refused();
    void test_F3_meter_slave_id_creates_the_connection();
    void test_F3_meter_slave_id_is_not_marked_advanced();
    void test_F3_generic_vendors_have_no_meter_slave_id();

    // B1 - duplicate guard by serial number
    void test_B1_storage_behind_the_inverter_common_block_is_not_a_duplicate();
    void test_B1_meter_behind_the_inverter_common_block_is_not_a_duplicate();
    void test_B1_same_model_on_other_registers_is_still_a_duplicate();
    void test_B1_phase_variant_of_the_same_meter_is_still_a_duplicate();
    void test_B1_second_storage_with_the_same_serial_is_a_duplicate();
    void test_B1_another_serial_is_never_a_duplicate();
    void test_B1_empty_serial_never_matches();
    void test_B1_unknown_family_falls_back_to_the_serial_alone();
    void test_B1_model_families();

    // B2 - energy totals of sunspecStorage
    void test_B2_storage_exposes_energy_totals_in_kilowatthour();
    void test_B2_charged_and_discharged_energies_are_kept_apart();
    void test_B2_energies_ignore_the_pv_strings();
    void test_B2_scaled_energy_is_passed_through_in_watthours();
    void test_B2_not_accumulated_energy_is_refused();
    void test_B2_register_full_of_ones_is_refused_whatever_the_scale();
    void test_B2_unimplemented_scale_factor_is_refused();
    void test_B2_one_unpublishable_block_withholds_its_role_only();
    void test_B2_no_battery_block_means_no_energy();
    void test_B2_a_reading_without_energy_is_never_a_zero();

private:
    QJsonObject m_pluginJson;

    QJsonObject thingClassByName(const QString &name) const;
    QJsonObject stateByName(const QJsonObject &thingClass, const QString &name) const;
    QJsonObject paramByName(const QJsonObject &thingClass, const QString &name) const;
    static QJsonObject findByName(const QJsonArray &array, const QString &name);
};

void SunSpecFroniusTest::initTestCase()
{
    QFile file(PLUGIN_JSON_PATH);
    QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable(QString("Cannot open %1").arg(PLUGIN_JSON_PATH)));

    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
    QVERIFY2(error.error == QJsonParseError::NoError, qPrintable(error.errorString()));
    m_pluginJson = document.object();
    QVERIFY(!m_pluginJson.isEmpty());
}

QJsonObject SunSpecFroniusTest::findByName(const QJsonArray &array, const QString &name)
{
    foreach (const QJsonValue &value, array) {
        if (value.toObject().value("name").toString() == name)
            return value.toObject();
    }
    return QJsonObject();
}

/* Looks up a thingClass at the root, then in each vendor. */
QJsonObject SunSpecFroniusTest::thingClassByName(const QString &name) const
{
    const QJsonObject direct = findByName(m_pluginJson.value("thingClasses").toArray(), name);
    if (!direct.isEmpty())
        return direct;

    foreach (const QJsonValue &vendor, m_pluginJson.value("vendors").toArray()) {
        const QJsonObject found = findByName(vendor.toObject().value("thingClasses").toArray(), name);
        if (!found.isEmpty())
            return found;
    }
    return QJsonObject();
}

QJsonObject SunSpecFroniusTest::stateByName(const QJsonObject &thingClass, const QString &name) const
{
    return findByName(thingClass.value("stateTypes").toArray(), name);
}

QJsonObject SunSpecFroniusTest::paramByName(const QJsonObject &thingClass, const QString &name) const
{
    return findByName(thingClass.value("paramTypes").toArray(), name);
}

// ---------------------------------------------------------------------------
// AC-1 - setpoint range
// ---------------------------------------------------------------------------

void SunSpecFroniusTest::test_AC_1_charging_rate_range_allows_forced_charge()
{
    const QJsonObject storage = thingClassByName("sunspecStorage");
    QVERIFY(!storage.isEmpty());

    const QJsonObject state = stateByName(storage, "chargingRate");
    QVERIFY(!state.isEmpty());

    // The real range of InWRte is -100.00 .. +100.00. Bounded at 0, a forced
    // charge setpoint is rejected by nymea before reaching the plugin.
    QCOMPARE(state.value("minValue").toDouble(), -100.0);
    QCOMPARE(state.value("maxValue").toDouble(), 100.0);
}

void SunSpecFroniusTest::test_AC_1_discharging_rate_range_allows_negative()
{
    const QJsonObject storage = thingClassByName("sunspecStorage");
    const QJsonObject state = stateByName(storage, "dischargingRate");
    QVERIFY(!state.isEmpty());

    QCOMPARE(state.value("minValue").toDouble(), -100.0);
    QCOMPARE(state.value("maxValue").toDouble(), 100.0);
}

// ---------------------------------------------------------------------------
// AC-2 - read/write consistency
// ---------------------------------------------------------------------------

void SunSpecFroniusTest::test_AC_2_charging_rate_is_read_from_in_w_rte()
{
    StorageBlock block;
    block.inWRte = -2500;    // -25.00 % with SF -2
    block.wChaGra = 300;     // gradient: deliberately different value
    StorageModelFixture model(block.toRegisters());

    const StorageStates states = readStates(&model);

    // The quantity written by the chargingRate action is InWRte. The state read
    // back must be the same quantity, not the WChaGra gradient.
    QCOMPARE(states.chargingRate, -25.0);
    QVERIFY(states.chargingRate != states.chargeGradient);
}

void SunSpecFroniusTest::test_AC_2_discharging_rate_is_read_from_out_w_rte()
{
    StorageBlock block;
    block.outWRte = 1000;      // 10.00 %
    block.wDisChaGra = 400;    // different gradient
    StorageModelFixture model(block.toRegisters());

    const StorageStates states = readStates(&model);

    QCOMPARE(states.dischargingRate, 10.0);
    QVERIFY(states.dischargingRate != states.dischargeGradient);
}

void SunSpecFroniusTest::test_AC_2_gradients_exposed_under_distinct_names()
{
    StorageBlock block;
    block.wChaGra = 300;         // 3.00 % WChaMax/s with SF -2
    block.wDisChaGra = 400;      // 4.00 % WChaMax/s
    StorageModelFixture model(block.toRegisters());

    const StorageStates states = readStates(&model);

    QCOMPARE(states.chargeGradient, 3.0);
    QCOMPARE(states.dischargeGradient, 4.0);

    // The gradients remain exposed, but under names that cannot be confused
    // with the setpoints, and read-only.
    const QJsonObject storage = thingClassByName("sunspecStorage");
    const QJsonObject chargeGradient = stateByName(storage, "chargeGradient");
    const QJsonObject dischargeGradient = stateByName(storage, "dischargeGradient");
    QVERIFY(!chargeGradient.isEmpty());
    QVERIFY(!dischargeGradient.isEmpty());
    QVERIFY(!chargeGradient.value("writable").toBool());
    QVERIFY(!dischargeGradient.value("writable").toBool());
}

// ---------------------------------------------------------------------------
// AC-3 - reconciliation of enableDischarging
// ---------------------------------------------------------------------------

void SunSpecFroniusTest::test_AC_3_enable_discharging_reconciled_from_stor_ctl_mod()
{
    StorageBlock discharging;
    discharging.storCtlMod = 0x2;    // bit 1 = DISCHARGE
    StorageModelFixture dischargingModel(discharging.toRegisters());
    QCOMPARE(readStates(&dischargingModel).enableDischarging, true);

    StorageBlock notDischarging;
    notDischarging.storCtlMod = 0x1; // CHARGE only
    StorageModelFixture notDischargingModel(notDischarging.toRegisters());
    QCOMPARE(readStates(&notDischargingModel).enableDischarging, false);
}

void SunSpecFroniusTest::test_AC_3_enable_charging_still_reconciled()
{
    // Safeguard: the fix of enableDischarging must not break
    // enableCharging, which was already correct upstream.
    StorageBlock block;
    block.storCtlMod = 0x1;
    StorageModelFixture model(block.toRegisters());

    const StorageStates states = readStates(&model);
    QCOMPARE(states.enableCharging, true);
    QCOMPARE(states.enableDischarging, false);
}

/* enableDischarging is the DISCHARGE bit of StorCtl_Mod: true means « discharge
 * limit active », hence discharge clamped to OutWRte. The name and identifier
 * remain, the engine and the app read them; the labels say what the bit does. */
void SunSpecFroniusTest::test_AC_3_enable_discharging_is_named_after_the_limit()
{
    const QJsonObject storage = thingClassByName("sunspecStorage");
    const QJsonObject state = stateByName(storage, "enableDischarging");
    QVERIFY(!state.isEmpty());
    QCOMPARE(state.value("id").toString(), QString("bc99a159-815a-40ab-a6e8-b46f315305f7"));
    QVERIFY(state.value("writable").toBool());
    QCOMPARE(state.value("type").toString(), QString("bool"));

    foreach (const QString &key, QStringList({"displayName", "displayNameEvent", "displayNameAction"})) {
        const QString text = state.value(key).toString();
        QVERIFY2(text.contains("limit", Qt::CaseInsensitive),
                 qPrintable(QString("%1 ne nomme pas la limite : \"%2\"").arg(key, text)));
        QVERIFY2(!text.startsWith("Enable discharging", Qt::CaseInsensitive),
                 qPrintable(QString("%1 dit l'inverse du bit : \"%2\"").arg(key, text)));
    }
}

// ---------------------------------------------------------------------------
// AC-4 - writing InOutWRte_RvrtTms
// ---------------------------------------------------------------------------

void SunSpecFroniusTest::test_AC_4_setpoint_plan_writes_revert_time_last()
{
    const SetpointPlan plan = planSetpoint({{RegisterInWRte, 20.0},
                                            {RegisterStorCtlMod, 3.0}}, 300);

    QVERIFY(!plan.writes.isEmpty());
    QCOMPARE(plan.writes.last().reg, RegisterRvrtTms);
    QCOMPARE(plan.writes.last().value, 300.0);

    // RvrtTms appears only once, and only in the last position.
    int revertCount = 0;
    foreach (const Write &write, plan.writes) {
        if (write.reg == RegisterRvrtTms)
            revertCount++;
    }
    QCOMPARE(revertCount, 1);
}

void SunSpecFroniusTest::test_AC_4_revert_time_is_written_even_for_a_single_setpoint()
{
    // A single setpoint must also arm the timer: this is precisely the
    // case where a value persisting after a HEMS stop immobilises the
    // battery.
    const SetpointPlan plan = planSetpoint({{RegisterOutWRte, 0.0}}, 300);

    QCOMPARE(plan.writes.size(), 2);
    QCOMPARE(plan.writes.at(0).reg, RegisterOutWRte);
    QCOMPARE(plan.writes.at(1).reg, RegisterRvrtTms);
}

void SunSpecFroniusTest::test_AC_4_generic_vendor_defaults_revert_time_to_zero()
{
    // The generic vendor does not change its default behaviour: this is a
    // fix that can be proposed upstream, not a product change.
    const QJsonObject storage = thingClassByName("sunspecStorage");
    const QJsonObject param = paramByName(storage, "revertTime");
    QVERIFY2(!param.isEmpty(), "sunspecStorage doit exposer un parametre revertTime");

    QCOMPARE(param.value("defaultValue").toInt(), 0);
    QCOMPARE(param.value("minValue").toInt(), 0);
    QCOMPARE(param.value("maxValue").toInt(), 28800);
}

// ---------------------------------------------------------------------------
// AC-5 - manufacturer filter
// ---------------------------------------------------------------------------

void SunSpecFroniusTest::test_AC_5_manufacturer_filter_matches_fronius_case_insensitively()
{
    QVERIFY(FroniusRules::matchesManufacturer({"Fronius"}));
    QVERIFY(FroniusRules::matchesManufacturer({"fronius"}));
    QVERIFY(FroniusRules::matchesManufacturer({"FRONIUS International GmbH"}));
    // The manufacturer may appear on only one of the Common models.
    QVERIFY(FroniusRules::matchesManufacturer({"", "Fronius"}));
}

void SunSpecFroniusTest::test_AC_5_manufacturer_filter_rejects_other_manufacturers()
{
    QVERIFY(!FroniusRules::matchesManufacturer({"SolarEdge"}));
    QVERIFY(!FroniusRules::matchesManufacturer({"Kostal"}));
    QVERIFY(!FroniusRules::matchesManufacturer({}));
    QVERIFY(!FroniusRules::matchesManufacturer({""}));
}

// ---------------------------------------------------------------------------
// AC-6 - byte order
// ---------------------------------------------------------------------------

void SunSpecFroniusTest::test_AC_6_fronius_uses_sunspec_default_byte_order()
{
    // MEASURED on a real Symo, read-only: the device emits
    // in big endian, high-order word first, in accordance with the SunSpec
    // specification.
    //
    //   meter 213 unit 200     : Hz = 4248 0000 -> 50.0000
    //   inverter 113 unit 1    : Hz = 4247 fc48 -> 49.9964
    //
    // This test asserted the opposite, justified by « we assume the
    // SunSpec default ». The assumption confused the default of the SPEC with
    // that of libnymea-sunspec, which is its opposite. The test name is
    // kept so as not to break references, but « sunspec_default »
    // now designates the default of the specification, not that of the lib.
    QCOMPARE(FroniusRules::byteOrder(), SunSpecDataPoint::ByteOrderBigEndian);
}

void SunSpecFroniusTest::test_AC_18_swapped_words_yield_denormals()
{
    // Antidote: locks in the REASON for the choice, not only the choice.
    // The registers below are those read on the Symo. Read in the right
    // order they give a grid frequency; read with words swapped, a denormal.
    // If someone switches byteOrder() back to little endian, this absurd
    // reading is what the plugin will produce on real hardware.
    const quint16 hi = 0x4248, lo = 0x0000;      // Hz of meter 213

    auto asFloat = [](quint16 first, quint16 second) {
        const quint32 raw = (quint32(first) << 16) | second;
        float value;
        memcpy(&value, &raw, sizeof(raw));
        return value;
    };

    // Big endian: the high-order register first.
    QVERIFY(qAbs(asFloat(hi, lo) - 50.0f) < 0.01f);

    // Swapped words: a denormal, never a plausible measurement.
    const float swapped = asFloat(lo, hi);
    QVERIFY(swapped > 0.0f);
    QVERIFY(swapped < 1.0e-40f);
}

// ---------------------------------------------------------------------------
// AC-7 - froniusConnection parameters
// ---------------------------------------------------------------------------

void SunSpecFroniusTest::test_AC_7_fronius_connection_declares_expected_parameters()
{
    const QJsonObject connection = thingClassByName("froniusConnection");
    QVERIFY2(!connection.isEmpty(), "la thingClass froniusConnection doit exister");

    foreach (const QString &name, QStringList({"address", "hostName", "macAddress", "port", "slaveId", "meterSlaveId"})) {
        QVERIFY2(!paramByName(connection, name).isEmpty(), qPrintable("parametre manquant : " + name));
    }

    QCOMPARE(paramByName(connection, "port").value("defaultValue").toInt(), 502);

    // Fixed over TCP on GEN24.
    QCOMPARE(paramByName(connection, "slaveId").value("defaultValue").toInt(), 1);

    // 0 = no meter on this connection.
    const QJsonObject meterSlaveId = paramByName(connection, "meterSlaveId");
    QCOMPARE(meterSlaveId.value("defaultValue").toInt(), 200);
    QCOMPARE(meterSlaveId.value("minValue").toInt(), 0);
    QCOMPARE(meterSlaveId.value("maxValue").toInt(), 247);
}

void SunSpecFroniusTest::test_AC_7_fronius_connection_does_not_expose_endianness()
{
    const QJsonObject connection = thingClassByName("froniusConnection");
    QVERIFY(paramByName(connection, "endianness").isEmpty());
}

// ---------------------------------------------------------------------------
// AC-8 - polling rate floor
// ---------------------------------------------------------------------------

void SunSpecFroniusTest::test_AC_8_update_interval_floor_is_seven_seconds()
{
    // Going below the floor can take the customer's inverter offline,
    // with recovery only by an AC+DC power cut on site. This is a
    // safeguard in the code, not a modifiable default value.
    QCOMPARE(FroniusRules::clampUpdateInterval(1), 7);
    QCOMPARE(FroniusRules::clampUpdateInterval(6), 7);
    QCOMPARE(FroniusRules::clampUpdateInterval(0), 7);
    QCOMPARE(FroniusRules::clampUpdateInterval(-5), 7);
}

void SunSpecFroniusTest::test_AC_8_update_interval_above_floor_is_untouched()
{
    QCOMPARE(FroniusRules::clampUpdateInterval(7), 7);
    QCOMPARE(FroniusRules::clampUpdateInterval(10), 10);
    QCOMPARE(FroniusRules::clampUpdateInterval(60), 60);
}

void SunSpecFroniusTest::test_AC_8_fronius_polling_defaults()
{
    const QJsonObject connection = thingClassByName("froniusConnection");
    QVERIFY(!connection.isEmpty());

    QCOMPARE(FroniusRules::DefaultUpdateIntervalSeconds, 10);
    QCOMPARE(FroniusRules::DefaultTimeoutMilliSeconds, 2000);
    QCOMPARE(FroniusRules::DefaultNumberOfRetries, 3);

    const QJsonObject updateInterval = paramByName(connection, "updateInterval");
    if (!updateInterval.isEmpty()) {
        QCOMPARE(updateInterval.value("defaultValue").toInt(), 10);
        QCOMPARE(updateInterval.value("minValue").toInt(), 7);
    }
}

// ---------------------------------------------------------------------------
// AC-9 - write-forbidden registers
// ---------------------------------------------------------------------------

void SunSpecFroniusTest::test_AC_9_forbidden_registers_are_refused()
{
    // These writes are accepted then ignored without a Modbus exception code:
    // the failure is invisible on the HEMS side. So we refuse before sending.
    foreach (const QString &name, QStringList({"WChaMax", "WChaGra", "WDisChaGra",
                                               "InOutWRte_WinTms", "InOutWRte_RmpTms",
                                               "VAChaMax"})) {
        QVERIFY2(FroniusRules::isWriteForbidden(name), qPrintable("doit etre refuse : " + name));
    }
}

void SunSpecFroniusTest::test_AC_9_allowed_registers_are_not_refused()
{
    foreach (const QString &name, QStringList({"StorCtl_Mod", "InWRte", "OutWRte",
                                               "InOutWRte_RvrtTms", "ChaGriSet", "MinRsvPct"})) {
        QVERIFY2(!FroniusRules::isWriteForbidden(name), qPrintable("ne doit pas etre refuse : " + name));
    }
}

// ---------------------------------------------------------------------------
// AC-10 - revertTime at 300 s on fronius
// ---------------------------------------------------------------------------

void SunSpecFroniusTest::test_AC_10_fronius_revert_time_default_is_300_seconds()
{
    // WORKING HYPOTHESIS (H-2 of the spec): a setpoint persisting after a
    // HEMS crash immobilises the customer's battery. So we arm the timer.
    QCOMPARE(FroniusRules::DefaultRevertTimeSeconds, 300);

    // Register range.
    QVERIFY(FroniusRules::DefaultRevertTimeSeconds > 0);
    QVERIFY(FroniusRules::DefaultRevertTimeSeconds <= 28800);
}

/* Without a timer, a limit that was set survives a HEMS stop. A setpoint that
 * puts a limit in place is therefore refused when revertTime is 0. The refusal
 * applies to the plan, that is, to the path common to all actions. */
void SunSpecFroniusTest::test_AC_10_limit_without_revert_timer_is_refused_data()
{
    QTest::addColumn<int>("currentStorCtlMod");
    QTest::addColumn<int>("reg");
    QTest::addColumn<double>("value");

    // StorCtl_Mod: bit 0 CHARGE, bit 1 DISCHARGE.
    QTest::newRow("enableDischarging true") << 0 << int(RegisterStorCtlMod) << 2.0;
    QTest::newRow("enableCharging true") << 0 << int(RegisterStorCtlMod) << 1.0;
    QTest::newRow("enableCharging true, decharge deja bridee") << 2 << int(RegisterStorCtlMod) << 3.0;
    QTest::newRow("dischargingRate 0") << 0 << int(RegisterOutWRte) << 0.0;
    QTest::newRow("chargingRate 50") << 0 << int(RegisterInWRte) << 50.0;
    QTest::newRow("chargingRate negatif (charge forcee)") << 0 << int(RegisterInWRte) << -60.0;
    QTest::newRow("gridCharging true") << 0 << int(RegisterChaGriSet) << double(SunSpecStorageModel::ChagrisetGrid);
}

void SunSpecFroniusTest::test_AC_10_limit_without_revert_timer_is_refused()
{
    QFETCH(int, currentStorCtlMod);
    QFETCH(int, reg);
    QFETCH(double, value);

    const SetpointPlan plan = planSetpoint({{static_cast<Register>(reg), value}}, 0);
    QVERIFY(FroniusRules::isRefusedWithoutRevertTimer(plan, currentStorCtlMod));
}

/* Handing control back to the battery always goes through, timer or not: refusing
 * would leave in place the very limit the caller wants to lift. */
void SunSpecFroniusTest::test_AC_10_release_without_revert_timer_is_accepted_data()
{
    QTest::addColumn<int>("currentStorCtlMod");
    QTest::addColumn<int>("reg");
    QTest::addColumn<double>("value");

    QTest::newRow("enableDischarging false") << 2 << int(RegisterStorCtlMod) << 0.0;
    QTest::newRow("enableCharging false") << 1 << int(RegisterStorCtlMod) << 0.0;
    QTest::newRow("une limite levee sur deux") << 3 << int(RegisterStorCtlMod) << 2.0;
    QTest::newRow("NORMAL deja en place") << 0 << int(RegisterStorCtlMod) << 0.0;
    QTest::newRow("dischargingRate 100") << 2 << int(RegisterOutWRte) << 100.0;
    QTest::newRow("chargingRate 100") << 1 << int(RegisterInWRte) << 100.0;
    QTest::newRow("gridCharging false") << 0 << int(RegisterChaGriSet) << double(SunSpecStorageModel::ChagrisetPv);
}

void SunSpecFroniusTest::test_AC_10_release_without_revert_timer_is_accepted()
{
    QFETCH(int, currentStorCtlMod);
    QFETCH(int, reg);
    QFETCH(double, value);

    const SetpointPlan plan = planSetpoint({{static_cast<Register>(reg), value}}, 0);
    QVERIFY(!FroniusRules::isRefusedWithoutRevertTimer(plan, currentStorCtlMod));

    // The return to NORMAL via the AC-11 table is also a lift.
    const SetpointPlan normal = FroniusRules::planBatteryMode(FroniusRules::BatteryModeNormal, 0, 0, 0);
    QVERIFY(!FroniusRules::isRefusedWithoutRevertTimer(normal, currentStorCtlMod));
}

void SunSpecFroniusTest::test_AC_10_armed_revert_timer_refuses_nothing()
{
    const SetpointPlan block = FroniusRules::planBatteryMode(FroniusRules::BatteryModeNoDischarge, 0, 0, 0);
    QVERIFY(FroniusRules::isRefusedWithoutRevertTimer(block, 0));

    const SetpointPlan armed = FroniusRules::planBatteryMode(FroniusRules::BatteryModeNoDischarge, 0, 0,
                                                             FroniusRules::DefaultRevertTimeSeconds);
    QVERIFY(!FroniusRules::isRefusedWithoutRevertTimer(armed, 0));

    const SetpointPlan oneSecond = planSetpoint({{RegisterStorCtlMod, 2.0}}, 1);
    QVERIFY(!FroniusRules::isRefusedWithoutRevertTimer(oneSecond, 0));
}

/* planSetpoint() always closes the sequence with the timer; a plan built
 * by hand without it must not pass for armed. */
void SunSpecFroniusTest::test_AC_10_plan_without_timer_write_counts_as_disabled()
{
    SetpointPlan plan;
    plan.writes.append({RegisterStorCtlMod, 2.0});
    QVERIFY(FroniusRules::isRefusedWithoutRevertTimer(plan, 0));

    SetpointPlan release;
    release.writes.append({RegisterStorCtlMod, 0.0});
    QVERIFY(!FroniusRules::isRefusedWithoutRevertTimer(release, 2));
}

/* enableCharging is the CHARGE bit of StorCtl_Mod: true means « charge
 * limit active », hence charge clamped to InWRte. The name and identifier remain,
 * the engine and the app read them; the labels say what the bit does. */
void SunSpecFroniusTest::test_AC_3_enable_charging_is_named_after_the_limit()
{
    const QJsonObject storage = thingClassByName("sunspecStorage");
    const QJsonObject state = stateByName(storage, "enableCharging");
    QVERIFY(!state.isEmpty());
    QCOMPARE(state.value("id").toString(), QString("1f530f79-c0d2-466b-90e1-79149e34d92f"));
    QVERIFY(state.value("writable").toBool());
    QCOMPARE(state.value("type").toString(), QString("bool"));

    foreach (const QString &key, QStringList({"displayName", "displayNameEvent", "displayNameAction"})) {
        const QString text = state.value(key).toString();
        QVERIFY2(text.contains("charge limit", Qt::CaseInsensitive),
                 qPrintable(QString("%1 ne nomme pas la limite : \"%2\"").arg(key, text)));
        QVERIFY2(!text.startsWith("Enable charging", Qt::CaseInsensitive) && text != "Charging",
                 qPrintable(QString("%1 dit l'inverse du bit : \"%2\"").arg(key, text)));
    }
}

// ---------------------------------------------------------------------------
// AC-11 - HEMS intent -> registers table
// ---------------------------------------------------------------------------

void SunSpecFroniusTest::test_AC_11_mode_produces_contract_registers_data()
{
    QTest::addColumn<int>("mode");
    QTest::addColumn<double>("chargeLimit");
    QTest::addColumn<double>("dischargeLimit");
    QTest::addColumn<int>("chaGriSet");      // -1 = register not written
    QTest::addColumn<int>("storCtlMod");
    QTest::addColumn<double>("inWRte");      // NaN = register not written
    QTest::addColumn<double>("outWRte");

    const double none = qQNaN();

    QTest::newRow("NORMAL")
        << int(FroniusRules::BatteryModeNormal) << 0.0 << 0.0
        << -1 << 0 << none << 100.0;

    QTest::newRow("NO_DISCHARGE")
        << int(FroniusRules::BatteryModeNoDischarge) << 0.0 << 0.0
        << -1 << 2 << none << 0.0;

    QTest::newRow("NO_CHARGE")
        << int(FroniusRules::BatteryModeNoCharge) << 0.0 << 0.0
        << -1 << 1 << 0.0 << none;

    QTest::newRow("FROZEN")
        << int(FroniusRules::BatteryModeFrozen) << 0.0 << 0.0
        << -1 << 3 << 0.0 << 0.0;

    // Manufacturer's sign convention: negative = charge.
    QTest::newRow("FORCE_CHARGE_60")
        << int(FroniusRules::BatteryModeForceCharge) << 60.0 << 0.0
        << 1 << 2 << none << -60.0;

    QTest::newRow("LIMIT_40_70")
        << int(FroniusRules::BatteryModeLimit) << 40.0 << 70.0
        << -1 << 3 << 40.0 << 70.0;
}

void SunSpecFroniusTest::test_AC_11_mode_produces_contract_registers()
{
    QFETCH(int, mode);
    QFETCH(double, chargeLimit);
    QFETCH(double, dischargeLimit);
    QFETCH(int, chaGriSet);
    QFETCH(int, storCtlMod);
    QFETCH(double, inWRte);
    QFETCH(double, outWRte);

    const SetpointPlan plan = FroniusRules::planBatteryMode(
        static_cast<FroniusRules::BatteryMode>(mode), chargeLimit, dischargeLimit,
        FroniusRules::DefaultRevertTimeSeconds);

    auto valueOf = [&plan](Register reg, bool *found) {
        *found = false;
        double value = 0;
        foreach (const Write &write, plan.writes) {
            if (write.reg == reg) {
                *found = true;
                value = write.value;
            }
        }
        return value;
    };

    bool found = false;
    double value = 0;

    value = valueOf(RegisterChaGriSet, &found);
    if (chaGriSet < 0) {
        QVERIFY2(!found, "ChaGriSet ne doit pas etre ecrit pour ce mode");
    } else {
        QVERIFY(found);
        QCOMPARE(int(value), chaGriSet);
    }

    value = valueOf(RegisterStorCtlMod, &found);
    QVERIFY2(found, "StorCtl_Mod est ecrit pour tous les modes");
    QCOMPARE(int(value), storCtlMod);

    value = valueOf(RegisterInWRte, &found);
    if (qIsNaN(inWRte)) {
        QVERIFY2(!found, "InWRte ne doit pas etre ecrit pour ce mode");
    } else {
        QVERIFY(found);
        QCOMPARE(value, inWRte);
    }

    value = valueOf(RegisterOutWRte, &found);
    if (qIsNaN(outWRte)) {
        QVERIFY2(!found, "OutWRte ne doit pas etre ecrit pour ce mode");
    } else {
        QVERIFY(found);
        QCOMPARE(value, outWRte);
    }

    // RvrtTms is armed for every mode.
    value = valueOf(RegisterRvrtTms, &found);
    QVERIFY(found);
    QCOMPARE(int(value), 300);
}

void SunSpecFroniusTest::test_AC_11_write_order_follows_contract()
{
    // Mandatory order: ChaGriSet -> StorCtl_Mod -> InWRte / OutWRte -> RvrtTms.
    const SetpointPlan plan = FroniusRules::planBatteryMode(
        FroniusRules::BatteryModeLimit, 40.0, 70.0, 300);

    QVector<int> ranks;
    foreach (const Write &write, plan.writes) {
        switch (write.reg) {
        case RegisterChaGriSet: ranks.append(0); break;
        case RegisterStorCtlMod: ranks.append(1); break;
        case RegisterInWRte:
        case RegisterOutWRte: ranks.append(2); break;
        case RegisterRvrtTms: ranks.append(3); break;
        }
    }

    QVERIFY(!ranks.isEmpty());
    for (int i = 1; i < ranks.size(); i++) {
        QVERIFY2(ranks.at(i) >= ranks.at(i - 1), "ordre d'ecriture non respecte");
    }
    QCOMPARE(ranks.last(), 3);

    // The FORCE_CHARGE mode writes ChaGriSet: it must come first.
    const SetpointPlan forceCharge = FroniusRules::planBatteryMode(
        FroniusRules::BatteryModeForceCharge, 60.0, 0.0, 300);
    QCOMPARE(forceCharge.writes.first().reg, RegisterChaGriSet);
}

/* The setDischargeBlocked action: NO_DISCHARGE or NORMAL of the table, in ONE plan.
 * Each row pins down the full sequence, timer included, and not only
 * the values: it is the order that decides what the inverter sees if the
 * sequence stops midway. */
void SunSpecFroniusTest::test_AC_11_discharge_block_is_one_plan_data()
{
    QTest::addColumn<bool>("blocked");
    QTest::addColumn<int>("storCtlMod");
    QTest::addColumn<double>("outWRte");

    QTest::newRow("bloquee = NO_DISCHARGE") << true << 2 << 0.0;
    QTest::newRow("rendue = NORMAL") << false << 0 << 100.0;
}

void SunSpecFroniusTest::test_AC_11_discharge_block_is_one_plan()
{
    QFETCH(bool, blocked);
    QFETCH(int, storCtlMod);
    QFETCH(double, outWRte);

    const SetpointPlan plan = FroniusRules::planDischargeBlock(blocked, 300);

    // StorCtl_Mod first, OutWRte next, the timer last. Nothing else.
    QCOMPARE(plan.writes.count(), 3);
    QCOMPARE(plan.writes.at(0).reg, RegisterStorCtlMod);
    QCOMPARE(int(plan.writes.at(0).value), storCtlMod);
    QCOMPARE(plan.writes.at(1).reg, RegisterOutWRte);
    QCOMPARE(plan.writes.at(1).value, outWRte);
    QCOMPARE(plan.writes.at(2).reg, RegisterRvrtTms);
    QCOMPARE(int(plan.writes.at(2).value), 300);

    // AC-12 and AC-13: SF read back before writing, both setpoints read back after.
    QVERIFY(plan.rereadScaleFactor);
    QCOMPARE(plan.verifyReadback, QVector<Register>({RegisterStorCtlMod, RegisterOutWRte}));

    // The same plan as the row of the AC-11 table, not a variant.
    const SetpointPlan table = FroniusRules::planBatteryMode(
        blocked ? FroniusRules::BatteryModeNoDischarge : FroniusRules::BatteryModeNormal, 0, 0, 300);
    QCOMPARE(plan.writes.count(), table.writes.count());
    for (int i = 0; i < plan.writes.count(); i++) {
        QCOMPARE(plan.writes.at(i).reg, table.writes.at(i).reg);
        QCOMPARE(plan.writes.at(i).value, table.writes.at(i).value);
    }
}

/* What the engine will read: one action, one boolean parameter. */
void SunSpecFroniusTest::test_AC_11_discharge_block_action_is_declared()
{
    const QJsonObject storage = thingClassByName("sunspecStorage");
    const QJsonObject action = findByName(storage.value("actionTypes").toArray(), "setDischargeBlocked");
    QVERIFY2(!action.isEmpty(), "action setDischargeBlocked absente de sunspecStorage");
    QCOMPARE(action.value("id").toString(), QString("8d905115-ea91-445c-a4b8-66e4533e5869"));

    const QJsonArray params = action.value("paramTypes").toArray();
    QCOMPARE(params.count(), 1);
    const QJsonObject param = params.first().toObject();
    QCOMPARE(param.value("name").toString(), QString("dischargeBlocked"));
    QCOMPARE(param.value("id").toString(), QString("cd3871c0-e63d-4b08-a66c-77dd3f045a26"));
    QCOMPARE(param.value("type").toString(), QString("bool"));

    // A state of the same name would make the action a state action, with a
    // different parameter identifier.
    QVERIFY(stateByName(storage, "setDischargeBlocked").isEmpty());
    QVERIFY(stateByName(storage, "dischargeBlocked").isEmpty());
}

// ---------------------------------------------------------------------------
// AC-12 - reading back the scale factor before writing
// ---------------------------------------------------------------------------

void SunSpecFroniusTest::test_AC_12_plan_rereads_scale_factor_before_writing()
{
    // The SFs change with the firmware and during operation (auto-scale). An SF
    // cached during an earlier cycle produces a setpoint off by a factor
    // of 10.
    const SetpointPlan withRate = FroniusRules::planBatteryMode(
        FroniusRules::BatteryModeLimit, 40.0, 70.0, 300);
    QVERIFY2(withRate.rereadScaleFactor,
             "toute ecriture de InWRte/OutWRte doit etre precedee d'une relecture de InOutWRte_SF");

    const SetpointPlan generic = planSetpoint({{RegisterInWRte, 20.0}}, 300);
    QVERIFY(generic.rereadScaleFactor);

    // A plan that writes no scaled quantity does not need to
    // read the SF back.
    const SetpointPlan noRate = planSetpoint({{RegisterStorCtlMod, 3.0}}, 300);
    QVERIFY(!noRate.rereadScaleFactor);
}

// ---------------------------------------------------------------------------
// AC-13 - verification by readback
// ---------------------------------------------------------------------------

void SunSpecFroniusTest::test_AC_13_plan_verifies_written_registers_by_readback()
{
    // The only defence against silent failures: Modbus control not allowed, grid
    // charging not allowed, IP restriction, third-party assistant overwriting.
    const SetpointPlan plan = FroniusRules::planBatteryMode(
        FroniusRules::BatteryModeForceCharge, 60.0, 0.0, 300);

    QVERIFY(plan.verifyReadback.contains(RegisterStorCtlMod));
    QVERIFY(plan.verifyReadback.contains(RegisterOutWRte));
    QVERIFY(plan.verifyReadback.contains(RegisterChaGriSet));

    // Every written register, except RvrtTms, is read back.
    foreach (const Write &write, plan.writes) {
        if (write.reg == RegisterRvrtTms)
            continue;
        QVERIFY2(plan.verifyReadback.contains(write.reg),
                 "tout registre ecrit doit etre relu");
    }
}

// ---------------------------------------------------------------------------
// AC-14 - classification of model 160 blocks by IDStr
// ---------------------------------------------------------------------------

void SunSpecFroniusTest::test_AC_14_blocks_are_classified_by_id_string()
{
    using namespace FroniusRules;

    QCOMPARE(classifyBlock("MPPT 1"), BlockRolePvString);
    QCOMPARE(classifyBlock("MPPT 2"), BlockRolePvString);
    QCOMPARE(classifyBlock("StCha 3"), BlockRoleBatteryCharge);
    QCOMPARE(classifyBlock("StDisCha 4"), BlockRoleBatteryDischarge);

    // Case-insensitive and ignoring leading/trailing spaces.
    QCOMPARE(classifyBlock("  mppt 1  "), BlockRolePvString);
    QCOMPARE(classifyBlock("STDISCHA 4"), BlockRoleBatteryDischarge);

    // On an inverter with 3 or 4 MPPTs, the battery blocks are not in
    // position 3 and 4: the string decides, never the index.
    QCOMPARE(classifyBlock("MPPT 3"), BlockRolePvString);
    QCOMPARE(classifyBlock("MPPT 4"), BlockRolePvString);
    QCOMPARE(classifyBlock("StCha 5"), BlockRoleBatteryCharge);
    QCOMPARE(classifyBlock("StDisCha 6"), BlockRoleBatteryDischarge);
}

void SunSpecFroniusTest::test_AC_14_st_dis_cha_is_not_confused_with_st_cha()
{
    // Trap: an implementation using contains() would classify StDisCha as StCha.
    QCOMPARE(FroniusRules::classifyBlock("StDisCha 4"), FroniusRules::BlockRoleBatteryDischarge);
    QVERIFY(FroniusRules::classifyBlock("StDisCha 4") != FroniusRules::BlockRoleBatteryCharge);
}

void SunSpecFroniusTest::test_AC_14_unknown_id_string_is_not_classified()
{
    QCOMPARE(FroniusRules::classifyBlock(""), FroniusRules::BlockRoleUnknown);
    QCOMPARE(FroniusRules::classifyBlock("Something else"), FroniusRules::BlockRoleUnknown);
}

// ---------------------------------------------------------------------------
// AC-15 - aggregates of model 160
// ---------------------------------------------------------------------------

void SunSpecFroniusTest::test_AC_15_pv_power_sums_mppt_blocks_only()
{
    using namespace FroniusRules;

    const QList<ModuleReading> readings = {
        {"MPPT 1", 1200.0},
        {"MPPT 2", 800.0},
        {"StCha 3", 500.0},        // battery: does not count towards PV
        {"StDisCha 4", 0.0},
        {"Something else", 999.0}  // unclassified: counts nowhere
    };

    QCOMPARE(pvPower(readings), 2000.0);
}

void SunSpecFroniusTest::test_AC_15_battery_power_is_positive_when_discharging()
{
    using namespace FroniusRules;

    // DCW is a uint16, always positive: the sign is carried by the active
    // block, the inactive block being at zero.
    const QList<ModuleReading> readings = {
        {"MPPT 1", 1200.0},
        {"StCha 3", 0.0},
        {"StDisCha 4", 750.0}
    };

    QCOMPARE(batteryPower(readings), 750.0);
}

void SunSpecFroniusTest::test_AC_15_battery_power_is_negative_when_charging()
{
    using namespace FroniusRules;

    const QList<ModuleReading> readings = {
        {"MPPT 1", 1200.0},
        {"StCha 3", 600.0},
        {"StDisCha 4", 0.0}
    };

    QCOMPARE(batteryPower(readings), -600.0);
}

// ---------------------------------------------------------------------------
// F3 - multiple unit IDs
//
// The GEN24 answers for the inverter on one unit ID and for the Smart Meter on
// another, behind the same host:port. The decision whether to open the second
// connection is isolated here to be testable without nymea; the plugin merely
// logs the reason for the refusal.
// ---------------------------------------------------------------------------

void SunSpecFroniusTest::test_F3_meter_slave_id_zero_disables_the_meter_connection()
{
    using namespace FroniusRules;

    // 0 is the documented way of saying "no meter behind this inverter".
    QCOMPARE(decideMeterConnection(0, InverterSlaveId), MeterConnectionDisabled);
}

void SunSpecFroniusTest::test_F3_meter_slave_id_out_of_range_is_refused()
{
    using namespace FroniusRules;

    QCOMPARE(decideMeterConnection(248, InverterSlaveId), MeterConnectionInvalidSlaveId);
    QCOMPARE(decideMeterConnection(-1, InverterSlaveId), MeterConnectionInvalidSlaveId);

    // The upper bound itself remains valid.
    QCOMPARE(decideMeterConnection(MaximumSlaveId, InverterSlaveId), MeterConnectionCreate);
}

void SunSpecFroniusTest::test_F3_meter_slave_id_equal_to_the_inverter_is_refused()
{
    using namespace FroniusRules;

    // A second socket on the unit ID already served by the inverter would rediscover
    // exactly the same models and duplicate every child thing. The case
    // is not theoretical: discovery writes into the thing's slaveId parameter
    // the unit ID it was asked to scan.
    QCOMPARE(decideMeterConnection(1, 1), MeterConnectionCollidesWithInverter);
    QCOMPARE(decideMeterConnection(200, 200), MeterConnectionCollidesWithInverter);
}

void SunSpecFroniusTest::test_F3_meter_slave_id_creates_the_connection()
{
    using namespace FroniusRules;

    QCOMPARE(decideMeterConnection(DefaultMeterSlaveId, InverterSlaveId), MeterConnectionCreate);
    QCOMPARE(DefaultMeterSlaveId, 200);

    // Secondary meters.
    QCOMPARE(decideMeterConnection(201, InverterSlaveId), MeterConnectionCreate);
    QCOMPARE(decideMeterConnection(202, InverterSlaveId), MeterConnectionCreate);
}

/* AC-7 asks for meterSlaveId as "advanced". This property does not exist on the
 * paramTypes of nymea 1.15.2: ParamType::typeProperties() does not know it, and
 * the metadata verifier rejects any unknown field. Rejected metadata
 * is not a badly displayed parameter, it is a plugin that no longer
 * loads at all.
 *
 * This test does not endorse the gap: it prevents it being "fixed" without measuring.
 * See sunspec/DIVERGENCES.md. */
void SunSpecFroniusTest::test_F3_meter_slave_id_is_not_marked_advanced()
{
    const QJsonObject connection = thingClassByName("froniusConnection");
    const QJsonObject meterSlaveId = paramByName(connection, "meterSlaveId");
    QVERIFY(!meterSlaveId.isEmpty());

    QVERIFY2(!meterSlaveId.contains("advanced"),
             "\"advanced\" n'est pas une propriete de paramType connue de nymea 1.15.2 : "
             "l'ajouter rend la metadonnee du plugin invalide et le plugin ne se charge plus. "
             "Voir sunspec/DIVERGENCES.md.");
}

// AC-16 - sign of the grid meter
// ---------------------------------------------------------------------------

/* D-9 - The Thing declares `battery` but not `energystorage`, so the three
 * consumers that filter by `energystorage` do not find it: the power
 * balance (currentPowerStorage, the one of the customer dashboard), the
 * EV charging reserve, and the waterfall reserve. See DIVERGENCES.md D-9. */
void SunSpecFroniusTest::test_D9_storage_declares_energystorage_without_losing_battery()
{
    const QJsonObject storage = thingClassByName("sunspecStorage");
    QVERIFY(!storage.isEmpty());

    QStringList interfaces;
    foreach (const QJsonValue &value, storage.value("interfaces").toArray())
        interfaces.append(value.toString());

    QVERIFY2(interfaces.contains("energystorage"),
             "sans energystorage, filterByInterface ne trouve pas la batterie");

    // `battery` is kept: energystorage extends it, and other things
    // may depend on it.
    QVERIFY(interfaces.contains("battery"));
    QVERIFY(interfaces.contains("connectable"));
}

void SunSpecFroniusTest::test_D9_storage_exposes_current_power_in_watt()
{
    const QJsonObject storage = thingClassByName("sunspecStorage");
    QVERIFY(!storage.isEmpty());

    const QJsonObject state = stateByName(storage, "currentPower");
    QVERIFY2(!state.isEmpty(), "energystorage exige currentPower");

    // Type and unit are checked by PluginMetadata against the interface: if they
    // diverge, the plugin no longer loads at all.
    QCOMPARE(state.value("type").toString(), QString("double"));
    QCOMPARE(state.value("unit").toString(), QString("Watt"));
}

void SunSpecFroniusTest::test_D9_storage_exposes_capacity_in_kilowatthour()
{
    const QJsonObject storage = thingClassByName("sunspecStorage");
    QVERIFY(!storage.isEmpty());

    const QJsonObject state = stateByName(storage, "capacity");
    QVERIFY2(!state.isEmpty(), "energystorage exige capacity");

    QCOMPARE(state.value("type").toString(), QString("double"));
    QCOMPARE(state.value("unit").toString(), QString("KiloWattHour"));
}

/* nymea convention, as found in libnymea/interfaces/energystorage.json:
 * positive = consumes. A charging battery consumes.
 *
 * Measured on a test installation (unit 1, model 160 @40265), battery setpoint
 * imposed externally and registers read back read-only:
 *   setpoint +2000 W -> StCha 2000.0 W  StDisCha    0.0 W  ChaSt Charging
 *   setpoint -1500 W -> StCha    0.0 W  StDisCha 1500.0 W  ChaSt Discharging
 * The sign is therefore not deduced from the documentation, it is observed. */
void SunSpecFroniusTest::test_D9_current_power_is_positive_when_charging()
{
    using namespace FroniusRules;

    const QList<ModuleReading> readings = {
        {"MPPT 1", 496.5},
        {"MPPT 2", 496.5},
        {"StCha 3", 2000.0},
        {"StDisCha 4", 0.0}
    };

    QCOMPARE(storageCurrentPower(readings), 2000.0);
}

void SunSpecFroniusTest::test_D9_current_power_is_negative_when_discharging()
{
    using namespace FroniusRules;

    const QList<ModuleReading> readings = {
        {"MPPT 1", 621.8},
        {"MPPT 2", 621.8},
        {"StCha 3", 0.0},
        {"StDisCha 4", 1500.0}
    };

    QCOMPARE(storageCurrentPower(readings), -1500.0);
}

/* The two conventions coexist in the plugin - the manufacturer's for
 * setpoints, nymea's for the state. This test pits them against each other: only
 * one of the two could change sign without anything saying so. */
void SunSpecFroniusTest::test_D9_current_power_is_the_opposite_of_battery_power()
{
    using namespace FroniusRules;

    const QList<ModuleReading> charging = {{"StCha 3", 2000.0}, {"StDisCha 4", 0.0}};
    const QList<ModuleReading> discharging = {{"StCha 3", 0.0}, {"StDisCha 4", 1500.0}};

    QCOMPARE(storageCurrentPower(charging), -batteryPower(charging));
    QCOMPARE(storageCurrentPower(discharging), -batteryPower(discharging));
}

void SunSpecFroniusTest::test_D9_current_power_ignores_the_pv_strings()
{
    using namespace FroniusRules;

    // The PV strings were at 746.4 W each during the idle phase of the
    // capture, battery idle. Counting them would give 1492.8 W of charge.
    const QList<ModuleReading> readings = {
        {"MPPT 1", 746.4},
        {"MPPT 2", 746.4},
        {"StCha 3", 0.0},
        {"StDisCha 4", 0.0}
    };

    QCOMPARE(storageCurrentPower(readings), 0.0);
}

void SunSpecFroniusTest::test_D9_battery_blocks_are_detected_by_id_string()
{
    using namespace FroniusRules;

    QVERIFY(hasBatteryBlocks({{"MPPT 1", 1200.0}, {"StCha 3", 0.0}}));
    QVERIFY(hasBatteryBlocks({{"MPPT 1", 1200.0}, {"StDisCha 4", 0.0}}));
}

/* The distinction this test protects: storageCurrentPower() answers 0 both
 * for an idle battery and for an inverter that has none. Without
 * hasBatteryBlocks(), the plugin would publish a confident 0 on a machine that
 * has nothing to measure - exactly the zero that is worse than absence. */
void SunSpecFroniusTest::test_D9_an_inverter_without_battery_has_no_battery_block()
{
    using namespace FroniusRules;

    // A GEN24 without a battery publishes N=2: the two PV strings, nothing else.
    QVERIFY(!hasBatteryBlocks({{"MPPT 1", 1200.0}, {"MPPT 2", 800.0}}));
    QVERIFY(!hasBatteryBlocks({}));
}

/* The distinction this test protects is the one that produced a false positive in
 * an on-site test, at startup: the warning « no StCha/StDisCha block »
 * was emitted on a machine whose two blocks existed and whose
 * currentPower worked from the next cycle on.
 *
 * Cause: libnymea-sunspec creates the repeating blocks from `initFinished`, emitted
 * during the FIRST read, just before `blockUpdated`. On this first
 * signal the blocks exist but have never been filled - all IDStr are
 * empty. hasBatteryBlocks() answers false, which is accurate and beside the point: the
 * question is not « no battery » but « not read yet ».
 *
 * What the false positive cost: the log line is the one the README
 * tells operations to look for first to distinguish a broken battery
 * from a non-measurable one. A line that appears on EVERY healthy
 * startup no longer distinguishes anything. */
void SunSpecFroniusTest::test_D9_unread_blocks_are_not_mistaken_for_a_missing_battery()
{
    using namespace FroniusRules;

    // Blocks created but never filled: the power is at zero and the IDStr empty.
    const QList<ModuleReading> jamaisLus = {
        {"", 0.0}, {"", 0.0}, {"", 0.0}, {"", 0.0}
    };

    QVERIFY(!blocksAreIdentified(jamaisLus));
    // hasBatteryBlocks() answers false too - which is exactly why it is not
    // enough on its own to decide that there is no battery.
    QVERIFY(!hasBatteryBlocks(jamaisLus));

    QVERIFY(!blocksAreIdentified({}));
    QVERIFY(!blocksAreIdentified({{"   ", 0.0}}));
}

void SunSpecFroniusTest::test_D9_read_blocks_are_identified()
{
    using namespace FroniusRules;

    // A GEN24 without a battery: read, identified, and without a battery block. This is THE
    // case where the warning must indeed come out.
    const QList<ModuleReading> sansBatterie = {{"MPPT 1", 1200.0}, {"MPPT 2", 800.0}};
    QVERIFY(blocksAreIdentified(sansBatterie));
    QVERIFY(!hasBatteryBlocks(sansBatterie));

    const QList<ModuleReading> avecBatterie = {
        {"MPPT 1", 1200.0}, {"StCha 3", 0.0}, {"StDisCha 4", 0.0}
    };
    QVERIFY(blocksAreIdentified(avecBatterie));
    QVERIFY(hasBatteryBlocks(avecBatterie));
}

void SunSpecFroniusTest::test_D9_capacity_converts_watthours_to_kilowatthours()
{
    QCOMPARE(SunSpecStorageLogic::nameplateCapacityKwh(10000.0), 10.0);
    QCOMPARE(SunSpecStorageLogic::nameplateCapacityKwh(7500.0), 7.5);
}

/* An unimplemented WHRtg is 0xFFFF, which libnymea-sunspec returns as an invalid
 * data point left at 0 - indistinguishable from a nameplate that really
 * announces zero. Both mean "unknown", and neither must
 * be published as a measurement. */
void SunSpecFroniusTest::test_D9_a_nameplate_without_capacity_is_refused()
{
    QVERIFY(SunSpecStorageLogic::nameplateCapacityKwh(0.0) < 0);
    QVERIFY(SunSpecStorageLogic::nameplateCapacityKwh(-1.0) < 0);
}

void SunSpecFroniusTest::test_AC_16_fronius_meter_keeps_the_device_sign()
{
    // The Fronius Smart Meter already emits in the nymea convention: negative =
    // export. The generic negation must therefore be cancelled for it.
    QCOMPARE(FroniusRules::meterPowerSign(true), 1.0);
}

void SunSpecFroniusTest::test_AC_16_generic_meter_keeps_the_historical_negation()
{
    // Non-regression antidote: the generic path and solarEdge do not move.
    // This fix targets Fronius, and nothing else.
    QCOMPARE(FroniusRules::meterPowerSign(false), -1.0);
}

void SunSpecFroniusTest::test_AC_16_field_case_export_is_reported_negative()
{
    // Field case, Primo GEN24 3.0 single-phase (model 101) with a
    // Smart Meter TS 65A-3 (model 203). The inverter exports on phase
    // A only. The plugin reported phase A = +1003 W (import) and phases B and
    // C negative, i.e. export on two phases with no source at all: the
    // sign was inverted. The arbiter saw "Surplus insuffisant (-842 W)" and
    // left the water heater and heat pump off while the site was exporting.
    const double sign = FroniusRules::meterPowerSign(true);

    const double wattsTotal  = -708.0;   // raw meter register
    const double wattsPhaseA = -1003.1;
    const double wattsPhaseB = 32.8;
    const double wattsPhaseC = 261.6;

    // Export seen as export, no longer as import.
    QVERIFY(sign * wattsTotal < 0.0);
    // The phase carrying the inverter exports; the two others import.
    QVERIFY(sign * wattsPhaseA < 0.0);
    QVERIFY(sign * wattsPhaseB > 0.0);
    QVERIFY(sign * wattsPhaseC > 0.0);
}

void SunSpecFroniusTest::test_AC_16_three_phase_meter_export_on_every_phase()
{
    // Reproduced on the simulator, three-phase Symo profile (inverter model 103,
    // meter model 203). The meter emits -500 / -100 / -100 W, i.e. an
    // export on all three phases. The etm6 plugin reported +500 / +100 /
    // +100, that is import on all three. The three-phase path is
    // the one of the field hardware: it deserves its own lock.
    const double sign = FroniusRules::meterPowerSign(true);

    QCOMPARE(sign * -500.0, -500.0);
    QCOMPARE(sign * -100.0, -100.0);

    // And the total follows the same rule, whatever its sign.
    QCOMPARE(sign * -700.0, -700.0);
    QCOMPARE(sign * 3262.0, 3262.0);
}

// AC-17 - physical presence of the battery
// ---------------------------------------------------------------------------

void SunSpecFroniusTest::test_AC_17_no_battery_means_no_storage_child()
{
    // Phantom case: the hybrid GEN24 publishes model 124 without a wired battery.
    // Observed on a real GEN24 without a battery, where the battery child was
    // created and stayed stuck on batteryCritical.
    QVERIFY(!FroniusRules::hasBattery(0.0, 0.0));
}

void SunSpecFroniusTest::test_AC_17_an_installed_battery_is_still_detected()
{
    // Symmetric antidote: the fix must not make a real battery disappear.
    // An installed battery carries a voltage at rest.
    QVERIFY(FroniusRules::hasBattery(230.4, 0.0));   // voltage only
    QVERIFY(FroniusRules::hasBattery(0.0, 12.5));    // available capacity only
    QVERIFY(FroniusRules::hasBattery(230.4, 12.5));  // both
}

void SunSpecFroniusTest::test_AC_17_presence_does_not_depend_on_state_of_charge()
{
    // The SoC is deliberately left out of the discriminator: 0 % cannot be told
    // apart from a really empty battery. A flat but wired battery keeps its
    // voltage, so it remains detected.
    QVERIFY(FroniusRules::hasBattery(230.4, 0.0));
}

void SunSpecFroniusTest::test_F3_generic_vendors_have_no_meter_slave_id()
{
    // Multiple unit IDs are a Fronius peculiarity. The generic and
    // solarEdge vendors keep exactly one connection per thing.
    QVERIFY(paramByName(thingClassByName("sunspecConnection"), "meterSlaveId").isEmpty());
    QVERIFY(paramByName(thingClassByName("solarEdgeConnection"), "meterSlaveId").isEmpty());
}

/* The original defect: a model 124 behind the Common block of its
 * inverter carries the serial number of the inverter. The guard compared only this
 * number, and therefore refused the storage as a « duplicate » of the inverter as soon
 * as the inverter child existed: battery added after installation, or
 * plugin restarted after the 124 appeared. */
void SunSpecFroniusTest::test_B1_storage_behind_the_inverter_common_block_is_not_a_duplicate()
{
    using namespace SunSpecDuplicates;

    const QList<KnownThing> known = { {"SN-INV-1", ModelFamilyInverter} };
    QVERIFY2(!isDuplicate("SN-INV-1", modelFamily(124), known),
             "un stockage n'est pas un doublon de l'onduleur dont il partage le bloc Common");
}

void SunSpecFroniusTest::test_B1_meter_behind_the_inverter_common_block_is_not_a_duplicate()
{
    using namespace SunSpecDuplicates;

    const QList<KnownThing> known = { {"SN-INV-1", ModelFamilyInverter}, {"SN-INV-1", ModelFamilyStorage} };
    QVERIFY(!isDuplicate("SN-INV-1", modelFamily(203), known));
}

/* The original case of the guard, which must stay closed: the same device announces
 * the same model a second time, at other registers. The pair
 * (model, register) does not recognise it, the serial number does. */
void SunSpecFroniusTest::test_B1_same_model_on_other_registers_is_still_a_duplicate()
{
    using namespace SunSpecDuplicates;

    const QList<KnownThing> known = { {"SN-INV-1", modelFamily(103)} };
    QVERIFY(isDuplicate("SN-INV-1", modelFamily(103), known));
    // Same family under the other encoding (float): still the same inverter.
    QVERIFY(isDuplicate("SN-INV-1", modelFamily(113), known));
}

/* The upstream comment cites a device that announces a single-phase meter at
 * one register, then a three-phase one a few minutes later. The phase
 * variants are therefore deliberately a single family. */
void SunSpecFroniusTest::test_B1_phase_variant_of_the_same_meter_is_still_a_duplicate()
{
    using namespace SunSpecDuplicates;

    const QList<KnownThing> known = { {"SN-MTR-1", modelFamily(201)} };
    QVERIFY(isDuplicate("SN-MTR-1", modelFamily(203), known));
    QVERIFY(isDuplicate("SN-MTR-1", modelFamily(213), known));
}

/* A 124 that reappears at another address (the chain of a hybrid shifts
 * when the number of blocks of the 160 changes) does not open a second storage. */
void SunSpecFroniusTest::test_B1_second_storage_with_the_same_serial_is_a_duplicate()
{
    using namespace SunSpecDuplicates;

    const QList<KnownThing> known = { {"SN-INV-1", ModelFamilyInverter}, {"SN-INV-1", ModelFamilyStorage} };
    QVERIFY(isDuplicate("SN-INV-1", modelFamily(124), known));
}

void SunSpecFroniusTest::test_B1_another_serial_is_never_a_duplicate()
{
    using namespace SunSpecDuplicates;

    const QList<KnownThing> known = { {"SN-INV-1", ModelFamilyStorage} };
    QVERIFY(!isDuplicate("SN-INV-2", modelFamily(124), known));
}

/* Upstream behaviour kept: without a serial number, the guard says nothing. */
void SunSpecFroniusTest::test_B1_empty_serial_never_matches()
{
    using namespace SunSpecDuplicates;

    const QList<KnownThing> known = { {QString(), ModelFamilyStorage} };
    QVERIFY(!isDuplicate(QString(), modelFamily(124), known));
}

/* An unknown family is distinguished from nothing: we fall back on the original
 * rule, the serial number alone. */
void SunSpecFroniusTest::test_B1_unknown_family_falls_back_to_the_serial_alone()
{
    using namespace SunSpecDuplicates;

    QVERIFY(isDuplicate("SN-1", ModelFamilyUnknown, { {"SN-1", ModelFamilyInverter} }));
    QVERIFY(isDuplicate("SN-1", ModelFamilyStorage, { {"SN-1", ModelFamilyUnknown} }));
}

void SunSpecFroniusTest::test_B1_model_families()
{
    using namespace SunSpecDuplicates;

    foreach (quint16 id, QList<quint16>({101, 102, 103, 111, 112, 113}))
        QCOMPARE(modelFamily(id), ModelFamilyInverter);
    foreach (quint16 id, QList<quint16>({201, 202, 203, 204, 211, 212, 213, 214}))
        QCOMPARE(modelFamily(id), ModelFamilyMeter);
    QCOMPARE(modelFamily(124), ModelFamilyStorage);

    // The storage companions create no thing: no family.
    QCOMPARE(modelFamily(1), ModelFamilyUnknown);
    QCOMPARE(modelFamily(120), ModelFamilyUnknown);
    QCOMPARE(modelFamily(160), ModelFamilyUnknown);
}

/* nymead reads these two states by their name (energymanagerimpl.cpp, power
 * balance and per-thing log) and the smartmeterconsumer /
 * smartmeterproducer interfaces define them in kWh. Another name or another unit
 * and nothing would be read. */
void SunSpecFroniusTest::test_B2_storage_exposes_energy_totals_in_kilowatthour()
{
    const QJsonObject storage = thingClassByName("sunspecStorage");
    QVERIFY(!storage.isEmpty());

    foreach (const QString &name, QStringList({"totalEnergyConsumed", "totalEnergyProduced"})) {
        const QJsonObject state = stateByName(storage, name);
        QVERIFY2(!state.isEmpty(), qPrintable(name + " absent du sunspecStorage"));
        QCOMPARE(state.value("type").toString(), QString("double"));
        QCOMPARE(state.value("unit").toString(), QString("KiloWattHour"));
        QCOMPARE(state.value("defaultValue").toDouble(), 0.0);
    }

    /* No additional meter interface: smartmeterproducer would make nymead count
     * battery discharge as PV production. */
    const QJsonArray interfaces = storage.value("interfaces").toArray();
    QVERIFY(!interfaces.contains("smartmeterproducer"));
    QVERIFY(!interfaces.contains("smartmeterconsumer"));
}

/* Charge = what the battery consumes, discharge = what it gives back: the
 * same convention as currentPower. */
void SunSpecFroniusTest::test_B2_charged_and_discharged_energies_are_kept_apart()
{
    using namespace FroniusRules;

    QList<ModuleReading> readings;
    readings.append({"StCha 3", 0.0, 123450.0});
    readings.append({"StDisCha 4", 1500.0, 67890.0});

    QCOMPARE(storageChargedEnergyKwh(readings), 123.45);
    QCOMPARE(storageDischargedEnergyKwh(readings), 67.89);
}

void SunSpecFroniusTest::test_B2_energies_ignore_the_pv_strings()
{
    using namespace FroniusRules;

    QList<ModuleReading> readings;
    readings.append({"MPPT 1", 1200.0, 9000000.0});
    readings.append({"MPPT 2", 800.0, 8000000.0});
    readings.append({"StCha 3", 2000.0, 1000.0});
    readings.append({"StDisCha 4", 0.0, 2000.0});

    QCOMPARE(storageChargedEnergyKwh(readings), 1.0);
    QCOMPARE(storageDischargedEnergyKwh(readings), 2.0);
}

/* The library returns DCWH already multiplied by 10^DCWH_SF. The rule does not
 * rescale it a second time. */
void SunSpecFroniusTest::test_B2_scaled_energy_is_passed_through_in_watthours()
{
    using namespace FroniusRules;

    QCOMPARE(moduleLifetimeEnergyWh(123450, 1, true), 123450.0);
    QCOMPARE(moduleLifetimeEnergyWh(12345, 0, true), 12345.0);
    // 50 MWh, well beyond the precision of a float, still accepted.
    QCOMPARE(moduleLifetimeEnergyWh(50000000, 1, true), 50000000.0);
}

/* 0 is the « not accumulated » value of a SunSpec acc32, the initial value of the
 * library, and what a firmware older than 1.31 publishes on the inactive
 * battery block. Never a reset. */
void SunSpecFroniusTest::test_B2_not_accumulated_energy_is_refused()
{
    using namespace FroniusRules;

    QVERIFY(moduleLifetimeEnergyWh(0, 0, true) < 0);
    QVERIFY(moduleLifetimeEnergyWh(0, 1, true) < 0);
}

/* A register at 0xFFFFFFFF is not an acc32 sentinel, but it is the shape
 * of an unimplemented 32-bit point on most devices. After scaling
 * by the library, it arrives close to 2^32 or saturates. */
void SunSpecFroniusTest::test_B2_register_full_of_ones_is_refused_whatever_the_scale()
{
    using namespace FroniusRules;

    QVERIFY(moduleLifetimeEnergyWh(0xFFFFFFFF, 0, true) < 0);
    // 0xFFFFFFFF * 10^-1, rounded to float then truncated by the library.
    QVERIFY(moduleLifetimeEnergyWh(static_cast<quint32>(static_cast<float>(0xFFFFFFFFu * 0.1)), -1, true) < 0);
    // Positive factor: the value no longer fits in 32 bits and saturates.
    QVERIFY(moduleLifetimeEnergyWh(0xFFFFFFFF, 1, true) < 0);
}

void SunSpecFroniusTest::test_B2_unimplemented_scale_factor_is_refused()
{
    using namespace FroniusRules;

    QVERIFY(moduleLifetimeEnergyWh(123450, 0, false) < 0);
}

/* A partial sum would be a total that jumps on the day the missing block
 * starts publishing. */
void SunSpecFroniusTest::test_B2_one_unpublishable_block_withholds_its_role_only()
{
    using namespace FroniusRules;

    QList<ModuleReading> readings;
    readings.append({"StCha 3", 0.0, -1.0});
    readings.append({"StDisCha 4", 0.0, 2000.0});

    QVERIFY(storageChargedEnergyKwh(readings) < 0);
    QCOMPARE(storageDischargedEnergyKwh(readings), 2.0);

    // Two charge blocks, only one readable: no partial sum.
    readings.append({"StCha 5", 0.0, 3000.0});
    QVERIFY(storageChargedEnergyKwh(readings) < 0);
}

/* Other vendors, generic, inverter without a battery: nothing invented. */
void SunSpecFroniusTest::test_B2_no_battery_block_means_no_energy()
{
    using namespace FroniusRules;

    QList<ModuleReading> readings;
    readings.append({"MPPT 1", 1200.0, 9000000.0});
    readings.append({"MPPT 2", 800.0, 8000000.0});

    QVERIFY(storageChargedEnergyKwh(readings) < 0);
    QVERIFY(storageDischargedEnergyKwh(readings) < 0);
    QVERIFY(storageChargedEnergyKwh({}) < 0);
    QVERIFY(storageDischargedEnergyKwh({}) < 0);
}

void SunSpecFroniusTest::test_B2_a_reading_without_energy_is_never_a_zero()
{
    using namespace FroniusRules;

    const ModuleReading reading = {"StCha 3", 2000.0};
    QVERIFY(reading.lifetimeEnergy < 0);
    QVERIFY(storageChargedEnergyKwh({reading}) < 0);
}

QTEST_MAIN(SunSpecFroniusTest)

#include "tst_sunspec.moc"
