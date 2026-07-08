/**
 * DIY Timer Switch
 * 
 * ATmega8 pin mapping:
 * - Segment Pins: PC0-PC5, PD5
 * - DISPLAY_EN: PB0 (Arduino D8)
 * - TOUCH_PIN: PD0 (Arduino D0)
 * - BUZZER_PIN: PB1 (Arduino D9)
 * - RELAY_PIN: PB2 (Arduino D10)
 */

const int SEG_A = A0; // PC0
const int SEG_B = A1; // PC1
const int SEG_C = A2; // PC2
const int SEG_D = A3; // PC3
const int SEG_E = A4; // PC4
const int SEG_F = A5; // PC5
const int SEG_G = 5;  // PD5

const int DISPLAY_EN = 8; // PB0
const int TOUCH_PIN = 0;  // PD0
const int BUZZER_PIN = 9; // PB1
// VERY IMPORTANT: Do NOT change RELAY_PIN to 16! 
// In Arduino code, we must use the Arduino Pin Number, not the physical IC pin.
// On ATmega8, physical pin 16 is Arduino D10. If you change this to 16, 
// it will conflict with A2 (which is D16) and break Segment C!
const int RELAY_PIN = 10; // PB2 (Arduino D10 corresponds to physical pin 16)

const bool TOUCHED_STATE = HIGH;
const int segmentPins[] = {SEG_A, SEG_B, SEG_C, SEG_D, SEG_E, SEG_F, SEG_G};

const byte digitPatterns[] = {
  0b00111111, // 0
  0b00000110, // 1
  0b01011011, // 2
  0b01001111, // 3
  0b01100110, // 4
  0b01101101, // 5
  0b01111101, // 6
  0b00000111, // 7
  0b01111111, // 8
  0b01101111, // 9
  0b01110111, // a
  0b01111100, // b
  0b01011000, // c
  0b01011110, // d
  0b01111001, // e
  0b01110001  // f
};

int timeRemainingMinutes = 0;
int lastDisplayedChar = -1;

// Variables for touch sensor state change detection & debouncing
int lastTouchState = LOW;
int debouncedTouchState = LOW;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

// Variables for auto-repeat when held down
unsigned long lastIncrementTime = 0;
const unsigned long firstRepeatDelay = 600;
const unsigned long repeatSpeed = 250;
bool wasIncremented = false;

// Variables for double-tap reset
unsigned long lastTouchTime = 0;
const unsigned long doubleTapThreshold = 350;

// Timer and idle variables
unsigned long lastMinuteTick = 0;
unsigned long lastActivityTime = 0;

// Non-blocking buzzer timing variables
unsigned long buzzerTurnOffTime = 0;

void setup() {
  for (int i = 0; i < 7; i++) {
    pinMode(segmentPins[i], OUTPUT);
    digitalWrite(segmentPins[i], LOW);
  }
  
  pinMode(DISPLAY_EN, OUTPUT);
  digitalWrite(DISPLAY_EN, HIGH);

  pinMode(TOUCH_PIN, INPUT);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH); // Active LOW, turn OFF initially
}

void triggerBeep(unsigned long duration) {
  digitalWrite(BUZZER_PIN, HIGH);
  buzzerTurnOffTime = millis() + duration;
}

int getNextPreset(int current) {
  if (current < 9) return current + 1;
  if (current < 10) return 10;
  if (current < 20) return 20;
  if (current < 30) return 30;
  if (current < 40) return 40;
  if (current < 50) return 50;
  if (current < 60) return 60;
  return 0; // Wrap around to 0
}

int getDisplayChar(int minutes) {
  if (minutes <= 9) return minutes; // 0-9
  if (minutes < 20) return 10; // 'a' 
  if (minutes < 30) return 11; // 'b'
  if (minutes < 40) return 12; // 'c'
  if (minutes < 50) return 13; // 'd'
  if (minutes < 60) return 14; // 'e'
  return 15; // 'f'
}

