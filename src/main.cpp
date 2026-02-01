/**
 * M5Stack Cardputer ADV - SD Card Tool
 * Features: CID Info, Speed Test, Integrity Check, Quick Format
 */

#include <M5Unified.h>
#include <M5Cardputer.h>
#include <SdFat.h>

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
    "1. Card Info (CID) - Working",
    "2. Speed Test - Working",
    "3. Integrity Check WIP",
    "4. Format (Quick) WIP",
    "5. Reboot"
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
    SdSpiConfig(SD_CS_PIN, SHARED_SPI, SD_SCK_MHZ(20), &sdSpi)
);

    if (!cardPresent) {
        return; // No card inserted, continue normally
    }

    // Card detected — warn user
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setCursor(0, 0);
    M5.Display.setTextColor(TFT_RED, TFT_BLACK);
    M5.Display.println(" XXX SD CARD DETECTED XXX \n");
    M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);
    M5.Display.println("Please REMOVE the SD card\n");
    M5.Display.println("to avoid accidental data loss.\n");
    M5.Display.println("Waiting for removal...");

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
    M5.Display.println("SD card removed.");
    M5.Display.println("Starting SD Tool...");
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
                case 4: ESP.restart();                               break;
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
    M5.Display.println("=== SD TOOL ADV ===");
    M5.Display.println("ENTER: select/back\n");
    M5.Display.println("BKSP: abort\n");

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
    M5.Display.println("Speed Test\n");

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

    uint32_t s = millis();
    for (int i = 0; i < 1280; i++) {
        f.write(buf, 4096);
    }
    f.sync();
    float writeTime = (millis() - s) / 1000.0f;
    M5.Display.printf("Write: %.2f MB/s\n", 5.0f / writeTime);

    f.rewind();
    s = millis();
    while (f.read(buf, 4096) > 0) {}
    float readTime = (millis() - s) / 1000.0f;
    M5.Display.printf("Read:  %.2f MB/s\n", 5.0f / readTime);

    f.close();
    sd.remove("spd.tmp");

    waitForInput();
}

// --- Integrity Check (H2TestW‑style, fixed & Cardputer‑safe) ---
void runIntegrityCheck() {
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setCursor(0, 0);
    M5.Display.println("Integrity Check (50MB)\n");
    M5.Display.println("ENTER: start");
    M5.Display.println("BKSP: abort");

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
        M5.Display.println("Open test.h2w failed");
        waitForInput();
        return;
    }

    uint8_t buf[512];
    uint32_t limit = 50 * 1024 * 1024;  // 50MB
    uint32_t total = 0;

    // H2testw-style monotonic counter
    uint32_t counter = 0;

    M5.Display.println("\nWriting...");

    // --- WRITE PHASE ---
    while (total < limit) {
        uint32_t* p = (uint32_t*)buf;

        // Fill block with monotonic pattern
        for (int i = 0; i < 128; i++) {
            p[i] = counter++;
        }

        if (f.write(buf, 512) != 512) {
            M5.Display.setTextColor(TFT_RED, TFT_BLACK);
            M5.Display.println("\nWrite error");
            break;
        }

        total += 512;

        // Update progress every 1MB
        if ((total % (1024 * 1024)) == 0) {
            M5.Display.setCursor(0, 40);
            M5.Display.printf("Written: %d MB   ", total / 1024 / 1024);
            M5Cardputer.update();
        }

        // Keep UI responsive
        if ((total & 0xFFF) == 0) {
            M5Cardputer.update();
            delay(1);
        }

        // Abort
        if (M5Cardputer.Keyboard.isKeyPressed(KEY_BACKSPACE)) {
            M5.Display.setCursor(0, 60);
            M5.Display.println("\nAborted by user");
            break;
        }
    }

    f.sync();
    sd.card()->syncDevice();  // Ensure SPI flush
    f.rewind();

    // --- VERIFY PHASE ---
    M5.Display.setCursor(0, 80);
    M5.Display.println("Verifying...");

    uint32_t r = 0;
    uint32_t expected = 0;
    uint32_t err = 0;

    while (r < total) {
        int n = f.read(buf, 512);
        if (n != 512) {
            M5.Display.setTextColor(TFT_RED, TFT_BLACK);
            M5.Display.println("\nRead error");
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
            M5.Display.setCursor(0, 100);
            M5.Display.printf("Verified: %d MB   ", r / 1024 / 1024);
            M5Cardputer.update();
        }

        // Keep UI responsive
        if ((r & 0xFFF) == 0) {
            M5Cardputer.update();
            delay(1);
        }

        // Abort
        if (M5Cardputer.Keyboard.isKeyPressed(KEY_BACKSPACE)) {
            M5.Display.setCursor(0, 120);
            M5.Display.println("\nAborted by user");
            break;
        }
    }

    f.close();
    sd.remove("test.h2w");

    // --- RESULT SCREEN ---
    M5.Display.fillScreen(err ? TFT_RED : TFT_GREEN);
    M5.Display.setCursor(0, 0);
    M5.Display.printf("Result: %s\nErrors: %d\n",
                      err ? "FAIL" : "PASS", err);

    waitForInput();
}


