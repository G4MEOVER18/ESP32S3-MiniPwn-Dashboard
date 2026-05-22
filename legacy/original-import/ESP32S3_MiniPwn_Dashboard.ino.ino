#include <WiFi.h>
#include <time.h>
#include <Arduino_GFX_Library.h>

// ------------------ Farben ------------------
#define BLACK   0x0000
#define WHITE   0xFFFF
#define RED     0xF800
#define GREEN   0x07E0
#define BLUE    0x001F
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define ORANGE  0xFD20

// ------------------ WLAN / Zeit ------------------
const char* ssid     = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";

long timezoneOffset = 1;   // UTC+1
byte daylight       = 1;   // Sommerzeit

bool wifiConnected = false;

// ------------------ Display / Pins ------------------
#define TFT_MOSI 45
#define TFT_SCLK 40
#define TFT_CS   42
#define TFT_DC   41
#define TFT_RST  39
#define TFT_BL   48

// SD-Pins (nur belegt, aktuell in diesem Sketch NICHT benutzt)
#define SD_CS    21
#define SD_SCK   14
#define SD_MOSI  15
#define SD_MISO  16

// Panelgröße – 1.47" ST7789, effektiv 172x320
#define PANEL_WIDTH  172
#define PANEL_HEIGHT 320

#define BTN_BOOT 0   // BOOT-Taste

Arduino_ESP32SPI *bus = new Arduino_ESP32SPI(
  TFT_DC, TFT_CS, TFT_SCLK, TFT_MOSI, GFX_NOT_DEFINED
);

// ST7789: Controller hat 240x320, sichtbarer Bereich 172 breit.
// → links/rechts je 34 Pixel Offsets setzen.
Arduino_GFX *gfx = new Arduino_ST7789(
  bus,
  TFT_RST,
  3,         // Rotation (quer, USB nach oben)
  true,      // IPS
  PANEL_WIDTH,
  PANEL_HEIGHT,
  34, 0,     // col_offset1, row_offset1
  34, 0      // col_offset2, row_offset2
);

// etwas weiter vom Rand weg, wegen Abrundung → 16px
const int MARGIN_X = 16;

// ------------------ Seiten / UI ------------------
int currentPage = 0;           // 0: Uhr, 1: WLAN-Liste, 2: Mein Netz, 3: System
int lastButtonState = HIGH;

// ------------------ Systeminfos ------------------
int  batteryPercent   = 100;      // Platzhalter
bool onExternalPower  = true;     // per USB → true

// ------------------ WiFi-Scan ------------------
const int MAX_APS = 12;
struct ApInfo {
  String ssid;
  int32_t rssi;
  uint8_t channel;
  wifi_auth_mode_t auth;
};

ApInfo apList[MAX_APS];
int    apCount      = 0;
unsigned long lastScanMillis = 0;
const unsigned long SCAN_INTERVAL = 15000;  // alle 15s WiFi-Scan

// ------------------ Helfer: Systeminfos ------------------
void getSystemInfo(size_t &freeHeap, uint32_t &freqMHz, int &rssi) {
  freeHeap = ESP.getFreeHeap();
  freqMHz  = getCpuFrequencyMhz();
  rssi     = (wifiConnected ? WiFi.RSSI() : -100);
}

int wifiBarsFromRSSI(int rssi) {
  if (!wifiConnected) return 0;
  if (rssi >= -55) return 4;
  if (rssi >= -65) return 3;
  if (rssi >= -75) return 2;
  if (rssi >= -85) return 1;
  return 0;
}

void drawWifiIcon(int x, int y, int bars) {
  int w = 4;
  int h = 3;
  for (int i = 0; i < 4; i++) {
    uint16_t color = (i < bars) ? GREEN : BLUE;
    gfx->fillRect(x + i*(w+1), y + (3-i)*h, w, h, color);
  }
}

void drawBatteryIcon(int x, int y, int percent) {
  gfx->drawRect(x, y, 22, 9, WHITE);
  gfx->fillRect(x+22, y+2, 2, 5, WHITE);
  int innerWidth = 22 - 2;
  int filled = (innerWidth * percent) / 100;
  gfx->fillRect(x+1, y+1, filled, 7, (percent > 20 ? GREEN : RED));
}

void drawPowerIcon(int x, int y) {
  gfx->drawLine(x,   y,   x+3, y+6, YELLOW);
  gfx->drawLine(x+3, y+6, x+1, y+6, YELLOW);
  gfx->drawLine(x+1, y+6, x+4, y+12, YELLOW);
}

