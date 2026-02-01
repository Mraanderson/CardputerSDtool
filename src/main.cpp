/**
 * M5Stack Cardputer ADV - SD Card Tool
 * Features: CID Info, Speed Test, Integrity Check, Quick Format
 */

#include <M5Unified.h>
#include <M5Cardputer.h>
#include <SdFat.h>
#include <FatLib/FatFormatter.h>

// --- SD SPI Pins for Cardputer ADV ---
#define SD_SCK_PIN   40
#define SD_MISO_PIN  39
#define SD_MOSI_PIN  14
static const int SD_CS_PIN = 12;

#define SPI_CLOCK SD_SCK_MHZ(20)

SdFat sd;
SPIClass sdSpi(HSPI);

// --- Key helpers ---
inline bool isUp(char k)    { return k == ';'; }
inline bool isDown(char k)  { return k == '.'; }

char pollKey() {
    const char keys[] = { ';', '.', 0 };
    for (int i = 0; keys[i]; i++) {
        if (M5Cardputer.Keyboard.isKeyPressed(keys[i])) {
            return keys[i];
        }
    }
    return 0;
}

enum State { MENU, INFO, SPEED, H2TEST, FORMAT };
State currentState = MENU;

int menuIndex = 0;
const char* menuItems[] = {
    " 1. Card Info",
    " 2. Speed Test",
    " 3. Integrity Check",
    " 4. Format (Quick) WIP",
    " 5. Reboot"
};

// --- Forward declarations ---
void drawMenu();
void showCardInfo();
void runSpeedTest();
void runIntegrityCheck();
void runFormat();
void waitForInput();
bool initSD();

// ------------------------------------------------------------
// NEW: Require SD card removal at startup
// ------------------------------------------------------------
void requireCardRemovedAtStartup() {
    // Try a non-blocking check first
    bool cardPresent = sd.cardBegin(
    SdSpiConfig(SD_CS_PIN, DEDICATED_SPI, SD_SCK_MHZ(20), &sdSpi)
);

    if (!cardPresent) {
        return; // No card inserted, continue normally
    }

    // Card detected — warn user
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setCursor(0, 0);
    M5.Display.setTextColor(TFT_RED, TFT_BLACK);
    M5.Display.println("\n");
    M5.Display.println(" XXX SD CARD DETECTED XXX \n");
    M5.Display.println("\n");
    M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);
    M5.Display.println(" Please REMOVE the SD card\n");
    M5.Display.println(" DANGER of data loss\n");
    M5.Display.println(" Waiting for removal...");

    // Wait until card is removed
    while (true) {
        M5Cardputer.update();
        delay(200);

        if (!sd.cardBegin(SdSpiConfig(SD_CS_PIN, SHARED_SPI, SPI_CLOCK, &sdSpi))) {
            break;
        }
    }

    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setCursor(0, 0);
    M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);
    M5.Display.println(" SD card removed.");
    M5.Display.println(" Starting SD Tool...");
    delay(800);
}

// ------------------------------------------------------------

void setup() {
    M5Cardputer.begin();
    M5.Display.setRotation(1);
    M5.Display.setTextSize(1.5);
    M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);

    sdSpi.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);

    // NEW: Safety check before showing menu
    requireCardRemovedAtStartup();

    drawMenu();
}

void loop() {
    M5Cardputer.update();
    char key = pollKey();

    if (currentState == MENU) {

        if (isUp(key)) {
            menuIndex = (menuIndex + 4) % 5;
            drawMenu();
            delay(150);
        }

        if (isDown(key)) {
            menuIndex = (menuIndex + 1) % 5;
            drawMenu();
            delay(150);
        }

        if (M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER)) {
            while (M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER)) {
                M5Cardputer.update();
                delay(10);
            }

            switch (menuIndex) {
                case 0: currentState = INFO;   showCardInfo();      break;
                case 1: currentState = SPEED;  runSpeedTest();      break;
                case 2: currentState = H2TEST; runIntegrityCheck(); break;
                case 3: currentState = FORMAT; runFormat();         break;
                case 4: ESP.restart();                              break;
            }
        }

    } else if (currentState == MENU &&
           M5Cardputer.Keyboard.isKeyPressed(KEY_BACKSPACE)) {

        currentState = MENU;
        drawMenu();
        delay(150);
    }
}

