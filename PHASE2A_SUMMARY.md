# Phase 2A Summary: Passive Data Sniffing

**Date:** 2026-04-09  
**Hardware:** Raspberry Pi + packom.net M-Bus Master HAT  
**Target devices:** Kamstrup MULTICAL 602 (addr 206), Zenner Zelsius (addrs 0, 11, 12, 13)

---

## Objective

Demonstrate that any M-Bus master connected to the bus can read complete meter data from all connected devices without credentials, challenge-response, or prior knowledge of any kind.

---

## Method

Connected the Pi HAT to the M-Bus bus and used `mbus-serial-request-data` (libmbus) to request full data from each device. No configuration, no pairing, no credentials. Just the device's primary address.

```bash
mbus-serial-request-data -b 2400 /dev/ttyAMA0 206
mbus-serial-request-data -b 2400 /dev/ttyAMA0 0
```

---

## Results

### Kamstrup MULTICAL 602 - 27 data records returned in plaintext

| Field | Value |
|-------|-------|
| Manufacturer | KAM (Kamstrup) |
| Serial number | 78180206 |
| Energy (cumulative) | 19 kWh |
| Volume | 20.45 m³ |
| Flow temperature | 20.30°C |
| Return temperature | 19.99°C |
| Temperature differential | 0.31°C |
| Current power | (not recorded) |
| Flow rate | (not recorded) |
| On-time | 5,954 hours |
| Historical snapshot | 2020-12-31 billing date |
| Access number | 1 |

No encryption. No authentication. No access control. Data returned immediately.

### Zenner Zelsius - 4 sub-meters, all returned data

| Address | Medium | Key Reading |
|---------|--------|-------------|
| 0 | Heat: Outlet | 378,105 kWh |
| 11 | Water | 7,975 m³ |
| 12 | Warm water | 747 m³ |
| 13 | Other | 58 m³ |

Serial numbers and manufacturer identity returned on all sub-meters.

---

## What the Data Includes

The Kamstrup dataset includes:

- Consumption history: cumulative energy and volume plus historical snapshots with billing dates
- Usage patterns: on-time hours and current operational state
- Thermal efficiency: flow/return temperature differential
- Device identity: serial number and manufacturer



---

## Protocol Observation

EN 13757-2:2018 defines no encryption and no authentication for wired M-Bus. The REQ_UD2 command (data request) requires only a valid primary address. There is no mechanism for a slave device to verify the identity of the requesting master. This is not a vendor implementation flaw. It is the standard as written.

---

## Tools Used

```bash
# Read full dataset from a meter
mbus-serial-request-data -b 2400 /dev/ttyAMA0 <address>

# Read with raw hex debug output
mbus-serial-request-data -d -b 2400 /dev/ttyAMA0 <address>
```
