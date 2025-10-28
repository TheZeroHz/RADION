/*
 Industrial Buzzer Control System with TFT Display
 ESP32 with 160x128 TFT LCD and 3-Button Control
 
 Features:
 - TFT menu interface with visual feedback
 - 3-button navigation (Left/OK/Right)
 - Multiple buzzer patterns
 - Real-time status display
 - Debounced button inputs using Bounce2
 
 Hardware:
 - ESP32 (any variant)
 - 160x128 TFT Display (ST7735/ILI9163) - HORIZONTAL
 - Active/Passive Buzzer on GPIO 33
 - 3 Push Buttons:
   * GPIO 39 - LEFT button
   * GPIO 35 - OK button (select/stop)
   * GPIO 36 - RIGHT button
 
 Author: Industrial Automation Design
 Version: 1.0
*/

#include <TFT_eSPI.h>
#include <SPI.h>
#include <Bounce2.h>

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite sprite = TFT_eSprite(&tft);

// ============ PIN CONFIGURATION ============
#define BUZZER_PIN 33
#define BTN_LEFT   39
#define BTN_OK     35
#define BTN_RIGHT  36

#define BUZZER_CHANNEL 0
#define BUZZER_RESOLUTION 8

// ============ COLOR SCHEME ============
#define COLOR_BG           0x0841  // Dark blue-gray
#define COLOR_HEADER       0x1082  // Darker blue-gray
#define COLOR_PRIMARY      0x07FF  // Cyan
#define COLOR_SUCCESS      0x07E0  // Green
#define COLOR_WARNING      0xFD20  // Orange
#define COLOR_ERROR        0xF800  // Red
#define COLOR_TEXT         0xFFFF  // White
#define COLOR_TEXT_DIM     0x8410  // Gray
#define COLOR_ACCENT       0x051D  // Navy blue
#define COLOR_SELECTED     0xFD20  // Orange highlight

// ============ DISPLAY LAYOUT ============
#define SCREEN_WIDTH  160
#define SCREEN_HEIGHT 128
#define HEADER_HEIGHT 14
#define FOOTER_HEIGHT 18
#define MENU_START_Y  HEADER_HEIGHT
#define MENU_ITEM_H   15

// ============ BUZZER FREQUENCIES ============
#define FREQ_LOW      800
#define FREQ_MEDIUM   1500
#define FREQ_HIGH     2500
#define FREQ_CRITICAL 3500

#define VOLUME_OFF    0
#define VOLUME_LOW    64
#define VOLUME_MEDIUM 128
#define VOLUME_HIGH   192
#define VOLUME_MAX    255

// ============ BUZZER PATTERN STRUCTURE ============
struct BuzzerNote {
  uint16_t frequency;
  uint16_t duration;
  uint8_t volume;
};

// ============ BUZZER PATTERNS ============
BuzzerNote PATTERN_STARTUP[] = {
  {FREQ_LOW, 100, VOLUME_MEDIUM},
  {0, 50, 0},
  {FREQ_MEDIUM, 100, VOLUME_MEDIUM},
  {0, 50, 0},
  {FREQ_HIGH, 150, VOLUME_MEDIUM},
  {0, 0, 0}
};

BuzzerNote PATTERN_WARNING[] = {
  {FREQ_MEDIUM, 200, VOLUME_HIGH},
  {0, 100, 0},
  {FREQ_MEDIUM, 200, VOLUME_HIGH},
  {0, 0, 0}
};

BuzzerNote PATTERN_ERROR[] = {
  {FREQ_HIGH, 100, VOLUME_HIGH},
  {0, 100, 0},
  {FREQ_HIGH, 100, VOLUME_HIGH},
  {0, 100, 0},
  {FREQ_HIGH, 100, VOLUME_HIGH},
  {0, 0, 0}
};

BuzzerNote PATTERN_SUCCESS[] = {
  {FREQ_LOW, 80, VOLUME_MEDIUM},
  {FREQ_MEDIUM, 80, VOLUME_MEDIUM},
  {FREQ_HIGH, 200, VOLUME_MEDIUM},
  {0, 0, 0}
};

BuzzerNote PATTERN_CRITICAL[] = {
  {FREQ_CRITICAL, 300, VOLUME_MAX},
  {FREQ_LOW, 300, VOLUME_MAX},
  {FREQ_CRITICAL, 300, VOLUME_MAX},
  {FREQ_LOW, 300, VOLUME_MAX},
  {0, 0, 0}
};