// --- UI ---

void drawMenu() {
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setCursor(0, 0);
    M5.Display.println("   === SD TOOL ADV ===\n");
    M5.Display.println(" ENTER: select/back");
    M5.Display.println(" BKSP: abort\n");

    for (int i = 0; i < 5; i++) {
        bool sel = (i == menuIndex);
        M5.Display.setTextColor(sel ? TFT_BLACK : TFT_GREEN,
                                sel ? TFT_WHITE : TFT_BLACK);
        M5.Display.println(menuItems[i]);
    }

    M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);
}

// --- SD Init ---

bool initSD() {
    SdSpiConfig cfg(
        SD_CS_PIN,
        SHARED_SPI,          // Cardputer uses shared SPI bus
        SD_SCK_MHZ(20),      // Safe speed for ADV
        &sdSpi
    );

    if (!sd.begin(cfg)) {
        M5.Display.setTextColor(TFT_RED, TFT_BLACK);
        M5.Display.println("SD Init Failed!");
        return false;
    }
    return true;
}

// --- Card Info ---

void showCardInfo() {
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setCursor(0, 0);

    if (!initSD()) { 
        waitForInput(); 
        return; 
    }

    
    cid_t cid;
    if (!sd.card()->readCID(&cid)) {
        M5.Display.setTextColor(TFT_RED, TFT_BLACK);
        M5.Display.println("Read CID Failed");
        waitForInput();
        return;
    }

    const char* maker = "Unknown";
    switch (cid.mid) {
        case 0x03: maker = "SanDisk";        break;
        case 0x1B: maker = "Samsung";        break;
        case 0x1D: maker = "Kingston";       break;
        case 0x27: maker = "Phison";         break;
        case 0x28: maker = "Lexar";          break;
        case 0x31: maker = "Silicon Power";  break;
    }

    uint64_t sizeMB =
        (uint64_t)sd.card()->sectorCount() * 512ULL / 1024ULL / 1024ULL;

    M5.Display.printf("Manufacturer: %s\n", maker);
    M5.Display.printf("Product Name: %c%c%c%c%c\n",
        cid.pnm[0], cid.pnm[1], cid.pnm[2], cid.pnm[3], cid.pnm[4]);

    M5.Display.printf("Capacity: %llu MB\n", sizeMB);

    uint8_t fs = sd.fatType();
    M5.Display.print("Filesystem: ");
    switch (fs) {
       case FAT_TYPE_EXFAT: M5.Display.println("exFAT"); break;
       case 32:             M5.Display.println("FAT32"); break;
       case 16:             M5.Display.println("FAT16"); break;
       case 12:             M5.Display.println("FAT12"); break;
       default:             M5.Display.println("Unknown"); break;
    }

    M5.Display.printf("\nMID: 0x%02X\n", cid.mid);
    M5.Display.printf("OID: %c%c\n", cid.oid[0], cid.oid[1]);

    waitForInput();
}

// --- Speed Test ---

