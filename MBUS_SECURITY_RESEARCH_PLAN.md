# M-Bus Security Lab — Combined Research & Test Plan

**Date:** 2026-04-09 | **Author:** Bealers / Siftware Ltd
**Purpose:** Validate wired M-Bus security vulnerabilities in a controlled lab environment, building towards HNTAS consulting capability.

---

## 1. Fact-Check of Original Research

### Claims That Check Out ✓

**Wired M-Bus has no encryption or authentication on the bus.**
Confirmed. EN 13757-2 (physical/link layer) and EN 13757-3 (application layer) for *wired* M-Bus define no security mechanisms beyond a simple checksum. The 2013 Black Hat whitepaper by Cyrill Brunschwiler (Compass Security) analysed M-Bus security and found fundamental weaknesses in confidentiality, integrity, and authentication. The OMS (Open Metering System) specification has since added optional AES-128 encryption modes (Mode 5, 7, etc.) and an Authentication and Fragmentation Layer (AFL), but these apply primarily to *wireless* M-Bus. Most deployed wired M-Bus meters — especially older Kamstrup MULTICAL 402s — do not implement any of this.

**Passive sniffing is trivial.**
Confirmed. The bus is a shared two-wire medium. Master-to-slave communication uses voltage signalling (36V idle, 24V mark), slave-to-master uses current modulation. Any device connected to the bus can observe all traffic. Data is transmitted in plaintext including meter IDs, consumption values, and timestamps.

**SND_UD commands are unauthenticated.**
Confirmed. The M-Bus link layer defines SND_UD (Send User Data) as a master-to-slave command. The slave confirms receipt with a single-byte ACK (0xE5). There is no authentication challenge — the slave will accept SND_UD from any device that can generate valid voltage signalling on the bus. The Kamstrup MULTICAL 402 M-Bus module datasheet confirms that primary address, M-Bus ID number, date/time, and pulse inputs can all be programmed via the M-Bus network using SND_UD.

**Secondary address wildcard scanning works.**
Confirmed. libmbus (rscada/libmbus on GitHub) implements `mbus-serial-scan-secondary` which performs binary-search scanning using wildcard characters in the 8-byte secondary address. This efficiently enumerates all devices on a bus without needing to know their addresses in advance.

**Kamstrup MULTICAL 402 is a good test meter.**
Confirmed. It's a standard EN 13757-compliant M-Bus slave with well-documented protocol. The wired M-Bus module supports primary/secondary/enhanced secondary addressing, 300/2400/9600 baud with auto-detection, and is widely available second-hand (discontinued product). Kamstrup publish detailed technical descriptions of the M-Bus data packets.

**Diehl IZAR Centre works as described.**
Confirmed. The IZAR CENTER is an M-Bus master/level converter supporting 60/120/250 slaves, with USB, RS232, and LAN interfaces. It polls M-Bus slaves on schedule and stores data for IZAR@NET 2 software.

**libmbus installation and commands.**
Confirmed. The GitHub repo (rscada/libmbus) contains `mbus-serial-scan`, `mbus-serial-request-data`, `mbus-serial-switch-baudrate`, `mbus-serial-set-address` and others. The build process uses autoconf/libtool. The Pi serial device for the packom.net HAT would be `/dev/ttyAMA0` (or `/dev/serial0` depending on Pi model and config).

### Claims That Need Correction or Caveats ⚠

**Axioma Qalcosonic W1 "integrated motorized ball valve"**
**NOT CONFIRMED.** I could not find any evidence that the standard Qalcosonic W1 has an integrated motorized ball valve. The W1 is a water *meter* — it measures flow using ultrasonic transit-time, it doesn't control it. The W1 datasheet mentions an optional "check valve" (a passive backflow prevention device) and a "strainer", neither of which are remotely actuated. The W1 also uses *wireless* M-Bus (868 MHz) and LoRaWAN — not wired M-Bus — so it wouldn't be on your wired bus anyway.

There *are* smart water meters with integrated shut-off valves (e.g., Kamstrup flowIQ 2200 with motorised valve, various Chinese manufacturers), but the Qalcosonic W1 doesn't appear to be one of them. **You should verify this claim before including it in any client-facing material.** If you want a valve demo, you'd likely need a separate motorised ball valve actuated by a relay/controller on the M-Bus, or a different meter entirely.