BuzzerNote PATTERN_ACK[] = {
  {FREQ_MEDIUM, 50, VOLUME_LOW},
  {0, 0, 0}
};

BuzzerNote PATTERN_ATTENTION[] = {
  {FREQ_MEDIUM, 500, VOLUME_HIGH},
  {0, 100, 0},
  {FREQ_MEDIUM, 100, VOLUME_HIGH},
  {0, 0, 0}
};

BuzzerNote PATTERN_HEARTBEAT[] = {
  {FREQ_LOW, 30, VOLUME_LOW},
  {0, 100, 0},
  {FREQ_LOW, 30, VOLUME_LOW},
  {0, 0, 0}
};

// ============ MENU STRUCTURE ============
struct MenuItem {
  const char* name;
  BuzzerNote* pattern;
  uint16_t color;
  bool repeat;
};

MenuItem menuItems[] = {
  {"1. Startup", PATTERN_STARTUP, COLOR_PRIMARY, false},
  {"2. Warning", PATTERN_WARNING, COLOR_WARNING, false},
  {"3. Error", PATTERN_ERROR, COLOR_ERROR, false},
  {"4. Success", PATTERN_SUCCESS, COLOR_SUCCESS, false},
  {"5. Critical", PATTERN_CRITICAL, COLOR_ERROR, true},
  {"6. Acknowledge", PATTERN_ACK, COLOR_TEXT_DIM, false},
  {"7. Attention", PATTERN_ATTENTION, COLOR_WARNING, false},
  {"8. Heartbeat", PATTERN_HEARTBEAT, COLOR_TEXT_DIM, false}
};

const int MENU_COUNT = sizeof(menuItems) / sizeof(MenuItem);

// ============ BUZZER CLASS ============
class IndustrialBuzzer {
private:
  uint8_t pin;
  uint8_t channel;
  BuzzerNote* currentPattern;
  uint8_t patternIndex;
  unsigned long noteStartTime;
  bool isPlaying;
  bool repeat;
  String currentName;

public:
  IndustrialBuzzer(uint8_t buzzerPin, uint8_t pwmChannel) {
    pin = buzzerPin;
    channel = pwmChannel;
    currentPattern = nullptr;
    patternIndex = 0;
    noteStartTime = 0;
    isPlaying = false;
    repeat = false;
    currentName = "";
  }

  void begin() {
    pinMode(pin, OUTPUT);
    ledcSetup(channel, 1000, BUZZER_RESOLUTION);
    ledcAttachPin(pin, channel);
    ledcWrite(channel, 0);
  }

  void playPattern(BuzzerNote* pattern, String name, bool repeatPattern = false) {
    currentPattern = pattern;
    currentName = name;
    patternIndex = 0;
    isPlaying = true;
    repeat = repeatPattern;
    noteStartTime = millis();
    playCurrentNote();
  }

  void stop() {
    isPlaying = false;
    repeat = false;
    currentName = "";
    ledcWriteTone(channel, 0);
    ledcWrite(channel, 0);
  }

  void update() {
    if (!isPlaying || currentPattern == nullptr) {
      return;
    }

    BuzzerNote* note = &currentPattern[patternIndex];
    
    if (millis() - noteStartTime >= note->duration) {
      patternIndex++;
      note = &currentPattern[patternIndex];
      
      if (note->frequency == 0 && note->duration == 0) {
        if (repeat) {
          patternIndex = 0;
          note = &currentPattern[patternIndex];
        } else {
          stop();
          return;
        }
      }
      
      noteStartTime = millis();
      playCurrentNote();
    }
  }

  bool isActive() {
    return isPlaying;
  }

  String getCurrentName() {
    return currentName;
  }

private:
  void playCurrentNote() {
    BuzzerNote* note = &currentPattern[patternIndex];
    
    if (note->frequency > 0) {
      ledcWriteTone(channel, note->frequency);
      ledcWrite(channel, note->volume);
    } else {
      ledcWrite(channel, 0);
    }
  }
};

// ============ BUTTON INSTANCES ============
Bounce btnLeft = Bounce();
Bounce btnOk = Bounce();
Bounce btnRight = Bounce();

// ============ GLOBAL INSTANCES ============
IndustrialBuzzer buzzer(BUZZER_PIN, BUZZER_CHANNEL);
int selectedItem = 0;
int scrollOffset = 0;
const int MAX_VISIBLE_ITEMS = 6;

// ============ UI FUNCTIONS ============