void runSpeedTest() {
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setCursor(0, 0);
    M5.Display.println("\n");
    M5.Display.println(" Speed Test\n");
    M5.Display.println(" BKSP: abort\n");

    if (!initSD()) { 
        waitForInput(); 
        return; 
    }

    static uint8_t buf[4096];
    SdFile f;
    if (!f.open("spd.tmp", O_RDWR | O_CREAT | O_TRUNC)) {
        M5.Display.setTextColor(TFT_RED, TFT_BLACK);
        M5.Display.println("Open spd.tmp failed");
        waitForInput();
        return;
    }

    // --- WRITE TEST ---
    uint32_t s = millis();
    for (int i = 0; i < 1280; i++) {

        // Abort check
        M5Cardputer.update();
        if (M5Cardputer.Keyboard.isKeyPressed(KEY_BACKSPACE)) {
            while (M5Cardputer.Keyboard.isKeyPressed(KEY_BACKSPACE)) {
                M5Cardputer.update();
                delay(10);
            }
            M5.Display.println("\nAborted by user");
            f.close();
            sd.remove("spd.tmp");
            waitForInput();
            return;
        }

        f.write(buf, 4096);
    }
    f.sync();

    float writeTime = (millis() - s) / 1000.0f;
    M5.Display.printf(" Write: %.2f MB/s\n", 5.0f / writeTime);

    // --- READ TEST ---
    f.rewind();
    s = millis();
    while (f.read(buf, 4096) > 0) {

        // Abort check
        M5Cardputer.update();
        if (M5Cardputer.Keyboard.isKeyPressed(KEY_BACKSPACE)) {
            while (M5Cardputer.Keyboard.isKeyPressed(KEY_BACKSPACE)) {
                M5Cardputer.update();
                delay(10);
            }
            M5.Display.println("\nAborted by user");
            f.close();
            sd.remove("spd.tmp");
            waitForInput();
            return;
        }
    }

    float readTime = (millis() - s) / 1000.0f;
    M5.Display.printf(" Read:  %.2f MB/s\n", 5.0f / readTime);

    f.close();
    sd.remove("spd.tmp");

    waitForInput();
}


// --- Integrity Check (H2TestW‑style, Cardputer‑optimised layout) ---
void runIntegrityCheck() {
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setCursor(0, 0);
    M5.Display.println(" Integrity Check (50MB)");

    M5.Display.setCursor(0, 20);
    M5.Display.println(" ENTER: start");

    M5.Display.setCursor(0, 35);
    M5.Display.println(" BKSP: abort");

    // Wait for ENTER or BACKSPACE
    while (!M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER)) {
        M5Cardputer.update();
        if (M5Cardputer.Keyboard.isKeyPressed(KEY_BACKSPACE)) {
            currentState = MENU;
            drawMenu();
            return;
        }
        delay(10);
    }

    // Debounce ENTER
    while (M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER)) {
        M5Cardputer.update();
        delay(10);
    }

    // Init SD
    if (!initSD()) {
        waitForInput();
        return;
    }

    SdFile f;
    if (!f.open("test.h2w", O_RDWR | O_CREAT | O_TRUNC)) {
        M5.Display.setTextColor(TFT_RED, TFT_BLACK);
        M5.Display.setCursor(0, 60);
        M5.Display.println("Open test.h2w failed");
        waitForInput();
        return;
    }

    uint8_t buf[512];
    uint32_t limit = 50 * 1024 * 1024;  // 50MB
    uint32_t total = 0;

    // H2testw-style monotonic counter
    uint32_t counter = 0;

    // --- WRITE PHASE ---
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setCursor(0, 0);
    M5.Display.println(" Writing blocks...");

    M5.Display.setCursor(0, 25);
    M5.Display.println(" Progress:");

    while (total < limit) {
        uint32_t* p = (uint32_t*)buf;

        // Fill block with monotonic pattern
        for (int i = 0; i < 128; i++) {
            p[i] = counter++;
        }

        if (f.write(buf, 512) != 512) {
            M5.Display.setTextColor(TFT_RED, TFT_BLACK);
            M5.Display.setCursor(0, 60);
            M5.Display.println("Write error");
            break;
        }

        total += 512;

        // Update progress every 1MB
        if ((total % (1024 * 1024)) == 0) {
            M5.Display.setCursor(0, 45);
            M5.Display.printf(" Written: %d MB   ", total / 1024 / 1024);
            M5Cardputer.update();
        }

        // Keep UI responsive
        if ((total & 0xFFF) == 0) {
            M5Cardputer.update();
            delay(1);
        }

        // Abort
        if (M5Cardputer.Keyboard.isKeyPressed(KEY_BACKSPACE)) {
            M5.Display.setCursor(0, 70);
            M5.Display.println("Aborted by user");
            break;
        }
    }

    f.sync();
    sd.card()->syncDevice();  // Ensure SPI flush
    delay(200);
    f.rewind();

    // --- VERIFY PHASE ---
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setCursor(0, 0);
    M5.Display.println("Verifying blocks...");

    M5.Display.setCursor(0, 25);
    M5.Display.println("Progress:");

    uint32_t r = 0;
    uint32_t expected = 0;
    uint32_t err = 0;

    while (r < total) {
        int n = f.read(buf, 512);
        if (n != 512) {
            M5.Display.setTextColor(TFT_RED, TFT_BLACK);
            M5.Display.setCursor(0, 60);
            M5.Display.println("Read error");
            break;
        }

        uint32_t* p = (uint32_t*)buf;

        // Compare block
        for (int i = 0; i < 128; i++) {
            if (p[i] != expected++) {
                err++;
            }
        }

        r += 512;

        // Update progress every 1MB
        if ((r % (1024 * 1024)) == 0) {
            M5.Display.setCursor(0, 45);
            M5.Display.printf(" Verified: %d MB   ", r / 1024 / 1024);
            M5Cardputer.update();
        }

        // Keep UI responsive
        if ((r & 0xFFF) == 0) {
            M5Cardputer.update();
            delay(1);
        }

        // Abort
        if (M5Cardputer.Keyboard.isKeyPressed(KEY_BACKSPACE)) {
            M5.Display.setCursor(0, 70);
            M5.Display.println("Aborted by user");
            break;
        }
    }

    f.close();
    sd.remove("test.h2w");

    // --- RESULT SCREEN ---
    M5.Display.fillScreen(err ? TFT_RED : TFT_GREEN);
    M5.Display.setCursor(0, 0);
    M5.Display.printf(" Result: %s\nErrors: %d\n",
                      err ? "FAIL" : "PASS", err);

    waitForInput();
}

