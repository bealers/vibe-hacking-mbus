# Phase 2C Summary: Unauthenticated Command Injection (SND_UD)

**Date:** 2026-04-09  
**Operator:** Bealers  
**Target devices:** Kamstrup MULTICAL 602 (addr 206), Zenner Zelsius (addrs 0, 11, 12, 13)

---

## Objective

Demonstrate that M-Bus SND_UD commands are accepted without any authentication, allowing an attacker to persistently modify meter configuration — specifically primary address and baud rate — using only a standard M-Bus master.

---

## Results

### Address Reassignment

All five M-Bus addresses across both physical devices accepted unauthenticated address change commands.

| Device | Original Address | Changed To | Verified at New Address | Restored |
|--------|-----------------|------------|------------------------|----------|
| Kamstrup MULTICAL 602 | 206 | 10 | YES — gone from 206 | YES |
| Zenner (sub 0) | 0 | 99 | YES — gone from 0 | YES |
| Zenner (sub 1) | 11 | 99 | YES — gone from 11 | YES |
| Zenner (sub 2) | 12 | 99 | YES — gone from 12 | YES |
| Zenner (sub 3) | 13 | 99 | YES — gone from 13 | YES |

Command used: `mbus-serial-set-address -b 2400 /dev/ttyAMA0 <old> <new>`

No credentials, no challenge-response, no acknowledgement required from the operator. The change takes effect immediately and persists until explicitly reversed.

### Baud Rate Switching

| Device | Address | Result |
|--------|---------|--------|
| Kamstrup MULTICAL 602 | 206 | **Not supported** — no reply |
| Zenner (sub 0) | 0 | **SUCCESS** — switched to 9600, invisible at 2400 until restored |
| Zenner (subs 1-3) | 11-13 | Not supported |

Command used: `mbus-serial-switch-baudrate -b 2400 /dev/ttyAMA0 <address> 9600`

The Zenner's primary sub-meter accepted the baud rate switch and was confirmed unreachable at 2400 baud until explicitly restored at 9600.

---

## Access Counter Observations

Every M-Bus read and write command increments the meter's `AccessNumber` field. This is a forensic artefact — evidence of our activity is recorded on the meter itself.

**Kamstrup MULTICAL 602:**

| Point | AccessNumber |
|-------|-------------|
| Before any Phase 2C activity | 5 |
| After address change to 10 | 6 |
| After address restore to 206 | 7 |

**Zenner (each sub-meter incremented independently per operation).**

On a real installation, elevated access counters could be detected by the operator via the meter's front panel or manufacturer software (e.g. IZAR Centre). However, in practice most heat network operators do not routinely audit access counters.

---

## Impact

An unauthenticated attacker with physical or electrical access to the M-Bus wiring can:

1. **Orphan a meter from its BMS** — by changing its primary address, the head-end system stops receiving readings. The meter continues operating normally but is invisible to the legitimate master. The fault would appear as a communications failure, not an attack.

2. **Disrupt polling by baud rate mismatch** — where supported, switching a meter to a non-standard baud rate achieves the same effect. The legitimate master times out on every poll.

3. **Enumerate and re-address an entire installation** — systematically rename all meters, replacing the operator's known address map with attacker-controlled values.

No specialist equipment is required beyond a standard M-Bus master and libmbus (open source, freely available).

---

## Commands Reference

```bash
# Change primary address
mbus-serial-set-address -b 2400 /dev/ttyAMA0 <old_addr> <new_addr>

# Switch baud rate
mbus-serial-switch-baudrate -b 2400 /dev/ttyAMA0 <address> <new_baud>

# Verify meter at new address
mbus-serial-request-data -b 2400 /dev/ttyAMA0 <new_addr>

# Confirm gone from old address (expect: "Failed to receive M-Bus response frame")
mbus-serial-request-data -b 2400 /dev/ttyAMA0 <old_addr>
```
