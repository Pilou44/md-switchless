#include <EEPROM.h>

enum Mode {
  EUR,
  USA,
  JAP,
  MODES_NO // Leave at end
};

#define MODE_ROM_OFFSET 42
#define DEFAULT_MODE USA;

#define FORCE_RESET_ACTIVE_LEVEL LOW

#define FORCE_RESET_AT_BOOT 2500

#define RESET_IN_PIN A1
#define RESET_OUT_PIN A0
#define VIDEOMODE_PIN A2
#define LANGUAGE_PIN A3

#define MODE_LED_R_PIN 9         // PWM
#define MODE_LED_G_PIN 10        // PWM

byte resetInactiveLevel = HIGH;

/* Presses of the reset button longer than this amount of milliseconds will
 * switch to the next mode, shorter presses will reset the console.
 */
const unsigned long LONGPRESS_LEN = 700U;
// Duration of the reset pulse (milliseconds)
const unsigned long RESET_LEN = 350U;
// Debounce duration for the reset button
const unsigned long DEBOUNCE_RESET_MS = 20U;

Mode currentMode;
boolean resetDown = false;

void setup() {
  /* Init video mode: We do this as soon as possible since the MegaDrive's
   * reset line seems to be edge-triggered, so we cannot hold the console
   * in the reset state while we are setting up stuff. We'll take care of
   * the rest later.
   */
  noInterrupts();
  pinMode(VIDEOMODE_PIN, OUTPUT);
  pinMode(LANGUAGE_PIN, OUTPUT);
  pinMode(MODE_LED_R_PIN, OUTPUT);
  pinMode(MODE_LED_G_PIN, OUTPUT);
  pinMode(RESET_IN_PIN, INPUT_PULLUP);
  pinMode(RESET_OUT_PIN, OUTPUT);
  digitalWrite (RESET_OUT_PIN, resetInactiveLevel);
  currentMode = readMode();
  applyMode();
  interrupts();

  resetInactiveLevel = !FORCE_RESET_ACTIVE_LEVEL;
  
  // Reset console so that it picks up the new mode/lang
#ifdef FORCE_RESET_AT_BOOT
  delay(FORCE_RESET_AT_BOOT);
  resetConsole ();
#endif
}

void loop() {  
  handleResetButton();
}

inline void handleResetButton () {
  static byte debounceLevel = LOW;
  static bool resetPressedBefore = false;
  static unsigned long lastInt = 0, resetPressStart = 0;
  static unsigned int holdCycles = 0;

  byte reset_level = digitalRead(RESET_IN_PIN);
  if (reset_level != debounceLevel) {
    // Reset debouncing timer
    lastInt = millis();
    debounceLevel = reset_level;
  } else if (millis() - lastInt > DEBOUNCE_RESET_MS) {
    // OK, button is stable, see if it has changed
    if (reset_level != resetInactiveLevel && !resetPressedBefore) {
      // Button just pressed
      resetPressStart = millis();
      holdCycles = 0;
    } else if (reset_level == resetInactiveLevel && resetPressedBefore) {
      // Button released
      if (holdCycles == 0) {
        resetConsole();
      } else {
        saveMode();
      }
    } else {
      // Button has not just been pressed/released
      if (reset_level != resetInactiveLevel && millis() - resetPressStart >= LONGPRESS_LEN * (holdCycles + 1)) {
        // Reset has been held for a while
        ++holdCycles;
        nextMode();
      }
    }

    resetPressedBefore = (reset_level != resetInactiveLevel);
  }
}

void resetConsole () {
  digitalWrite(RESET_OUT_PIN, !resetInactiveLevel);
  delay (RESET_LEN);
  digitalWrite(RESET_OUT_PIN, resetInactiveLevel);
}

inline void nextMode() {
 // This also loops in [0, MODES_NO) backwards
  currentMode = static_cast<Mode> ((currentMode + 1) % MODES_NO);
  applyMode();
}

Mode readMode() {
  byte savedMode = EEPROM.read (MODE_ROM_OFFSET);
  if (savedMode == EUR || savedMode == USA || savedMode == JAP) {
    return savedMode;
  } else {
    Mode mode = DEFAULT_MODE;
    return mode;
  }
}

void saveMode() {
  EEPROM.write (MODE_ROM_OFFSET, static_cast<byte> (currentMode));
}

void applyMode() {
  switch (currentMode) {
    default:
      // Invalid value
      // Get back to something meaningful
      currentMode = DEFAULT_MODE;
    case EUR:
      digitalWrite(VIDEOMODE_PIN, LOW);    // PAL 50Hz
      digitalWrite(LANGUAGE_PIN, HIGH);    // ENG
      break;
    case USA:
      digitalWrite(VIDEOMODE_PIN, HIGH);   // NTSC 60Hz
      digitalWrite(LANGUAGE_PIN, HIGH);    // ENG
      break;
    case JAP:
      digitalWrite(VIDEOMODE_PIN, HIGH);   // NTSC 60Hz
      digitalWrite(LANGUAGE_PIN, LOW);     // JAP
      break;
  }
  updateLed();
}

void updateLed() {
  switch (currentMode) {
    case EUR:
      analogWrite(MODE_LED_R_PIN, 0);
      analogWrite(MODE_LED_G_PIN, 255);
      break;
    case USA:
      analogWrite(MODE_LED_R_PIN, 255);
      analogWrite(MODE_LED_G_PIN, 0);
      break;
    case JAP:
      analogWrite(MODE_LED_R_PIN, 255);
      analogWrite(MODE_LED_G_PIN, 255);
      break;
  }
}