// --- Format with spinner + remount ---

void runFormat() {
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setCursor(0, 0);
    M5.Display.println("Quick Format\n");
    M5.Display.println("\n");
    M5.Display.println("ENTER: format\n");
    M5.Display.println("BKSP: abort");

    while (!M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER)) {
        M5Cardputer.update();
        if (M5Cardputer.Keyboard.isKeyPressed(KEY_BACKSPACE)) {
            currentState = MENU;
            drawMenu();
            return;
        }
        delay(10);
    }

    while (M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER)) {
        M5Cardputer.update();
        delay(10);
    }

    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setCursor(0, 0);
    M5.Display.println("Formatting...");

    const char spinner[4] = {'|','/','-','\\'};
    int spinIndex = 0;

    bool ok = sd.format(&Serial);

    for (int i = 0; i < 20; i++) {
        M5.Display.setCursor(0, 20);
        M5.Display.printf("%c", spinner[spinIndex]);
        spinIndex = (spinIndex + 1) % 4;
        M5Cardputer.update();
        delay(120);
    }

    sd.end();
    delay(300);

    bool mounted = sd.begin(SdSpiConfig(SD_CS_PIN, DEDICATED_SPI, SPI_CLOCK, &sdSpi));

    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setCursor(0, 0);

    if (ok && mounted) {
        M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);
        M5.Display.println("Format OK");

        uint8_t fs = sd.vol()->fatType();
        if (fs == 32)
            M5.Display.println("Filesystem: FAT32");
        else if (fs == 16)
            M5.Display.println("Filesystem: FAT16");
        else if (fs == 12)
            M5.Display.println("Filesystem: FAT12");
        else
            M5.Display.printf("Filesystem: FAT type %d\n", fs);

        SdFile test;
        if (test.open("format_ok.txt", O_RDWR | O_CREAT | O_TRUNC)) {
            test.println("Cardputer SD Tool format check");
            test.close();
            M5.Display.println("Test file: format_ok.txt written");
        } else {
            M5.Display.println("Test file write FAILED");
        }
    } else {
        M5.Display.setTextColor(TFT_RED, TFT_BLACK);
        M5.Display.println("Format Failed");
    }

    waitForInput();
}

// --- Return to menu ---
void waitForInput() {
    M5.Display.println("\nPress ENTER to return");

    // Wait for ENTER to be released if it's currently held
    while (M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER)) {
        M5Cardputer.update();
        delay(10);
    }

    // Wait for ENTER (or BACKSPACE) to be pressed
    while (!M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER) &&
           !M5Cardputer.Keyboard.isKeyPressed(KEY_BACKSPACE)) {
        M5Cardputer.update();
        delay(10);
    }

    // NEW: Wait for ENTER to be released again before returning to menu
    while (M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER)) {
        M5Cardputer.update();
        delay(10);
    }

    currentState = MENU;
    drawMenu();
}