// ===============================
// FAT32 Quick Formatter — Core Types
// ===============================

// A simple 512‑byte sector buffer
struct Sector {
    uint8_t b[512];
};

// Zero a sector
static inline void clearSector(Sector &s) {
    memset(s.b, 0, 512);
}

// -------------------------------
// Cluster size selection
// -------------------------------
// FAT32 rules:
//  - ≤32GB → 32KB clusters
//  - ≥64GB → 64KB clusters
//  - FAT16 only for ≤2GB
static inline uint32_t chooseClusterSize(uint64_t sizeMB) {
    if (sizeMB <= 2048) {
        // FAT16 case — handled separately in quickFormat()
        return 0;
    }
    if (sizeMB <= 32768) {
        return 32 * 1024;   // 32KB
    }
    return 64 * 1024;       // 64KB
}

// -------------------------------
// Partition alignment
// -------------------------------
// SDXC requires 1MB alignment.
// SDHC can use 128KB or 1MB — we use 1MB for simplicity.
static inline uint32_t partitionStartSector(uint64_t sizeMB) {
    return 2048;   // 2048 * 512 = 1MB
}

// -------------------------------
// FAT32 BPB template builder
// -------------------------------
static void buildFAT32BPB(
    Sector &bpb,
    uint32_t totalSectors,
    uint32_t fatStart,
    uint32_t fatSize,
    uint32_t rootCluster,
    uint32_t sectorsPerCluster
) {
    clearSector(bpb);

    // Jump instruction + OEM name
    bpb.b[0] = 0xEB;
    bpb.b[1] = 0x58;
    bpb.b[2] = 0x90;
    memcpy(&bpb.b[3], "MSDOS5.0", 8);

    // Bytes per sector
    bpb.b[11] = 0x00; // 512 bytes (0x0200 little endian)
    bpb.b[12] = 0x02;

    // Sectors per cluster
    bpb.b[13] = sectorsPerCluster;

    // Reserved sectors (BPB + FSInfo + backup)
    bpb.b[14] = 0x20; // 32 reserved sectors (typical)
    bpb.b[15] = 0x00;

    // Number of FATs
    bpb.b[16] = 0x02;

    // Root entries (FAT12/16 only)
    bpb.b[17] = 0x00;
    bpb.b[18] = 0x00;

    // Total sectors (16‑bit) — zero for FAT32
    bpb.b[19] = 0x00;
    bpb.b[20] = 0x00;

    // Media descriptor
    bpb.b[21] = 0xF8;

    // FAT size (16‑bit) — zero for FAT32
    bpb.b[22] = 0x00;
    bpb.b[23] = 0x00;

    // Sectors per track / heads — dummy geometry
    bpb.b[24] = 0x3F;
    bpb.b[25] = 0x00;
    bpb.b[26] = 0xFF;
    bpb.b[27] = 0x00;

    // Hidden sectors (partition start)
    bpb.b[28] = (uint8_t)(fatStart & 0xFF);
    bpb.b[29] = (uint8_t)((fatStart >> 8) & 0xFF);
    bpb.b[30] = (uint8_t)((fatStart >> 16) & 0xFF);
    bpb.b[31] = (uint8_t)((fatStart >> 24) & 0xFF);

    // Total sectors (32‑bit)
    bpb.b[32] = (uint8_t)(totalSectors & 0xFF);
    bpb.b[33] = (uint8_t)((totalSectors >> 8) & 0xFF);
    bpb.b[34] = (uint8_t)((totalSectors >> 16) & 0xFF);
    bpb.b[35] = (uint8_t)((totalSectors >> 24) & 0xFF);

    // FAT32 extended fields
    bpb.b[36] = (uint8_t)(fatSize & 0xFF);
    bpb.b[37] = (uint8_t)((fatSize >> 8) & 0xFF);
    bpb.b[38] = (uint8_t)((fatSize >> 16) & 0xFF);
    bpb.b[39] = (uint8_t)((fatSize >> 24) & 0xFF);

    // Flags
    bpb.b[40] = 0x00;
    bpb.b[41] = 0x00;

    // Version
    bpb.b[42] = 0x00;
    bpb.b[43] = 0x00;

    // Root cluster
    bpb.b[44] = (uint8_t)(rootCluster & 0xFF);
    bpb.b[45] = (uint8_t)((rootCluster >> 8) & 0xFF);
    bpb.b[46] = (uint8_t)((rootCluster >> 16) & 0xFF);
    bpb.b[47] = (uint8_t)((rootCluster >> 24) & 0xFF);

    // FSInfo sector
    bpb.b[48] = 0x01;
    bpb.b[49] = 0x00;

    // Backup boot sector
    bpb.b[50] = 0x06;
    bpb.b[51] = 0x00;

    // Drive number
    bpb.b[64] = 0x80;

    // Boot signature
    bpb.b[66] = 0x29;

    // Volume ID (random-ish)
    uint32_t volId = 0x12345678;
    memcpy(&bpb.b[67], &volId, 4);

    // Volume label
    memcpy(&bpb.b[71], "NO NAME    ", 11);

    // File system type
    memcpy(&bpb.b[82], "FAT32   ", 8);

    // Boot sector signature
    bpb.b[510] = 0x55;
    bpb.b[511] = 0xAA;
}