**"Temu master" — described as working "exactly like the Pi HAT"**
Partially confirmed. Cheap USB-to-M-Bus adapters based on TSS721A or FC722 chips do exist and do function as M-Bus masters (they generate bus voltage and can send/receive frames). However, quality varies enormously. Some are electrically noisy, some can't power enough slaves, and some have timing issues. Don't assume they'll work identically to the packom.net HAT without testing. They're fine as a *second* master for bus contention demos, but I wouldn't rely on one as your primary research tool.

**"CI=0x51 with valve close command"**
I can't verify this specific CI field assignment for valve control from the public EN 13757-3 documentation available to me. CI field 0x51 is in the range used for application layer data, but the exact valve control semantics would be device-specific. The claim about finding it in the Axioma APK decompilation might be accurate for a *different* Axioma product (perhaps the F1 or E2 which have wired M-Bus options), but since the W1 valve claim doesn't check out, this whole section needs re-examination.

**IZAR@NET 2 — do you need a licence?**
The IZAR@NET 2 software is a commercial product from Diehl Metering. It's modular and licensed per-module. The base IZAR@NET software includes an Oracle Express database. **You would need a licence to use it legitimately.** However, for your lab purposes, the Izar Centre itself can be used without IZAR@NET — you can read meters via USB/serial directly using libmbus. The value of IZAR@NET would be to show the *victim's view* (a commercial system being corrupted), but it's not essential for the security research itself.

**Bus voltage levels: "36V idle, 24V for logic 0"**
The M-Bus spec (EN 13757-2) defines the idle/space state as approximately 36V and the mark/active state as approximately 24V. Your doc says "24V for logic 0" — this is roughly correct but note that M-Bus uses UART-style framing where the mark state (lower voltage, ~24V) represents a logical 1 in UART terms, and space (~36V) represents logical 0. This is sometimes confusingly described in different sources. For your purposes the exact logic mapping doesn't matter much — what matters is that the voltage swing is observable and reproducible.

---

## 2. Is This Worth Doing? Honest Assessment

**Yes, but scope it correctly.**

The core thesis is sound: wired M-Bus has no authentication, no encryption, and minimal integrity checking on a shared physical medium. Any device with physical access to the two-wire bus can sniff, inject, and disrupt. This is a real vulnerability in real building infrastructure.

**What will definitely work:**
- Passive sniffing of all meter data (occupancy patterns, consumption, meter IDs)
- Independent scanning/enumeration of all meters with a second master
- Sending SND_UD commands to change primary addresses, reset counters
- Bus contention/DoS by operating a second master simultaneously
- All of the above with off-the-shelf tools (libmbus, Pi + HAT)

**What might be harder than the docs suggest:**
- Rogue slave injection is more complex than just spoofing an address. M-Bus slaves respond to master polling with current modulation. You'd need precise timing to respond within the master's timeout window *and* suppress or beat the real slave's response. The Arduino + TSS721A approach is plausible but will need careful firmware work.
- Showing data corruption in IZAR@NET 2 requires the software licence. Without it, you'd need an alternative way to show the "victim's view."
- The valve control demo needs a completely different meter than what's described.

**What makes it valuable:**
- Almost nobody is doing this. The M-Bus security space is virtually unresearched for wired deployments — the only major academic work is the 2013 Black Hat paper on *wireless* M-Bus.
- The HNTAS angle is real — ~14,000 UK heat networks need compliance assessment, and security of metering infrastructure is relevant.
- Physical demonstrations are powerful for consulting — if you can show address hijacking or data corruption in front of a client, that's compelling.

**The risk:**
- If the meters you buy happen to have newer firmware with some vendor-specific protections, your attacks might not all work out of the box. Kamstrup has been adding features like access counters and FCB violation detection.
- You might end up with a "yes, we already know the bus is insecure, that's why we lock the cupboard" response from clients. The value is in *quantifying* what an attacker with brief physical access can do, and how quickly.

---

## 3. Equipment Decision

**Use the Raspberry Pi + packom.net HAT as your primary research platform.**

Reasons:
- Linux-based, so libmbus, Python scripting, and logging all work natively
- You can script and automate everything, making results reproducible
- Logs everything with timestamps for evidence
- You're comfortable with Pi and Linux

Keep the Danfoss and Temu masters for later — specifically for the dual-master bus contention demo (Attack 5 in your lab guide). Don't complicate the initial bring-up with multiple masters.

**Hold off on the Izar Centre + IZAR@NET for now.** Unless you already have the software licence or can get a trial, don't spend money on it yet. Get the Pi reading meters first. You can add the "victim's view" layer later.

---

## 4. Simulating Consumption (The Flow Problem)

