/*
 Industrial Grade NTP Digital Clock
 WT32-ETH01 with 160x128 TFT LCD (HORIZONTAL)
 
 Features:
 - Sprite-based smooth rendering
 - Professional industrial UI design
 - Robust error handling and auto-recovery
 - Connection quality monitoring
 - Uptime tracking
 - Watchdog timer
 - Visual status indicators
 
 Hardware:
 - WT32-ETH01 (LAN8720 Ethernet)
 - 160x128 TFT Display (ST7735 or ILI9163) - HORIZONTAL
 
 Author: Industrial Automation Design
 Version: 2.1 - Sprite Edition
*/

#include <TFT_eSPI.h>
#include <SPI.h>
#include <ETH.h>
#include <time.h>
#include <esp_task_wdt.h>

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite sprite = TFT_eSprite(&tft);

// ============ CONFIGURATION ============
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 21600;       // UTC+6 for Dhaka (6 * 3600 seconds)
const int   daylightOffset_sec = 0;      // DST offset in seconds
const int   WDT_TIMEOUT = 30;            // Watchdog timeout (seconds)

// Ethernet Configuration (WT32-ETH01)
#define ETH_CLK_MODE    ETH_CLOCK_GPIO0_IN
#define ETH_POWER_PIN   16
#define ETH_TYPE        ETH_PHY_LAN8720
#define ETH_ADDR        1
#define ETH_MDC_PIN     23
#define ETH_MDIO_PIN    18

// ============ COLOR SCHEME ============
// Industrial dark theme with accent colors
#define COLOR_BG           0x0841  // Dark blue-gray
#define COLOR_STATUS_BAR   0x1082  // Darker blue-gray
#define COLOR_PRIMARY      0x07FF  // Cyan
#define COLOR_SUCCESS      0x07E0  // Green
#define COLOR_WARNING      0xFD20  // Orange
#define COLOR_ERROR        0xF800  // Red
#define COLOR_TEXT         0xFFFF  // White
#define COLOR_TEXT_DIM     0x8410  // Gray
#define COLOR_ACCENT       0x051D  // Navy blue
#define COLOR_TIME         0xFFE0  // Yellow
#define COLOR_DATE         0x7FFF  // Light cyan

// ============ DISPLAY LAYOUT - HORIZONTAL ============
#define SCREEN_WIDTH  160
#define SCREEN_HEIGHT 128

// Status bar (top)
#define STATUS_BAR_HEIGHT 12
#define STATUS_BAR_Y      0

// Time display (center-left)
#define TIME_X           8
#define TIME_Y           22
#define TIME_WIDTH       100
#define TIME_HEIGHT      45

// Date display (below time)
#define DATE_X           8
#define DATE_Y           72

// Info bar (right side)
#define INFO_BAR_X       112
#define INFO_BAR_WIDTH   48
#define INFO_BAR_Y       STATUS_BAR_HEIGHT

// Status area (bottom)
#define STATUS_AREA_Y    95
#define STATUS_AREA_H    20

// Bottom bar
#define BOTTOM_BAR_Y     115
#define BOTTOM_BAR_H     13

// ============ STATE VARIABLES ============
struct SystemState {
  bool ethConnected = false;
  bool timeConfigured = false;
  bool ntpSynced = false;
  uint8_t lastHour = 99;
  uint8_t lastMinute = 99;
  uint8_t lastSecond = 99;
  char lastDateStr[32] = "";
  unsigned long bootTime = 0;
  unsigned long lastSyncTime = 0;
  unsigned long lastReconnectAttempt = 0;
  uint8_t reconnectAttempts = 0;
  int8_t signalQuality = -1;
  uint16_t linkSpeed = 0;
  bool initialBoot = true;
} state;

// ============ TIMING ============
unsigned long targetTime = 0;
unsigned long lastWatchdogReset = 0;
const unsigned long RESYNC_INTERVAL = 3600000;  // Resync every hour
const unsigned long RECONNECT_DELAY = 5000;      // Wait 5s between reconnects

// ============ GRAPHICS PRIMITIVES ============

void drawConnectionIcon(int16_t x, int16_t y, bool connected, int8_t quality) {
  uint16_t color = connected ? COLOR_SUCCESS : COLOR_ERROR;
  
  // Draw signal bars
  for (int i = 0; i < 4; i++) {
    int barHeight = 2 + (i * 2);
    int barX = x + (i * 3);
    int barY = y + 8 - barHeight;
    
    if (connected && i <= quality) {
      sprite.fillRect(barX, barY, 2, barHeight, color);
    } else {
      sprite.drawRect(barX, barY, 2, barHeight, COLOR_TEXT_DIM);
    }
  }
}