void showDigit(int number) {
  if (number < 0 || number > 15) {
    for (int i = 0; i < 7; i++) {
      digitalWrite(segmentPins[i], LOW);
    }
    return;
  }
  
  byte pattern = digitPatterns[number];
  for (int i = 0; i < 7; i++) {
    bool bitValue = bitRead(pattern, i);
    digitalWrite(segmentPins[i], bitValue ? HIGH : LOW);
  }
}

void loop() {
  unsigned long currentMillis = millis();

  // Handle buzzer turn off
  if (buzzerTurnOffTime > 0 && currentMillis >= buzzerTurnOffTime) {
    digitalWrite(BUZZER_PIN, LOW);
    buzzerTurnOffTime = 0;
  }

  // Handle minute tick decrement
  if (timeRemainingMinutes > 0) {
    if (currentMillis - lastMinuteTick >= 60000UL) {
      lastMinuteTick += 60000UL;
      timeRemainingMinutes--;

      if (timeRemainingMinutes == 0) {
        triggerBeep(500); // Long beep at 0
      }
    }
  }

  // Read current touch sensor pin state
  int reading = digitalRead(TOUCH_PIN);
  
  if (reading != lastTouchState) {
    lastDebounceTime = currentMillis;
  }
  
  if ((currentMillis - lastDebounceTime) > debounceDelay) {
    if (reading != debouncedTouchState) {
      debouncedTouchState = reading;
      
      // Touch Rising Edge
      if (debouncedTouchState == TOUCHED_STATE) {
        lastActivityTime = currentMillis; // Wake up / update activity time
        unsigned long timeSinceLastTouch = currentMillis - lastTouchTime;
        
        if (timeSinceLastTouch <= doubleTapThreshold) {
          // Double tap detected: cancel timer
          timeRemainingMinutes = 0;
          wasIncremented = false;
          triggerBeep(150); // Distinct beep for cancellation
        } else {
          // Single tap: increment timer
          timeRemainingMinutes = getNextPreset(timeRemainingMinutes);
          lastMinuteTick = currentMillis; // Reset minute timer for full minute
          wasIncremented = true;
          triggerBeep(50);
        }
        
        lastTouchTime = currentMillis;
        lastIncrementTime = currentMillis;
      } else {
        wasIncremented = false; // Reset repeat logic when released
      }
    }
  }
  
  lastTouchState = reading;

  // Auto-repeat logic: if the sensor is held
  if (debouncedTouchState == TOUCHED_STATE && wasIncremented) {
    unsigned long requiredDelay = (currentMillis - lastDebounceTime < 700) ? firstRepeatDelay : repeatSpeed;
    
    if (currentMillis - lastIncrementTime >= requiredDelay) {
      timeRemainingMinutes = getNextPreset(timeRemainingMinutes);
      lastMinuteTick = currentMillis; // Reset minute timer for full minute
      lastActivityTime = currentMillis; // Keep awake
      lastIncrementTime = currentMillis;
      triggerBeep(50);
    }
  }

  // Display handling
  bool displayActive = false;
  if (timeRemainingMinutes > 0) {
    displayActive = true;
  } else {
    // Idle timeout: 1 minute = 60000ms
    if (currentMillis - lastActivityTime < 60000UL) {
      displayActive = true;
    } else {
      displayActive = false;
    }
  }

  if (displayActive) {
    digitalWrite(DISPLAY_EN, HIGH);
    int currentDisplayedChar = getDisplayChar(timeRemainingMinutes);
    showDigit(currentDisplayedChar);
    lastDisplayedChar = currentDisplayedChar;
  } else {
    digitalWrite(DISPLAY_EN, LOW); // Turn off display completely via EN pin
    showDigit(-1); // Turn off segments
    lastDisplayedChar = -1;
  }

  // Relay handling
  if (timeRemainingMinutes > 0) {
    digitalWrite(RELAY_PIN, LOW); // Active LOW -> ON
  } else {
    digitalWrite(RELAY_PIN, HIGH); // Active LOW -> OFF
  }
}
