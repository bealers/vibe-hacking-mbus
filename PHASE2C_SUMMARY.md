# Phase 2C Summary: Unauthenticated Command Injection (SND_UD)

**Date:** 2026-04-09  
**Operator:** Bealers (vibed/Claude)  
**Target devices:** Kamstrup MULTICAL 602 (addr 206), Zenner Zelsius heat meter (addrs 0, 11, 12, 13)

---

## Objective

Demonstrate that M-Bus SND_UD commands are accepted without any authentication, allowing persistent modification of meter configuration using only a standard M-Bus master.

---

## Results

### Address Reassignment

All five M-Bus addresses across both physical devices accepted unauthenticated address change commands.

| Device | Original Address | Changed To | Verified at New Address | Restored |
|--------|-----------------|------------|------------------------|----------|
| Kamstrup MULTICAL 602 | 206 | 10 | YES (absent from 206) | YES |
| Zenner (sub 0) | 0 | 99 | YES (absent from 0) | YES |
| Zenner (sub 1) | 11 | 99 | YES (absent from 11) | YES |
| Zenner (sub 2) | 12 | 99 | YES (absent from 12) | YES |
| Zenner (sub 3) | 13 | 99 | YES (absent from 13) | YES |

Command used: `mbus-serial-set-address -b 2400 /dev/ttyAMA0 <old> <new>`

No credentials, no challenge-response. The change takes effect immediately and persists until explicitly reversed.

### Baud Rate Switching

| Device | Address | Result |
|--------|---------|--------|
| Kamstrup MULTICAL 602 | 206 | Not supported (no reply) |
| Zenner (sub 0) | 0 | Supported: switched to 9600, invisible at 2400 until restored |
| Zenner (subs 1-3) | 11-13 | Not supported |

Command used: `mbus-serial-switch-baudrate -b 2400 /dev/ttyAMA0 <address> 9600`

The Zenner's primary sub-meter accepted the baud rate switch and was confirmed unreachable at 2400 baud until explicitly restored at 9600.

---

## Access Counter Observations

Every M-Bus read and write command increments the Kamstrup's `AccessNumber` field. Evidence of activity is recorded on the meter itself and persists after the session ends.

**Kamstrup MULTICAL 602:**

| Point | AccessNumber |
|-------|-------------|
| Before any Phase 2C activity | 5 |
| After address change to 10 | 6 |
| After address restore to 206 | 7 |

The Zenner did not expose a readable AccessNumber field in the same way.

---

## Commands Reference

```bash
# Change primary address
mbus-serial-set-address -b 2400 /dev/ttyAMA0 <old_addr> <new_addr>

# Switch baud rate
mbus-serial-switch-baudrate -b 2400 /dev/ttyAMA0 <address> <new_baud>

# Verify meter at new address
mbus-serial-request-data -b 2400 /dev/ttyAMA0 <new_addr>

# Confirm absent from old address (expect: "Failed to receive M-Bus response frame")
mbus-serial-request-data -b 2400 /dev/ttyAMA0 <old_addr>
```