void drawHeader() {
  sprite.createSprite(SCREEN_WIDTH, HEADER_HEIGHT);
  sprite.fillSprite(COLOR_HEADER);
  sprite.drawLine(0, HEADER_HEIGHT - 1, SCREEN_WIDTH, HEADER_HEIGHT - 1, COLOR_ACCENT);
  
  sprite.setTextColor(COLOR_PRIMARY, COLOR_HEADER);
  sprite.setTextSize(1);
  sprite.setTextDatum(ML_DATUM);
  sprite.drawString("BUZZER CONTROL", 4, 7, 2);
  
  // Status indicator
  if (buzzer.isActive()) {
    sprite.fillCircle(SCREEN_WIDTH - 8, 7, 4, COLOR_ERROR);
    sprite.drawCircle(SCREEN_WIDTH - 8, 7, 5, COLOR_ERROR);
  } else {
    sprite.fillCircle(SCREEN_WIDTH - 8, 7, 4, COLOR_SUCCESS);
  }
  
  sprite.pushSprite(0, 0);
  sprite.deleteSprite();
}

void drawFooter() {
  sprite.createSprite(SCREEN_WIDTH, FOOTER_HEIGHT);
  sprite.fillSprite(COLOR_HEADER);
  sprite.drawLine(0, 0, SCREEN_WIDTH, 0, COLOR_ACCENT);
  
  sprite.setTextColor(COLOR_TEXT_DIM, COLOR_HEADER);
  sprite.setTextSize(1);
  sprite.setTextDatum(TC_DATUM);
  
  if (buzzer.isActive()) {
    sprite.setTextColor(COLOR_ERROR, COLOR_HEADER);
    sprite.drawString("PLAYING: " + buzzer.getCurrentName(), SCREEN_WIDTH / 2, 4, 1);
    sprite.setTextColor(COLOR_TEXT, COLOR_HEADER);
    sprite.drawString("Press OK to STOP", SCREEN_WIDTH / 2, 13, 1);
  } else {
    sprite.drawString("< PREV", 20, 5, 1);
    sprite.drawString("OK SELECT", SCREEN_WIDTH / 2, 5, 1);
    sprite.drawString("NEXT >", SCREEN_WIDTH - 20, 5, 1);
  }
  
  sprite.pushSprite(0, SCREEN_HEIGHT - FOOTER_HEIGHT);
  sprite.deleteSprite();
}

void drawMenu() {
  int menuHeight = SCREEN_HEIGHT - HEADER_HEIGHT - FOOTER_HEIGHT;
  sprite.createSprite(SCREEN_WIDTH, menuHeight);
  sprite.fillSprite(COLOR_BG);
  
  // Calculate visible range
  int startIdx = scrollOffset;
  int endIdx = min(scrollOffset + MAX_VISIBLE_ITEMS, MENU_COUNT);
  
  for (int i = startIdx; i < endIdx; i++) {
    int y = (i - scrollOffset) * MENU_ITEM_H + 2;
    
    // Draw selection highlight
    if (i == selectedItem) {
      sprite.fillRoundRect(2, y - 1, SCREEN_WIDTH - 4, MENU_ITEM_H - 2, 3, COLOR_ACCENT);
      sprite.drawRoundRect(2, y - 1, SCREEN_WIDTH - 4, MENU_ITEM_H - 2, 3, COLOR_SELECTED);
    }
    
    // Draw menu item
    uint16_t textColor = (i == selectedItem) ? COLOR_TEXT : COLOR_TEXT_DIM;
    sprite.setTextColor(textColor, (i == selectedItem) ? COLOR_ACCENT : COLOR_BG);
    sprite.setTextDatum(ML_DATUM);
    sprite.drawString(menuItems[i].name, 8, y + 6, 1);
    
    // Draw indicator if repeating
    if (menuItems[i].repeat) {
      sprite.fillCircle(SCREEN_WIDTH - 10, y + 6, 2, menuItems[i].color);
    }
  }
  
  // Draw scroll indicators
  if (scrollOffset > 0) {
    // Up arrow
    sprite.fillTriangle(SCREEN_WIDTH / 2, 2, SCREEN_WIDTH / 2 - 4, 8, SCREEN_WIDTH / 2 + 4, 8, COLOR_PRIMARY);
  }
  
  if (scrollOffset + MAX_VISIBLE_ITEMS < MENU_COUNT) {
    // Down arrow
    int arrowY = menuHeight - 8;
    sprite.fillTriangle(SCREEN_WIDTH / 2, menuHeight - 2, SCREEN_WIDTH / 2 - 4, arrowY, SCREEN_WIDTH / 2 + 4, arrowY, COLOR_PRIMARY);
  }
  
  sprite.pushSprite(0, MENU_START_Y);
  sprite.deleteSprite();
}