void drawStatusLED(int16_t x, int16_t y, uint16_t color) {
  sprite.fillCircle(x, y, 3, color);
  sprite.drawCircle(x, y, 4, color);
}

// ============ UI COMPONENTS ============

void drawStatusBar() {
  // Create sprite for status bar
  sprite.createSprite(SCREEN_WIDTH, STATUS_BAR_HEIGHT);
  sprite.fillSprite(COLOR_STATUS_BAR);
  
  // Draw bottom line
  sprite.drawLine(0, STATUS_BAR_HEIGHT - 1, SCREEN_WIDTH, STATUS_BAR_HEIGHT - 1, COLOR_ACCENT);
  
  // System title
  sprite.setTextColor(COLOR_PRIMARY, COLOR_STATUS_BAR);
  sprite.setTextSize(1);
  sprite.setCursor(4, 2);
  sprite.print("NTP CLOCK v2.1");
  
  // Connection icon
  drawConnectionIcon(120, 1, state.ethConnected, state.signalQuality);
  
  // Status LED
  uint16_t ledColor = COLOR_ERROR;
  if (state.ethConnected && state.timeConfigured) {
    ledColor = COLOR_SUCCESS;
  } else if (state.ethConnected) {
    ledColor = COLOR_WARNING;
  }
  drawStatusLED(150, 6, ledColor);
  
  // Push to screen
  sprite.pushSprite(0, STATUS_BAR_Y);
  sprite.deleteSprite();
}

void drawTimeDisplay(uint8_t hh, uint8_t mm, uint8_t ss) {
  // Create sprite for time area
  sprite.createSprite(TIME_WIDTH, TIME_HEIGHT);
  sprite.fillSprite(COLOR_BG);
  
  // Draw time container
  sprite.fillRoundRect(0, 0, TIME_WIDTH, TIME_HEIGHT, 4, COLOR_ACCENT);
  sprite.drawRoundRect(0, 0, TIME_WIDTH, TIME_HEIGHT, 4, COLOR_PRIMARY);
  
  // Format time string
  char timeStr[9];
  sprintf(timeStr, "%02d:%02d", hh, mm);
  
  // Draw time (medium size - font 4)
  sprite.setTextColor(COLOR_TIME, COLOR_ACCENT);
  sprite.setTextDatum(MC_DATUM);
  sprite.drawString(timeStr, TIME_WIDTH / 2, 16, 4);
  
  // Draw seconds
  char secStr[4];
  sprintf(secStr, ":%02d", ss);
  sprite.setTextColor(COLOR_TEXT_DIM, COLOR_ACCENT);
  sprite.drawString(secStr, TIME_WIDTH / 2, 32, 2);
  
  // Push to screen
  sprite.pushSprite(TIME_X, TIME_Y);
  sprite.deleteSprite();
}

void drawDateDisplay(struct tm* timeinfo) {
  char dateStr[32];
  strftime(dateStr, sizeof(dateStr), "%a, %b %d %Y", timeinfo);
  
  if (strcmp(dateStr, state.lastDateStr) != 0 || state.initialBoot) {
    // Create sprite for date
    sprite.createSprite(100, 18);
    sprite.fillSprite(COLOR_BG);
    
    // Draw date
    sprite.setTextColor(COLOR_DATE, COLOR_BG);
    sprite.setTextDatum(ML_DATUM);
    sprite.drawString(dateStr, 0, 9, 1);
    
    // Push to screen
    sprite.pushSprite(DATE_X, DATE_Y);
    sprite.deleteSprite();
    
    strcpy(state.lastDateStr, dateStr);
  }
}

