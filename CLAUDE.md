# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a **security research lab documentation repository** for conducting controlled testing of M-Bus (Meter Bus) vulnerabilities on wired metering infrastructure. It is owned by Siftware Ltd to build HNTAS (Heat Networks Technical Assurance Scheme) consulting capability. There is no source code — only Markdown documentation.

**Primary documents:**
- `START-HERE.md` — Phase-by-phase task checklist with shell commands and troubleshooting
- `MBUS_SECURITY_RESEARCH_PLAN.md` — Comprehensive research plan with fact-checked claims and attack methodology

## Lab Hardware Context

- Raspberry Pi + packom.net M-Bus Master HAT (primary controller)
- Kamstrup MULTICAL 602 heat meter (wired M-Bus)
- Zenner Zelsius M-Bus meter (USB flow simulator)
- Danfoss M-Bus master + "Temu" USB-to-M-Bus adapter (secondary, for bus contention tests)
- Windows PC (for IZAR Centre software)

## Lab Phases

| Phase | Goal |
|-------|------|
| 0 | Pi access & serial port enablement (disable Bluetooth UART conflict) |
| 1 | Baseline documentation of meters using libmbus |
| 2 | Four practical attacks (2A sniffing, 2B enumeration, 2C command injection, 2D DoS) |
| 3 | Evidence packaging with SHA-256 integrity hashes |

## Key Tool: libmbus

All active testing uses [libmbus](https://github.com/rscada/libmbus). Install on the Pi:

```bash
git clone https://github.com/rscada/libmbus
cd libmbus
./build.sh
sudo make install
sudo ldconfig
```

Core commands used: `mbus-serial-request-data`, `mbus-serial-scan`, `mbus-serial-scan-secondary`, `mbus-serial-set-address`, `mbus-serial-switch-baudrate`.

Default M-Bus baud rate: **2400**. Meters also support 300 and 9600.

## Serial Port Setup (Pi)

Disable Bluetooth competing for UART:
```
# /boot/config.txt
dtoverlay=disable-bt
```
Also run: `sudo systemctl disable hciuart` and use `raspi-config` to disable serial console login while keeping hardware serial enabled.

## Attack Scope

The four Phase 2 attacks (against own hardware in a controlled lab):

- **2A Passive sniffing** — read plaintext meter data (no encryption in EN 13757-2)
- **2B Enumeration** — discover meters without credentials via `mbus-serial-scan`
- **2C Command injection (SND_UD)** — unauthenticated address/baud rate changes via `mbus-serial-set-address`
- **2D Bus contention/DoS** — introduce second master, measure read failure rates

## Known Caveats

- Kamstrup meters have **access counters** that increment on each M-Bus read — this creates forensic traces
- FCB (Frame Count Bit) must toggle correctly on some meters
- The Axioma Qalcosonic W1 "motorized valve control" claim from the original brief is **unverified** — W1 is wireless-only with no confirmed controllable valve
- Evidence directory structure is defined in `MBUS_SECURITY_RESEARCH_PLAN.md` section 6

## Standards References

- **EN 13757-2:2018** — Wired M-Bus physical/link layer (no encryption or authentication defined)
- **EN 13757-3:2018** — Application layer
- **EN 13757-7:2018** — Transport and security
- **OMS Specification Vol 2** — Security profiles (wireless M-Bus only)
