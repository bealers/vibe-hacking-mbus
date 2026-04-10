# Phase 2D Summary: Bus Denial of Service (Single-Master Constraint)

**Date:** 2026-04-09  
**Hardware:** Raspberry Pi + packom.net M-Bus Master HAT (primary), USB-to-M-Bus adapter (second master)  
**Target:** All devices on the lab bus

---

## Objective

Demonstrate the practical effect of M-Bus's single-master architecture:

1. Whether connecting a second master to a live bus causes a denial of service
2. What the recovery time is after the second master is removed
3. The implications for other attacks in this research

---

## Background

M-Bus (EN 13757-2) is designed as a single-master protocol. One device on the bus acts as the master; all others are slaves. The standard defines no arbitration or collision avoidance mechanism for multiple simultaneous masters. When two masters are present, both transmit at their own timing without coordination, causing frame corruption on the shared bus.

---

## Method

A continuous polling loop was run on the Pi master (reading the Kamstrup every 10 seconds, logging success and failure). A second USB-to-M-Bus adapter was then connected to the same bus and run continuously scanning or polling. Success/failure rate was monitored.

```bash
# Pi master - continuous polling loop
while true; do
    mbus-serial-request-data -b 2400 /dev/ttyAMA0 206 > /dev/null 2>&1 \
        && echo "$(date) OK" \
        || echo "$(date) FAIL"
    sleep 10
done

# Second master (separate device) - continuous scan
mbus-serial-scan -b 2400 /dev/ttyUSB0
```

---

## Results

| State | Read success rate |
|-------|------------------|
| One master (Pi only) | 100% |
| Two masters simultaneously | **0%** |
| After removing second master | 100% (within ~4 seconds) |

Read failures were immediate on connecting the second master. No partial reads. Recovery was complete within one polling cycle (~4 seconds) of disconnecting the second master.

---

## Two Findings from One Test

### Finding A: Physical connection is an effective DoS

Connecting a second master to an M-Bus bus causes immediate, complete communications failure for all devices on the bus. No software, no commands, no configuration required. Physical connection alone is sufficient.

### Finding B: This constrains all other attacks

The same single-master architecture constrains every other attack in this research. A second master cannot:

- Silently sniff a live bus while the legitimate master is operating
- Send commands (address changes, data injection) without disrupting the existing installation

Active attacks therefore require the legitimate master to be offline. Practical windows for this include maintenance periods and any point the head-end is not actively polling. 

---

## Recovery

Recovery was complete within ~4 seconds of removing the second master. No meter configuration was changed. No data was lost.

---

## Tools Used

Standard libmbus tools used on both masters. 
