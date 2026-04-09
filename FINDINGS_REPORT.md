# M-Bus Security Research: Findings Report

**Date:** 2026-04-09  
**Author:** Siftware Ltd  
**Lab:** Controlled wired M-Bus test environment  
**Hardware:** Raspberry Pi + packom.net M-Bus Master HAT, Kamstrup MULTICAL 602, Zenner Zelsius  

---

## Summary

Wired M-Bus (EN 13757-2) has no authentication or encryption at the protocol level. Any device capable of acting as an M-Bus master has unrestricted read and write access to every meter on the bus. This was confirmed empirically against two real meters using open-source tooling.

---

## Finding 1: Unauthenticated Meter Data Exposure

**Severity:** High  

Any M-Bus master connected to the bus can read complete meter data from all devices without credentials of any kind. There is no challenge-response, no pairing, no access control.

**Confirmed exposed data (Kamstrup MULTICAL 602):**
- Device serial number and manufacturer identity
- Cumulative energy (kWh) and volume (m³)
- Flow and return temperatures, temperature differential
- Current power and flow rate
- On-time hours
- Historical snapshot records

**Confirmed exposed data (Zenner Zelsius):**
- Device serial numbers across all sub-meters
- Volume readings per sub-meter channel

**Tool used:** `mbus-serial-request-data` (libmbus, open source)  
**Evidence:** XML and hex captures in `~/mbus_lab/phase1/` on lab Pi

---

## Finding 2: Unauthenticated Device Enumeration

Any M-Bus master can discover all devices on a bus without any prior knowledge — no addresses, no serial numbers required.

- Primary scan (addresses 0–250) completed in ~90 seconds
- Secondary scan discovered all devices by 64-bit secondary address
- Both scans require zero credentials

**Tool used:** `mbus-serial-scan`, `mbus-serial-scan-secondary`

---

## Finding 3: Unauthenticated Address Reassignment

Any M-Bus master can permanently change a meter's primary address without credentials. The change takes effect immediately and persists until explicitly reversed.

**Tested on:** Kamstrup MULTICAL 602 and all four Zenner sub-meters — all accepted address change commands without challenge.

**Impact:** An attacker can remap meter addresses, causing the legitimate BMS/head-end system to lose visibility of one or all meters. The fault presents as a communications error, not an attack. The meter continues operating normally and recording consumption — only the communications link to the operator is broken.

**Tool used:** `mbus-serial-set-address`

---

## Finding 4: Single-Master Bus Architecture

M-Bus is a single-master protocol. When two masters are simultaneously present on the same bus, the bus becomes non-functional — neither master can communicate with any slave device.

**Observed:** 100% read failure rate with two masters present. Full recovery within ~4 seconds of removing one master.

**Implication:** An attacker cannot passively sniff a live bus while a legitimate master is operating, nor can they send commands without disrupting the existing installation. The attack window requires either uncontested access (out of hours, the legitimate master is offline) or acceptance that connecting will cause a detectable outage.

This is also a finding in its own right: **connecting a second master to a live bus is an effective denial of service**, requiring no software — physical connection alone is sufficient.

---

## Limitations of This Research

- Baud rate switching (SND_UD) was tested but only confirmed on the Zenner simulator. The Kamstrup MULTICAL 602 did not respond to baud rate change commands.
- Passive sniffing by a second master was not achieved — the single-master constraint prevents this with the hardware tested.
- The IZAR Centre head-end software was not successfully tested. The proprietary serial interface cable was not available.
- Lab disruption: the packom HAT was likely damaged during testing (suspected voltage spike from briefly connecting IZAR Centre probe). Remaining tests were completed using a USB M-Bus master (NP Link) on a second Pi.

---

## Attack Scenario (Real-World)

An adversary with physical access to M-Bus wiring — in a riser cupboard, plant room, or meter cabinet — can:

1. Connect a USB M-Bus master (commercially available, ~£30) to the two-wire bus
2. Run `mbus-serial-scan` to enumerate all meters within ~90 seconds
3. Read full consumption, temperature and operational data from every meter
4. Remap meter addresses to orphan them from the BMS

No credentials, no specialist knowledge beyond running two commands. The attack is feasible outside business hours when the legitimate master is least likely to be actively polling.

---

## Protocol Context

These findings are consistent with the EN 13757-2:2018 specification, which defines no authentication or encryption for wired M-Bus. This is not a vendor implementation flaw — it is an inherent characteristic of the protocol as standardised. Encrypted M-Bus (EN 13757-7) exists but is optional and not widely deployed in UK heat networks.

---

## Evidence Files

All raw evidence is on the lab Pi at `~/mbus_lab/`:

| File | Description |
|------|-------------|
| `phase1/meter_addr206_baseline.xml` | Full Kamstrup dataset, 27 records |
| `phase1/meter_addr[0,11,12,13]_baseline.xml` | Zenner sub-meter data |
| `phase1/secondary_scan.txt` | Secondary address discovery output |
| `phase1/*_baseline_hex.txt` | Raw frame captures for all meters |
