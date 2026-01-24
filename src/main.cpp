#include <M5Cardputer.h>
#include <SdFat.h>

// Pins
#define SD_SCK_PIN   40
#define SD_MISO_PIN  39
#define SD_MOSI_PIN  14
#define SD_CS_PIN    12
#define SPI_CLOCK SD_SCK_MHZ(25)

// Global Objects
SdFat sd;
SPIClass sdSpi(HSPI);
int menuIndex = 0;
const char* menuItems[] = {
    "1. Card Info (CID)",
    "2. Speed Test",
    "3. Integrity Check",
    "4. Format (Quick)",
    "5. Reboot"
};

enum State { MENU, INFO, SPEED, H2TEST, FORMAT };
State currentState = MENU;

// --- Forward Declarations ---
void drawMenu();
void showCardInfo();
void runSpeedTest();
void runIntegrityCheck();
void runFormat();
void waitForInput();

bool initSD() {
    // Note: use the global sdSpi we initialized in setup
    if (!sd.begin(SdSpiConfig(SD_CS_PIN, SHARED_SPI, SPI_CLOCK, &sdSpi))) {
        return false;
    }
    return true;
}

void setup() {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg);
    M5.Display.setRotation(1);
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);

    // Initialize Global SPI
    sdSpi.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
    
    drawMenu();
}

void loop() {
    M5Cardputer.update();
    
    if (currentState == MENU) {
        if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
            if (M5Cardputer.Keyboard.isKeyPressed(';')) { 
                menuIndex = (menuIndex + 4) % 5;
                drawMenu();
            }
            else if (M5Cardputer.Keyboard.isKeyPressed('.')) {
                menuIndex = (menuIndex + 1) % 5;
                drawMenu();
            }
            else if (M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER)) {
                switch (menuIndex) {
                    case 0: currentState = INFO;   showCardInfo();      break;
                    case 1: currentState = SPEED;  runSpeedTest();      break;
                    case 2: currentState = H2TEST; runIntegrityCheck(); break;
                    case 3: currentState = FORMAT; runFormat();         break;
                    case 4: ESP.restart();                               break;
                }
            }
        }
    }
}

// ... Rest of your UI and Tool logic follows here ...


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
    while (f.read(buf, 4096) > 0) {
        // could add M5Cardputer.update() here if needed
    }
    float readTime = (millis() - s) / 1000.0f;
    M5.Display.printf("Read:  %.2f MB/s\n", 5.0f / readTime);

    f.close();
    sd.remove("spd.tmp");

    waitForInput();
}

// --- Integrity Check (H2TestW‑style) ---

void runIntegrityCheck() {
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setCursor(0, 0);
    M5.Display.println("Integrity Check (50MB)\n");
    M5.Display.println("ENTER: start   BKSP: abort");

    // Wait for ENTER to start or BACKSPACE to abort
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
    uint32_t limit = 50 * 1024 * 1024;
    uint32_t total = 0;

    M5.Display.println("\nWriting...");

    while (total < limit) {
        uint32_t* p = (uint32_t*)buf;
        for (int i = 0; i < 128; i++) p[i] = total + i * 4;

        if (f.write(buf, 512) != 512) {
            M5.Display.setTextColor(TFT_RED, TFT_BLACK);
            M5.Display.println("\nWrite error");
            break;
        }
        total += 512;

        if (total % (1024 * 1024) == 0) {
            M5.Display.setCursor(0, 40);
            M5.Display.printf("Written: %d MB   ", total / 1024 / 1024);
        }

        M5Cardputer.update();
        if (M5Cardputer.Keyboard.isKeyPressed(KEY_BACKSPACE)) {
            M5.Display.setCursor(0, 60);
            M5.Display.println("\nAborted by user");
            break;
        }
    }

    f.sync();
    f.rewind();

    M5.Display.setCursor(0, 80);
    M5.Display.println("Verifying...");

    uint32_t r = 0, err = 0;

    while (r < total) {
        int n = f.read(buf, 512);
        if (n != 512) {
            M5.Display.setTextColor(TFT_RED, TFT_BLACK);
            M5.Display.println("\nRead error");
            break;
        }

        uint32_t* p = (uint32_t*)buf;
        for (int i = 0; i < 128; i++) {
            if (p[i] != r + i * 4) err++;
        }

        r += 512;

        if (r % (1024 * 1024) == 0) {
            M5.Display.setCursor(0, 100);
            M5.Display.printf("Verified: %d MB   ", r / 1024 / 1024);
        }

        M5Cardputer.update();
        if (M5Cardputer.Keyboard.isKeyPressed(KEY_BACKSPACE)) {
            M5.Display.setCursor(0, 120);
            M5.Display.println("\nAborted by user");
            break;
        }
    }

    f.close();
    sd.remove("test.h2w");

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
    M5.Display.println("ENTER: format   BKSP: abort");

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

        // Write a tiny test file
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

// --- Return to menu (ENTER = back, BKSP = abort) ---

void waitForInput() {
    M5.Display.println("\nPress ENTER to return");

    // Debounce ENTER if currently held
    while (M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER)) {
        M5Cardputer.update();
        delay(10);
    }

    // Wait for ENTER (back) or BACKSPACE (abort)
    while (!M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER) &&
           !M5Cardputer.Keyboard.isKeyPressed(KEY_BACKSPACE)) {
        M5Cardputer.update();
        delay(10);
    }

    currentState = MENU;
    drawMenu();
}
