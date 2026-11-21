# Spec — Fronius support in the SunSpec plugin

Numbered acceptance criteria, based on the Fronius GEN24 Modbus
documentation, the nymea 1.15.2 code and the evcc implementation.

Two kinds of criteria, never to be mixed in the same commit:

| Block | Kind | Commits |
|---|---|---|
| **AC-1 … AC-4** | fixes for defects present in upstream nymea 1.15.2 | message in English, no reference to ETM / Fronius / PowerSync — they are meant to be proposed upstream |
| **AC-5 … AC-15** | ETM-specific (`fronius` vendor) | separate commits, free prefix |

Status of the criteria: see `## Status table` at the end of the document.

---

## Invariants that apply to the whole implementation

These rules are not criteria that can be tested one by one; they govern
every AC below.

- **INV-1 — no hard-coded register address.** Every read and every write
  goes through dynamic discovery and then through the model offsets. The number
  of repeating blocks of model 160 shifts the absolute address of model 124, and
  without a battery model 124 is absent: an absolute address is never
  valid for long. Addresses from manufacturer documentation are used to
  validate a dump, never to address.
- **INV-2 — `libnymea-sunspec` is never modified.** Everything lives in
  `sunspec/`. See `sunspec/VENDORED.md`.
- **INV-3 — a written setpoint is read back.** Several inverters accept a
  write and then ignore it without a Modbus exception code.
- **INV-4 — regulation closes on the measured power**, never on the
  setpoint sent.
- **INV-5 — scale factors are not static.** They change with the
  firmware and during operation (auto-scale). Re-read the relevant SF
  immediately before conversion and before writing.

---

## Block 0 — upstream fixes

### AC-1 — range of the charge/discharge setpoints (defect A1)

`chargingRate` and `dischargingRate` are declared `minValue: 0, maxValue: 100`
in `integrationpluginsunspec.json`. The actual range of the `InWRte` /
`OutWRte` points of model 124 is **−100.00 … +100.00** (`int16`, SF −2).

**Criterion.** In `integrationpluginsunspec.json`, the two states
`chargingRate` and `dischargingRate` of the `sunspecStorage` thingClass have
`minValue == -100` and `maxValue == 100`.

**Why it is blocking.** With the lower bound at 0, a forced charge setpoint
(negative value, manufacturer sign convention) is structurally
impossible to express: nymea rejects the action parameter before it reaches the
plugin.

---

### AC-2 — read/write consistency of the setpoints (defect A2)

`onStorageBlockUpdated()` feeds `chargingRate` from `wChaGra()` and
`dischargingRate` from `wDisChaGra()`. These are the **gradients**
(`% WChaMax/s`), read-only on GEN24. The actions, however, write to
`InWRte` / `OutWRte` (`% WChaMax`). The state read back is therefore not the
quantity written: the user writes 50 and reads back something else entirely.

**Criterion.**
1. `chargingRate` is fed from `inWRte()`.
2. `dischargingRate` is fed from `outWRte()`.
3. The gradients remain exposed, but under unambiguous names:
   `chargeGradient` ← `wChaGra()`, `dischargeGradient` ← `wDisChaGra()`,
   read-only.

---

### AC-3 — reconciliation of `enableDischarging` (defect A3)

`onStorageBlockUpdated()` reconciles `enableCharging` from
`storCtlMod().testFlag(Storctl_modCharge)` but does nothing for
`enableDischarging`. The state therefore stays frozen at its last written value,
and silently diverges from the inverter as soon as the latter changes state on its own.

**Criterion.** `enableDischarging` is fed from
`storCtlMod().testFlag(Storctl_modDiScharge)`, symmetrically to
`enableCharging`.

`enableCharging` is the CHARGE bit of `StorCtl_Mod`: `true` means "charge
limit active", charging is then capped at `InWRte`. Its name and
identifier are kept; its labels say what the bit does.

---

### AC-4 — writing `InOutWRte_RvrtTms` (defect A4)

