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

#define SPI_CLOCK SD_SCK_MHZ(25)

SdFat sd;
SPIClass sdSpi(HSPI);

// --- Key helpers ---
inline bool isUp(char k)    { return k == 'w' || k == ';'; }
inline bool isDown(char k)  { return k == 's' || k == '.'; }

// Only scan keys we actually use
char pollKey() {
    const char keys[] = { 'w', 's', ';', '.', 0 };
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
    "1. Card Info (CID)",
    "2. Speed Test",
    "3. Integrity Check",
    "4. Format (Quick)",
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

void setup() {
    M5Cardputer.begin();
    M5.Display.setRotation(1);
    M5.Display.setTextSize(1.5);
    M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);

    sdSpi.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
    drawMenu();
}

void loop() {
    M5Cardputer.update();
    char key = pollKey();

    if (currentState == MENU) {

        if (isUp(key)) {
            menuIndex = (menuIndex + 4) % 5;
            drawMenu();
            delay(300);
        }

        if (isDown(key)) {
            menuIndex = (menuIndex + 1) % 5;
            drawMenu();
            delay(300);
        }

        if (M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER)) {
            switch (menuIndex) {
                case 0: currentState = INFO;   showCardInfo(); break;
                case 1: currentState = SPEED;  runSpeedTest(); break;
                case 2: currentState = H2TEST; runIntegrityCheck(); break;
                case 3: currentState = FORMAT; runFormat(); break;
                case 4: ESP.restart(); break;
            }
        }

    } else if (M5Cardputer.Keyboard.isKeyPressed(KEY_BACKSPACE)) {
        currentState = MENU;
        drawMenu();
        delay(300);
    }
}

// --- UI ---

void drawMenu() {
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setCursor(0, 0);
    M5.Display.println("=== SD TOOL ADV ===");

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
    if (!sd.begin(SdSpiConfig(SD_CS_PIN, DEDICATED_SPI, SPI_CLOCK, &sdSpi))) {
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
        M5.Display.println("Read CID Failed");
        waitForInput();
        return;
    }

    // --- Manufacturer lookup ---
    const char* maker = "Unknown";
    switch (cid.mid) {
        case 0x03: maker = "SanDisk"; break;
        case 0x1B: maker = "Samsung"; break;
        case 0x1D: maker = "Kingston"; break;
        case 0x27: maker = "Phison"; break;
        case 0x28: maker = "Lexar"; break;
        case 0x31: maker = "Silicon Power"; break;
    }

    // --- Card size ---
    uint64_t sizeMB =
        (uint64_t)sd.card()->sectorCount() * 512ULL / 1024ULL / 1024ULL;

    // --- Display info ---
    M5.Display.printf("Manufacturer: %s\n", maker);
    M5.Display.printf("Product Name: %c%c%c%c%c\n",
        cid.pnm[0], cid.pnm[1], cid.pnm[2], cid.pnm[3], cid.pnm[4]);

    M5.Display.printf("Capacity: %llu MB\n", sizeMB);

    // ⭐️ --- Filesystem type (your requested snippet) ---
    uint8_t fs = sd.vol()->fatType();
    if (fs == 32)
        M5.Display.println("Filesystem: FAT32 (OK)");
    else if (fs == 0xEF)   // exFAT volume type code
    M5.Display.println("Filesystem: exFAT");
    else
        M5.Display.println("Filesystem: Unknown / Not Mounted");

    // Optional: raw IDs for advanced users
    M5.Display.printf("\nMID: 0x%02X\n", cid.mid);
    M5.Display.printf("OID: %c%c\n", cid.oid[0], cid.oid[1]);

    M5.Display.println("\nPress ENTER to return");

    // Wait for ENTER instead of Backspace
    while (!M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER)) {
        M5Cardputer.update();
        delay(10);
    }

    currentState = MENU;
    drawMenu();
}

// --- Speed Test ---

void runSpeedTest() {
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setCursor(0, 0);

    if (!initSD()) { waitForInput(); return; }

    static uint8_t buf[4096];
    SdFile f;
    f.open("spd.tmp", O_RDWR | O_CREAT | O_TRUNC);

    uint32_t s = millis();
    for (int i = 0; i < 1280; i++) f.write(buf, 4096);
    f.sync();
    M5.Display.printf("Write: %.2f MB/s\n", 5.0 / ((millis() - s) / 1000.0));

    f.rewind();
    s = millis();
    while (f.read(buf, 4096) > 0) {}
    M5.Display.printf("Read: %.2f MB/s\n", 5.0 / ((millis() - s) / 1000.0));

    f.close();
    sd.remove("spd.tmp");

    waitForInput();
}

// --- Integrity Check (H2TestW‑style) ---

void runIntegrityCheck() {
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setCursor(0, 0);
    M5.Display.println("Press ENTER to start (50MB limit)");

    while (!M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER)) {
        M5Cardputer.update();
        if (M5Cardputer.Keyboard.isKeyPressed(KEY_BACKSPACE)) {
            currentState = MENU;
            return;
        }
    }

    if (!initSD()) { waitForInput(); return; }

    SdFile f;
    f.open("test.h2w", O_RDWR | O_CREAT);

    uint8_t buf[512];
    uint32_t limit = 50 * 1024 * 1024;
    uint32_t total = 0;

    M5.Display.println("Writing...");

    while (total < limit) {
        uint32_t* p = (uint32_t*)buf;
        for (int i = 0; i < 128; i++) p[i] = total + i * 4;

        if (f.write(buf, 512) != 512) break;
        total += 512;

        if (total % (1024 * 1024) == 0) {
            M5.Display.setCursor(0, 30);
            M5.Display.printf("%d MB", total / 1024 / 1024);
        }

        M5Cardputer.update();
        if (M5Cardputer.Keyboard.isKeyPressed(KEY_BACKSPACE)) break;
    }

    f.sync();
    f.rewind();

    M5.Display.println("\nVerifying...");
    uint32_t r = 0, err = 0;

    while (r < total) {
        f.read(buf, 512);
        uint32_t* p = (uint32_t*)buf;

        for (int i = 0; i < 128; i++) {
            if (p[i] != r + i * 4) err++;
        }

        r += 512;

        if (r % (1024 * 1024) == 0) {
            M5.Display.setCursor(0, 60);
            M5.Display.printf("%d MB", r / 1024 / 1024);
        }
    }

    f.close();
    sd.remove("test.h2w");

    M5.Display.fillScreen(err ? TFT_RED : TFT_GREEN);
    M5.Display.setCursor(0, 0);
    M5.Display.printf("Result: %s\nErrors: %d", err ? "FAIL" : "PASS", err);

    waitForInput();
}

// --- Format with spinner + remount ---

void runFormat() {
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setCursor(0, 0);
    M5.Display.println("ENTER to Format");

    while (!M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER)) {
        M5Cardputer.update();
        if (M5Cardputer.Keyboard.isKeyPressed(KEY_BACKSPACE)) {
            currentState = MENU;
            return;
        }
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
    } else {
        M5.Display.setTextColor(TFT_RED, TFT_BLACK);
        M5.Display.println("Format Failed");
    }

    waitForInput();
}

// --- Return to menu ---

void waitForInput() {
    while (!M5Cardputer.Keyboard.isKeyPressed(KEY_BACKSPACE)) {
        M5Cardputer.update();
        delay(10);
    }
    currentState = MENU;
    drawMenu();
}
