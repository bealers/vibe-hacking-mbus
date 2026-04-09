# Phase 1 Summary: Baseline Documentation

**Date:** 2026-04-09  
**Operator:** Bealers  
**Platform:** Raspberry Pi + packom.net M-Bus Master HAT  

---

## What We Did

Connected two physical meters to the HAT's M-Bus terminals and used libmbus to perform a full primary scan, read all meter data, and perform a secondary address scan. All data was saved to `~/mbus_lab/phase1/` on the Pi.

---

## Devices Found

Five devices responded on the bus at 2400 baud:

| Primary Addr | Secondary Address        | Manufacturer | Medium            | Serial   | Key Reading |
|-------------|--------------------------|--------------|-------------------|----------|-------------|
| 0           | `10257088496A8804`       | ZRI (Zenner) | Heat: Outlet      | 10257088 | 378,105 kWh |
| 11          | `01257088496A8807`       | ZRI (Zenner) | Water             | 1257088  | 7,975 m³    |
| 12          | `02257088496A8806`       | ZRI (Zenner) | Warm water        | 2257088  | 747 m³      |
| 13          | `03257088496A8800`       | ZRI (Zenner) | Other             | 3257088  | 58 m³       |
| 206         | `781802062D2C0F04`       | KAM (Kamstrup) | Heat: Outlet    | 78180206 | 19 kWh      |

**Note:** The Zenner Zelsius presents as four virtual sub-meters (addresses 0, 11, 12, 13) via its USB flow simulator — this is expected behaviour, not four physical devices.

### Kamstrup MULTICAL 602 (address 206) — full data exposed in plaintext:
- Serial: 78180206
- Energy: 19 kWh (lifetime), 253 kWh (sub-device 3)
- Volume: 20.45 m³
- Flow temperature: 20.30°C, Return: 19.99°C, ΔT: 0.31°C
- On time: 5,954 hours
- Access number at baseline: **1** (increments on each read — forensic trace)
- Billing date snapshot: 2020-12-31
- No encryption, no authentication. Full configuration and metrological data readable by any M-Bus master.

---

## Evidence Files (on Pi: `~/mbus_lab/phase1/`)

| File | Contents |
|------|----------|
| `meter_addr0_baseline.xml` | Zenner Heat: Outlet — XML |
| `meter_addr0_baseline_hex.txt` | Zenner Heat: Outlet — raw debug hex |
| `meter_addr11_baseline.xml` | Zenner Water — XML |
| `meter_addr11_baseline_hex.txt` | Zenner Water — raw debug hex |
| `meter_addr12_baseline.xml` | Zenner Warm Water — XML |
| `meter_addr12_baseline_hex.txt` | Zenner Warm Water — raw debug hex |
| `meter_addr13_baseline.xml` | Zenner Other — XML |
| `meter_addr13_baseline_hex.txt` | Zenner Other — raw debug hex |
| `meter_addr206_baseline.xml` | Kamstrup MULTICAL 602 — XML (full dataset, 27 records) |
| `meter_addr206_baseline_hex.txt` | Kamstrup MULTICAL 602 — raw debug hex |
| `secondary_scan.txt` | Full secondary address scan output |

---

## Gotchas & Notes

### 1. GPIO 26 must be held high to enable 36V bus power
The packom HAT does not automatically power the M-Bus bus. Bus power is controlled via **GPIO 26 on `/dev/gpiochip0`**. This is not persistent across reboots.

**To enable the bus (must be done before every scan/read session):**
```bash
gpioset -c /dev/gpiochip0 -z 26=1 &
```
The `-z` flag keeps the process alive in the background holding the line high. The "BUS" LED on the HAT illuminates when this is active. Without this, all scans return empty.

**To make it persistent across reboots**, add a systemd service or run it from `/etc/rc.local`. Quick one-liner for rc.local:
```bash
echo "gpioset -c /dev/gpiochip0 -z 26=1 &" | sudo tee -a /etc/rc.local
```

### 2. Wiring — Kamstrup connection was loose on first attempt
Initial scan returned nothing. One wire on the Kamstrup terminal was not fully seated. M-Bus is polarity-independent but the connection must be solid. After reseating, both meters were found immediately.

### 3. Serial console was occupying /dev/ttyAMA0
The Pi's boot cmdline had `console=serial0,115200` which caused systemd to spawn `serial-getty@ttyAMA0`, occupying the port. Fixed by removing the console entry from `/boot/firmware/cmdline.txt` and rebooting.

### 4. Bluetooth UART conflict
`dtoverlay=disable-bt` added to `/boot/firmware/config.txt` and `hciuart` disabled to free `/dev/ttyAMA0` from Bluetooth competition.

### 5. GPIO chip numbering on newer Pi OS
On Pi OS with kernel 6.12, the legacy sysfs GPIO interface (`/sys/class/gpio/export`) does not work. Use `gpiod` tools instead. The Pi GPIO chip is `/dev/gpiochip0` (not `gpiochip512` as suggested by `/sys/class/gpio/`).

### 6. Access counter increments on every read
The Kamstrup's `AccessNumber` field increments with each successful M-Bus read. This is a forensic artefact — an attacker's polling activity would be detectable by examining access counter history via the meter's front panel or manufacturer software.

---

## Commands Used

```bash
# Enable bus power (required each session)
gpioset -c /dev/gpiochip0 -z 26=1 &

# Primary scan
mbus-serial-scan -b 2400 /dev/ttyAMA0

# Read meter data
mbus-serial-request-data -b 2400 /dev/ttyAMA0 <address>

# Read meter data with debug hex
mbus-serial-request-data -d -b 2400 /dev/ttyAMA0 <address>

# Secondary address scan (no credentials required)
mbus-serial-scan-secondary -b 2400 /dev/ttyAMA0
```