The setter `setInOutWRteRvrtTms()` exists in `libnymea-sunspec` but is
never called. `RvrtTms` is the delay after which the inverter drops the
setpoint and resumes its native behaviour. At 0 — the default — a setpoint stays
active indefinitely, including if the HEMS stops, loses the network or crashes.

**Criterion.**
1. A `revertTime` value is configurable per thing (parameter of the
   `sunspecStorage` thingClass), in seconds.
2. Every setpoint sequence (`InWRte`, `OutWRte`, `StorCtlMod`, `ChaGriSet`)
   ends with a write of `InOutWRte_RvrtTms` at this value.
3. `RvrtTms` is written **last** in the sequence.
4. Default of the generic `sunspec` vendor: **0** (upstream behaviour unchanged).
   Default of the `fronius` vendor: **300** (see AC-10).

**Range.** 0 – 28800 s.

---

## Block 1 — `fronius` vendor, structure

### AC-5 — manufacturer filter at discovery

**Criterion.** For `froniusConnectionThingClassId`, `discoverThings()` keeps
only the results where at least one Common model carries a manufacturer name
containing `Fronius`, case-insensitively (`hasManufacturer()` already does a
case-insensitive `contains()`). Other results are ignored, without
creating a `ThingDescriptor`.

Conversely, a GEN24 must not be offered twice: the generic
`sunspecConnection` vendor keeps offering it (upstream behaviour
unchanged); it is the user who chooses which thingClass to pair.

---

### AC-6 — byte order

**Criterion.** A `fronius` connection uses `ByteOrderLittleEndian`, that is
the SunSpec default, at discovery as well as at connection.

The `ByteOrderBigEndian` forced by the `solarEdge` vendor is a quirk specific to
SolarEdge and must not be extended.

> **Inference, not a measured fact.** The GEN24 byte order is documented
> nowhere in the manufacturer documentation consulted; the SunSpec default is
> assumed. To be confirmed on a simulator (phase 2) then on hardware (phase 3).
>
> **Superseded by measurement — see D-8 in `sunspec/DIVERGENCES.md`.** The device
> emits its float32 values most significant word first, as the SunSpec
> specification requires. Yet libnymea-sunspec's "little-endian" swaps the two words:
> the `fronius` connection therefore uses `ByteOrderBigEndian`, and `test_AC_6`
> asserts it.

---

### AC-7 — parameters of the `froniusConnection` thingClass

**Criterion.** The `froniusConnection` thingClass exists, under a `vendors`
→ `fronius` block, and carries exactly:

| Parameter | Default | Constraint |
|---|---|---|
| `address` | — | as `sunspecConnection` |
| `hostName` | — | as `sunspecConnection` |
| `macAddress` | — | as `sunspecConnection` |
| `port` | 502 | — |
| `slaveId` | **1** | fixed over TCP on GEN24: hidden or read-only |
| `meterSlaveId` | **200** | range 0 – 247, `advanced`, **0 = no meter on this connection** |

No `endianness` parameter is exposed (see AC-6: the value is imposed).

---

### AC-8 — polling rate floor

Manufacturer support recommends a polling interval greater than
**7 seconds** for third-party applications. Below that, the communication
overload can take the inverter offline with respect to the manufacturer
portal; recovery requires an AC+DC power cut of about one minute
**on site**.

**Criterion.**
1. Defaults of the `fronius` vendor: `updateInterval = 10` s, `timeout = 2000` ms,
   `numberOfRetries = 3`.
2. **Hard safeguard**: any interval value strictly below 7 s
   configured on a `fronius` connection is raised to 7 s, with an explicit
   `qCWarning`. This is not a modifiable default value: it is
   a floor in the code.
3. The floor also applies in debug.
4. The generic `sunspec` vendor and the `solarEdge` vendor are not affected.

---

### AC-9 — registers forbidden for writing

