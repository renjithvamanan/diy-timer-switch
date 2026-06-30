/**
 * DIY Timer Switch - Seven Segment Display Test Code
 * 
 * This code displays hexadecimal digits 0 to f on a single-digit seven segment display.
 * The displayed digit/character increments by 1 on every touch of the touch sensor module.
 * If kept pressed, the digit continues to auto-increment.
 * A quick double tap resets the digit count to 0.
 * 
 * ATmega8 pin mapping (according to the MiniCore board package):
 * -----------------------------------------------------------
 * - segmentPins[] = {15, 14, 16, 17, 18, 19, 5}; // A, B, C, D, E, F, G (No DP)
 * - DISPLAY_EN (Display Enable)     = Arduino D8 (PB0 / Physical Pin 14)
 * - TOUCH_PIN (Touch Sensor Input)  = Arduino D0 (PD0 / Physical Pin 2)
 * - BUZZER_PIN (Buzzer Output)      = Arduino D9 (PB1 / Physical Pin 15)
 */

// Segment pin definitions (Common Cathode)
// Segment pins are connected from PC0 to PC5, and Segment G is on PD5.
const int SEG_A = A0; // PC0 (Arduino D14 / Physical Pin 23)
const int SEG_B = A1; // PC1 (Arduino D15 / Physical Pin 24)
const int SEG_C = A2; // PC2 (Arduino D16 / Physical Pin 25)
const int SEG_D = A3; // PC3 (Arduino D17 / Physical Pin 26)
const int SEG_E = A4; // PC4 (Arduino D18 / Physical Pin 27)
const int SEG_F = A5; // PC5 (Arduino D19 / Physical Pin 28)
const int SEG_G = 5;  // PD5 (Arduino D5  / Physical Pin 11)

// Single digit display enable pin (Controls Q2 NPN transistor to enable common cathode)
const int DISPLAY_EN = 8; // PB0 (Arduino D8 / Physical Pin 14)

// Touch sensor pin definition (JP1 Pin 2 connected to PD0)
const int TOUCH_PIN = 0; // PD0 (Arduino D0 / Physical Pin 2)
// TTP223 module outputs HIGH when touched and LOW when idle (active-high).
const bool TOUCHED_STATE = HIGH; 

// Buzzer pin definition (TMB12A03 active buzzer connected to PB1)
const int BUZZER_PIN = 9; // PB1 (Arduino D9 / Physical Pin 15)

// Store segment pins in an array for easy iteration (G, F, E, D, C, B, A order for bit matching)
const int segmentPins[] = {SEG_A, SEG_B, SEG_C, SEG_D, SEG_E, SEG_F, SEG_G};

// Lookup table for digits 0-9 and a-f (Common Cathode: HIGH turns segment ON)
// Bit order: G F E D C B A (DP is not connected)
const byte digitPatterns[] = {
  0b00111111, // 0: A, B, C, D, E, F
  0b00000110, // 1: B, C
  0b01011011, // 2: A, B, D, E, G
  0b01001111, // 3: A, B, C, D, G
  0b01100110, // 4: B, C, F, G
  0b01101101, // 5: A, C, D, F, G
  0b01111101, // 6: A, C, D, E, F, G
  0b00000111, // 7: A, B, C
  0b01111111, // 8: A, B, C, D, E, F, G
  0b01101111, // 9: A, B, C, D, F, G
  0b01110111, // a: A, B, C, E, F, G
  0b01111100, // b: C, D, E, F, G
  0b01011000, // c: D, E, G (lowercase 'c' representation)
  0b01011110, // d: B, C, D, E, G
  0b01111001, // e: A, D, E, F, G (uppercase 'E' representation)
  0b01110001  // f: A, E, F, G (uppercase 'F' representation)
};

int currentDigit = 0;

// Variables for touch sensor state change detection & debouncing
int lastTouchState = LOW;              // Previous hardware reading from touch pin (idle is LOW)
int debouncedTouchState = LOW;          // Debounced state of touch sensor (idle is LOW)
unsigned long lastDebounceTime = 0;    // Time when the touch pin state last changed
const unsigned long debounceDelay = 50; // Debounce delay in milliseconds

