/**
 * DIY Timer Switch - Seven Segment Display Test Code
 * 
 * This code displays digits 0 to 9 on a 2-digit multiplexed seven segment display.
 * The displayed digit increments by 1 on every touch of the touch sensor module.
 * If kept pressed, the digit continues to auto-increment.
 * A quick double tap resets the digit count to 0.
 * 
 * ATmega8 pin mapping (according to the MiniCore board package):
 * -----------------------------------------------------------
 * - segmentPins[] = {A0, A1, A2, A3, A4, A5, 6, 7}; // A, B, C, D, E, F, G, DP
 * - DIGIT_1 (Left Display Control)  = Pin 8  (PB0)
 * - DIGIT_2 (Right Display Control) = Pin 9  (PB1)
 * - TOUCH_PIN (Touch Sensor Input)  = Pin 0  (PD0)
 */

// Segment pin definitions (Common Cathode)
// Segment A to F are connected to PC0 to PC5
const int SEG_A = 14; // PC0 (A0 / D14)
const int SEG_B = 15; // PC1 (A1 / D15)
const int SEG_C = 16; // PC2 (A2 / D16)
const int SEG_D = 17; // PC3 (A3 / D17)
const int SEG_E = 18; // PC4 (A4 / D18)
const int SEG_F = 19; // PC5 (A5 / D19)
const int SEG_G = 6;  // PD6 (D6)
const int SEG_DP = 7; // PD7 (D7)

// Digit selection pins (Transistor control)
// Q1 controls the tens (left) digit
// Q2 controls the units (right) digit
const int DIGIT_1 = 8; // PB0 (D8) - Tens (Left)
const int DIGIT_2 = 9; // PB1 (D9) - Units (Right)

// Touch sensor pin definition (JP1 Pin 2 connected to PD0)
const int TOUCH_PIN = 0; // PD0 (D0) - Touch Sensor I/O
// TTP223 module outputs HIGH when touched and LOW when idle (active-high).
const bool TOUCHED_STATE = HIGH; 

// Buzzer pin definition (TMB12A05 active buzzer connected to PD1)
const int BUZZER_PIN = 1; // PD1 (D1) - Buzzer Output

// Store segment pins in an array for easy iteration
const int segmentPins[] = {SEG_A, SEG_B, SEG_C, SEG_D, SEG_E, SEG_F, SEG_G, SEG_DP};

// Lookup table for digits 0-9 (Common Cathode: HIGH turns segment ON)
// Bit order: DP G F E D C B A
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
  0b01101111  // 9: A, B, C, D, F, G
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
  // Set all segment pins as OUTPUT
  for (int i = 0; i < 8; i++) {
    pinMode(segmentPins[i], OUTPUT);
    digitalWrite(segmentPins[i], LOW); // Turn off initially
  }
  
  // Set digit control pins as OUTPUT
  pinMode(DIGIT_1, OUTPUT);
  pinMode(DIGIT_2, OUTPUT);
  
  digitalWrite(DIGIT_1, LOW); // OFF
  digitalWrite(DIGIT_2, LOW); // OFF

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
          // Single tap: increment count
          currentDigit = (currentDigit + 1) % 10;
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
      currentDigit = (currentDigit + 1) % 10;
      lastIncrementTime = currentMillis;
      triggerBeep();
    }
  }
  
  // Display both digits using multiplexing (performing blanking to avoid ghosting)
  
  // --- Phase 1: Display '0' on the Left Digit (Tens) ---
  digitalWrite(DIGIT_1, LOW);  // Turn off both digits (Blanking)
  digitalWrite(DIGIT_2, LOW);
  showDigit(0);
  digitalWrite(DIGIT_1, HIGH); // Turn on Left Digit
  delay(5);                    // Small delay for persistence of vision
  
  // --- Phase 2: Display current Digit on the Right Digit (Units) ---
  digitalWrite(DIGIT_1, LOW);  // Turn off both digits (Blanking)
  digitalWrite(DIGIT_2, LOW);
  showDigit(currentDigit);
  digitalWrite(DIGIT_2, HIGH); // Turn on Right Digit
  delay(5);                    // Small delay for persistence of vision
}

// Function to output the digit pattern to the segment pins
void showDigit(int number) {
  if (number < 0 || number > 9) return;
  
  byte pattern = digitPatterns[number];
  
  // Turn each segment pin ON/OFF based on the corresponding pattern bit
  for (int i = 0; i < 8; i++) {
    bool bitValue = bitRead(pattern, i);
    digitalWrite(segmentPins[i], bitValue ? HIGH : LOW);
  }
}