On GEN24, these writes are accepted and then **ignored without a Modbus
exception code** — the failure is invisible on the HEMS side:
`WChaMax`, `WChaGra`, `WDisChaGra`, `InOutWRte_WinTms`, `InOutWRte_RmpTms`,
`VAChaMax` (whose scale factor is moreover not supported).

**Criterion.** On a `fronius` connection, the corresponding setters
(`setWChaMax`, `setWChaGra`, `setWDisChaGra`, `setInOutWRteWinTms`,
`setInOutWRteRmpTms`, `setVAChaMax`) are **never** called. An attempt
is refused before sending, with a `qCWarning` naming the register.

---

### AC-10 — `revertTime` defaults to 300 s on `fronius`

**Criterion.** The `froniusConnection` thingClass imposes `revertTime = 300` s on
the `sunspecStorage` things created under it, where the generic vendor leaves 0
(AC-4.4).

> **Working assumption H-2, not validated.** The design adopts
> `RvrtTms = 300 s` on every setpoint, refreshed on every optimisation
> cycle, rather than the opposite choice (writing nothing) made by evcc.
> The reasoning adopted: what is acceptable for self-hosted software
> is not for a product installed under ETM's responsibility — a
> setpoint persisting after a HEMS crash immobilises the customer's battery.
> **Unresolved prerequisite:** the timer semantics are described in the
> general chapter of the manual and illustrated on model 123, not on 124. They must
> be verified experimentally on model 124 before being frozen. The
> fallback considered in case the verification fails, `RvrtTms = 0` + an
> application watchdog on the HEMS side, is **abandoned**: the refusal described
> below makes it impossible.

**First installation.** The storage created at the same time as its connection
also receives 300 s. The discovery of the first installation completes
during `setupThing()`, before the connection is registered: the criterion is
therefore taken from the connection the discovery comes from, not from already
registered things. A storage already registered at 0 keeps its parameters.

**Timer disabled.** On a Fronius storage whose `revertTime` is 0, a
setpoint that sets a limit fails ("revert timer disabled: …") and nothing is
written. Handing control back to the battery always goes through: `StorCtl_Mod`
with no new bit, rate brought back to 100 %, `ChaGriSet` brought back to PV. The
refusal sits in the common path of the plans, so every control action inherits
it. It rules out the "`RvrtTms = 0` + watchdog" fallback, abandoned for this reason.

---

## Block 2 — battery control chain

### AC-11 — mapping of HEMS intent → registers

Manufacturer sign convention: **negative = charge, positive = discharge**.

**Criterion.** The six modes produce exactly this register sequence:

| Mode | `ChaGriSet` | `StorCtl_Mod` | `InWRte` | `OutWRte` | `RvrtTms` |
|---|---|---|---|---|---|
| `NORMAL` | — | 0 | — | 100 | 300 |
| `NO_DISCHARGE` | — | 2 | — | 0 | 300 |
| `NO_CHARGE` | — | 1 | 0 | — | 300 |
| `FROZEN` | — | 3 | 0 | 0 | 300 |
| `FORCE_CHARGE(p %)` | 1 | 2 | — | **−p** | 300 |
| `LIMIT(c %, d %)` | — | 3 | c | d | 300 |

`—` = register not written for this mode.

**Mandatory write order:** `ChaGriSet` → `StorCtl_Mod` → `InWRte` / `OutWRte`
→ `RvrtTms` last.

`StorCtl_Mod` is a bitfield: bit 0 = CHARGE, bit 1 = DISCHARGE.
`ChaGriSet`: 0 = PV, 1 = GRID.

> The examples in the manufacturer manual show power windows whose
> signs are inconsistent with the stated convention. The **register
> values** of these examples are correct, the annotated windows are
> not: the table above is built on the register values.

**`setDischargeBlocked` action.** Two rows of the table are exposed as
a `sunspecStorage` action, with a boolean parameter `dischargeBlocked`:
`true` writes `NO_DISCHARGE`, `false` writes `NORMAL`, each in a single plan
(`FroniusRules::planDischargeBlock()`). If the `StorCtl_Mod` write fails,
nothing else is written. The `enableDischarging` and `dischargingRate` actions
remain available and unchanged.

