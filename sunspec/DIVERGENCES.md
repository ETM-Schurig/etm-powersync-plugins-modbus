# Divergences — Fronius vendor of the SunSpec plugin

Deliberate gaps between what `specs/fronius-sunspec.md` requires on one hand
and what the code does on the other. Each entry states **why**
and **how to resolve it**.

Two labels:

- **MEASURED** — the gap rests on a reproducible observation made on hardware
  or on the platform code.
- **INFERRED** — the gap rests on reasoning, for lack of data. To be revisited
  as soon as a measurement exists.

---

## D-1 — `meterSlaveId` is not marked `advanced` (MEASURED)

**What AC-7 requires.** The `meterSlaveId` parameter of the `froniusConnection`
thingClass must be `advanced`.

**What the code does.** Nothing: the parameter does not carry this flag.

**Why.** `advanced` is not a paramType property in nymea 1.15.2.
`ParamType::typeProperties()` lists exactly `id`, `name`, `displayName`,
`type`, `defaultValue`, `inputType`, `unit`, `minValue`, `maxValue`, `stepSize`,
`allowedValues`, `readOnly`. The metadata checker rejects any unknown
field:

```
Param type "meterSlaveId" has unknown fields: "advanced"
```

and `PluginMetadata::isValid()` becomes `false`. This is not a badly displayed
parameter: **the plugin no longer loads at all**, and the cascade of follow-up
errors (the paramType is dropped, so `froniusConnection` no longer satisfies
the `networkdevice` interface) makes the diagnosis misleading.

Verified by loading the JSON, with and without the flag, into a
`PluginMetadata` linked against the installed `libnymea` — not merely read in
an upstream source that might diverge from the package.

**How to resolve.** Nothing to do on the plugin side. Either nymea adds the
property, or AC-7 is corrected. A guard test
(`test_F3_meter_slave_id_is_not_marked_advanced`) prevents anyone from adding
the flag without redoing the measurement.

---

## D-2 — grace delay of the orphaned-children sweep (INFERRED)

**Context.** `processDiscoveryResult()` removes child things whose model is no
longer announced. With two connections under the same parent, each one sees
only half of the picture: sweeping on that half would remove the other one's
children on every discovery. The sweep therefore waits until **all** of the
parent's connections have completed a discovery, then checks the children
against the union of their models.

**The gap.** A connection whose discovery never completes would block the
sweep forever. A grace delay unblocks it:
`OrphanSweepGraceSeconds = 600` (10 minutes).

**Why this value, and why it is INFERRED.** No data pins it down. The choice is
asymmetric and deliberately lazy: a sweep that comes too late leaves a stale
thing around, shown as disconnected; a sweep that comes too early **deletes a
user's thing**. Unequal consequences call for a cautious setting. 10 minutes is
far beyond any observed discovery cycle, without being infinite.

**How to resolve.** Measure, on a simulator then on hardware, how long a
SunSpec discovery takes to fail for good on a unit ID that does not respond
(timeout × retries × number of base registers scanned). The grace delay
should be a few times that duration, not an arbitrary ten minutes. The value
is a single `const` in `integrationpluginsunspec.cpp`; changing it affects
nothing else.

---

## D-3 — the plugin configuration overrides the Fronius timing defaults (MEASURED, pre-existing)

**Context.** `createConnection()` enforces the Fronius vendor's `timeout` and
`numberOfRetries` (AC-8) at creation time. But `onPluginConfigurationChanged()`
then re-applies the plugin's global values to **all** connections,
Fronius included.

**The gap.** It predates F3 and already affects the inverter connection on its
own. F3 does not fix it; it extends it symmetrically to the meter connection,
so that both connections of the same thing behave identically. The AC-8 floor
on the polling interval, however, is still applied: it lives in
`effectiveUpdateInterval()`, not here.

**How to resolve.** Outside the scope of F3: it requires deciding whether the
plugin's global settings may override a vendor default, which the spec does
not settle.