void drawInfoBar() {
  // Create sprite for info bar
  sprite.createSprite(INFO_BAR_WIDTH, SCREEN_HEIGHT - STATUS_BAR_HEIGHT - BOTTOM_BAR_H);
  sprite.fillSprite(COLOR_STATUS_BAR);
  
  // Draw left border
  sprite.drawLine(0, 0, 0, sprite.height(), COLOR_ACCENT);
  
  // Calculate uptime
  unsigned long uptime = (millis() - state.bootTime) / 1000;
  uint16_t hours = uptime / 3600;
  uint8_t mins = (uptime % 3600) / 60;
  
  // UPTIME section
  sprite.setTextColor(COLOR_TEXT_DIM, COLOR_STATUS_BAR);
  sprite.setTextSize(1);
  sprite.setTextDatum(TC_DATUM);
  sprite.drawString("UPTIME", INFO_BAR_WIDTH / 2, 4, 1);
  
  sprite.setTextColor(COLOR_PRIMARY, COLOR_STATUS_BAR);
  char uptimeStr[12];
  if (hours < 100) {
    sprintf(uptimeStr, "%dh %dm", hours, mins);
    sprite.drawString(uptimeStr, INFO_BAR_WIDTH / 2, 14, 1);
  } else {
    sprintf(uptimeStr, "%dh", hours);
    sprite.drawString(uptimeStr, INFO_BAR_WIDTH / 2, 14, 1);
  }
  
  // STATUS section
  sprite.setTextColor(COLOR_TEXT_DIM, COLOR_STATUS_BAR);
  sprite.drawString("STATUS", INFO_BAR_WIDTH / 2, 32, 1);
  
  if (state.ethConnected) {
    sprite.setTextColor(COLOR_SUCCESS, COLOR_STATUS_BAR);
    sprite.drawString("ONLINE", INFO_BAR_WIDTH / 2, 42, 1);
  } else {
    sprite.setTextColor(COLOR_ERROR, COLOR_STATUS_BAR);
    sprite.drawString("OFFLINE", INFO_BAR_WIDTH / 2, 42, 1);
  }
  
  if (state.ntpSynced) {
    sprite.setTextColor(COLOR_SUCCESS, COLOR_STATUS_BAR);
    sprite.drawString("SYNCED", INFO_BAR_WIDTH / 2, 52, 1);
  } else {
    sprite.setTextColor(COLOR_WARNING, COLOR_STATUS_BAR);
    sprite.drawString("NO SYNC", INFO_BAR_WIDTH / 2, 52, 1);
  }
  
  // LINK SPEED section
  sprite.setTextColor(COLOR_TEXT_DIM, COLOR_STATUS_BAR);
  sprite.drawString("SPEED", INFO_BAR_WIDTH / 2, 70, 1);
  
  if (state.ethConnected && state.linkSpeed > 0) {
    sprite.setTextColor(COLOR_PRIMARY, COLOR_STATUS_BAR);
    char speedStr[16];
    sprintf(speedStr, "%dmb/s", state.linkSpeed);
    sprite.drawString(speedStr, INFO_BAR_WIDTH / 2, 80, 1);
  } else {
    sprite.setTextColor(COLOR_TEXT_DIM, COLOR_STATUS_BAR);
    sprite.drawString("---mb/s", INFO_BAR_WIDTH / 2, 80, 1);
  }
  
  // Push to screen
  sprite.pushSprite(INFO_BAR_X, INFO_BAR_Y);
  sprite.deleteSprite();
}

void drawBottomBar() {
  // Create sprite for bottom bar
  sprite.createSprite(INFO_BAR_X, BOTTOM_BAR_H);
  sprite.fillSprite(COLOR_STATUS_BAR);
  
  // Draw top line
  sprite.drawLine(0, 0, sprite.width(), 0, COLOR_ACCENT);
  
  // IP Address
  sprite.setTextColor(COLOR_TEXT_DIM, COLOR_STATUS_BAR);
  sprite.setTextSize(1);
  sprite.setTextDatum(ML_DATUM);
  sprite.drawString("IP:", 2, 6, 1);
  
  if (state.ethConnected) {
    sprite.setTextColor(COLOR_SUCCESS, COLOR_STATUS_BAR);
    String ip = ETH.localIP().toString();
    sprite.drawString(ip, 16, 6, 1);
  } else {
    sprite.setTextColor(COLOR_ERROR, COLOR_STATUS_BAR);
    sprite.drawString("---.---.---.---", 16, 6, 1);
  }
  
  // Push to screen
  sprite.pushSprite(0, BOTTOM_BAR_Y);
  sprite.deleteSprite();
}

void drawStatusArea(const char* message, uint16_t color) {
  // Create sprite for status message
  sprite.createSprite(INFO_BAR_X, STATUS_AREA_H);
  sprite.fillSprite(COLOR_BG);
  
  // Draw status message
  sprite.setTextColor(color, COLOR_BG);
  sprite.setTextDatum(MC_DATUM);
  sprite.drawString(message, sprite.width() / 2, sprite.height() / 2, 2);
  
  // Push to screen
  sprite.pushSprite(0, STATUS_AREA_Y);
  sprite.deleteSprite();
}

