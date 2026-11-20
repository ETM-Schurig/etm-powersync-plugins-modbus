# etm-powersync-plugins-modbus

Modbus integration plugins for [nymea](https://nymea.io), built and packaged by
ETM-Schurig for the ETM PowerSync home energy management system.

This repository is a fork of
[nymea-plugins-modbus](https://github.com/nymea/nymea-plugins-modbus), based on
upstream **1.15.2**. It is not a full copy of upstream: it only carries the
plugins ETM builds and ships. The Modbus libraries (`libnymea-modbus`,
`libnymea-sunspec`) and every other upstream plugin stay upstream; they are
used as the system packages published by nymea.

## Where the shipped code lives

The packages published in the ETM APT repository are built from the
`release/…` branches of this repository (currently `release/etm17`, package
version `1.15.2+etm17`). The default branch `main` is behind them: it does not
contain the `sunspec` module yet. Read the release branch to see what is
installed on a system.

## Modules and packages

| Module | Debian package | Devices |
|---|---|---|
| `eastron` | `powersync-plugin-eastron` | Eastron SDM energy meters over Modbus RTU (SDM72, SDM120, SDM220, SDM230, SDM630), each usable as grid meter, consumer or PV meter. |
| `sunspec` | `nymea-plugin-sunspec-etm01` | SunSpec inverters, meters and storage over Modbus TCP. ETM fork of the upstream SunSpec plugin that adds a Fronius vendor (a GEN24 serves the inverter and its Smart Meter on separate unit IDs behind one host and port). It ships the same plugin file and plugin ID as `nymea-plugin-sunspec`, which it conflicts with and replaces. |

Release channels: `powersync-plugin-eastron` and `nymea-plugin-sunspec-etm01`
are in **stable** (both pulled in by `powersync-core` on an ETM PowerSync box).

The V2C Trydan Modbus TCP plugin (`v2c`, package `powersync-plugin-modbus-v2c`) was
retired on 2026-11-19: the V2C Trydan is driven over its local HTTP API by
`powersync-plugin-v2c` (repository `etm-powersync-plugins`).

The changes made to the vendored SunSpec plugin are described in
[`sunspec/README.md`](sunspec/README.md) and
[`sunspec/DIVERGENCES.md`](sunspec/DIVERGENCES.md) on the release branch.

## Building

Build dependencies (from `debian/control`):

- `debhelper`, `pkg-config`
- `libnymea-dev`, `nymea-dev-tools`
- `libnymea-modbus-dev`, `libnymea-sunspec-dev`
- `qt6-base-dev`, `qt6-base-dev-tools`, `qt6-serialport-dev`, `qt6-serialbus-dev`

Build the Debian packages:

```sh
dpkg-buildpackage -b -uc -us
```

Or build with qmake directly:

```sh
qmake6 [WITH_PLUGINS="..."] [WITHOUT_PLUGINS="..."]
make
```

Build against the same nymea version as the target system (1.15.2 for the
current packages). To build against local source trees instead of the system
packages, pass `NYMEA_MODBUS_PATH` (see `modbus.pri`) or `NYMEA_SUNSPEC_PATH`
(see `sunspec.pri`) to qmake.

## Tests

Tests are not part of the package build; each suite is built on its own.

- `eastron/tests`: unit tests of the SDM72 net energy integration (no nymea,
  no Modbus needed):

  ```sh
  cd eastron/tests && qmake6 && make check
  ```

- `sunspec/tests` (release branch): `logic`, `plugin`, `multiunit` and
  `discovery` run with `make check`; `discovery` starts an in-process SunSpec
  Modbus TCP server on 127.0.0.1. The `plugin`, `multiunit` and `discovery`
  suites also need the `nymea` and `nymea-core` pkg-config modules. The `integration` suite is
  built but never run by `make check`: it needs a reachable device given by
  `SUNSPEC_TEST_HOST` and skips every test without it.

  ```sh
  cd sunspec/tests && qmake6 && make check
  ```

## Documentation

User documentation, supported devices and release channels:
<https://docs.etm-powersync.fr>

## License

The plugin sources are licensed under the GNU General Public License,
version 3 or later (SPDX headers in the sources, full text in
[`LICENSE`](LICENSE) / [`COPYING`](COPYING)). The GNU Lesser General Public
License version 3 texts are also included
([`COPYING.LESSER`](COPYING.LESSER), [`LICENSE.LGPL3`](LICENSE.LGPL3)).
