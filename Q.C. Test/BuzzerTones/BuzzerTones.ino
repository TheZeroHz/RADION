

// ============ CONFIGURATION ============
#define BUZZER_PIN 33
#define BUZZER_CHANNEL 0      // PWM channel (0-15)
#define BUZZER_RESOLUTION 8   // 8-bit resolution (0-255)

// Buzzer frequencies (Hz)
#define FREQ_LOW      800
#define FREQ_MEDIUM   1500
#define FREQ_HIGH     2500
#define FREQ_CRITICAL 3500

// Volume levels (0-255, PWM duty cycle)
#define VOLUME_OFF    0
#define VOLUME_LOW    64
#define VOLUME_MEDIUM 128
#define VOLUME_HIGH   192
#define VOLUME_MAX    255

// ============ BUZZER PATTERN STRUCTURE ============
struct BuzzerNote {
  uint16_t frequency;  // Frequency in Hz (0 = silence)
  uint16_t duration;   // Duration in milliseconds
  uint8_t volume;      // Volume 0-255
};

// ============ PREDEFINED PATTERNS ============

// Startup sequence: Ascending tones
BuzzerNote PATTERN_STARTUP[] = {
  {FREQ_LOW, 100, VOLUME_MEDIUM},
  {0, 50, 0},
  {FREQ_MEDIUM, 100, VOLUME_MEDIUM},
  {0, 50, 0},
  {FREQ_HIGH, 150, VOLUME_MEDIUM},
  {0, 0, 0}  // End marker
};

// Warning: Double beep
BuzzerNote PATTERN_WARNING[] = {
  {FREQ_MEDIUM, 200, VOLUME_HIGH},
  {0, 100, 0},
  {FREQ_MEDIUM, 200, VOLUME_HIGH},
  {0, 0, 0}
};

// Error: Triple short beeps
BuzzerNote PATTERN_ERROR[] = {
  {FREQ_HIGH, 100, VOLUME_HIGH},
  {0, 100, 0},
  {FREQ_HIGH, 100, VOLUME_HIGH},
  {0, 100, 0},
  {FREQ_HIGH, 100, VOLUME_HIGH},
  {0, 0, 0}
};

// Success: Ascending melody
BuzzerNote PATTERN_SUCCESS[] = {
  {FREQ_LOW, 80, VOLUME_MEDIUM},
  {FREQ_MEDIUM, 80, VOLUME_MEDIUM},
  {FREQ_HIGH, 200, VOLUME_MEDIUM},
  {0, 0, 0}
};

// Critical alarm: Continuous alternating tones
BuzzerNote PATTERN_CRITICAL[] = {
  {FREQ_CRITICAL, 300, VOLUME_MAX},
  {FREQ_LOW, 300, VOLUME_MAX},
  {FREQ_CRITICAL, 300, VOLUME_MAX},
  {FREQ_LOW, 300, VOLUME_MAX},
  {0, 0, 0}
};

// Acknowledgment: Single short beep
BuzzerNote PATTERN_ACK[] = {
  {FREQ_MEDIUM, 50, VOLUME_LOW},
  {0, 0, 0}
};

// Attention: Long-short pattern
BuzzerNote PATTERN_ATTENTION[] = {
  {FREQ_MEDIUM, 500, VOLUME_HIGH},
  {0, 100, 0},
  {FREQ_MEDIUM, 100, VOLUME_HIGH},
  {0, 0, 0}
};

// Heartbeat: System alive indicator
BuzzerNote PATTERN_HEARTBEAT[] = {
  {FREQ_LOW, 30, VOLUME_LOW},
  {0, 100, 0},
  {FREQ_LOW, 30, VOLUME_LOW},
  {0, 0, 0}
};

// ============ BUZZER CONTROL CLASS ============
class IndustrialBuzzer {
private:
  uint8_t pin;
  uint8_t channel;
  BuzzerNote* currentPattern;
  uint8_t patternIndex;
  unsigned long noteStartTime;
  bool isPlaying;
  bool repeat;

public:
  IndustrialBuzzer(uint8_t buzzerPin, uint8_t pwmChannel) {
    pin = buzzerPin;
    channel = pwmChannel;
    currentPattern = nullptr;
    patternIndex = 0;
    noteStartTime = 0;
    isPlaying = false;
    repeat = false;
  }

  void begin() {
    pinMode(pin, OUTPUT);
    // Configure PWM
    ledcSetup(channel, 1000, BUZZER_RESOLUTION);
    ledcAttachPin(pin, channel);
    ledcWrite(channel, 0);
    Serial.println("Industrial Buzzer initialized on pin " + String(pin));
  }

  void playPattern(BuzzerNote* pattern, bool repeatPattern = false) {
    currentPattern = pattern;
    patternIndex = 0;
    isPlaying = true;
    repeat = repeatPattern;
    noteStartTime = millis();
    playCurrentNote();
  }

  void stop() {
    isPlaying = false;
    repeat = false;
    ledcWriteTone(channel, 0);
    ledcWrite(channel, 0);
  }