---

## D-4 — connection destroyed by `ThingSetupInfo::aborted` and left in the table (MEASURED, pre-existing, upstream)

**Context.** `setupThing()` connects `ThingSetupInfo::aborted` to
`SunSpecConnection::deleteLater()`, whereas `createConnection()` has already
inserted the connection into `m_sunSpecConnections`. An aborted setup therefore
leaves a dangling pointer in the table; the next setup of the same thing takes
the "reconfiguration" branch and calls `deleteLater()` on an object that has
already been destroyed.

**The gap.** F3 does not fix this path for the primary connection: it is shared
by all three vendors, including `solarEdge`, whose behaviour no test covers,
and the scope of F3 is explicitly "generic and solarEdgeConnection do not
change behaviour". The **meter** connection added by F3, on the other hand,
removes itself cleanly from its table when destroyed.

**How to resolve.** Connect `QObject::destroyed` to removal from the table for
both connections, in a change that accepts touching the shared path, with test
coverage for `solarEdge`.

---

## D-5 — the meter sign is corrected on the Fronius side only (MEASURED)

**Context.** `onMeterBlockUpdated()` handled the sign **generically**: all
meter models (201, 202, 203, 204, 211…214) did `-meter->watts()`. There was no
manufacturer-specific mapping.

**The gap.** On Fronius this negation is wrong: the Smart Meter already reports
in the nymea convention (negative = export). The fix drops the negation only
when the meter is a child of a `froniusConnection`; the generic and solarEdge
vendors keep the historical negation, locked by
`test_AC_16_generic_meter_keeps_the_historical_negation`.

**Evidence.** Captured on a Primo GEN24 3.0, a **single-phase** inverter
(model 101), with a Smart Meter TS 65A-3 (model **203**, not 213). The inverter
exports on phase A only; the plugin reported phase A = +1003 W (import) and
phases B and C at −33 W and −262 W, i.e. export on two phases carrying no
source. With the sign inverted, the balance closes: production 1190 W =
phase A load 187 W + export 1003 W; import B+C 294 W; consumption 481 W; grid
export 709 W against 708 W measured. Independent corroboration: over the same
window, the device's *import* energy counter stays at 0 while the *export*
counter increments. Observed consequence: the arbiter refused to switch on the
water heater for « surplus insuffisant (−842 W) » (insufficient surplus) while the site was exporting
~842 W.

**Not touched.** The energy counters (`totalEnergyConsumed` /
`totalEnergyProduced`) are **correct** and are not swapped: they are the ones
used as corroborating evidence above.

**How to resolve.** Determining whether the generic negation is wrong for all
manufacturers or only for Fronius would require an on-site test with a meter
from another vendor. Until that is settled, the generic path does not change.

---

## D-6 — battery presence: the "battery present" side is not validated (INFERRED)

**Context.** A hybrid GEN24 publishes model 124 even with no battery wired.
The plugin therefore always created a storage child, hence a phantom battery
and a permanent `batteryCritical`.

**The gap.** `FroniusRules::hasBattery()` now requires `InBatV > 0` or
`StorAval > 0` to create the child, and only under a `froniusConnection`.
The SoC is ruled out (0 % cannot be told apart from an empty battery), as are
the setpoint registers (`WChaMax`… keep their defaults without a battery).

**What is established.** The "absent" side is confirmed on hardware: a real
GEN24 without a battery exposed model 124 with `ChaSt = Off` and
a SoC of 0, and nymea logged "There is no energy storage available to
consider" while the child remained visible.

**What is not.** Since `InBatV` and `StorAval` are not exposed as nymea states,
their values on that battery-less GEN24 **could not be captured**, and no GEN24
**with** a battery was reachable. The assumption "an installed battery carries
a resting voltage" is therefore **INFERRED on both sides**.

