# M-Bus Security Research

Practical security testing of wired M-Bus (EN 13757-2) heat metering infrastructure, conducted as part of research into the UK Heat Networks Technical Assurance Scheme (HNTAS) consultation.

All testing was conducted against equipment owned by the author in a controlled lab environment. No live installations were accessed.

**Blog post:** [Vibe Hacking M-Bus] *(link to follow on publication)*

---

## Background

Wired M-Bus is a protocol used in a large number of metering installations: the two-wire bus that connects individual dwelling meters back to a building management system or data logger. It was designed to be simple and robust. Security was not a design consideration in EN 13757-2.

The question this research set out to answer: if you can physically reach an M-Bus cable from a riser cupboard, a shared plant room, or within a property, what can you actually do?

---

## Hardware Used

| Device | Role |
|--------|------|
| Raspberry Pi + [packom.net M-Bus Master HAT](https://packom.net/m-bus-master-hat/) | Primary M-Bus master |
| Kamstrup MULTICAL 602 | Wired M-Bus heat meter (target) |
| Zenner Zelsius | Heat meter with USB connector on flow body (presents as four sub-meters) |
| USB-to-M-Bus adapter (x2) | Secondary master / rogue slave |

Software: [libmbus](https://github.com/rscada/libmbus) (open source M-Bus master library).

---

## Repository Structure

```
README.md                   This file
FINDINGS_REPORT.md          Summary of all findings
PHASE1_SUMMARY.md           Baseline meter documentation
PHASE2A_SUMMARY.md          Passive data sniffing
PHASE2B_SUMMARY.md          Unauthenticated device enumeration
PHASE2C_SUMMARY.md          Unauthenticated command injection (address reassignment)
PHASE2D_SUMMARY.md          Bus denial of service
PHASE2E_SUMMARY.md          Rogue slave / meter identity cloning
scripts/
  mbus-rogue-slave.c        Rogue M-Bus slave implementation (Phase 2E)
  mbus-bus-power.service    Systemd unit to hold GPIO high for HAT bus power
```

---

## Findings Summary

| Finding | Summary |
|--------:|---------|
| 1 | All meter data readable by any M-Bus master, no credentials required |
| 2 | Complete device enumeration in ~90 seconds, no prior knowledge required |
| 3 | Meter addresses can be permanently changed without credentials |
| 4 | Two masters on the same bus causes immediate total bus failure |
| 5 | Rogue slave can impersonate a real meter, fabricated readings accepted by master |

Full details: [FINDINGS_REPORT.md](FINDINGS_REPORT.md)

---

## Reproducing This

### Hardware

The minimum setup to replicate findings 1-4:
- A Raspberry Pi (any model with a 40-pin header) or any Linux machine with a serial port
- A USB-to-M-Bus adapter or the packom.net HAT
- Any EN 13757-compliant wired M-Bus meter

For finding 5 (rogue slave), a second USB-to-M-Bus adapter is needed.

### Software

Build libmbus from source:

```bash
git clone https://github.com/rscada/libmbus
cd libmbus
./build.sh
sudo make install
sudo ldconfig
```

Build the rogue slave (finding 5):

```bash
gcc scripts/mbus-rogue-slave.c -lmbus -lm -o mbus-rogue-slave
```

### Serial Device

- packom.net HAT: `/dev/ttyAMA0` (disable Bluetooth UART conflict: `dtoverlay=disable-bt` in `/boot/firmware/config.txt`)
- USB adapter: `/dev/ttyUSB0` (or check `dmesg` after plugging in)

### Bus Power (packom HAT only)

The HAT does not automatically power the M-Bus bus. Hold GPIO 26 high before scanning:

```bash
gpioset -c /dev/gpiochip0 -z 26=1 &
```

---

## Important Caveats

- **Single-master constraint:** M-Bus is a single-master protocol. If a legitimate master (BMS/head-end) is already on the bus, connecting a second master causes immediate bus failure. Active attacks (findings 3 and 5) require the legitimate master to be offline.
- **Access counter forensics:** Interesting. The Kamstrup MULTICAL 602 increments its `AccessNumber` field on every read. Activity is recorded on the meter itself.
- **Protocol, not vendor:** These findings are consistent with EN 13757-2:2018 as written. They are not vendor implementation flaws.

---

## Licence

Code in `scripts/`: MIT  
Documentation: [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/)