  void update() {
    if (!isPlaying || currentPattern == nullptr) {
      return;
    }

    BuzzerNote* note = &currentPattern[patternIndex];
    
    // Check if current note duration has elapsed
    if (millis() - noteStartTime >= note->duration) {
      patternIndex++;
      note = &currentPattern[patternIndex];
      
      // Check if we've reached the end marker
      if (note->frequency == 0 && note->duration == 0) {
        if (repeat) {
          // Restart pattern
          patternIndex = 0;
          note = &currentPattern[patternIndex];
        } else {
          // Stop playing
          stop();
          return;
        }
      }
      
      noteStartTime = millis();
      playCurrentNote();
    }
  }

  void playTone(uint16_t frequency, uint16_t duration, uint8_t volume = VOLUME_MEDIUM) {
    if (frequency > 0) {
      ledcWriteTone(channel, frequency);
      ledcWrite(channel, volume);
    } else {
      ledcWrite(channel, 0);
    }
    delay(duration);
    ledcWrite(channel, 0);
  }

  bool isActive() {
    return isPlaying;
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

// ============ GLOBAL INSTANCES ============
IndustrialBuzzer buzzer(BUZZER_PIN, BUZZER_CHANNEL);

// ============ MENU SYSTEM ============
void printMenu() {
  Serial.println("\n========================================");
  Serial.println("  INDUSTRIAL BUZZER CONTROL SYSTEM");
  Serial.println("========================================");
  Serial.println("1 - Startup Pattern");
  Serial.println("2 - Warning Pattern");
  Serial.println("3 - Error Pattern");
  Serial.println("4 - Success Pattern");
  Serial.println("5 - Critical Alarm (repeating)");
  Serial.println("6 - Acknowledgment Beep");
  Serial.println("7 - Attention Pattern");
  Serial.println("8 - Heartbeat Pattern");
  Serial.println("9 - Custom Tone Test");
  Serial.println("0 - Stop All");
  Serial.println("========================================");
  Serial.print("Select pattern: ");
}

void handleSerialInput() {
  if (Serial.available() > 0) {
    char input = Serial.read();
    Serial.println(input);
    
    // Clear any remaining input
    while (Serial.available() > 0) {
      Serial.read();
    }
    
    switch (input) {
      case '1':
        Serial.println("Playing: STARTUP");
        buzzer.playPattern(PATTERN_STARTUP);
        break;
        
      case '2':
        Serial.println("Playing: WARNING");
        buzzer.playPattern(PATTERN_WARNING);
        break;
        
      case '3':
        Serial.println("Playing: ERROR");
        buzzer.playPattern(PATTERN_ERROR);
        break;
        
      case '4':
        Serial.println("Playing: SUCCESS");
        buzzer.playPattern(PATTERN_SUCCESS);
        break;
        
      case '5':
        Serial.println("Playing: CRITICAL ALARM (repeating - press 0 to stop)");
        buzzer.playPattern(PATTERN_CRITICAL, true);
        break;
        
      case '6':
        Serial.println("Playing: ACKNOWLEDGMENT");
        buzzer.playPattern(PATTERN_ACK);
        break;
        
      case '7':
        Serial.println("Playing: ATTENTION");
        buzzer.playPattern(PATTERN_ATTENTION);
        break;
        
      case '8':
        Serial.println("Playing: HEARTBEAT");
        buzzer.playPattern(PATTERN_HEARTBEAT);
        break;
        
      case '9':
        Serial.println("Custom tone test: Low -> Medium -> High");
        buzzer.playTone(FREQ_LOW, 300, VOLUME_MEDIUM);
        delay(100);
        buzzer.playTone(FREQ_MEDIUM, 300, VOLUME_MEDIUM);
        delay(100);
        buzzer.playTone(FREQ_HIGH, 300, VOLUME_MEDIUM);
        Serial.println("Done!");
        break;
        
      case '0':
        Serial.println("STOPPING all patterns");
        buzzer.stop();
        break;
        
      default:
        Serial.println("Invalid selection!");
        break;
    }
    
    if (input != '5') {  // Don't show menu immediately for repeating alarm
      delay(100);
      printMenu();
    }
  }
}

// ============ SETUP ============
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n========================================");
  Serial.println("Industrial Buzzer Control System v1.0");
  Serial.println("========================================\n");
  
  // Initialize buzzer
  buzzer.begin();
  
  // Play startup sequence
  Serial.println("Playing startup sequence...");
  buzzer.playPattern(PATTERN_STARTUP);
  
  // Wait for startup to finish
  while (buzzer.isActive()) {
    buzzer.update();
    delay(10);
  }
  
  Serial.println("System ready!\n");
  printMenu();
}

// ============ MAIN LOOP ============
void loop() {
  // Update buzzer (handles pattern playback)
  buzzer.update();
  
  // Handle serial commands
  handleSerialInput();
  
  // Small delay
  delay(10);
}