// Icons oben rechts
void drawStatusIcons() {
  size_t freeHeap;
  uint32_t freqMHz;
  int rssi;
  getSystemInfo(freeHeap, freqMHz, rssi);
  int bars = wifiBarsFromRSSI(rssi);

  int baseX = PANEL_WIDTH - 70;
  drawWifiIcon(baseX, 6, bars);
  drawBatteryIcon(baseX+25, 6, batteryPercent);
  if (onExternalPower) {
    drawPowerIcon(baseX+10, 18);
  }
}

// ------------------ WLAN & Zeit ------------------
void connectWiFi() {
  WiFi.disconnect(true);
  delay(200);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");

  wifiConnected = false;
  for (int i = 0; i < 60; i++) {
    if (WiFi.status() == WL_CONNECTED) {
      wifiConnected = true;
      break;
    }
    delay(250);
    Serial.print(".");
  }
  Serial.println();

  if (wifiConnected) {
    Serial.print("WiFi OK, IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi connect FAILED");
  }
}

void setupTime() {
  if (!wifiConnected) return;

  configTime(3600 * timezoneOffset,
             daylight * 3600,
             "0.pool.ntp.org",
             "1.pool.ntp.org",
             "2.pool.ntp.org");

  struct tm t;
  for (int i = 0; i < 20; i++) {
    if (getLocalTime(&t, 1000)) {
      Serial.println("NTP synced");
      return;
    }
    delay(500);
  }
  Serial.println("NTP FAILED");
}

// ------------------ WiFi-Scan-Logik ------------------
const char* authModeToStr(wifi_auth_mode_t m) {
  switch (m) {
    case WIFI_AUTH_OPEN:           return "OPEN";
    case WIFI_AUTH_WEP:            return "WEP";
    case WIFI_AUTH_WPA_PSK:        return "WPA";
    case WIFI_AUTH_WPA2_PSK:       return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK:   return "WPA/WPA2";
    case WIFI_AUTH_WPA2_ENTERPRISE:return "WPA2-ENT";
    case WIFI_AUTH_WPA3_PSK:       return "WPA3";
    case WIFI_AUTH_WPA2_WPA3_PSK:  return "WPA2/WPA3";
    default:                       return "?";
  }
}

void performWifiScan() {
  if (!wifiConnected) {
    apCount = 0;
    return;
  }

  Serial.println("Scanning WiFi...");
  int n = WiFi.scanNetworks();
  if (n < 0) {
    Serial.println("scanNetworks() failed");
    apCount = 0;
    return;
  }

  apCount = (n > MAX_APS) ? MAX_APS : n;
  for (int i = 0; i < apCount; i++) {
    apList[i].ssid    = WiFi.SSID(i);
    apList[i].rssi    = WiFi.RSSI(i);
    apList[i].channel = WiFi.channel(i);
    apList[i].auth    = (wifi_auth_mode_t)WiFi.encryptionType(i);
  }
  WiFi.scanDelete();
  Serial.print("Found APs: ");
  Serial.println(n);
}