// -------------------------------
// FSInfo sector builder
// -------------------------------
static void buildFSInfo(Sector &fs) {
    clearSector(fs);

    // Lead signature
    fs.b[0] = 0x52;
    fs.b[1] = 0x52;
    fs.b[2] = 0x61;
    fs.b[3] = 0x41;

    // Struct signature
    fs.b[484] = 0x72;
    fs.b[485] = 0x72;
    fs.b[486] = 0x41;
    fs.b[487] = 0x61;

    // Free cluster count unknown
    fs.b[488] = 0xFF;
    fs.b[489] = 0xFF;
    fs.b[490] = 0xFF;
    fs.b[491] = 0xFF;

    // Next free cluster unknown
    fs.b[492] = 0xFF;
    fs.b[493] = 0xFF;
    fs.b[494] = 0xFF;
    fs.b[495] = 0xFF;

    // Boot sector signature
    fs.b[510] = 0x55;
    fs.b[511] = 0xAA;
}

// ===============================
// FAT32 Quick Formatter — Raw I/O Helpers
// ===============================

// Write a 512-byte sector to the card
static bool writeSectorRaw(SdCard *card, uint32_t sector, const Sector &s) {
    return card->writeSector(sector, s.b);
}

// Clear a sector on the card
static bool clearSectorRaw(SdCard *card, uint32_t sector) {
    Sector s;
    clearSector(s);
    return card->writeSector(sector, s.b);
}

