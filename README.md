Cardputer‑ADV SD Card Tool (Prototype)
A work‑in‑progress SD diagnostics utility for the M5Stack Cardputer‑ADV (StampS3A).
This tool is under active development and not fully tested. Expect bugs, incomplete features, and behaviour that varies depending on SD card brand, capacity, and filesystem.
The goal is to provide a lightweight, portable SD diagnostics suite that runs directly on the Cardputer‑ADV — no PC required.

Overview
This project provides a standalone SD card utility offering:
- SD card information (manufacturer, product name, capacity)
- Filesystem detection (FAT32, FAT16, exFAT, Unknown)
- Raw CID field display for advanced users
- Speed test (simple write/read benchmark)
- Integrity check (H2TestW‑style 50MB write/verify)
- Quick format (SdFat quick format + remount)
- Keyboard‑driven UI

## Feature Status

| Feature               | Status            | Notes                                           |
|-----------------------|-------------------|-------------------------------------------------|
| SD Card Information   | Stable            | Manufacturer lookup, PNM, capacity, CID fields  |
|                       | Partially reliable 🔴| Occasional SD re-init failures after format     | 
| Speed Test            | Partially reliable 🔴| Write speed then needs freezes and needs reset  |
| Integrity Check       | Known issues   🔴   | Slow; no progress bar                           | 
| Quick Format          | Stable      🟡      | SdFat quick format + automatic remount          | 
| Reboot                | Stable       🟢      |                                                 |
| Navigation / UI       | Stable         🟢    | Scroll speed may feel fast                      |
| SPI Stability         | Uncertain    🟡     | Varies by card brand and age                    | 
| Card Health Metrics   | Not implemented  ⚪  | CSD/SCR parsing planned                         |
| Filesystem Detection  | Needs testing  🟡    | exFAT depends on SdFat configuration            |

Legend:
🟢 Stable 🟡 Needs testing 🔴 Known issues ⚪ Not implemented

Current Status
Prototype – not fully validated
Some features work reliably, others require more testing across different SD cards and capacities. Behaviour may vary depending on:
- Card brand
- Card age/health
- Filesystem type
- SPI bus stability
- SdFat configuration
This repository is intended for experimentation, feedback, and iteration.

Features
SD Card Information
- Manufacturer lookup (SanDisk, Samsung, Kingston, Phison, Lexar, SP)
- Product name (PNM)
- Capacity in MB
- Filesystem detection
- Raw CID fields (MID, OID)
Speed Test
- Writes 5MB in 4096‑byte blocks
- Reads back the same file
- Reports MB/s
Integrity Check
- Writes 50MB of patterned data
- Verifies every 512‑byte block
- Reports PASS/FAIL and error count
Quick Format
- SdFat quick format
- Spinner animation
- Automatic SD remount
- Filesystem detection after format
Navigation
- ; → Up
- . → Down
- ENTER → Select
- BACKSPACE → Return to menu

Known Issues
- Speed test may fail to re‑initialise SD after formatting
- Menu scroll speed may feel too fast
- exFAT detection depends on SdFat build configuration
- No progress bar for long operations
- No SPI auto‑speed fallback
- No card health metrics (erase block size, CSD/SCR parsing)

Hardware
M5Stack Cardputer‑ADV (StampS3A)
Dedicated HSPI pins:
|  |  | 
|  |  | 
|  |  | 
|  |  | 
|  |  | 



Build Instructions (PlatformIO)
- Install PlatformIO (VSCode recommended)
- Clone this repository
- Open the folder in VSCode
- Build & upload using PlatformIO: Upload
- Insert an SD card and reboot the Cardputer‑ADV



License
MIT (or choose your preferred license)

Contributions
Bug reports, test results, and pull requests are welcome.
Testing across a wide range of SD cards and capacities is especially valuable.