// ------------------ UI: Zeitblock (Seite 0) ------------------
void drawTimeBlock() {
  gfx->fillScreen(BLACK);

  struct tm t;
  bool got = getLocalTime(&t, 150);

  if (!got) {
    gfx->setTextSize(3);
    gfx->setTextColor(RED, BLACK);
    gfx->setCursor(MARGIN_X, 60);
    gfx->print("NO TIME");
    drawStatusIcons();
    return;
  }

  int hour   = t.tm_hour;
  int minute = t.tm_min;
  int second = t.tm_sec;

  uint16_t hourColor = (hour >= 7 && hour <= 16) ? GREEN : BLUE;

  uint16_t minuteColor;
  if      (minute <= 15) minuteColor = GREEN;
  else if (minute <= 30) minuteColor = YELLOW;
  else if (minute <= 45) minuteColor = ORANGE;
  else                   minuteColor = MAGENTA;

  char buf[8];

  int timeSize = 4;
  gfx->setTextSize(timeSize);

  int yTime = 70;

  // Stunden
  gfx->setCursor(MARGIN_X, yTime);
  gfx->setTextColor(hourColor, BLACK);
  snprintf(buf, sizeof(buf), "%02d", hour);
  gfx->print(buf);

  // :
  gfx->setTextColor(WHITE, BLACK);
  gfx->print(":");

  // Minuten
  gfx->setTextColor(minuteColor, BLACK);
  snprintf(buf, sizeof(buf), "%02d", minute);
  gfx->print(buf);

  // :
  gfx->setTextColor(WHITE, BLACK);
  gfx->print(":");

  // Sekunden
  snprintf(buf, sizeof(buf), "%02d", second);
  gfx->print(buf);

  // Datum
  gfx->setTextSize(2);
  gfx->setTextColor(WHITE, BLACK);
  int yDate = yTime + 8 * timeSize + 24;
  gfx->setCursor(MARGIN_X, yDate);
  char dateBuf[20];
  snprintf(dateBuf, sizeof(dateBuf), "%04d-%02d-%02d",
           t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
  gfx->print(dateBuf);

  drawStatusIcons();
}

// ------------------ Seite 0: Uhr ------------------
void drawPage0() {
  drawTimeBlock();
}

// ------------------ Seite 1: WLAN-Liste ------------------
void drawPage1() {
  gfx->fillScreen(BLACK);
  drawStatusIcons();

  int x = MARGIN_X;
  int y = 32;

  gfx->setTextSize(2);
  gfx->setTextColor(CYAN, BLACK);
  gfx->setCursor(x, y);
  gfx->print("WiFi Scan");
  y += 20;

  gfx->setTextSize(1);

  if (!wifiConnected) {
    gfx->setCursor(x, y);
    gfx->setTextColor(RED, BLACK);
    gfx->print("WiFi not connected");
    return;
  }

  if (apCount == 0) {
    gfx->setCursor(x, y);
    gfx->setTextColor(YELLOW, BLACK);
    gfx->print("Scanning...");
    return;
  }

  for (int i = 0; i < apCount && i < 7; i++) {
    gfx->setCursor(x, y);
    gfx->setTextColor(WHITE, BLACK);

    String s = apList[i].ssid;
    if (s.length() > 12) s = s.substring(0, 11) + ">";

    gfx->print(i);
    gfx->print(": ");
    gfx->print(s);
    y += 10;

    gfx->setCursor(x, y);
    gfx->setTextColor(GREEN, BLACK);
    gfx->print(apList[i].rssi);
    gfx->print(" dBm  Ch:");
    gfx->print(apList[i].channel);
    gfx->print("  ");
    gfx->print(authModeToStr(apList[i].auth));
    y += 12;

    if (y > PANEL_HEIGHT - 14) break;
  }
}

// ------------------ Seite 2: Mein Netzwerk ------------------
void drawPage2() {
  gfx->fillScreen(BLACK);
  drawStatusIcons();

  int x = MARGIN_X;
  int y = 32;

  gfx->setTextSize(2);
  gfx->setTextColor(CYAN, BLACK);
  gfx->setCursor(x, y);
  gfx->print("Mein Netz");
  y += 20;

  gfx->setTextSize(1);

  if (!wifiConnected) {
    gfx->setCursor(x, y);
    gfx->setTextColor(RED, BLACK);
    gfx->print("Nicht verbunden");
    return;
  }

  // SSID
  gfx->setCursor(x, y);
  gfx->setTextColor(WHITE, BLACK);
  gfx->print("SSID: ");
  gfx->print(WiFi.SSID());
  y += 12;

  // IP
  gfx->setCursor(x, y);
  gfx->print("IP  : ");
  gfx->print(WiFi.localIP());
  y += 12;

  // RSSI
  int rssi = WiFi.RSSI();
  gfx->setCursor(x, y);
  gfx->print("RSSI: ");
  gfx->print(rssi);
  gfx->print(" dBm");
  y += 12;

  // Kanal
  uint8_t ch = WiFi.channel();
  gfx->setCursor(x, y);
  gfx->print("Kanal: ");
  gfx->print(ch);
  y += 12;

  // Verschlüsselung – wir schauen im Scan nach eigener SSID
  wifi_auth_mode_t authMode = WIFI_AUTH_OPEN;
  for (int i = 0; i < apCount; i++) {
    if (apList[i].ssid == WiFi.SSID()) {
      authMode = apList[i].auth;
      break;
    }
  }

  gfx->setCursor(x, y);
  gfx->print("Auth: ");
  gfx->print(authModeToStr(authMode));
  y += 14;

  // einfache Qualitätsanzeige (auf Basis RSSI)
  int bars = wifiBarsFromRSSI(rssi);
  gfx->setCursor(x, y);
  gfx->setTextColor(YELLOW, BLACK);
  gfx->print("Qualitaet: ");
  gfx->print(bars);
  gfx->print("/4");
  y += 12;

  // kleiner horizontaler "Signalbalken"
  int barX = x;
  int barY = y;
  int barW = 100;
  int barH = 8;
  gfx->drawRect(barX, barY, barW, barH, WHITE);
  int fillW = (bars * barW) / 4;
  gfx->fillRect(barX+1, barY+1, fillW-2, barH-2, GREEN);
}

// ------------------ Seite 3: System ------------------
void drawPage3() {
  gfx->fillScreen(BLACK);
  drawStatusIcons();

  int x = MARGIN_X;
  int y = 32;

  gfx->setTextSize(2);
  gfx->setTextColor(CYAN, BLACK);
  gfx->setCursor(x, y);
  gfx->print("System");
  y += 20;

  gfx->setTextSize(1);

  size_t freeHeap;
  uint32_t freqMHz;
  int rssi;
  getSystemInfo(freeHeap, freqMHz, rssi);

  uint32_t uptimeSec = millis() / 1000;
  uint32_t upMin  = uptimeSec / 60;
  uint32_t upHour = upMin / 60;

  gfx->setCursor(x, y);
  gfx->setTextColor(WHITE, BLACK);
  gfx->print("Heap : ");
  gfx->print(freeHeap / 1024);
  gfx->print(" kB");
  y += 10;

  gfx->setCursor(x, y);
  gfx->print("CPU  : ");
  gfx->print(freqMHz);
  gfx->print(" MHz");
  y += 10;

  gfx->setCursor(x, y);
  gfx->print("Uptime: ");
  gfx->print(upHour);
  gfx->print("h ");
  gfx->print(upMin % 60);
  gfx->print("m");
  y += 10;

  gfx->setCursor(x, y);
  gfx->print("WiFi : ");
  gfx->print(wifiConnected ? "OK" : "NO");
  y += 10;

  gfx->setCursor(x, y);
  gfx->print("APs  : ");
  gfx->print(apCount);
  y += 10;

  // Letzter Scan vor X Sekunden
  unsigned long now = millis();
  unsigned long delta = (lastScanMillis == 0) ? 0 : (now - lastScanMillis) / 1000;
  gfx->setCursor(x, y);
  gfx->print("Scan vor: ");
  gfx->print(delta);
  gfx->print(" s");
}

// ------------------ Boot-Ladebalken ------------------
void drawBootProgress(int percent, const char* label) {
  gfx->fillScreen(BLACK);
  gfx->setTextColor(WHITE, BLACK);
  gfx->setTextSize(2);
  gfx->setCursor(MARGIN_X, 50);
  gfx->print("Booting...");

  gfx->setTextSize(1);
  gfx->setCursor(MARGIN_X, 70);
  gfx->print(label);

  int barX = MARGIN_X;
  int barY = 90;
  int barW = PANEL_WIDTH - 2*MARGIN_X;
  int barH = 10;

  gfx->drawRect(barX, barY, barW, barH, WHITE);
  int fillW = (barW-2) * percent / 100;
  gfx->fillRect(barX+1, barY+1, fillW, barH-2, GREEN);
}

// ------------------ Button-Handling ------------------
void handleButton() {
  int state = digitalRead(BTN_BOOT);
  if (state == LOW && lastButtonState == HIGH) {
    currentPage++;
    if (currentPage > 3) currentPage = 0;
  }
  lastButtonState = state;
}

// ------------------ setup ------------------
void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  pinMode(BTN_BOOT, INPUT_PULLUP);
  lastButtonState = digitalRead(BTN_BOOT);

  gfx->begin();
  gfx->fillScreen(BLACK);

  drawBootProgress(5, "Init Display");

  drawBootProgress(25, "WiFi verbinden");
  connectWiFi();
  setupTime();

  // erster WiFi-Scan
  drawBootProgress(60, "WiFi scan");
  performWifiScan();
  lastScanMillis = millis();

  drawBootProgress(100, "Fertig");
  delay(600);

  currentPage = 0;
}

// ------------------ loop ------------------
void loop() {
  static unsigned long lastUpdate = 0;
  static unsigned long lastWifiCheck = 0;
  unsigned long now = millis();

  // Seiten regelmäßig neu zeichnen
  if (now - lastUpdate > 1000) {
    lastUpdate = now;
    if      (currentPage == 0) drawPage0();
    else if (currentPage == 1) drawPage1();
    else if (currentPage == 2) drawPage2();
    else if (currentPage == 3) drawPage3();
  }

  // WLAN alle 30s prüfen
  if (now - lastWifiCheck > 30000) {
    lastWifiCheck = now;
    if (WiFi.status() != WL_CONNECTED) {
      wifiConnected = false;
      connectWiFi();
      setupTime();
      performWifiScan();
      lastScanMillis = millis();
    }
  }

  // regelmäßiger WiFi-Scan
  if (wifiConnected && (now - lastScanMillis > SCAN_INTERVAL)) {
    performWifiScan();
    lastScanMillis = now;
  }

  handleButton();
}
