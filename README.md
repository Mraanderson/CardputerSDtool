# Cardputer‑ADV SD Card Tool (Prototype)

A lightweight, work‑in‑progress SD diagnostics utility for the **M5Stack Cardputer‑ADV (StampS3A)**.  
This tool runs entirely on‑device — no PC required — and aims to provide practical, real‑world SD card testing and formatting capabilities tailored to the Cardputer‑ADV’s unique SPI behaviour.

This project is **experimental**. Expect quirks, incomplete features, and behaviour that varies by SD card brand, age, and filesystem.

---

## ✨ Overview

The Cardputer‑ADV SD Tool provides:

- SD card information (manufacturer, product name, capacity)
- Filesystem detection (FAT32, FAT16, exFAT, Unknown)
- Raw CID field display for advanced users
- Speed test (simple write/read benchmark)
- Integrity check (H2TestW‑style 50MB write/verify)
- Quick format (SdFat‑based quick format + remount)
- Keyboard‑driven UI designed for the Cardputer‑ADV

The goal is to build a **portable SD diagnostics suite** that helps users understand card health, performance, and compatibility directly from the device.

---

## 📊 Feature Status

| Feature               | Status        | Notes                                                   |
|-----------------------|---------------|---------------------------------------------------------|
| SD Card Information   | 🟢 Stable     | Manufacturer lookup, PNM, capacity, CID fields          |
| Filesystem Detection  | 🟡 Needs testing | exFAT depends on SdFat configuration                    |
| Speed Test            | 🟢 Stable     | Occasional freezes; may require device reset            |
| Integrity Check       | 🟢 Stable     | Slow; no progress bar; fixed 50MB test size             |
| Quick Format          | 🟡 Needs testing | SdFat quick format + remount; re‑init can be flaky      |
| Navigation / UI       | 🟢 Stable     | Scroll speed may feel fast                              |
| Reboot                | 🟢 Stable     |                                                         |
| SPI Stability         | 🟡 Uncertain  | Varies by card brand, age, and controller behaviour     |
| Card Health Metrics   | ⚪ Not implemented | CSD/SCR parsing planned                                 |

**Legend:**  
🟢 Stable 🟡 Needs testing 🔴 Known issues ⚪ Not implemented

---

## 📌 Current Status

**Prototype — not fully validated.**

Some features behave consistently across multiple cards; others depend heavily on:

- Card brand and controller
- Card age and wear
- Filesystem type (FAT32 vs exFAT)
- SPI bus stability on the Cardputer‑ADV
- SdFat configuration and caching behaviour

This repository is intended for experimentation, testing, and community feedback.

---

## 🧩 Features in Detail

### **SD Card Information**
- Manufacturer lookup (SanDisk, Samsung, Kingston, Lexar, Phison, SP, etc.)
- Product name (PNM)
- Capacity in MB
- Filesystem detection
- Raw CID fields (MID, OID, PNM, PRV, PSN)

### **Speed Test**
- Writes a 5MB file in 4096‑byte blocks
- Reads it back
- Reports write/read MB/s
- Useful for spotting failing or counterfeit cards

### **Integrity Check**
- Writes 50MB of patterned data
- Verifies every 512‑byte block
- Reports PASS/FAIL and error count
- Inspired by H2TestW / F3

### **Quick Format**
- SdFat quick format
- Spinner animation
- Automatic SD remount
- Filesystem detection after format

### **Navigation**
- `;` → Up  
- `.` → Down  
- `ENTER` → Select  
- `BACKSPACE` → Return to menu  

---

## ⚠ Known Issues

- Speed test may fail to re‑initialise SD after formatting
- exFAT detection depends on SdFat build options  
- No progress bar for long operations  
- No SPI auto‑speed fallback  
- No card health metrics (erase block size, CSD/SCR parsing)  
- Some SD cards require additional settle time after raw writes  

---

## 🛠 Build Instructions (PlatformIO)

1. Install PlatformIO (VSCode recommended)
2. Clone this repository
3. Open the folder in VSCode
4. Build & upload using **PlatformIO: Upload**

---

## 🤝 Contributions

Contributions, bug reports, and test results are welcome.

Testing across a wide range of SD cards — especially older, slower, or off‑brand models — is extremely valuable.  
Pull requests for new diagnostics, UI improvements, or SPI stability fixes are encouraged.

---

## 🗺 Roadmap (Planned)

- CSD/SCR parsing for card health metrics  
- More robust SPI fallback logic  
- Progress bars for long operations  
- Extended integrity test options  
- exFAT read‑only inspection  
- Optional full‑card wipe / zero‑fill  