You're right that this is the hard part. A MULTICAL 402 is an *ultrasonic flow meter with temperature sensors*. It measures:
- Volume of water passing through the flow sensor (ultrasonic transit-time)
- Temperature difference between inlet and outlet (Pt500 sensors)
- Calculates energy from V × ΔΘ × k

**For your security research, you probably don't need real flow.**

Here's why: the M-Bus attacks target the *communication layer*, not the measurement layer. You don't need the meter to be measuring real energy — you need it to be powered up, responding on M-Bus, and reporting *something*. A MULTICAL 402 that's powered on with no flow will still:
- Respond to SND_NKE (link reset)
- Respond to REQ_UD2 (data request) with its current readings (which will be zero flow but will include accumulated historical data, temperatures, info codes, etc.)
- Accept SND_UD commands
- Have a valid primary and secondary address

**What you need for each attack:**

| Attack | Need Flow? | Why |
|--------|-----------|-----|
| 1. Passive sniffing | No | Just need meter responding with *any* data |
| 2. Independent harvest | No | Scanning/reading works regardless of flow state |
| 3. SND_UD injection | No | Address change, baud rate change, application reset work without flow |
| 4. Rogue slave | No | You're spoofing responses, not real measurements |
| 5. Bus DoS | No | Flooding doesn't need flow |

**If you want non-zero readings for more realistic demos:**

The MULTICAL 402 retains historical data in its loggers (monthly, yearly, hourly depending on config). A second-hand meter from a decommissioned installation will likely have years of historical data still in memory. That's actually *better* for your sniffing demo — you'd be showing real consumption patterns from a real building, which illustrates the privacy risk more powerfully than synthetic data.

**If you really want to generate flow:**

The hot tea / ice drink idea for temperature differential is creative but won't help with *flow*. The MULTICAL 402 flow sensor is an inline ultrasonic device — water needs to actually pass through the pipe. You'd need:
- A small closed-loop circuit with a pump (aquarium/fountain pump, ~£10-20)
- The flow sensor plumbed inline
- Temperature sensors in the water

This is a plumbing project, not an electronics project, and it's not worth it for the security research. Save it for if you ever need to demo the meter working properly.

---

## 5. Test Plan

### Phase 0: Hardware Bring-Up (Day 1)

**Goal:** Confirm the Pi + HAT can communicate with at least one meter.

**Steps:**

1. **Pi preparation**
   - Ensure the packom.net M-Bus Master HAT is seated correctly
   - Identify the serial device: likely `/dev/ttyAMA0` or `/dev/serial0`
   - Disable Bluetooth on the Pi if it's competing for the UART (common on Pi 3/4): add `dtoverlay=disable-bt` to `/boot/config.txt`
   - `sudo systemctl disable hciuart`

2. **Install libmbus**
   ```bash
   sudo apt-get update
   sudo apt-get install autoconf libtool build-essential git
   git clone https://github.com/rscada/libmbus
   cd libmbus
   ./build.sh
   sudo make install
   sudo ldconfig  # Important - missing from your original doc
   ```

3. **Wire one meter**
   - Connect MULTICAL 402 M-Bus module terminals to the HAT's M-Bus output (2-wire, polarity doesn't matter on M-Bus)
   - Keep cable short (<1m for initial test)
   - Power the MULTICAL 402 (battery or mains module, depending on what you got)
   - Wait 30 seconds after connecting M-Bus cable (Kamstrup docs say the module needs time to read meter data)

4. **First scan**
   ```bash
   # Primary address scan at 2400 baud (M-Bus default)
   mbus-serial-scan -b 2400 /dev/ttyAMA0
   ```
   Expected: "Found a M-Bus device at address N" where N is the meter's primary address (often 0 or 1 on unconfigured meters).

   If nothing found, try:
   ```bash
   # Try all baud rates
   mbus-serial-scan -b 300 /dev/ttyAMA0
   mbus-serial-scan -b 9600 /dev/ttyAMA0
   ```

5. **First data read**
   ```bash
   mbus-serial-request-data -b 2400 /dev/ttyAMA0 <address>
   ```
   This should return XML with meter data. Save this output — it's your baseline.

6. **Secondary address scan**
   ```bash
   mbus-serial-scan-secondary -b 2400 /dev/ttyAMA0
   ```
   This will find the meter's unique secondary address (8 bytes: serial number + manufacturer + version + device type).

**Success criteria:** You can scan and read data from at least one meter. If this doesn't work, stop and debug before proceeding.