// ===============================
// MBR Writer
// ===============================
static bool writeMBR(SdCard *card, uint32_t partStart, uint32_t totalSectors) {
    Sector mbr;
    clearSector(mbr);

    // Partition entry at offset 446
    uint8_t *p = &mbr.b[446];

    p[0] = 0x00;          // Boot flag
    p[1] = 0x20;          // CHS begin (dummy)
    p[2] = 0x21;
    p[3] = 0x00;

    p[4] = 0x0C;          // Partition type = FAT32 LBA
    p[5] = 0xFE;          // CHS end (dummy)
    p[6] = 0xFF;
    p[7] = 0xFF;

    // LBA start
    p[8]  = (uint8_t)(partStart & 0xFF);
    p[9]  = (uint8_t)((partStart >> 8) & 0xFF);
    p[10] = (uint8_t)((partStart >> 16) & 0xFF);
    p[11] = (uint8_t)((partStart >> 24) & 0xFF);

    // Total sectors in partition
    uint32_t partSize = totalSectors - partStart;
    p[12] = (uint8_t)(partSize & 0xFF);
    p[13] = (uint8_t)((partSize >> 8) & 0xFF);
    p[14] = (uint8_t)((partSize >> 16) & 0xFF);
    p[15] = (uint8_t)((partSize >> 24) & 0xFF);

    // Signature
    mbr.b[510] = 0x55;
    mbr.b[511] = 0xAA;

    return writeSectorRaw(card, 0, mbr);
}

// ===============================
// BPB Writer
// ===============================
static bool writeBPB(SdCard *card, uint32_t partStart, const Sector &bpb) {
    // Write main BPB
    if (!writeSectorRaw(card, partStart, bpb)) return false;

    // Backup boot sector at partStart + 6
    if (!writeSectorRaw(card, partStart + 6, bpb)) return false;

    return true;
}

// ===============================
// FSInfo Writer
// ===============================
static bool writeFSInfo(SdCard *card, uint32_t partStart, const Sector &fs) {
    // FSInfo at partStart + 1
    if (!writeSectorRaw(card, partStart + 1, fs)) return false;

    // Backup FSInfo at partStart + 7
    if (!writeSectorRaw(card, partStart + 7, fs)) return false;

    return true;
}

// ===============================
// FAT Header Writer
// ===============================
// FAT32 requires the first two FAT entries:
//  - FAT[0] = media descriptor + reserved bits
//  - FAT[1] = end-of-chain marker
static bool writeFATHeaders(
    SdCard *card,
    uint32_t fatStart,
    uint32_t fatSize
) {
    Sector s;
    clearSector(s);

    // FAT[0]
    s.b[0] = 0xF8;  // Media descriptor
    s.b[1] = 0xFF;
    s.b[2] = 0xFF;
    s.b[3] = 0x0F;

    // FAT[1]
    s.b[4] = 0xFF;
    s.b[5] = 0xFF;
    s.b[6] = 0xFF;
    s.b[7] = 0x0F;

    // Write first FAT sector
    if (!writeSectorRaw(card, fatStart, s)) return false;

    // Clear remaining FAT sectors
    for (uint32_t i = 1; i < fatSize; i++) {
        if (!clearSectorRaw(card, fatStart + i)) return false;
    }

    return true;
}

// ===============================
// Root Directory Writer
// ===============================
// Root directory cluster is empty for Quick Format
static bool writeRootDir(
    SdCard *card,
    uint32_t dataStart,
    uint32_t sectorsPerCluster
) {
    // Root directory = cluster 2
    uint32_t rootSector = dataStart;

    for (uint32_t i = 0; i < sectorsPerCluster; i++) {
        if (!clearSectorRaw(card, rootSector + i)) return false;
    }

    return true;
}

// ===============================
// FAT32 Quick Formatter — Core quickFormat()
// ===============================