`enableDischarging` is the DISCHARGE bit of `StorCtl_Mod`: `true` means
"discharge limit active", discharging is then capped at `OutWRte`. Its
name and identifier are kept; its labels say what the bit does.

---

### AC-12 — re-reading the scale factor before writing

**Criterion.** `InOutWRte_SF` is re-read immediately before every write of
`InWRte` / `OutWRte`. An SF value cached during an earlier polling
cycle is not reused.

Rationale (INV-5): the manufacturer warns that SFs change with the firmware
**and during operation**. An SF auto-scaled between the read and the write
produces a setpoint off by a factor of 10.

---

### AC-13 — verification by read-back and `controlHealthy` state

**Criterion.**
1. After every setpoint sequence, `StorCtl_Mod`, `InWRte`, `OutWRte` and
   `ChaGriSet` are read back and compared with the written values.
2. On divergence: explicit `qCWarning` naming the register, the value
   written and the value read back.
3. A `controlHealthy` state (bool) goes to `false` on divergence, and to
   `true` after a sequence verified as conforming.

This is the only defence against the silent failures of the inverter commissioning
(Modbus control not authorised, grid charging not authorised, IP restriction,
tariff assistant overwriting the setpoints; see Fronius, "GEN24
Offline — Causes and solutions").

---

## Block 3 — model 160 (MPPT and battery blocks)

### AC-14 — identifying blocks by `IDStr`, never by index

The repeating blocks of model 160 carry the strings `MPPT 1`, `MPPT 2`,
`StCha 3`, `StDisCha 4`. On an inverter with 3 or 4 MPPTs, the battery blocks are
**not** in positions 3 and 4.

**Criterion.**
1. Classifying a block as "PV string" or "battery" is done on
   `inputIdSting()`, never on the block index.
2. A block whose `IDStr` starts with `MPPT` is a PV string.
3. A block whose `IDStr` starts with `StCha` is the battery charge block;
   `StDisCha`, the battery discharge block.
4. An unknown `IDStr` is not classified and contributes to no aggregate; it is
   reported with a `qCWarning`.

The prefix is compared case-insensitively and after stripping leading and
trailing whitespace. **Mind the order of the test**: `StDisCha` contains
`StCha` as a substring only if tested with `contains()` — the test must
therefore be on the prefix, and `StDisCha` must be tested before `StCha` if an
implementation using cascaded `startsWith` is chosen.

---

### AC-15 — power aggregates from model 160

`DCW` is a `uint16`, hence always positive: the sign is carried by the active
block, the inactive block being zero.

**Criterion.**
1. PV power = Σ `DCW` of the blocks classified as "PV string".
2. Battery power = `DCW[StDisCha]` − `DCW[StCha]`, **positive = discharge**.
3. `DCW_SF` is shared by all modules and auto-scaled: it is re-read before
   conversion (INV-5).
4. **No computation of PV production by AC/DC ± battery difference.** The GEN24
   exposes the real PV power string by string; replicating the workaround
   of the `solarEdge` vendor — which exists because that inverter does not expose
   model 160 — would be a regression.

---

## Working assumptions — open points

Each of these points is handled according to the stated assumption, for lack of
human confirmation at the time of implementation.

| # | Point | Status | Assumption adopted |
|---|---|---|---|
| H-1 | Multi-unit-ID architecture | ✅ settled, **implemented** | **Option A′**: one `froniusConnection` thing carrying two internal `SunSpecConnection`s (inverter `slaveId`, meter `meterSlaveId`). Correct UX, no modification of `libnymea-sunspec`, rebasability preserved. The second connection lives in a table separate from `m_sunSpecConnections`, which leaves the generic and `solarEdge` vendors unchanged line for line. |
| H-2 | Battery watchdog | ⬜ to be validated | **`RvrtTms = 300 s`** on every setpoint (AC-10). Unresolved prerequisite: timer semantics to be verified on model 124, not only on 123. Fallback `RvrtTms = 0` + watchdog abandoned: refusing a setpoint without a timer rules it out. |
| H-3 | GEN24 byte order | ✅ **measured** on a Symo | Most significant word first, the order of the SunSpec specification: libnymea-sunspec's `ByteOrderBigEndian` (D-8). Not documented by the manufacturer; still to be confirmed on a GEN24. |
| H-4 | Two simultaneous TCP connections to the same inverter | ✅ **measured** on a real GEN24 | Both connections hold simultaneously. Still distinct, and **not measured**, is the load of two simultaneous *discoveries*: the plugin therefore only brings up the meter connection once the primary discovery has finished. Serialising the polling reads remains a separate piece of work. |
| H-5 | Tariff assistant overwriting the setpoints | "to be confirmed", absent from the manufacturer documentation | Third-party empirical finding. Handled as a diagnostic message (AC-13.2), not in control logic. |
| H-6 | Order of the work phases | phase 2 (plugin) does not start before phase 1 (simulator) is green | The ACs are validated by the offline suites, which does not replace validation against a simulator reproducing the manufacturer's semantics. The `tests/integration` suite serves this validation (see `## Status table`). |

---

## Not verifiable by the unit tests of this repository

To be carried into phase 2 (simulator) then phase 3 (hardware):

- the actual GEN24 byte order, measured on a Symo only (H-3);
- the behaviour with a double TCP connection (H-4);
- the effective expiry of `RvrtTms` on model 124 (H-2);
- write refusal via a Modbus exception when control is not authorised
  on the inverter side;
- the effective auto-scaling of the scale factors during operation (INV-5);
- the fact that the AC-9 registers are indeed silently ignored;
- the behaviour of the model 160 battery blocks depending on the firmware version
  (before/after 1.31, `DCV` and `DCWH` reported continuously or zeroed when stopped).

---

## Out of scope

- 7xx models;
- Modbus RTU;
- Verto / Tauro inverters (the MPPT mapping differs — this is the rationale for AC-14,
  but they are not tested);
- any modification of `libnymea-sunspec` (INV-2);
- any SolarEdge-style PV compensation mechanism (AC-15.4);
- defects A5 (model 160 missing from the `switch` in `processDiscoveryResult()`),
  A6 (`MinRsvPct` not exposed) and A7 (`.toInt()` on parameters intended for
  `float` setters): real, found in the nymea 1.15.2 code, but
  outside the four fixes of block 0.

---

## Status table

State taken from the branch code. Five suites under `sunspec/tests/`; the first
four are offline and run by `make check`.

| Suite | Purpose | Test functions |
|---|---|---|
| `logic` | pure rules (`FroniusRules`, `SunSpecStorageLogic`) and plugin metadata | 77 (one of them data-driven, six cases) |
| `plugin` | plugin dispatch for `froniusConnection` (`test_F1_*`) | 4 |
| `multiunit` | two connections per Fronius thing (`test_F3_*`) | 8 |
| `discovery` | storage child created only once (`test_B1_*`), energy totals (`test_B2_*`) | 13 |
| `integration` | plugin against a real or simulated device (`test_F3_*`) | 6 |

`tests/integration` is built but **never run by `make check`**:
it requires `SUNSPEC_TEST_HOST`, and every test goes to `QSKIP` without it. It
covers what no offline suite can: the sweep of orphaned
children across the two connections and the aggregation of the `connected` state.

Outside any numbered criterion, the `test_D9_*` tests lock the D-9 gap of
`sunspec/DIVERGENCES.md`.

Build: `qmake6 NYMEA_SUNSPEC_PATH=<sysroot> sunspec/tests/tests.pro && make check`.
Without `NYMEA_SUNSPEC_PATH`, `sunspec.pri` resolves `libnymea-sunspec` through
`pkg-config`, which assumes `libnymea-sunspec-dev` is installed system-wide.

| AC | Purpose | Test | Status |
|---|---|---|---|
| AC-1 | range −100…+100 | `test_AC_1_*` | green |
| AC-2 | read `inWRte`/`outWRte` | `test_AC_2_*` | green |
| AC-3 | `enableDischarging` read back | `test_AC_3_*` | green |
| AC-4 | `RvrtTms` written last | `test_AC_4_*` | green |
| AC-5 | manufacturer filter | `test_AC_5_*` | green |
| AC-6 | byte order | `test_AC_6_*` | green — asserts `ByteOrderBigEndian`, not the criterion's little-endian (H-3, D-8) |
| AC-7 | `froniusConnection` parameters | `test_AC_7_*` | green; `meterSlaveId` is not `advanced` (D-1) |
| AC-8 | 7 s floor | `test_AC_8_*` | green, with the reservation below |
| AC-9 | forbidden registers | `test_AC_9_*` | green |
| AC-10 | `revertTime` 300 s | `test_AC_10_*` | green — assumption H-2 not validated |
| AC-11 | mode → registers table | `test_AC_11_*` | green at plan level; `NO_DISCHARGE` and `NORMAL` exposed through `setDischargeBlocked`, green on the wire (`discovery` suite); the other four modes have no action |
| AC-12 | SF re-read | `test_AC_12_*` | green |
| AC-13 | read-back + `controlHealthy` | `test_AC_13_*` | green |
| AC-14 | classification by `IDStr` | `test_AC_14_*` | green; wired to the storage child, not to a model 160 thing (reservation below) |
| AC-15 | model 160 aggregates | `test_AC_15_*` | green; battery power published, PV power computed but not published (reservation below) |
| AC-16 | grid meter sign | `test_AC_16_*` | green; **criterion not written in this spec**, see D-5 |
| AC-17 | physical presence of the battery | `test_AC_17_*` | green; **criterion not written in this spec**, see D-6 and D-7 |
| AC-18 | float32 word order | `test_AC_18_*` | green; **criterion not written in this spec**, see D-8 |

### Reservations to be resolved

- **AC-8** — the nymea polling timer is **global to the plugin**, not
  per connection. The floor is therefore applied plugin-wide as soon as a
  `fronius` connection is configured: this is stricter than the AC, never looser.
  The per-thing `updateInterval` parameter has been **removed** from
  `froniusConnection`: the plugin architecture cannot honour it per
  connection, and a setting with no effect would be worse than none at all. See also
  D-3: the plugin configuration overrides the Fronius timing defaults.
- **AC-11** — the function `FroniusRules::planBatteryMode()` produces exactly
  the AC-11 table, and the plan can be executed by
  `IntegrationPluginSunSpec::executeSetpointPlan()`. The
  `setDischargeBlocked` action exposes `NO_DISCHARGE` and `NORMAL` from it. The
  other four modes have no action: their naming is not settled.
- **AC-14 / AC-15** — model 160 is still not in the `switch` of
  `processDiscoveryResult()` (defect A5, out of scope): no MPPT thing
  is created. The `sunspecStorage` child, however, borrows model 160 from its
  own connection, classifies its blocks by `IDStr` and derives `currentPower` from them
  (with the nymea sign, positive = charge, D-9) and its energy totals (D-12).
  `FroniusRules::pvPower()` (AC-15.1) is only called by the tests.
- **AC-15.4** — met by construction: no computation of PV production by
  difference has been added.

### Multi-unit-ID (H-1)

**Implemented.** A `froniusConnection` thing carries two `SunSpecConnection`s:
the inverter on `slaveId`, the meter on `meterSlaveId`. The meter connection
lives in `m_meterConnections`, apart from `m_sunSpecConnections`, which leaves
the generic and `solarEdge` vendors without a second connection. It is only brought up
once the inverter discovery has finished (H-4). `meterSlaveId = 0`
does not open a second connection; a value out of range or equal to the
inverter's unit ID is refused. Removing the thing destroys both connections.

Tests: `test_F3_*` in `logic`, `multiunit` and `integration`.