**How to resolve.** Read, read-only, block 124 of a GEN24 fitted with a
battery and check that `InBatV`/`StorAval` are non-zero. **Must be done before
any deployment**: an overly strict discriminator would make a real battery
disappear at a customer's site.

---

## D-7 — battery presence cannot be verified by the logic suite (MEASURED)

**Context.** The first version of the D-6 guard asked
`isFroniusChild(thing->id())`, which resolves the parent via `myThings().findById()`.

**The gap.** During the initial setup, the parent connection is **not yet
registered** in `myThings()`: the lookup returns false, the Fronius guard is
bypassed, and the storage child is created despite `InBatV = StorAval = 0`.
Corresponding nymead trace:
`ThingManager: Invalid parent thing id … Not adding thing to the system.`

The guard now queries directly the connection `thing` the function already
holds (`thing->thingClassId() == froniusConnectionThingClassId`), without any
lookup. Same treatment for the meter sign, which uses the already resolved,
non-null `parentThing`.

**What this teaches.** The `tests/logic` suite cannot catch this defect: it
covers the pure decision (`FroniusRules`), not the plugin wiring, that is
*which predicate is queried and at what point of the lifecycle*. Only a
closed-loop test against a simulator reveals it. Any change to the presence
guard must therefore be replayed against a simulator, not only in unit tests.

**How to resolve.** Extend `tests/plugin`, which actually instantiates the
plugin, with an initial-setup scenario where model 124 is all zeros. Out of
scope here.

---

## D-8 — float32 word order: the library default is not the spec's (MEASURED)

**Context.** `FroniusRules::byteOrder()` returned `ByteOrderLittleEndian`, with
the justification "not documented by the manufacturer, the SunSpec default is
assumed".

**The gap.** The assumption confused two "defaults". A SunSpec float32
spans two registers, **most significant word first**. Yet
`SunSpecDataPoint::convertToUInt32` reads `registers.at(0)` then `.at(1)` in
`ByteOrderBigEndian`, and the **reverse** in `ByteOrderLittleEndian` — which is
the libnymea-sunspec default. Keeping that default means reading every float32
of the Fronius personality with its two words swapped.

**Measurement.** Captured on a real Fronius Symo, read-only
(`sunspec/tools/read-float-order.py`):

| point | raw registers | big endian | swapped words |
|---|---|---|---|
| meter 213 u200 `Hz` | `4248 0000` | **50.0000** | 2.3777e−41 |
| meter 213 u200 `PhVphA` | `436a 8000` | **234.5000** | −2.4184e−41 |
| inverter 113 u1 `Hz` | `4247 fc48` | **49.9964** | −4.1592e+36 |

**What it cost.** Not an offset: an absurdity. Reproduced with a simulator
emitting the hardware's order — a meter **importing 2.7 kW** was reported as
**exporting 107 MW** (root meter load at −1.076 × 10⁸ W), and the arbiter
switched on every load on a 107 MW PV surplus. Such a device presents itself
as `connected = true`, without any Modbus error.

**Order of the fixes.** `meterPowerSign()` (D-5) remains correct, but is only
**observable** once the word order is fixed: correcting the sign of a
denormal produces nothing.

**Not touched.** The solarEdge vendor already forces big endian on its own
path; the generic vendor keeps its user-adjustable `endianness` parameter.
Neither is modified.

**Lock.** `test_AC_6` now asserts `ByteOrderBigEndian`, and
`test_AC_18_swapped_words_yield_denormals` pins down the reasoning: the real
registers read in reverse give a denormal, never a plausible measurement.

---

## D-9 — `sunspecStorage` declares `energystorage` but not all of its carriers can measure (MEASURED)

**The original defect.** The Thing declared
`battery` + `connectable`, so the three consumers that filter with
`filterByInterface("energystorage")` did not find it — the power balance the
client reads, the EV charging reserve, the waterfall reserve. On a test
installation, `currentPowerStorage` was 0 while the battery showed 50 % and
"Holding": it was not at zero, it was invisible.

