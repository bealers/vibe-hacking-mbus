# M-Bus Security Research: Findings Report

**Date:** 2026-04-09  
**Author:** Bealers (vibed by Claude)  
**Lab:** Controlled wired M-Bus test environment (My desk)  
**Hardware:** Raspberry Pi + packom.net M-Bus Master HAT, Kamstrup MULTICAL 602, Zenner Zelsius  

---

## Summary

As we already knew, wired M-Bus ([EN 13757-2](https://m-bus.com/)) has no authentication or encryption at the protocol level. Any device capable of acting as an M-Bus master has unrestricted read and write access to every meter on the bus, **but only when it is the sole master present**. During my testing, a second master being added to bus causes immediate total bus failure, so a real world attack would require the legitimate master to be offline.

---

## Finding 1: Unauthenticated Meter Data Exposure

No surprise. Any M-Bus master connected to the bus can read complete meter data from all devices without credentials of any kind. There is no challenge-response, no pairing, no access control.

**Confirmed data returned (Kamstrup MULTICAL 602):**
- Device serial number and manufacturer identity
- Cumulative energy (kWh) and volume (m³)
- Flow and return temperatures, temperature differential
- Current power and flow rate
- On-time hours
- Historical snapshot records

**Confirmed data returned (Zenner Zelsius):**
- Device serial numbers across all sub-meters
- Volume readings per sub-meter channel

**Tool used:** `mbus-serial-request-data` (libmbus, open source)  
**Details:** [PHASE2A_SUMMARY.md](PHASE2A_SUMMARY.md)

---

## Finding 2: Unauthenticated Device Enumeration

Again, by design. Any M-Bus master can discover all devices on a bus without any prior knowledge - no addresses, no serial numbers required.

- Primary scan (addresses 0-250) completed in ~90 seconds
- Secondary scan discovered all devices by 64-bit secondary address
- Both scans require zero credentials

**Tool used:** `mbus-serial-scan`, `mbus-serial-scan-secondary`  
**Details:** [PHASE2B_SUMMARY.md](PHASE2B_SUMMARY.md)

---

## Finding 3: Unauthenticated Address Reassignment

Any M-Bus master can permanently change a meter's primary address without credentials. The change takes effect immediately and persists until explicitly reversed.

**Tested on:** Kamstrup MULTICAL 602 and all four Zenner sub-meters. All accepted address change commands without challenge.

**Impact:** Remapping a meter's address causes the legitimate BMS/head-end to lose visibility of it. The fault presents as a communications error.

**Tool used:** `mbus-serial-set-address`  
**Details:** [PHASE2C_SUMMARY.md](PHASE2C_SUMMARY.md)

---

## Finding 4: Single-Master Bus Architecture

M-Bus is a single-master protocol. When two masters are simultaneously present on the same bus, the bus becomes non-functional. Neither master can communicate with any slave device.

**Observed:** 100% read failure rate with two masters present. Full recovery within ~4 seconds of removing one master.

**Implication:** A second master cannot passively sniff a live bus while the legitimate master is operating, nor send commands without disrupting the existing installation. The attack window for active attacks requires the legitimate master to be offline.

This is also a finding in its own right: connecting a second master to a live bus causes immediate total bus failure, basically a Denial of Service (DoS) attack. No software required. Physical connection alone is sufficient. 

**Details:** [PHASE2D_SUMMARY.md](PHASE2D_SUMMARY.md)

---

## Finding 5: Unauthenticated Meter Identity Cloning (Rogue Slave)

Theoretically interesting. A rogue M-Bus slave device can impersonate a legitimate meter - responding to its secondary address, presenting its manufacturer identity, and returning attacker-controlled readings. The master has no mechanism to detect the substitution.

**Demonstrated:** A custom slave program (`scripts/mbus-rogue-slave.c`, ~200 lines of C against the libmbus API, vibed using Claude Code) was used to clone the Kamstrup MULTICAL 602's secondary address (`060218782D2C0F04`, manufacturer KAM, medium Heat) and respond to secondary address queries with fabricated energy and volume readings. The real meter was first remapped to an unused address using the Phase 2C technique (takes ~2 seconds).

**Master received from rogue slave:** `Manufacturer: KAM, Energy: 54,321 kWh, Volume: 111,111 m³`  
**Master raised no alarm.**

**Hardware required:** One USB-to-M-Bus adapter (~£30).

**Practical constraint:** The address remapping step (Phase 2C) requires the legitimate master to be offline. You cannot send SND_UD commands while a master is actively on the bus (see Finding 4). The full substitution attack is only viable during a window when the master is offline.

However, a realistic within-property scenario exists without that constraint: an occupant can remap their own meter, which they have uncontested physical access to within their own property, and replace it with a rogue slave returning falsified readings. The master continues polling the secondary address and receives attacker-controlled data. No timing window or network disruption required.

**Combined attack chain (within-property variant):**

1. Enumerate the bus from within the property (Phase 2B - ~90 seconds, passive read-only)
2. Remap your own meter to an unused address (Phase 2C) or in extreme case, just unplug the mbus cable. 
3. Start rogue slave claiming the original address and your meter's secondary address (Phase 2E)
4. All subsequent master polls return attacker-controlled readings

**Tool:** `scripts/mbus-rogue-slave.c` (source committed to this repository)  
**Details:** [PHASE2E_SUMMARY.md](PHASE2E_SUMMARY.md)

---

## Limitations of This Research

- Baud rate switching (SND_UD) was tested but only confirmed on the Zenner Zelsius. The Kamstrup MULTICAL 602 did not respond to baud rate change commands.
- Passive sniffing by a second master was not achieved. The single-master constraint prevents this with the hardware tested.
- The full meter substitution attack (Finding 5) requires the legitimate master to be offline to remap another party's meter. The within-property variant (remapping your own meter) does not have this constraint.

---

## Attack Scenario (Real-World)

**Pre-condition:** M-Bus is a single-master protocol. In any live installation, a BMS or head-end system is permanently connected as master. Connecting a second master causes immediate, total bus failure - 100% read failure rate confirmed in testing. Active attacks therefore require the legitimate master to be offline: during maintenance, after a system fault, or at any point the head-end is not actively polling.

Given that window, a device with physical access to M-Bus wiring in a riser cupboard, plant room, or meter cabinet can:

1. Connect a USB M-Bus master (commercially available, ~£30) to the two-wire bus
2. Run `mbus-serial-scan` to enumerate all meters within ~90 seconds
3. Read full consumption, temperature and operational data from every meter

Steps 1-3 require only that the legitimate master is not actively on the bus. Read access alone exposes the kind of individual metering data that the HNTAS consultation identifies as sensitive personal information for residential occupants.

For an occupant acting from within their own property, where they have physical access to their own meter, the attack extends further with no dependency on the master being offline:

4. Remap their own meter off its primary address
5. Start a rogue slave claiming that meter's secondary address and identity
6. All subsequent BMS polls return attacker-controlled readings - no alarm was raised in testing at the head-end

No credentials required at any step.

---

## Protocol Context

These findings are consistent with the EN 13757-2:2018 specification, which defines no authentication or encryption for wired M-Bus. This is not a vendor implementation flaw. It is an inherent characteristic of the protocol as standardised. Encrypted M-Bus (EN 13757-7) exists but is optional and not commonly deployed.

---

## Phase Summaries

| Document | Contents |
|----------|----------|
| [PHASE1_SUMMARY.md](PHASE1_SUMMARY.md) | Baseline setup and device inventory |
| [PHASE2A_SUMMARY.md](PHASE2A_SUMMARY.md) | Data returned per device, full field list |
| [PHASE2B_SUMMARY.md](PHASE2B_SUMMARY.md) | Scan output, secondary addresses, timing |
| [PHASE2C_SUMMARY.md](PHASE2C_SUMMARY.md) | Address reassignment results, access counter observations |
| [PHASE2D_SUMMARY.md](PHASE2D_SUMMARY.md) | DoS failure rates, recovery timing |
| [PHASE2E_SUMMARY.md](PHASE2E_SUMMARY.md) | Rogue slave exchange, wire trace, attack chain |