void updateDisplay() {
  drawHeader();
  drawMenu();
  drawFooter();
}

void playFeedbackBeep() {
  ledcWriteTone(BUZZER_CHANNEL, FREQ_MEDIUM);
  ledcWrite(BUZZER_CHANNEL, VOLUME_LOW);
  delay(30);
  ledcWrite(BUZZER_CHANNEL, 0);
}

void handleButtons() {
  btnLeft.update();
  btnOk.update();
  btnRight.update();
  
  // LEFT button - Previous item
  if (btnLeft.fell()) {
    if (!buzzer.isActive()) {
      playFeedbackBeep();
      selectedItem--;
      if (selectedItem < 0) {
        selectedItem = MENU_COUNT - 1;
        scrollOffset = max(0, MENU_COUNT - MAX_VISIBLE_ITEMS);
      } else if (selectedItem < scrollOffset) {
        scrollOffset = selectedItem;
      }
      updateDisplay();
    }
  }
  
  // RIGHT button - Next item
  if (btnRight.fell()) {
    if (!buzzer.isActive()) {
      playFeedbackBeep();
      selectedItem++;
      if (selectedItem >= MENU_COUNT) {
        selectedItem = 0;
        scrollOffset = 0;
      } else if (selectedItem >= scrollOffset + MAX_VISIBLE_ITEMS) {
        scrollOffset = selectedItem - MAX_VISIBLE_ITEMS + 1;
      }
      updateDisplay();
    }
  }
  
  // OK button - Select/Stop
  if (btnOk.fell()) {
    playFeedbackBeep();
    
    if (buzzer.isActive()) {
      // Stop current pattern
      buzzer.stop();
    } else {
      // Play selected pattern
      MenuItem* item = &menuItems[selectedItem];
      buzzer.playPattern(item->pattern, item->name, item->repeat);
    }
    updateDisplay();
  }
}

// ============ SETUP ============
void setup() {
  Serial.begin(115200);
  delay(100);
  
  Serial.println("\n========================================");
  Serial.println("Industrial Buzzer Control with TFT");
  Serial.println("========================================\n");
  
  // Initialize TFT
  tft.init();
  tft.setRotation(1); // Landscape
  tft.fillScreen(COLOR_BG);
  
  // Initialize buzzer
  buzzer.begin();
  
  // Initialize buttons with Bounce2
  btnLeft.attach(BTN_LEFT, INPUT_PULLUP);
  btnLeft.interval(25);
  
  btnOk.attach(BTN_OK, INPUT_PULLUP);
  btnOk.interval(25);
  
  btnRight.attach(BTN_RIGHT, INPUT_PULLUP);
  btnRight.interval(25);
  
  // Show splash screen
  sprite.createSprite(140, 40);
  sprite.fillRoundRect(0, 0, 140, 40, 5, COLOR_ACCENT);
  sprite.drawRoundRect(0, 0, 140, 40, 5, COLOR_PRIMARY);
  sprite.setTextColor(COLOR_PRIMARY, COLOR_ACCENT);
  sprite.setTextDatum(MC_DATUM);
  sprite.drawString("INDUSTRIAL", 70, 14, 2);
  sprite.drawString("BUZZER CONTROL", 70, 28, 2);
  sprite.pushSprite(10, 35);
  sprite.deleteSprite();
  
  tft.setTextColor(COLOR_TEXT_DIM, COLOR_BG);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("v1.0", SCREEN_WIDTH / 2, 80, 1);
  
  // Play startup
  buzzer.playPattern(PATTERN_STARTUP, "Startup", false);
  while (buzzer.isActive()) {
    buzzer.update();
    delay(10);
  }
  
  delay(500);
  
  // Initialize display
  updateDisplay();
  
  Serial.println("System ready!");
  Serial.println("Use buttons to navigate:");
  Serial.println("- LEFT (GPIO 39): Previous");
  Serial.println("- OK (GPIO 35): Select/Stop");
  Serial.println("- RIGHT (GPIO 36): Next\n");
}

// ============ MAIN LOOP ============
void loop() {
  // Update buzzer patterns
  buzzer.update();
  
  // Handle button inputs
  handleButtons();
  
  // Update status periodically
  static unsigned long lastStatusUpdate = 0;
  if (millis() - lastStatusUpdate > 500) {
    if (buzzer.isActive()) {
      drawHeader();
      drawFooter();
    }
    lastStatusUpdate = millis();
  }
  
  delay(10);
}