**What the code does.** `sunspecStorage` now declares
`["battery", "energystorage", "connectable"]` and carries `currentPower` (W) and
`capacity` (kWh), wired to registers.

### Where `currentPower` comes from

**Model 124 carries no power.** It describes what the battery *is*
— SoC, status, setpoints — not what it *does* in watts. `WChaMax` is a
**maximum** power, `StorAval` is in ampere-hours. Verified register by
register on a simulator.

Instantaneous power lives in **model 160**, spread over two distinct blocks
that Fronius names `StCha` (charge) and `StDisCha` (discharge). `DCW` is a
`uint16`: each block is always positive, and the inactive block is zero.
Hence

```
currentPower = DCW[StCha] − DCW[StDisCha]
```

**The sign is measured, not deduced.** Captured on a test installation,
unit 1, model 160 at 40265, battery setpoint imposed externally and
registers read back read-only:

| imposed setpoint | reported battery power | `StCha` | `StDisCha` | `ChaSt` (124) | `StCha − StDisCha` |
|---|---|---|---|---|---|
| +2000 W (charge) | +2000.0 W | **2000.0 W** | 0.0 W | Charging | **+2000.0 W** |
| −1500 W (discharge) | −1500.0 W | 0.0 W | **1500.0 W** | Discharging | **−1500.0 W** |
| 0 W | 0.0 W | 0.0 W | 0.0 W | Holding | 0.0 W |

`libnymea/interfaces/energystorage.json`: "positive is 'consumed'". A
charging battery consumes, so charge = positive. This is **the opposite** of the
manufacturer convention (AC-15: positive = discharge) and of
`FroniusRules::batteryPower()`. The two functions coexist, and `batteryPower()`
is written as the negation of `storageCurrentPower()` so that neither can drift
on its own.

**What the capture showed in addition.** `DCW_SF` went from **−2 to −1** between
the idle reading and the charging reading, without hysteresis (INV-5 of the
spec). A plugin that cached this factor at discovery would read powers off by a
factor of 10. libnymea-sunspec does not cache it: the 90 registers of model 160
are read in a single Modbus transaction, and `processBlockData()` re-reads the
scale factors of the fixed block **before** processing the repeating blocks.
The property therefore holds, but it depends on that order — breaking it
upstream would break this reading without any outward sign.

### Where `capacity` comes from

`WHRtg` of model 120 (nameplate), Wh → kWh.
`SunSpecStorageLogic::nameplateCapacityKwh()` returns a **negative** value when
the nameplate does not carry one, and the plugin then leaves the state alone
rather than publishing a zero that would read as a measurement.

**Not verifiable on a simulator.** The simulator used serves models 120 to
123 as **empty shells**: the 26 registers of model 120 are zero, `WHRtg`
included. `capacity` therefore stays at 0 in tests, and the read path is only
exercised by the unit tests. **How to resolve**: populate `WHRtg` in the
simulator, or capture model 120 of a real GEN24 fitted with a battery.
None of the three consumers mentioned above reads `capacity` — the consequence of a 0 is limited to the display.

### The accepted gap

**`sunspecStorage` is shared between the generic `sunspec` vendor and Fronius**,
and interfaces are declared per thingClass, not per thing. A generic SunSpec
device that publishes model 124 **without** model 160 — the case of a generic
simulator — now carries the `energystorage` interface with a
`currentPower` that will never move from zero. This is exactly the original
defect described above: a zero reads as "neither charging nor discharging" where
the absence read as "I don't know".

**Why accept it anyway.** The three alternatives are worse:

- creating a distinct Fronius thingClass (like `solarEdgeBattery`) contradicts
  the vendor's design choice — the Fronius battery is fully described by
  SunSpec models 124 and 160, with no thingClass of its own — and **destroys
  the existing Thing** of every installation along with its history, since the
  orphan sweep would remove the old one;
