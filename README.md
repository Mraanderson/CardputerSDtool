Cardputer‑ADV SD Card Tool (Prototype)

A work‑in‑progress SD diagnostics utility for the M5Stack Cardputer‑ADV (StampS3A). This tool is still under active development and not fully tested. Expect bugs, incomplete features, and behaviour that varies depending on SD card brand and filesystem.

Overview

This project provides a standalone SD card utility for the Cardputer‑ADV, offering:

SD card information (manufacturer, product name, capacity)

Filesystem detection (FAT32, FAT16, exFAT, Unknown)

CID raw fields for advanced users

Speed test (simple write/read benchmark)

Integrity check (H2TestW‑style 50MB write/verify)

Quick format (SdFat quick format + remount)

Simple keyboard‑driven UI

The goal is to create a lightweight, portable SD diagnostics tool that runs directly on the Cardputer‑ADV without needing a PC.

Current Status

⚠️ Prototype – not fully validated

Some features work reliably, others need more testing across different SD cards and capacities. Behaviour may vary depending on:

Card brand

Card age/health

Filesystem type

SPI bus stability

SdFat configuration

This repository is intended for experimentation, feedback, and iteration.

Features

✔ SD Card Information

Manufacturer lookup (SanDisk, Samsung, Kingston, Phison, Lexar, SP)

Product name (PNM)

Capacity in MB

Filesystem detection

Raw CID fields (MID, OID)

✔ Speed Test

Writes 5MB in 4096‑byte blocks

Reads back the same file

Reports MB/s

✔ Integrity Check

Writes 50MB of patterned data

Verifies every 512‑byte block

Reports PASS/FAIL and error count

✔ Quick Format

SdFat quick format

Spinner animation

Automatic SD remount

Filesystem detection after format

✔ Navigation

w / ; → Up

s / . → Down

ENTER → Select

BACKSPACE → Return to menu

Known Issues

Speed test may fail to re‑initialise SD after formatting

Menu scroll speed may feel too fast

exFAT detection depends on SdFat build configuration

No progress bar for long operations

No SPI auto‑speed fallback

No card health metrics (erase block size, CSD/SCR parsing)

Hardware

M5Stack Cardputer‑ADV (StampS3A)

Dedicated HSPI pins:

SCK = 40

MISO = 39

MOSI = 14

CS = 12

Build Instructions (PlatformIO)

Install PlatformIO (VSCode recommended)

Clone this repository

Open the folder in VSCode

Build & upload using:

PlatformIO: Upload

Insert an SD card and reboot the Cardputer‑ADV

Project Structure

Cardputer-SDTool/
├── src/
│   └── main.cpp
├── include/
├── lib/
├── platformio.ini
├── README.md
└── .gitignore

.gitignore

# PlatformIO
.pio/
.pioenvs/
.piolibdeps/
.clang_complete
.gcc-flags.json

# VSCode
.vscode/
*.code-workspace

# Mac / Linux / Windows junk
.DS_Store
Thumbs.db

# Logs
*.log

# Build artifacts
*.bin
*.elf
*.map

License

MIT (or choose your preferred license)

Contributions

Bug reports, test results, and pull requests are welcome. This tool benefits greatly from testing across a wide range of SD cards and capacities.