static bool quickFormat(SdFat &sd) {
    SdCard *card = sd.card();
    if (!card) return false;

    uint64_t sectors64 = card->sectorCount();
    if (sectors64 == 0) return false;

    // Limit to 32-bit sector count (up to 2TB, we only target ≤256GB)
    uint32_t totalSectors = (uint32_t)sectors64;

    // Card size in MB
    uint64_t sizeMB = (sectors64 * 512ULL) / (1024ULL * 1024ULL);

    // FAT16 for ≤2GB — let SdFat handle that
    if (sizeMB <= 2048) {
        // Silent: no Serial
        return sd.format(nullptr);
    }

    // Choose cluster size for FAT32
    uint32_t clusterBytes = chooseClusterSize(sizeMB);
    if (clusterBytes == 0) return false;

    uint32_t sectorsPerCluster = clusterBytes / 512;
    if (sectorsPerCluster == 0) return false;

    // Partition start (1MB aligned)
    uint32_t partStart = partitionStartSector(sizeMB);

    // Basic layout constants
    const uint32_t reservedSectors = 32;  // BPB + FSInfo + backup etc.
    const uint32_t fats = 2;

    // Iteratively compute FAT size and data region
    uint32_t fatSize = 0;
    uint32_t prevFatSize = 0;

    for (int i = 0; i < 8; i++) {
        prevFatSize = fatSize;

        uint32_t dataSectors = totalSectors - partStart - reservedSectors - (fats * fatSize);
        if ((int32_t)dataSectors <= 0) return false;

        uint32_t clusters = dataSectors / sectorsPerCluster;
        if (clusters < 65525) {
            // Not enough clusters for FAT32
            return false;
        }

        // FAT size in sectors: ceil(clusters * 4 / 512)
        fatSize = (clusters * 4 + 511) / 512;

        if (fatSize == prevFatSize && fatSize != 0) break;
    }

    if (fatSize == 0) return false;

    // Final layout
    uint32_t fatStart = partStart + reservedSectors;
    uint32_t dataStart = fatStart + fats * fatSize;
    uint32_t rootCluster = 2;

    // Build BPB and FSInfo
    Sector bpb;
    buildFAT32BPB(
        bpb,
        totalSectors - partStart, // total sectors in partition
        partStart,
        fatSize,
        rootCluster,
        sectorsPerCluster
    );

    Sector fsInfo;
    buildFSInfo(fsInfo);

    // Write MBR
    if (!writeMBR(card, partStart, totalSectors)) return false;

    // Clear reserved area (except BPB/FSInfo/backup which we overwrite)
    for (uint32_t i = 0; i < reservedSectors; i++) {
        if (!clearSectorRaw(card, partStart + i)) return false;
    }

    // Write BPB + backup
    if (!writeBPB(card, partStart, bpb)) return false;

    // Write FSInfo + backup
    if (!writeFSInfo(card, partStart, fsInfo)) return false;

    // Write both FATs
    if (!writeFATHeaders(card, fatStart, fatSize)) return false;
    if (!writeFATHeaders(card, fatStart + fatSize, fatSize)) return false;

    // Write empty root directory (cluster 2)
    if (!writeRootDir(card, dataStart, sectorsPerCluster)) return false;

    return true;
}

// ===============================
// UI wrapper around quickFormat()
// ===============================
void runFormat() {
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setCursor(0, 10);
    M5.Display.println(" Quick Format\n");
    M5.Display.println(" ENTER: format");
    M5.Display.println(" BKSP: abort");

    // Wait for ENTER or BACKSPACE
    while (!M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER)) {
        M5Cardputer.update();
        if (M5Cardputer.Keyboard.isKeyPressed(KEY_BACKSPACE)) {
            currentState = MENU;
            drawMenu();
            return;
        }
        delay(10);
    }

    // Debounce ENTER
    while (M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER)) {
        M5Cardputer.update();
        delay(10);
    }

    // --- Formatting Screen ---
M5.Display.fillScreen(TFT_BLACK);
M5.Display.setCursor(0, 0);
M5.Display.println(" Formatting...");
M5.Display.setCursor(0, 25);
M5.Display.println(" Please wait");