void drawBootScreen(const char* message, uint16_t color, int progress = -1) {
  static int lastProgress = -1;
  
  if (lastProgress == -1) {
    tft.fillScreen(COLOR_BG);
    
    // Create title sprite
    sprite.createSprite(140, 35);
    sprite.fillRoundRect(0, 0, 140, 35, 5, COLOR_ACCENT);
    sprite.drawRoundRect(0, 0, 140, 35, 5, COLOR_PRIMARY);
    sprite.setTextColor(COLOR_PRIMARY, COLOR_ACCENT);
    sprite.setTextDatum(MC_DATUM);
    sprite.drawString("INDUSTRIAL", 70, 12, 2);
    sprite.drawString("NTP CLOCK", 70, 26, 2);
    sprite.pushSprite(10, 20);
    sprite.deleteSprite();
    
    // Version
    tft.setTextColor(COLOR_TEXT_DIM, COLOR_BG);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("v2.1 Sprite Edition", SCREEN_WIDTH / 2, 60, 1);
  }
  
  // Status message
  sprite.createSprite(140, 20);
  sprite.fillSprite(COLOR_BG);
  sprite.setTextColor(color, COLOR_BG);
  sprite.setTextDatum(MC_DATUM);
  sprite.drawString(message, 70, 10, 2);
  sprite.pushSprite(10, 70);
  sprite.deleteSprite();
  
  // Progress bar
  if (progress >= 0 && progress != lastProgress) {
    sprite.createSprite(120, 12);
    sprite.fillSprite(COLOR_BG);
    sprite.drawRoundRect(0, 0, 120, 12, 3, COLOR_PRIMARY);
    
    int fillWidth = 116 * progress / 100;
    if (fillWidth > 0) {
      sprite.fillRoundRect(2, 2, fillWidth, 8, 2, COLOR_SUCCESS);
    }
    
    sprite.pushSprite(20, 95);
    sprite.deleteSprite();
    
    lastProgress = progress;
  }
}

// ============ ETHERNET EVENT HANDLER ============

void WiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      Serial.println("ETH Started");
      break;
      
    case ARDUINO_EVENT_ETH_CONNECTED:
      Serial.println("ETH Connected");
      break;
      
    case ARDUINO_EVENT_ETH_GOT_IP:
      state.ethConnected = true;
      state.signalQuality = 3;
      state.reconnectAttempts = 0;
      state.linkSpeed = ETH.linkSpeed();
      Serial.println("ETH Got IP: " + ETH.localIP().toString());
      Serial.println("MAC: " + ETH.macAddress());
      Serial.println("Speed: " + String(state.linkSpeed) + " Mbps");
      
      configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
      break;
      
    case ARDUINO_EVENT_ETH_DISCONNECTED:
      Serial.println("ETH Disconnected");
      state.ethConnected = false;
      state.signalQuality = -1;
      break;
      
    case ARDUINO_EVENT_ETH_STOP:
      Serial.println("ETH Stopped");
      state.ethConnected = false;
      state.signalQuality = -1;
      break;
      
    default:
      break;
  }
}

// ============ TIME MANAGEMENT ============

bool syncTime() {
  struct tm timeinfo;
  int attempts = 0;
  const int maxAttempts = 10;
  
  while (attempts < maxAttempts) {
    if (getLocalTime(&timeinfo)) {
      state.timeConfigured = true;
      state.ntpSynced = true;
      state.lastSyncTime = millis();
      Serial.println("Time synced successfully");
      return true;
    }
    delay(500);
    attempts++;
  }
  
  Serial.println("Time sync failed");
  return false;
}

void checkTimeSync() {
  if (state.ethConnected && state.timeConfigured) {
    if (millis() - state.lastSyncTime > RESYNC_INTERVAL) {
      Serial.println("Performing periodic time resync...");
      syncTime();
    }
  }
}

// ============ NETWORK RECOVERY ============

void attemptReconnect() {
  if (!state.ethConnected && 
      (millis() - state.lastReconnectAttempt > RECONNECT_DELAY)) {
    
    state.lastReconnectAttempt = millis();
    state.reconnectAttempts++;
    
    Serial.println("Attempting reconnect #" + String(state.reconnectAttempts));
    
    ETH.begin(ETH_ADDR, ETH_POWER_PIN, ETH_MDC_PIN, ETH_MDIO_PIN, ETH_TYPE, ETH_CLK_MODE);
  }
}

