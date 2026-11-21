# SunSpec

Connect nymea to SunSpec devices over Modbus TCP

## Supported Things

* SunSpec Inverter
	* Model ID 101 & 111 Single phase inverter
	* Model ID 102 & 112 Split phase inverter
	* Model ID 103 & 113 Three phase inverter
* SunSpec Meter
	* Model ID 201 & 211 Single phase meter
	* Model ID 202 & 212 Split phase meter
	* Model ID 203 & 213 Three phase meter
* SunSpec Storage 
	* Model ID 124 (state of charge, status, setpoints)
	* Model ID 160 (instantaneous power, `StCha` / `StDisCha` blocks)
	* Model ID 120 (rated capacity, `WHRtg`)


## Tested connections

* SolarEdge (with custom battery)

## Requirements

* The package 'nymea-plugin-sunspec' must be installed.
* The SunSpec device must support SunSpec over modbus TCP

## More
https://sunspec.org

---

## Operation — battery visible but not measurable

Since `1.15.2+etm10`, the `SunSpec Storage` thingClass declares the
**`energystorage`** interface. This is what makes it visible to nymea's power
balance (`currentPowerStorage`), to the vehicle charging reserve, and to the
waterfall reserve. Before, it only declared `battery` and **none of the three
found it**.

The `currentPower` state is read from **model 160**, not from model 124, which
has no power register. A consequence to know before diagnosing a battery
"stuck at zero":

> **A SunSpec inverter that publishes model 124 without model 160 carries the
> `energystorage` interface with a `currentPower` that will never move from
> zero.** This is the case of generic SunSpec devices. The battery then shows
> up in the balance, counted as 0 W — which reads "neither charging nor
> discharging", not "I don't know".

This case is **reported once per thing** in the `nymead` log, at startup or at
the first reading after discovery. Two wordings, depending on the cause:

```
W | SunSpec: Storage "<name>" has no model 160 on its connection: currentPower
    has no register to read and will stay at 0. This storage is visible to the
    energy balance but not measurable.

W | SunSpec: Storage "<name>" has a model 160 but no StCha/StDisCha block:
    currentPower has nothing to read and will stay at 0.
```

The second form is the case of a hybrid inverter **without a wired battery**: it
publishes `N = 2` (the two PV strings) and no battery block.

To check first on a site where `currentPowerStorage` stays at zero:

```bash
journalctl -u nymead | grep -i "will stay at 0"
```

If the line is there, the fault is in what the device publishes, not in nymea.
If it is absent, the battery is measured and the zero is a real measurement.

The `capacity` state (kWh) comes from `WHRtg` of model 120. When the nameplate
does not implement it, the state is **left as is** rather than reset to zero:
a zero would read as a measurement. No engine rule reads `capacity` to date.

Details of the reasoning, of the sign measurement and of the accepted gap:
`DIVERGENCES.md`, entry **D-9**.

## Operation — battery energy totals

`SunSpec Storage` publishes `totalEnergyConsumed` (energy charged) and
`totalEnergyProduced` (energy discharged), in kWh, from the `DCWH` total of the
`StCha` and `StDisCha` blocks of **model 160**. These are **DC-side**
energies: the gap with what the grid sees is the inverter efficiency.

Without model 160, or without these two blocks, both states stay at **0** and the
battery does not count in nymead's energy totals. No value is estimated from
the power. A "not implemented" value (`DCWH` at 0 or at `0xFFFFFFFF`, `DCWH_SF`
missing) is never published: the state keeps its last value.

A total that goes down (reset of the device) is published as is and reported
in the log:

```
I | SunSpec: Storage "<name>" totalEnergyProduced went down from <old> to
    <new> kWh: the device counter has been reset.
```

nymead treats it as a meter reset: no energy is counted on that cycle.
Details: `DIVERGENCES.md`, entry **D-12**.

## Control — blocking battery discharge

`SunSpec Storage` carries the **`setDischargeBlocked`** action (boolean parameter
`dischargeBlocked`). It writes in a single sequence, in this order:

| `dischargeBlocked` | `StorCtl_Mod` | `OutWRte` | then |
|---|---|---|---|
| `true` | 2 (discharge limit active) | 0 % | `InOutWRte_RvrtTms` = `revertTime` |
| `false` | 0 (no limit) | 100 % | `InOutWRte_RvrtTms` = `revertTime` |

`InOutWRte_SF` is read before writing, `StorCtl_Mod` and `OutWRte` are read back
afterwards: a setpoint acknowledged but not applied sets `controlHealthy` to
`false`. If writing `StorCtl_Mod` fails, the action fails and nothing else is
written.

The `revertTime` timer is meant to lift the limit when it expires, so the action
must be repeated before that. On the model 124 of a GEN24, this is an assumption
not yet verified on a real device (AC-10 of `sunspec/FRONIUS.md`).
With `revertTime` at 0, no timer is armed and the limit stays in place if
nymead stops.

The `enableDischarging` state ("Discharge limit active") is the DISCHARGE bit of
`StorCtl_Mod`. `true` means that discharging is **capped** at
`dischargingRate`, not that it is allowed.

## Operation — battery added to an already installed inverter

The storage of a hybrid inverter carries the inverter's serial number (same
Common block). A battery added after installation is now created at the next
discovery: when nymead restarts, or when the inverter reconnects. It is no
longer necessary to delete and recreate the connection.
Details: `DIVERGENCES.md`, entry **D-11**.
