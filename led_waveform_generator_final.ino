/*
  LED Waveform Generator - Final Version
  ---------------------------------------
  Push button (Pin 2): cycles Sine -> Triangle -> Square -> Sawtooth -> Sine...
  Pot 1 (A0): Frequency  (0.5 - 3.0 Hz - tuned so a 60fps camera gets
              15-30+ samples per cycle, enough to resolve waveform SHAPE)
  Pot 2 (A1): Amplitude  (peak LED brightness, 40-255; floor of 40 keeps
              the LED visibly on even at the lowest amplitude setting)
  Output: White LED on Pin 9 (PWM)

  Note on the square wave: this code sets the LED to fully ON or fully OFF
  with no software ramping - analogWrite() changes the duty cycle instantly.
  Any rounding you observe in a captured square wave is happening downstream
  (camera auto-exposure, sensor response), not here.
*/

// ---- pins ----
const int LED_PIN = 9;
const int BUTTON_PIN = 2;
const int POT_FREQ_PIN = A0;
const int POT_AMP_PIN = A1;

// ---- waveform selection ----
const int NUM_WAVEFORMS = 4;
enum Waveform { SINE = 0, TRIANGLE = 1, SQUARE = 2, SAWTOOTH = 3 };
const char* waveNames[NUM_WAVEFORMS] = {"SINE", "TRIANGLE", "SQUARE", "SAWTOOTH"};
volatile int waveformIndex = 0;

// ---- button debounce (tracks raw reading vs. confirmed stable state separately -
//      this separation is what makes the debounce actually work correctly) ----
int buttonState = HIGH;       // debounced, confirmed state
int lastButtonState = HIGH;   // raw reading from the previous loop
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50; // ms - real buttons bounce more than simulated ones

// ---- signal generation ----
double phase = 0.0;              // 0.0-1.0, position within the current cycle
unsigned long lastMicros = 0;
const float FREQ_MIN = 0.5;
const float FREQ_MAX = 3.0;

void setup() {
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  Serial.begin(115200);
  lastMicros = micros();
  Serial.println("time_ms,waveform,freq_hz,amplitude,brightness");
}

void loop() {
  handleButton();

  // elapsed time since last loop, used to advance phase smoothly regardless
  // of how fast/slow this particular loop iteration runs
  unsigned long now = micros();
  float dt = (now - lastMicros) / 1000000.0;
  lastMicros = now;

  int freqRaw = analogRead(POT_FREQ_PIN);
  int ampRaw  = analogRead(POT_AMP_PIN);

  float freq = FREQ_MIN + (FREQ_MAX - FREQ_MIN) * (freqRaw / 1023.0);
  int amplitude = map(ampRaw, 0, 1023, 40, 255);

  phase += freq * dt;
  if (phase >= 1.0) phase -= (long)phase; // wrap back into 0..1

  int brightness = computeBrightness(waveformIndex, phase, amplitude);
  analogWrite(LED_PIN, brightness);

  logSerial(freq, amplitude, brightness);
}

void handleButton() {
  int reading = digitalRead(BUTTON_PIN);

  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;
      if (buttonState == LOW) {
        waveformIndex = (waveformIndex + 1) % NUM_WAVEFORMS;
        Serial.print("Waveform changed to: ");
        Serial.println(waveNames[waveformIndex]);
      }
    }
  }

  lastButtonState = reading;
}

int computeBrightness(int wf, double ph, int amp) {
  double val = 0; // normalized 0..1 position within the waveform
  switch (wf) {
    case SINE:
      val = (sin(2 * PI * ph) + 1.0) / 2.0;
      break;
    case TRIANGLE:
      val = 1.0 - fabs(2.0 * ph - 1.0);
      break;
    case SQUARE:
      val = (ph < 0.5) ? 1.0 : 0.0; // hard on/off - no ramping
      break;
    case SAWTOOTH:
      val = ph;
      break;
  }
  return (int)(val * amp);
}

void logSerial(float freq, int amplitude, int brightness) {
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint < 20) return; // ~50Hz log, doesn't affect LED timing
  lastPrint = millis();

  Serial.print(millis());                        Serial.print(",");
  Serial.print(waveNames[waveformIndex]);         Serial.print(",");
  Serial.print(freq, 2);                          Serial.print(",");
  Serial.print(amplitude);                        Serial.print(",");
  Serial.println(brightness);
}