// ============ SETUP ============

void setup(void) {
  Serial.begin(115200);
  delay(100);
  
  Serial.println("\n\n=================================");
  Serial.println("Industrial NTP Clock v2.1 Sprite");
  Serial.println("=================================\n");
  
  // Initialize watchdog
  esp_task_wdt_init(WDT_TIMEOUT, true);
  esp_task_wdt_add(NULL);
  
  // Initialize TFT - HORIZONTAL
  tft.init();
  tft.setRotation(1); // Landscape mode (0=portrait, 1=landscape)
  tft.fillScreen(COLOR_BG);
  
  state.bootTime = millis();
  
  // Boot sequence
  drawBootScreen("Initializing...", COLOR_PRIMARY, 0);
  delay(500);
  
  drawBootScreen("Starting ETH...", COLOR_PRIMARY, 25);
  WiFi.onEvent(WiFiEvent);
  ETH.begin(ETH_ADDR, ETH_POWER_PIN, ETH_MDC_PIN, ETH_MDIO_PIN, ETH_TYPE, ETH_CLK_MODE);
  delay(1000);
  
  drawBootScreen("Connecting...", COLOR_WARNING, 50);
  int timeout = 20;
  while (!state.ethConnected && timeout > 0) {
    delay(1000);
    timeout--;
    esp_task_wdt_reset();
  }
  
  if (state.ethConnected) {
    drawBootScreen("Connected!", COLOR_SUCCESS, 75);
    delay(500);
    
    drawBootScreen("Syncing Time...", COLOR_PRIMARY, 85);
    if (syncTime()) {
      drawBootScreen("Ready!", COLOR_SUCCESS, 100);
      delay(1000);
    } else {
      drawBootScreen("Sync Failed", COLOR_WARNING, 90);
      delay(2000);
    }
  } else {
    drawBootScreen("No Connection", COLOR_ERROR, 50);
    delay(2000);
  }
  
  // Initialize main screen
  tft.fillScreen(COLOR_BG);
  drawStatusBar();
  drawInfoBar();
  drawBottomBar();
  
  targetTime = millis() + 1000;
  state.initialBoot = true;
  lastWatchdogReset = millis();
  
  Serial.println("Setup complete. Entering main loop.\n");
}

// ============ MAIN LOOP ============

void loop() {
  // Reset watchdog
  if (millis() - lastWatchdogReset > 5000) {
    esp_task_wdt_reset();
    lastWatchdogReset = millis();
  }
  
  // Check for reconnection needs
  if (!state.ethConnected) {
    attemptReconnect();
  }
  
  // Check time sync status
  checkTimeSync();
  
  // Update display every second
  if (targetTime < millis()) {
    targetTime = millis() + 1000;
    
    // Update status bar
    drawStatusBar();
    
    if (state.timeConfigured) {
      struct tm timeinfo;
      if (getLocalTime(&timeinfo)) {
        uint8_t hh = timeinfo.tm_hour;
        uint8_t mm = timeinfo.tm_min;
        uint8_t ss = timeinfo.tm_sec;
        
        // Update time display
        drawTimeDisplay(hh, mm, ss);
        
        // Update date display
        drawDateDisplay(&timeinfo);
        
        // Clear status area on first successful display
        if (state.initialBoot) {
          sprite.createSprite(INFO_BAR_X, STATUS_AREA_H);
          sprite.fillSprite(COLOR_BG);
          sprite.pushSprite(0, STATUS_AREA_Y);
          sprite.deleteSprite();
        }
        
        // Update state
        state.lastHour = hh;
        state.lastMinute = mm;
        state.lastSecond = ss;
        
      } else {
        drawStatusArea("TIME SYNC LOST", COLOR_ERROR);
        state.timeConfigured = false;
      }
    } else {
      if (state.ethConnected) {
        drawStatusArea("SYNCING...", COLOR_WARNING);
        syncTime();
      } else {
        drawStatusArea("NO NETWORK", COLOR_ERROR);
      }
    }
    
    // Update info bar and bottom bar every 10 seconds
    static uint8_t infoUpdateCounter = 0;
    if (infoUpdateCounter++ >= 10) {
      drawInfoBar();
      drawBottomBar();
      infoUpdateCounter = 0;
    }
    
    if (state.initialBoot) {
      state.initialBoot = false;
    }
  }
  
  delay(10);
}