/*
 Name:		WC_Timer_OLED.ino
 Created:	02.09.2026 15:46:18
 Author:	marti

  Received 7051022 closed
  Received 7051018 open
  enum Days {Saturday, Sunday, Tuesday, Wednesday, Thursday, Friday};
  Days day = Saturday;
  if (day == Saturday)

  //int secs = abs(seconds % 60);
  //int mins = abs((int)seconds / 60);
  //String minTerm = seconds < 0 ? "-" : "0";
  //String ms = mins < 10 ? minTerm + String(mins) : String(mins);
  //ms += ":" + (secs < 10 ? "0" + String(secs) : String(secs));
  //pOledLine(ms, xCol, 64); // rot 180: 16; 0: 64

*/

#include <U8g2lib.h>
#include <Wire.h>
#include <rcswitch.h>


constexpr int WINDOW_IS_OPEN = 7051018;
constexpr int WINDOW_IS_CLOSED = 7051022 ;

constexpr uint8_t STOP_TIMER_PIN = 1;
constexpr uint8_t PLUSONE_PIN = 2;
constexpr uint8_t RX_PIN = 20;
constexpr uint8_t BEEP_PIN = 3;  
constexpr uint8_t LED_PIN = 1;

constexpr uint8_t I2C_SDA = 5;
constexpr uint8_t I2C_SCL = 6;

constexpr uint8_t LED_COUNT = 1;
constexpr unsigned long DEBOUNCE_DELAY = 50; // The debounce time in milliseconds

// display stuff
// there is no 72x40 constructor in u8g2 hence the 72x40 screen is mapped in the middle of the 132x64 pixel buffer of the SSD1306 controller
// R0: x:28 y:24 - R1: x:24 y:28 (width <> height) - R2: x:28 y:0
// Rotation: U8G2_R0, U8G2_R1: 90, U8G2_R2: 180, U8G2_R3: 270
//U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R2, U8X8_PIN_NONE, 6, 5); 
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, I2C_SCL, I2C_SDA);

//int width = 72;
//int height = 40;
int xOffset = 28;
int yOffset = 0;
int xCol;
int startSeconds = 62;
int seconds = 0;
int millisToWait = 1000;
bool stopTimer = false;
int lastSteadyState = HIGH;  // The previous steady state from the input pin
int lastFlickerableState = HIGH; // The previous flickerable state from the input pin
int currentState;            // The current reading from the input pin

unsigned long lastDebounceTime = 0;  // The last time the output pin was toggled

RCSwitch mySwitch = RCSwitch();

enum class TimerState { Closed, ClosedNoTimer, NoTimer, TimerA, TimerN, TimerFinished };
TimerState tState;
TimerState lastTState;
int numberOfIterations = 3;

hw_timer_t* _timer = NULL;
volatile int interruptCounter = 0;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

void IRAM_ATTR onTimer() {
  portENTER_CRITICAL_ISR(&timerMux);
  interruptCounter++;
  portEXIT_CRITICAL_ISR(&timerMux);
}

int cnt = 0;

void setup() {
  Serial.begin(115200);
  delay(250);
  Serial.println("\nWC_Timer OLED");
  tState = TimerState::Closed;
  lastTState = TimerState::Closed;
  mySwitch.enableReceive(RX_PIN);

  xCol = xOffset;
  Wire.begin(I2C_SDA, I2C_SCL);
  u8g2.begin();
  u8g2.setContrast(255); // set contrast to maximum 
  u8g2.setBusClock(400000); //400kHz I2C 
  u8g2.setFont(u8g2_font_inb16_mn);

  pinMode(STOP_TIMER_PIN, INPUT_PULLUP);
  pinMode(PLUSONE_PIN, INPUT_PULLUP);
  mySwitch.resetAvailable();

  _timer = timerBegin(1000000);  // 1MHz (=1uS) 
  if (_timer == NULL) {
    Serial.println("timer not initialized");
    while (true);
  }
  serPrint("vor timerAttachInterrupt");
  timerAttachInterrupt(_timer, onTimer);
  serPrint("vor timerAlarm(..)");
  timerAlarm(_timer, 1000000, true, 0);


  Serial.println("Setup ends");
} // setup ends
// *************************************************************

// the loop function runs over and over again until power down or reset
void loop() {
  //portENTER_CRITICAL(&timerMux);
  //interruptCounter = 0;
  //portEXIT_CRITICAL(&timerMux);

  cnt = interruptCounter;
  prepDisplay(seconds);
  seconds--;
	Serial.print(seconds);
  Serial.print(", ");
  if (millisToWait == 500)
    delay(500);
  char retVal = waitMillis(millisToWait);
  if (retVal != 'n') {
    Serial.print("retVal:");
    Serial.println(retVal);
    if (retVal == 's') {
      Serial.println("stopTimer pressed");
      stopTimer = !stopTimer;
      if (stopTimer) {
        lastTState = tState;
        tState = (tState == TimerState::Closed) ? TimerState::ClosedNoTimer : TimerState::NoTimer;
      }
      else {
        tState = lastTState;
			}
    }
    else if (retVal == 'p' && (tState != TimerState::Closed) && (tState != TimerState::ClosedNoTimer)) {
      Serial.println("plusOne pressed");
      seconds += 60;
    }
    else if (retVal == 'c') {
      Serial.println("Window is closed");
      lastTState = tState;
      tState = (tState == TimerState::NoTimer) ? TimerState::ClosedNoTimer : TimerState::Closed;
      seconds = 0;
    }
    else if (retVal == 'o') {
      Serial.println("Window is open");
      lastTState = tState;
      tState = (tState == TimerState::ClosedNoTimer) ? TimerState::NoTimer : TimerState::TimerA;
      seconds = startSeconds;
    }
		millisToWait -= 500;
  }
  else millisToWait = 1000;
  
  if (cnt > 86400) { // 864000 = 24h
    beep(100);
    ESP.restart();
  }
  
  Serial.print("Status:");
  Serial.println(static_cast<int>(tState));

} // loop ends
// *************************************************************

