# M-Bus Security Lab — Claude Code Agent Tasklist

**Context:** Bealers is setting up a wired M-Bus security research lab. The full research plan and fact-checked technical details are in `MBUS_SECURITY_RESEARCH_PLAN.md` — read that first for background.

**Hardware on the bench:**
- Raspberry Pi + packom.net M-Bus Master HAT (primary research platform)
- 1 Kamstrup MULTICAL 602 heat meter (second-hand, wired M-Bus) probes are free to insert anywhere
- 1 Zenner Zelsius mbus meter which looks to have a usb flow simulator or similar, it lights up when plugged into usb 
- Danfoss M-Bus master (reserve, for bus contention tests later)
- "Temu" USB-to-M-Bus adapter (reserve)
- Windows PC available for IZAR Centre (no licence yet — park this)


**Bealers prefers:** vim, Arch Linux (but Pi is likely Raspbian/Pi OS), markdown for docs, minimal hand-holding — he knows Linux and networking.

---

## Phase 0: Pi Access & Connectivity

- [ ] Determine Pi's current IP (check UniFi dashboard or router DHCP leases for the Pi's MAC)
- [ ] SSH into Pi — confirm access works
- [ ] Generate SSH key pair on Bealers' workstation if needed, deploy public key to Pi `~/.ssh/authorized_keys`
- [ ] Set a static IP or DHCP reservation for the Pi on the Siftware VLAN (e.g. `10.0.10.xxx`)
- [ ] Optionally add DNS entry in AdGuard: `mbus-lab.siftware.dev` → Pi IP
- [ ] Confirm Pi serial port situation:
  - Check `/dev/ttyAMA0` and `/dev/serial0` exist
  - Disable Bluetooth competing for UART: `dtoverlay=disable-bt` in `/boot/config.txt` (or `/boot/firmware/config.txt` on newer Pi OS)
  - `sudo systemctl disable hciuart`
  - Disable serial console: `sudo raspi-config` → Interface Options → Serial Port → No login shell, Yes hardware enabled
  - Reboot and confirm `/dev/ttyAMA0` is free

## Phase 1: Install Tools

- [ ] Install build dependencies:
  ```bash
  sudo apt-get update
  sudo apt-get install -y autoconf libtool build-essential git python3 python3-pip
  ```
- [ ] Build and install libmbus:
  ```bash
  cd ~
  git clone https://github.com/rscada/libmbus
  cd libmbus
  ./build.sh
  sudo make install
  sudo ldconfig
  ```
- [ ] Verify tools are in PATH:
  ```bash
  which mbus-serial-scan
  mbus-serial-scan  # should print usage, not "not found"
  ```
- [ ] Create lab working directory:
  ```bash
  mkdir -p ~/mbus_lab/{phase0,phase1,phase2/{sniffing,enumeration,injection,dos},phase3}
  ```

## Phase 2: Wire & Scan First Meter

- [ ] Connect one MULTICAL 402 to the HAT's M-Bus terminals (2-wire, polarity irrelevant)
- [ ] Keep cable short (<1m)
- [ ] Power the meter (battery or mains module — whatever it came with)
- [ ] Wait 30 seconds for M-Bus module to initialise
- [ ] Start a terminal log: `script -a ~/mbus_lab/phase0/bringup.txt`
- [ ] Primary scan:
  ```bash
  mbus-serial-scan -b 2400 /dev/ttyAMA0
  ```
- [ ] If nothing found, try 300 and 9600 baud
- [ ] If still nothing, try `/dev/serial0` or check `ls /dev/tty*` for the correct device
- [ ] On success, read meter data:
  ```bash
  mbus-serial-request-data -b 2400 /dev/ttyAMA0 <address> | tee ~/mbus_lab/phase1/meter_baseline.xml
  ```
- [ ] Capture debug hex:
  ```bash
  mbus-serial-request-data -d -b 2400 /dev/ttyAMA0 <address> 2>&1 | tee ~/mbus_lab/phase1/meter_baseline_hex.txt
  ```
- [ ] Secondary address scan:
  ```bash
  mbus-serial-scan-secondary -b 2400 /dev/ttyAMA0 | tee ~/mbus_lab/phase1/secondary_scan.txt
  ```
- [ ] Record meter identity: primary addr, secondary addr, serial number, manufacturer ID, current readings, access counter value
- [ ] If second meter available, wire it in parallel on same bus and repeat scan — both should appear

## Phase 3: Attack Tests

**Do these in order. Restore original state after each test.**

### 3A: Passive Sniffing
- [ ] Poll meter, save full XML output
- [ ] Annotate what sensitive data is visible in plaintext (meter ID, consumption, temperatures, timestamps)
- [ ] Document: no encryption observed, all data readable

### 3B: Enumeration
- [ ] Time a full primary scan (addresses 0-250): `time mbus-serial-scan -b 2400 /dev/ttyAMA0`
- [ ] Time a full secondary scan: `time mbus-serial-scan-secondary -b 2400 /dev/ttyAMA0`
- [ ] Document: complete device discovery without credentials

### 3C: Command Injection (SND_UD)
- [ ] Note access counter BEFORE
- [ ] Change primary address:
  ```bash
  mbus-serial-set-address -b 2400 /dev/ttyAMA0 <old> <new>
  ```
- [ ] Verify with scan — meter should appear at new address
- [ ] **Restore original address immediately**
- [ ] Note access counter AFTER — did it change?
- [ ] Change baud rate:
  ```bash
  mbus-serial-switch-baudrate -b 2400 /dev/ttyAMA0 <address> 9600
  ```
- [ ] Verify: scan at 2400 fails, scan at 9600 succeeds
- [ ] **Restore to 2400 immediately**
- [ ] Document: unauthenticated configuration changes accepted

### 3D: Bus DoS (needs second master)
- [ ] Write a simple polling loop on Pi (e.g. read every 10s, log success/fail)
- [ ] Connect second master (Temu USB or Danfoss) to same bus
- [ ] Run continuous scan from second master simultaneously
- [ ] Monitor Pi polling — count failures/timeouts
- [ ] Remove second master, confirm recovery
- [ ] Document: percentage of disrupted reads, recovery time

## Phase 4: Evidence & Write-Up

- [ ] Hash all evidence files:
  ```bash
  find ~/mbus_lab -type f -exec sha256sum {} \; > ~/mbus_lab/evidence_hashes.txt
  ```
- [ ] For each attack: hypothesis, method, commands used, expected vs actual result, evidence files
- [ ] Photos of lab setup, meter displays, wiring

---

## Troubleshooting

**No devices found on scan:**
- Wrong serial device — `ls /dev/tty*` and try alternatives
- Bluetooth still on UART — check `dtoverlay=disable-bt` is applied, reboot
- Serial console still enabled — disable via raspi-config
- Wrong baud rate — try all three (300, 2400, 9600)
- Wiring issue — M-Bus is polarity-independent but check connections are solid
- Meter M-Bus module not fitted — open the MULTICAL 402 and check the module slot
- Module needs time — wait 30s after connecting before scanning

**"Collision at address X" errors:**
- Electrical noise on bus — shorten cable, check connections
- Multiple meters on same primary address — use secondary addressing
- Dodgy level converter — try different master

**"error while loading shared libraries: libmbus.so.0":**
- Run `sudo ldconfig` after `make install`