- not declaring the interface leaves the three consumers blind, which is
  the original defect;
- refusing to create the storage child when power is not measurable would
  make generic devices that work today disappear.

**What makes the gap audible.** The plugin **warns once per thing** when it
finds no power source, distinguishing the two cases:
no model 160 at all on the connection, or model 160 without a
`StCha`/`StDisCha` block (a GEN24 without a battery publishes `N=2`). The warning
does not replace the measurement, but it removes the silence — which was the
real cost of the original defect.

**How to resolve.** nymea allowing an interface to be declared per thing rather
than per thingClass, or the generic vendor getting its own storage thingClass
at the moment a generic device with a battery actually enters the installed base.

**Locks.** `test_D9_*` in `tests/logic`: the interface declaration, the two
states and their units, the sign in both directions, the strict opposition with
`batteryPower()`, the exclusion of PV strings, and the distinction between
"battery idle" and "no battery block". Unit consistency with the interface
is also checked **at compile time** by `nymea-plugininfocompiler`:
trying `KiloWattHour` on `currentPower` makes the build fail with
"claims to implement interface "energystorage" but state "currentPower" has not
matching unit".

---

## D-10 — the model geometry is frozen at discovery (MEASURED)

**Context.** The plugin binds each thing to a `SunSpecModel` at discovery
time: model ID, start register, length. The storage's companion
models (160, 120) are bound the same way, by
`bindStorageCompanionModels()`.

**The gap.** Nothing rediscovers the chain as long as the **connection** does
not drop. If the device changes the SHAPE of its chain without breaking the TCP
connection, the plugin keeps reading the old geometry.

**Measurement.** A GEN24 simulator was restarted without a battery, then again
with one. Three successive states, Modbus connection never broken:

| moment | chain | what the plugin does |
|---|---|---|
| before | 160 `L=88` (4 blocks) · 124 present | `currentPower` follows the battery |
| without battery | 160 `L=48` (2 blocks) · **124 absent** | `Read data block response error: ProtocolError` every 7 s on `"storage"` — it still reads 40355 |
| battery back | 160 `L=88` · 124 present | the errors stop, but **`currentPower` stays frozen at its last value** |

The storage's `thingPower` stayed at `0.0` over several successive readings while
the registers held `StCha = 2000 W` and `ChaSt = Charging`. A
restart of nymead forces rediscovery, and the value immediately goes back
to `2000.0`.

**What this teaches.** The defect is neither in the sign nor in the mapping of
D-9: those are correct, and prove it as soon as discovery is redone.
It lies in the fact that a thing↔model binding is established once and never
reconsidered. The storage is more exposed to it than other things because it
depends on **three** models instead of one.

**Actual reach.** On a simulator it is frequent: any restart of the simulator
with different options changes the chain. On hardware it is rare but not
theoretical — a firmware update, or a battery added to an already paired
hybrid, produces exactly this transition.

**Workaround today.** Restart `nymead`, or remove and re-add the
connection. Losing the TCP connection also suffices, since it triggers a
rediscovery.

**How to resolve.** Rediscover periodically, or on repeated Modbus errors at a
previously valid address — the `ProtocolError` every 7 s in the table above
is precisely the signal that the chain has moved, and it is currently logged
without consequence. Outside the scope of D-9, which does not touch the
discovery cycle.

---

## D-11 — duplicate guard: same serial number AND same kind of model (MEASURED)

**Context.** `autocreateSunSpecModelThing()` rejects any new thing whose
serial number is already carried by an existing thing. This is an upstream
guard: some devices temporarily announce the same model a second time, at other
registers, and the (model, register) pair is not enough to recognise them.

**The defect.** The serial number belongs to the Common block, not to the model:
all models behind the same Common carry it. Under Fronius, model 124 sits
behind the inverter's Common. The guard therefore took the storage for a
duplicate of its own inverter as soon as the inverter child existed:

- first installation: all children are announced before any of them is
  registered, so the storage gets through;
- battery added later, or restart after model 124 appeared: the inverter is
  already there, log "already have a thing with this serial number" then
  "Attempt to add: "Storage"", and no storage. The only way out: delete and
  recreate the connection.

**The rule.** A duplicate is the same serial number **and** the same model
family (`SunSpecDuplicates`):

| family | models |
|---|---|
| inverter | 101, 102, 103, 111, 112, 113 |
| meter | 201, 202, 203, 204, 211, 212, 213, 214 |
| storage | 124 (and `solarEdgeBattery`, which has no model) |

An empty serial number never matches (upstream behaviour). If the family of
either side is unknown, the serial number alone decides, as before.
The lookup still covers **all** of the plugin's things, not only the children
of the same parent: the same device added through two connections is still
rejected.

**Why the original case stays closed.** The original case is the same device
announcing *the same kind* of model again: a second 103, or a single-phase
meter turned three-phase at the same register (upstream comment in
`processDiscoveryResult()`). The phase and encoding variants (1x1/1x3,
2x1/2x3) are deliberately a single family: these cases are still rejected.
Only a model of a different kind gets through. Accepted limitation: two
*legitimate* models of the same kind behind the same Common (two meters, two
storages) were already refused by the old guard, and still are.

**What the rule does not settle.** A device that *transiently* announced a model
of a different kind from everything it already has would create that thing — this
was already the case at first installation. Under Fronius, the phantom storage
is held back by another guard, `FroniusRules::hasBattery()` (D-6), unchanged.

**The three paths, closed-loop** (`tests/discovery`, the real plugin against a
local Modbus TCP server):

| path | result |
|---|---|
| installed without 124, 124 appeared, restart | one storage |
| restart with the storage in the configuration | the same storage, no duplicate |
| 124 appeared while the connection is running, next discovery | one storage, still one after two more discoveries |
| battery wired on a running hybrid (160 from 2 to 4 blocks, 124 shifted) | one storage |
| same 103 repeated at other registers | rejected |
| hybrid without battery (124 with zero voltages) | no storage, at installation as at the next discovery |

The "never twice" check relies on two things: `getThingForSunSpecModel()`
finds the already registered thing (same model, same register, same parent),
and the guard above rejects a second storage of the same device at another
address. One window remains: between the announcement of a thing and its
registration, two event-queue turns (`autoThingsAppeared`, then
`ThingSetupInfo::finished`, both queued). Two complete discoveries of the
same connection cannot fit in it: a discovery is a series of Modbus
round trips.

**What remains tied to D-10.** In operation, a 124 that appears is only seen at
the next discovery, i.e. at a TCP reconnection or a nymead restart.
Two consequences of the frozen geometry, observed in the same suite:

- the old 160 (2 blocks) stays in the connection's model list next to
  the new one (4 blocks), same start register. `bindStorageCompanionModels()`
  keeps the last of the two in list order: today the new one,
  but nothing guarantees it;
- a 124 already present without a battery that does **not** change address
  when the battery is wired keeps its values from the first discovery
  (`InBatV = 0`): the connection keeps the old model object, which nobody
  re-reads as long as no thing is bound to it. The storage then only appears
  at a nymead restart — which, for its part, now works. On a GEN24, the
  spec (INV-1) says that model 124 shifts when 160 goes to 4 blocks: this case
  should not occur there. **INFERRED**, to be confirmed on hardware.

**How to resolve.** The second point is resolved with D-10 (a rediscovery that
replaces the models). The first one too; failing that, prefer the 160 that
carries battery blocks.

---

## D-12 — battery energy totals: DC side, and only when the device publishes them (INFERRED)