const char spinner[4] = {'|','/','-','\\'};
int spinIndex = 0;

uint32_t start = millis();

// --- Perform quick format (silent) ---
bool ok = quickFormat(sd);

// Spinner animation for ~2 seconds after format
while (millis() - start < 2000) {
    M5.Display.setCursor(0, 50);
    M5.Display.printf("%c", spinner[spinIndex]);
    spinIndex = (spinIndex + 1) % 4;
    M5Cardputer.update();
    delay(120);
}

// --- CRITICAL: flush controller + settle ---
sd.card()->syncDevice();
delay(200);

// --- FULL SD + SPI RESET (Cardputer ADV dedicated SD bus) ---
sd.end();
delay(50);

SPI.end();
delay(50);

// Re‑initialise the dedicated SD SPI bus
SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
delay(50);


    // Remount using DEDICATED_SPI on HSPI
    bool mounted = sd.begin(SdSpiConfig(
        SD_CS_PIN,
        DEDICATED_SPI,
        SD_SCK_MHZ(20),
        &sdSpi
    ));

    // --- Result Screen ---
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setCursor(0, 0);

    if (ok && mounted) {
        M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);
        M5.Display.println("Format OK");

        uint8_t fs = sd.vol()->fatType();
        M5.Display.setCursor(0, 20);
        M5.Display.printf("Filesystem: FAT%d\n", fs);

        SdFile test;
        if (test.open("format_ok.txt", O_RDWR | O_CREAT | O_TRUNC)) {
            test.println("Cardputer SD Tool format check");
            test.close();
            M5.Display.println(" Test file written");
        } else {
            M5.Display.println(" Test file FAILED");
        }

    } else {
        M5.Display.setTextColor(TFT_RED, TFT_BLACK);
        M5.Display.println("Format Failed");
        M5.Display.setCursor(0, 20);
        M5.Display.println(ok ? "Card init failed" : "Format error");
    }

    waitForInput();
}


// --- Return to menu ---
void waitForInput() {
    M5.Display.println("\n Press ENTER to return");

    // 1. Wait for ANY key release (ENTER or BACKSPACE)
    while (M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER) ||
           M5Cardputer.Keyboard.isKeyPressed(KEY_BACKSPACE)) {
        M5Cardputer.update();
        delay(10);
    }

    // 2. Wait for a NEW key press
    while (!M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER) &&
           !M5Cardputer.Keyboard.isKeyPressed(KEY_BACKSPACE)) {
        M5Cardputer.update();
        delay(10);
    }

    // 3. Debounce whichever key was pressed
    while (M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER) ||
           M5Cardputer.Keyboard.isKeyPressed(KEY_BACKSPACE)) {
        M5Cardputer.update();
        delay(10);
    }

    // 4. Return to menu cleanly
    currentState = MENU;
    drawMenu();
}

/*
 * NOTE FOR FUTURE DEVELOPMENT — SD CARD CAPACITY VERIFICATION
 * -----------------------------------------------------------
 * The current Integrity Check writes/reads a fixed 50MB region.
 * This verifies DATA INTEGRITY (detects corruption, weak flash,
 * controller issues, SPI instability), but it does NOT verify
 * the TRUE CAPACITY of the SD card.
 *
 * To detect FAKE-CAPACITY cards (e.g., 128GB cards that are
 * really 8GB and wrap around), a full-card write/verify is
 * required — OR a faster statistical method:
 *
 *   ✔ Write small probe blocks at RANDOM OFFSETS across the
 *     advertised capacity.
 *   ✔ Read them back and compare.
 *   ✔ If any probe wraps into earlier physical storage, the
 *     card is fake or misreporting its size.
 *
 * This "Random Capacity Probe" method is fast, low-wear, and
 * suitable for embedded devices like the Cardputer ADV.
 *
 * TODO (future):
 *   - Implement random-offset probe testing
 *   - Add menu option: "Capacity Probe (Fast)"
 *   - Add confidence scoring based on number of probes
 *   - Optionally add full-card test for thorough validation
 *
 * Circle back when ready to extend the SD Tool.
 */