void prepDisplay(int sec) {
  // variable = (condition) ? expressionTrue : expressionFalse;
	bool isClosed = (tState == TimerState::Closed || tState == TimerState::ClosedNoTimer);
  String ms;
  if (tState != TimerState::Closed) {
    int secs = abs(sec % 60);
    int mins = abs((int)sec / 60);
    String minTerm = sec < 0 ? "-" : "0";
    if (sec < -599) {
      ms = minTerm + String(mins) + ((sec % 2 == 0) ? ":" : " ") + (secs < 10 ? "0" + String(secs) : String(secs));
      ms = ms.substring(0, 5);
    }
    else {
      ms = mins < 10 ? minTerm + String(mins) : String(mins);
      ms += ":" + (secs < 10 ? "0" + String(secs) : String(secs));
    }
    dispOled(ms, xCol, 64, isClosed); // rot 180: 16; 0: 64
  }
  else {
    if (sec % 2 == 0)
      dispOled("-----", xCol, 64, isClosed); 
    else
      dispOled("-- --", xCol, 64, isClosed);
  }
}

void dispOled(String txt, int col, int row, bool isClosed) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_inb16_mn);
  u8g2.setCursor(col, row);
  u8g2.print(txt);
  u8g2.setFont(u8g2_font_t0_11_mr);
  if (!isClosed) 
     u8g2.drawButtonUTF8(xOffset, 36, U8G2_BTN_BW0, 0, 1, 1, "+1m");
  if (stopTimer)
    u8g2.drawButtonUTF8(xOffset + 41, 36, U8G2_BTN_INV, 0, 1, 1, "Taus");
  else
    u8g2.drawButtonUTF8(xOffset + 41, 36, U8G2_BTN_BW0, 0, 1, 1, "Tein");

	int temp = static_cast<int>(tState);
  u8g2.drawButtonUTF8(xOffset + 39, 36, U8G2_BTN_INV, 0, 1, 1, String(temp));
  
  u8g2.sendBuffer();
}

char readButton(int button) {
  char retVal = 'l';
  uint endMillis = millis() + DEBOUNCE_DELAY;
  // Read the state of the button
  while (millis() < endMillis) {
    currentState = digitalRead(button);
    // If the switch changed, due to noise or pressing
    if (currentState != lastFlickerableState) {
      // Save the flickerable state
      lastFlickerableState = currentState;
    }
    // If the button state has changed:
    if (lastSteadyState == HIGH && currentState == LOW) {
      retVal = 'h';
      break;
    }
    lastSteadyState = currentState;
  }
  return retVal;
}

char waitMillis(uint32_t wmillis) {
  bool actButton = false;
  char retVal = 'n';
  uint32_t maxmillis, actMillis;
  maxmillis = millis() + wmillis;
  while (true) {
    char res1 = readButton(STOP_TIMER_PIN);
    if (res1 == 'h') {
      retVal = 's';
      break;
    }
    char res2 = readButton(PLUSONE_PIN);
    if (res2 == 'h') {
      retVal = 'p';
      break;
    }
    if (mySwitch.available()) {
      unsigned long rv = mySwitch.getReceivedValue();
      Serial.println(rv);
      if (rv == WINDOW_IS_CLOSED)
        retVal = 'c';
      else if (rv == WINDOW_IS_OPEN)
        retVal = 'o';
      mySwitch.resetAvailable();
      break;
    }

    actMillis = millis();
    if (actMillis >= maxmillis)
      break;
  }
  return retVal;
}

void serPrint(String txt) {
#ifdef DEBUG
  Serial.println(txt);
#endif
}

static void beep(int duration) {
  tone(4, 1000, duration);
  delay(duration);
  noTone(4);

  //tone(BEEP_PIN, 2794, wait);
  //noTone(BEEP_PIN);
  //tone(BEEP_PIN, 2960, wait);
  //noTone(BEEP_PIN);
  //tone(BEEP_PIN, 3136, wait);
  //noTone(BEEP_PIN);
  //delay(200);
  //tone(BEEP_PIN, 3136, wait);
  //noTone(BEEP_PIN);
  //tone(BEEP_PIN, 2960, wait);
  //noTone(BEEP_PIN);
  //tone(BEEP_PIN, 2794, wait);
  //noTone(BEEP_PIN);

}

static void dispPixel(int color, int millis) {
  //pixel.setPixelColor(0, color);
  //pixel.show();
  //delay(millis);
  //pixel.clear();
  //pixel.show();
}

static void dispPixelFade() {
  //for (int ii = 4; ii <= 22; ii++) {
  //  pixel.setPixelColor(0, ii, ii - 2, 0, 0);
  //  //pixel.setPixelColor(0,1,2,3,4)  n, r, g, b, w
  //  pixel.show();
  //  if (mySwitch.available())
  //    return;
  //  delay(65);
  //}
  //delay(200);
  //for (int ii = 22; ii > 3; ii--) {
  //  //for (int ii = 15; ii > 0; ii--) {
  //  // pixel.setPixelColor(0, pixel.Color(0, max(0, ii - 2), 0, 0));
  //  pixel.setPixelColor(0, ii, ii - 2, 0, 0);
  //  pixel.show();
  //  if (mySwitch.available())
  //    return;
  //  delay(65);
  //}
  //pixel.clear();
  //pixel.show();
}