**What was missing.** `sunspecStorage` carried neither `totalEnergyConsumed` nor
`totalEnergyProduced`. nymead (`nymea-experience-plugin-energy`,
`EnergyManagerImpl::updatePowerBalance()`) computes `totalFromStorage` from the
`totalEnergyProduced` of `energystorage` things and adds it to
`totalConsumption`. Without it, battery discharge never entered
consumption: on an evening where the battery supplies most of the energy, the
published cumulative consumption equalled the house consumption **minus** the
discharge.

**What the code does.**

| state | source | unit |
|---|---|---|
| `totalEnergyConsumed` (energy charged) | Σ `DCWH` of the `StCha` blocks of model 160 | kWh |
| `totalEnergyProduced` (energy discharged) | Σ `DCWH` of the `StDisCha` blocks of model 160 | kWh |

Same convention as `currentPower`: charge is what the battery consumes.
`DCWH` is an `acc32` in Wh, multiplied by `10^DCWH_SF` by libnymea-sunspec in
the same transaction as the other points of model 160 (same read order as
`DCW_SF`, see D-9). No interface added: `smartmeterproducer` would count the
discharge as PV production.

**DC-side energies.** `DCWH` is measured between the battery and the inverter.
The energy the house sees on the AC side differs by the inverter's conversion
efficiency, in both directions: a 10 kWh charge on the DC side took
a little more on the AC side, a 10 kWh discharge on the DC side delivers a little
less. The nymead balance therefore mixes a DC-side discharge with AC-side grid
and PV energies; the discrepancy is of the order of the inverter losses.
There is no AC-side battery total in the SunSpec models that
Fronius publishes.

**Nothing invented.** No model 160, or a 160 without a `StCha`/`StDisCha` block
(generic vendor, SolarEdge — which has no 160 —, inverter without battery):
both states keep their default value, **0**. nymead reads a 0 as
"no counter yet" and synchronises on the first non-zero value
without counting it; as long as it stays at 0, the battery adds nothing to the
balance, exactly as before. No power integration in the plugin.

Never published, the state then keeping its last value:

- `DCWH = 0`: the "not accumulated" value of an `acc32`. It is also the
  library's initial value, and what a Fronius firmware earlier than 1.31
  puts on the inactive battery block (GEN24 Modbus documentation). The library
  itself keeps the previous value of an invalid point: a block that stops
  publishing leaves a frozen total, never a zero;
- a register at `0xFFFFFFFF`, whatever the scale factor;
- any `DCWH` read with an unimplemented `DCWH_SF` (`0x8000`);
- a role where one of the blocks has no publishable value: a partial sum
  would jump on the day that block starts publishing.

**A decreasing total** is published as is, with a log line. nymead
already treats a decrease as a reset: it resynchronises on the low
value and counts no energy for that cycle, as for a meter.
Holding the old value here would mask the reset, then swallow every
kWh until the device climbs back above it.

**What nymead will do with it, and what it will not.** In nymead 1.15.2,
`totalConsumption = acquisition + production + totalFromStorage − return`, where
`totalFromStorage` comes from `totalEnergyProduced` alone. Discharge is therefore
now counted in consumption. **Charge**, however, is not subtracted:
a battery charging from PV is still counted in the house
consumption, since nymead does not read a storage's `totalEnergyConsumed`
in this calculation. It does read it in the per-thing log
(`updateThingPower()`), where both totals are logged for the battery.
Fully separating house and battery in the balances requires a change
to nymead, not to the plugin.

**Why INFERRED.** Everything above has been exercised against a Modbus server
that follows the specification, not against a GEN24. Still to be captured
read-only on a GEN24 fitted with a battery: that `DCWH` of the `StCha`/`StDisCha`
blocks is indeed a lifetime total (and not a daily one), that `DCWH_SF` is
implemented, what the total does across a firmware update, and the actual
discrepancy with the AC-side energy counters.

**Locks.** `test_B2_*` in `tests/logic` (pure rules) and in
`tests/discovery` (full chain: registers, scaling, published states).