**Document everything:**
```bash
mkdir -p ~/mbus_lab/phase0
script -a ~/mbus_lab/phase0/terminal_log.txt  # Record terminal session
```

### Phase 1: Baseline Documentation (Day 1-2)

**Goal:** Create a complete, documented baseline of normal bus operation.

1. **Record each meter's identity**
   - Primary address
   - Secondary address (full 8 bytes)
   - Manufacturer ID
   - Serial number (from display and from M-Bus data)
   - Firmware/version info
   - Current accumulated energy, volume readings
   - Access counter value (Kamstrup meters have this — note it, it may increment with each M-Bus read)

2. **Photograph everything**
   - Each meter's display showing serial number
   - Wiring setup
   - Lab layout
   - RED TAPE isolation markings

3. **Capture raw hex of all communications**
   ```bash
   # Use -d flag for debug output showing hex frames
   mbus-serial-request-data -d -b 2400 /dev/ttyAMA0 <address> \
     2>&1 | tee ~/mbus_lab/phase1/meter_<serial>_baseline.txt
   ```

4. **Save baseline data as XML**
   ```bash
   mbus-serial-request-data -b 2400 /dev/ttyAMA0 <address> \
     > ~/mbus_lab/phase1/meter_<serial>_baseline.xml
   ```

5. **If you have a second meter, repeat for both and document the two-meter bus topology.**

### Phase 2: Attack Testing (Day 2-4)

Work through these in order. Each attack builds confidence and evidence.

#### Attack 2A: Passive Data Capture

**Hypothesis:** All meter data is transmitted in plaintext and can be captured by any device on the bus.

**Method:**
1. Use the Pi to poll meter(s) normally
2. Log all raw frames (hex) to file
3. Parse the captured data, showing:
   - Meter serial numbers
   - Accumulated energy/volume
   - Current temperatures
   - Current flow rate
   - Info codes / error states
   - Timestamps

**Evidence to collect:**
- Raw hex capture file
- Parsed XML showing readable data
- Annotated hex dump highlighting sensitive fields (meter ID, consumption values)

**Expected result:** 100% of data readable in plaintext. No encryption.

#### Attack 2B: Independent Enumeration

**Hypothesis:** An unauthorised device can discover all meters on the bus without any credentials.

**Method:**
1. Primary scan: `mbus-serial-scan` (brute-force addresses 0-250)
2. Secondary scan: `mbus-serial-scan-secondary` (wildcard binary search)
3. For each discovered address, perform `mbus-serial-request-data` to read full data

**Evidence to collect:**
- Complete list of discovered devices with addresses
- Full data dump from each
- Time taken for complete enumeration

**Expected result:** All meters discovered and all data read.

#### Attack 2C: Command Injection (SND_UD)

**Hypothesis:** Unauthenticated commands can modify meter configuration.

**This is the most important attack — proceed carefully and document meticulously.**

**Method:**

*Step 1: Change primary address*
```bash
# Record current address
mbus-serial-scan -b 2400 /dev/ttyAMA0

# Change primary address (e.g., from 1 to 5)
mbus-serial-set-address -b 2400 /dev/ttyAMA0 1 5

# Verify change
mbus-serial-scan -b 2400 /dev/ttyAMA0
# Meter should now appear at address 5, not 1
```

*Step 2: Restore original address*
```bash
mbus-serial-set-address -b 2400 /dev/ttyAMA0 5 1
```

*Step 3: Change baud rate*
```bash
# Switch from 2400 to 9600
mbus-serial-switch-baudrate -b 2400 /dev/ttyAMA0 <address> 9600

# Verify — old baud rate should fail, new should work
mbus-serial-scan -b 2400 /dev/ttyAMA0   # Should NOT find it
mbus-serial-scan -b 9600 /dev/ttyAMA0   # SHOULD find it

# Restore
mbus-serial-switch-baudrate -b 9600 /dev/ttyAMA0 <address> 2400
```

*Step 4: Application reset (if supported)*
```bash
# This resets the M-Bus module's application layer
# On Kamstrup, this makes the module re-read meter data
# On some meters, this can reset counters — check behaviour carefully
```

**Evidence to collect:**
- Terminal logs showing successful address change (before/after scans)
- Terminal logs showing successful baud rate change
- Access counter values before and after (did they increment? Is there any audit trail?)
- Any info codes triggered on the meter display

**IMPORTANT:** After each modification, *restore the original setting* before proceeding. Note the Kamstrup access counter — if it increments on SND_UD commands, that's actually a partial detection mechanism worth documenting.

