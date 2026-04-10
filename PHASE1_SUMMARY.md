# Phase 1 Summary: Baseline Documentation

**Date:** 2026-04-09  
**Operator:** Bealers (vibed/Claude)  
**Platform:** Raspberry Pi + packom.net M-Bus Master HAT  

---

## What We Did

Connected two physical meters to the HAT's M-Bus terminals and used libmbus to perform a full primary scan, read all meter data, and perform a secondary address scan.

---

## Devices Found

Five devices responded on the bus at 2400 baud:

| Primary Addr | Secondary Address  | Manufacturer   | Medium         | Serial   | Key Reading |
|-------------|-------------------|----------------|----------------|----------|-------------|
| 0           | `10257088496A8804` | ZRI (Zenner)   | Heat: Outlet   | 10257088 | 378,105 kWh |
| 11          | `01257088496A8807` | ZRI (Zenner)   | Water          | 1257088  | 7,975 m³    |
| 12          | `02257088496A8806` | ZRI (Zenner)   | Warm water     | 2257088  | 747 m³      |
| 13          | `03257088496A8800` | ZRI (Zenner)   | Other          | 3257088  | 58 m³       |
| 206         | `781802062D2C0F04` | KAM (Kamstrup) | Heat: Outlet   | 78180206 | 19 kWh      |

**Note:** The Zenner Zelsius is a real heat meter with a USB connector on the flow body, used here for development and testing. It presents as four sub-meters (addresses 0, 11, 12, 13), which is expected behaviour for this device.

### Kamstrup MULTICAL 602 (address 206)

Full dataset returned on first request. No encryption, no authentication. Selected fields:

- Serial: 78180206
- Energy: 19 kWh (lifetime), 253 kWh (sub-device 3)
- Volume: 20.45 m³
- Flow temperature: 20.30°C, Return: 19.99°C, delta-T: 0.31°C
- On time: 5,954 hours
- Access number at baseline: **1** (increments on each read)
- Billing date snapshot: 2020-12-31

---

## Setup Notes

### 1. GPIO 26 must be held high to enable 36V bus power

The packom HAT does not automatically power the M-Bus bus. Bus power is controlled via **GPIO 26 on `/dev/gpiochip0`**. This is not persistent across reboots.

**To enable the bus (required before every scan/read session):**
```bash
gpioset -c /dev/gpiochip0 -z 26=1 &
```
The `-z` flag keeps the process alive in the background holding the line high. The "BUS" LED on the HAT illuminates when this is active. Without this, all scans return empty.

**To persist across reboots**, use the included systemd service (`scripts/mbus-bus-power.service`) or add to `/etc/rc.local`:
```bash
echo "gpioset -c /dev/gpiochip0 -z 26=1 &" | sudo tee -a /etc/rc.local
```

### 2. Bluetooth UART conflict

`dtoverlay=disable-bt` added to `/boot/firmware/config.txt` and `hciuart` disabled to free `/dev/ttyAMA0` from Bluetooth competition.

### 3. GPIO chip numbering on newer Pi OS

On Pi OS with kernel 6.12, the legacy sysfs GPIO interface (`/sys/class/gpio/export`) does not work. Use `gpiod` tools instead. The Pi GPIO chip is `/dev/gpiochip0` (not `gpiochip512` as suggested by `/sys/class/gpio/`).

### 4. Access counter

The Kamstrup's `AccessNumber` field increments with each successful M-Bus read or write. Activity is recorded on the meter itself and persists after the session ends.

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

# Secondary address scan
mbus-serial-scan-secondary -b 2400 /dev/ttyAMA0
```
