# Phase 2B Summary: Unauthenticated Device Enumeration

**Date:** 2026-04-09  
**Hardware:** Raspberry Pi + packom.net M-Bus Master HAT  
**Target:** All devices on the lab bus

---

## Objective

Demonstrate that a complete inventory of all M-Bus devices on a bus can be obtained without any credentials, prior knowledge of device addresses, or prior knowledge of device identities.

---

## Method

Two complementary scan techniques were used:

**Primary scan:** polls each of the 250 valid primary addresses (0-250) in sequence, looking for an ACK response.

**Secondary scan:** uses a binary-search algorithm over the full 64-bit secondary address space to discover all devices regardless of their primary address. No knowledge of device addresses required.

```bash
# Primary scan - sweep addresses 0-250
time mbus-serial-scan -b 2400 /dev/ttyAMA0

# Secondary scan - discover all devices by hardware identity
time mbus-serial-scan-secondary -b 2400 /dev/ttyAMA0
```

---

## Results

### Primary scan

**Duration: ~90 seconds** for the full 0-250 sweep.

```
Found a M-Bus device at address 0
Found a M-Bus device at address 11
Found a M-Bus device at address 12
Found a M-Bus device at address 13
Found a M-Bus device at address 206
```

Five devices found across two physical meters. No credentials. No prior knowledge of addresses.

### Secondary scan

All five devices discovered by unique 64-bit secondary address:

```
Found a device on secondary address 01257088496A8807
Found a device on secondary address 02257088496A8806
Found a device on secondary address 03257088496A8800
Found a device on secondary address 10257088496A8804
Found a device on secondary address 060218782D2C0F04
```

The secondary address encodes the device's hardware identity: manufacturer, serial number, version, and medium. It is the address.

| Secondary Address | Manufacturer | Medium |
|------------------|--------------|--------|
| `01257088496A8807` | ZRI (Zenner) | Water |
| `02257088496A8806` | ZRI (Zenner) | Warm water |
| `03257088496A8800` | ZRI (Zenner) | Other |
| `10257088496A8804` | ZRI (Zenner) | Heat: Outlet |
| `060218782D2C0F04` | KAM (Kamstrup) | Heat: Outlet |

---

## What the Scan Returns

From a single scan, a connecting device learns:

- How many devices are on the bus
- Each device's primary address
- Each device's manufacturer and medium type (from the secondary address encoding)
- Each device's hardware serial number

In a real installation, the same technique would enumerate all meters on the bus with no credentials required. The output is identical to what a commissioning engineer would obtain during installation. Timing will vary with bus size and baud rate; ~90 seconds was observed in the lab on a five-device bus at 2400 baud.

---

## No Credentials Required

Both scans operate without any form of authentication. The M-Bus primary scan is a simple poll of each address; the secondary scan is a binary-tree search over the address space. Neither requires:

- Knowledge of any device address
- Any shared secret or pairing
- Any prior interaction with the network

Any device connecting to the bus for the first time has the same enumeration capability as the network operator. The protocol has no mechanism to distinguish them.

---

## Tools Used

```bash
mbus-serial-scan -b 2400 <device>
mbus-serial-scan-secondary -b 2400 <device>
```