#### Attack 2D: Bus Contention / DoS

**Hypothesis:** A second master on the bus can disrupt normal polling.

**Method:**
This is where you bring in the second master (Temu USB adapter or Danfoss).

1. Set up Pi polling meter(s) on a regular interval (e.g., every 10 seconds via a script)
2. Connect second master to same bus
3. Have second master continuously scan or send frames
4. Monitor Pi's polling success rate — count timeouts, errors, corrupted frames

**Evidence to collect:**
- Log of successful reads before introducing second master
- Log showing failures/timeouts during contention
- Percentage of failed reads
- Time to recover after removing second master

**Expected result:** Significant disruption to legitimate polling. Possible collision errors.

### Phase 3: Evidence Packaging & Analysis (Day 4-5)

1. **Organise evidence directory:**
   ```
   mbus_lab/
   ├── phase0_bringup/
   │   ├── terminal_log.txt
   │   └── photos/
   ├── phase1_baseline/
   │   ├── meter_<serial>_baseline.xml
   │   ├── meter_<serial>_baseline_hex.txt
   │   └── photos/
   ├── phase2_attacks/
   │   ├── 2a_sniffing/
   │   ├── 2b_enumeration/
   │   ├── 2c_injection/
   │   └── 2d_dos/
   └── phase3_analysis/
       ├── findings_summary.md
       └── raw_evidence_index.md
   ```

2. **For each attack, write up:**
   - Hypothesis
   - Method (exact commands, timestamps)
   - Expected vs actual result
   - Evidence files (with SHA256 hashes for integrity)
   - Implications for real-world deployments
   - Relevant EN 13757 section

3. **Hash all evidence files:**
   ```bash
   find ~/mbus_lab -type f -exec sha256sum {} \; > ~/mbus_lab/evidence_hashes.txt
   ```

---

## 6. Things to Buy

| Item | Est. Cost | Priority | Notes |
|------|-----------|----------|-------|
| 1-2× Kamstrup MULTICAL 402 (used) | £20-60 each | **Essential** | Check eBay, decommissioned stock. Ensure M-Bus module is fitted |
| Twisted pair cable + terminal blocks | £5-10 | **Essential** | 2-3m of alarm cable or similar |
| Second USB M-Bus master (if not using Danfoss) | £8-15 | Phase 2D only | For bus contention test |

**Don't buy yet:**
- Arduino + TSS721A for rogue slave (Phase 2 work may not need it)
- Axioma W1 (valve claim unverified, wireless not wired)
- IZAR@NET licence (not needed for core research)

---

## 7. What to Watch Out For

- **Access counters:** Kamstrup meters may increment a counter on each M-Bus access. If you're doing this for a client demo, you need to know whether your testing leaves forensic traces on the meter.
- **FCB (Frame Count Bit):** Some meters track the FCB and may reject frames if it doesn't toggle correctly. libmbus handles this, but custom scripts might not.
- **Baud rate auto-detection:** The MULTICAL 402 M-Bus module supports 300/2400/9600 with auto-detection. If you change the baud rate and then can't communicate, try all three.
- **Pi serial port conflicts:** Bluetooth, serial console, and the M-Bus HAT may all want the same UART. Disable the others.
- **Bus power:** The M-Bus master supplies power to slaves via the bus (36V). The packom.net HAT may have a limited number of unit loads it can supply. Check the HAT specs — if you connect too many meters, voltage will droop.

---

## 8. What's NOT in This Plan (Yet)

- **Rogue slave injection** — Needs Arduino firmware development. Park it until Phase 2 attacks are proven.
- **Valve control demo** — The Axioma W1 claim doesn't hold up. Research alternative meters with actual M-Bus-controllable valves if you want this.
- **Wireless M-Bus** — A different (and arguably larger) attack surface, but different equipment and techniques. Your SDR research from February is relevant here.
- **IZAR@NET "victim view"** — Adds impact but needs licence investment. Consider alternatives: a simple Python script that polls meters on schedule and logs to CSV could serve the same narrative purpose.

---

## 9. Standards References

- EN 13757-2:2018 — Communication systems for meters: Wired M-Bus
- EN 13757-3:2018 — Communication systems for meters: Application layer
- EN 13757-7:2018 — Communication systems for meters: Transport and security
- EN 1434-3 — Heat meters, Part 3: Data exchange and interfaces
- OMS Specification Vol 2 — Primary Communication (includes security profiles)