// Variables for auto-repeat when held down
unsigned long lastIncrementTime = 0;   // Timestamp of the last digit increment
const unsigned long firstRepeatDelay = 600; // Delay before starting auto-repeat (ms)
const unsigned long repeatSpeed = 250;      // Interval between increments during auto-repeat (ms)
bool wasIncremented = false;           // Track if we did the initial increment

// Variables for double-tap reset
unsigned long lastTouchTime = 0;        // Timestamp of the last touch release or press
const unsigned long doubleTapThreshold = 350; // Double-tap time window in milliseconds

// Non-blocking buzzer timing variables
unsigned long buzzerTurnOffTime = 0;
const unsigned long beepDuration = 50; // duration of beep in ms

void setup() {
  // Set all segment pins as OUTPUT (7 segments)
  for (int i = 0; i < 7; i++) {
    pinMode(segmentPins[i], OUTPUT);
    digitalWrite(segmentPins[i], LOW); // Turn off initially
  }
  
  // Set single-digit control pin as OUTPUT and enable it (HIGH controls NPN transistor to pull cathode to GND)
  pinMode(DISPLAY_EN, OUTPUT);
  digitalWrite(DISPLAY_EN, HIGH);

  // Set touch sensor pin as INPUT (PD0)
  pinMode(TOUCH_PIN, INPUT);

  // Set buzzer pin as OUTPUT
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW); // Turn off initially
}

// Helper function to trigger a short beep
void triggerBeep(unsigned long duration = beepDuration) {
  digitalWrite(BUZZER_PIN, HIGH);
  buzzerTurnOffTime = millis() + duration;
}

void loop() {
  // Turn off buzzer non-blockingly if duration has elapsed
  if (buzzerTurnOffTime > 0 && millis() >= buzzerTurnOffTime) {
    digitalWrite(BUZZER_PIN, LOW);
    buzzerTurnOffTime = 0;
  }

  // Read current touch sensor pin state
  int reading = digitalRead(TOUCH_PIN);
  unsigned long currentMillis = millis();
  
  // If the reading has changed, reset the debounce timer
  if (reading != lastTouchState) {
    lastDebounceTime = currentMillis;
  }
  
  // Check if the reading has been stable for longer than the debounce delay
  if ((currentMillis - lastDebounceTime) > debounceDelay) {
    // If the debounced state has changed
    if (reading != debouncedTouchState) {
      debouncedTouchState = reading;
      
      // Initial touch (rising edge)
      if (debouncedTouchState == TOUCHED_STATE) {
        unsigned long timeSinceLastTouch = currentMillis - lastTouchTime;
        
        if (timeSinceLastTouch <= doubleTapThreshold) {
          // Double tap detected: reset count to 0
          currentDigit = 0;
          wasIncremented = false; // Disable auto-repeat for the double-tap release
          triggerBeep();
        } else {
          // Single tap: increment count (0 to 15)
          currentDigit = (currentDigit + 1) % 16;
          wasIncremented = true;
          triggerBeep();
        }
        
        lastTouchTime = currentMillis;
        lastIncrementTime = currentMillis;
      } else {
        wasIncremented = false; // Reset repeat logic when released
      }
    }
  }
  
  // Save the reading for the next iteration
  lastTouchState = reading;
  
  // Auto-repeat logic: if the sensor is held in the TOUCHED_STATE
  if (debouncedTouchState == TOUCHED_STATE && wasIncremented) {
    // First repeat requires a longer delay (e.g. 600ms), subsequent repeats are faster (250ms)
    unsigned long requiredDelay = (currentMillis - lastDebounceTime < 700) ? firstRepeatDelay : repeatSpeed;
    
    if (currentMillis - lastIncrementTime >= requiredDelay) {
      currentDigit = (currentDigit + 1) % 16;
      lastIncrementTime = currentMillis;
      triggerBeep();
    }
  }
  
  // Continuously write the current pattern to the 7 segments (no multiplexing delay required)
  showDigit(currentDigit);
}

// Function to output the digit pattern to the segment pins
void showDigit(int number) {
  if (number < 0 || number > 15) return;
  
  byte pattern = digitPatterns[number];
  
  // Turn each segment pin ON/OFF based on the corresponding pattern bit (7 segments)
  for (int i = 0; i < 7; i++) {
    bool bitValue = bitRead(pattern, i);
    digitalWrite(segmentPins[i], bitValue ? HIGH : LOW);
  }
}
