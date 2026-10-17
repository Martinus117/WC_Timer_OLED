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
#include <RCSwitch.h>
#include <Adafruit_NeoPixel.h> 

#define DEBUG 
//constexpr int WINDOW_IS_OPEN = 4594186; // alt
constexpr int WINDOW_IS_OPEN = 7051018;
//constexpr int WINDOW_IS_CLOSED = 4594190; // alt
constexpr int WINDOW_IS_CLOSED = 7051022;

constexpr uint8_t STOP_TONE_PIN = 10;
constexpr uint8_t PLUSONE_PIN = 2; // 4
constexpr uint8_t RX_PIN = 20;
constexpr uint8_t BEEP_PIN = 3;  
constexpr uint8_t LED_PIN = 4;

constexpr uint8_t I2C_SDA = 5;
constexpr uint8_t I2C_SCL = 6;

constexpr uint8_t LED_COUNT = 1;
constexpr uint8_t  DEBOUNCE_DELAY = 50; // The debounce time in milliseconds

constexpr uint16_t secondsToOpen1 = 62;
constexpr uint16_t secondsToOpen2 = 31;
constexpr uint16_t secondsToOpen3 = 15;
unsigned int open1Seconds;
unsigned int open2Seconds;
unsigned int open3Seconds;
unsigned long endMillisLed = 0;
unsigned long endMillisBeep = 0;

uint8_t alarmCount = 0;

int whiteHigh, whiteLow, blueMid, blueLow, greenMid, greenLow, redHigh, redMid, redLow;

int seconds = 0;
int runSeconds = 0;
int millisToWait = 1000;
bool toneOff = false;
bool newState = false;

int buttonStateT, buttonStateP;            // the current reading from the input pin
int lastButtonStateT = HIGH, lastButtonStateP = HIGH;  // the previous reading from the input pin
unsigned long lastDebounceTimeT = 0, lastDebounceTimeP = 0;  // the last time the output pin was toggled



// display stuff
// there is no 72x40 constructor in u8g2 hence the 72x40 screen is mapped in the middle of the 132x64 pixel buffer of the SSD1306 controller
// R0: x:28 y:24 - R1: x:24 y:28 (width <> height) - R2: x:28 y:0
// Rotation: U8G2_R0, U8G2_R1: 90, U8G2_R2: 180, U8G2_R3: 270
//U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R2, U8X8_PIN_NONE, 6, 5); 
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, I2C_SCL, I2C_SDA);
int width = 72;
int height = 40;
int xOffset = 28;
int yOffset = 24;
int xCol;

RCSwitch mySwitch = RCSwitch();

enum class TimerState { Closing, Closed, Opening, Open1, Alarm1, Open2, Alarm2, Open3, Alarm3, TimerFinished };
TimerState tState;
TimerState lastTState;

hw_timer_t* _timer = NULL;
volatile int interruptCounter = 0;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

void IRAM_ATTR onTimer() {
  portENTER_CRITICAL_ISR(&timerMux);
  interruptCounter++;
  portEXIT_CRITICAL_ISR(&timerMux);
}

int cnt = 0;

Adafruit_NeoPixel pixel(LED_COUNT, LED_PIN, NEO_GRBW + NEO_KHZ800);

void serPrint(String txt, bool nl = true) {
  //#ifdef DEBUG
  if (nl)
    Serial.println(txt);
  else 
    Serial.print(txt);
  //#endif
}

void setup() {
  Serial.begin(115200);
  delay(250);
  serPrint("\nWC_Timer OLED");
  tState = TimerState::Closed;
  lastTState = TimerState::Closed;
  mySwitch.enableReceive(RX_PIN);

  xCol = xOffset;
  Wire.begin(I2C_SDA, I2C_SCL);
  u8g2.begin();
  u8g2.setContrast(255); // set contrast to maximum 
  u8g2.setBusClock(400000); //400kHz I2C 
  u8g2.setFont(u8g2_font_inb16_mn);

  pinMode(STOP_TONE_PIN, INPUT_PULLUP);
  pinMode(PLUSONE_PIN, INPUT_PULLUP);
  pinMode(BEEP_PIN, OUTPUT);
  mySwitch.resetAvailable();

  _timer = timerBegin(1000000);  // 1MHz (=1uS) 
  if (_timer == NULL) {
    serPrint("timer not initialized");
    while (true);
  }

  // _timer = timerEnd;
  timerAttachInterrupt(_timer, onTimer);
  timerAlarm(_timer, 1000000, true, 0);

  beep(50);
  initColors();
  pixel.begin();
  pixel.clear();
  pixel.show();
  
  testPixel();
  initOpenSeconds();
  serPrint("Setup ends");
} // setup ends
// *************************************************************

// *************************************************************
//        LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP 
// *************************************************************
void loop() {
  // TimerState: Closing, Closed, Opening, Open1, Alarm1, Open2, Alarm2, Open3, Alarm3, TimerFinished 
  cnt = interruptCounter;
  prepDisplay(seconds);
  seconds--;
  // serPrint(String(seconds) + ", ", false);

  // retVal: 'n' = nothing, 't' = toneOff pressed, 'p' = plusOne pressed, 'c' = window closed, 'o' = window open
  // retval from dispAlarm: r = running, f = finished, n = nothing
  char retVal = waitMillis(millisToWait);
  if (retVal != 'n') {
    //serPrint("retVal:" + String(retVal));
    //Serial.println(retVal);
    if (retVal == 't') {
      serPrint("toneOff pressed");
      dispPixel('t');
      toneOff = !toneOff;
    }
    else if (retVal == 'p' && (tState != TimerState::Closed)) {
      serPrint("plusOne pressed");
      dispPixel('p');
      seconds += 60;
    }
    else if (retVal == 'c') {
      if (tState != TimerState::Closing) {
        serPrint("Window is closed");
        lastTState = tState;
        tState = TimerState::Closing;
        newState = true;
        seconds = 0;
      }
    }
    else if (retVal == 'o') {
      if (tState != TimerState::Opening) {
        serPrint("Window is open");
        lastTState = tState;
        tState = TimerState::Opening;
        newState = true;
      }
    }
  }
  char retValA;
  if (tState == TimerState::Closing) {
    retValA = dispAlarm(newState);
    // alarmCount is set in dispAlarm
    if (retValA == 'f') {
      tState = TimerState::Closed;
      newState = true;
    }
  }
  else if (tState == TimerState::Closed) {
    retValA = dispAlarm(newState);
  }
  else if (tState == TimerState::Opening) {
    retValA = dispAlarm(newState);
    if (retValA == 'f') {
      tState = TimerState::Open1;
      newState = true;
      seconds = secondsToOpen1;
    }
  }
  else if (tState == TimerState::Open1) {
    retValA = dispAlarm(newState);
    // open1 finished
    if (retValA == 'f') {  // if (seconds <= 0) {
      tState = TimerState::Alarm1;
      seconds = 0;
      newState = true;
    }
  }
  else if (tState == TimerState::Alarm1) {
    dispAlarm(newState);
    if (alarmCount == 0) {
      tState = TimerState::Open2;
      newState = true;
      seconds = secondsToOpen2;
    }
  }
  else if (tState == TimerState::Open2) {

  }
  else if (tState == TimerState::Alarm2) {

  }
  else if (tState == TimerState::Open3) {

  }
  else if (tState == TimerState::Alarm3) {

  }
  else if (tState == TimerState::TimerFinished) {

  }
  
  millisToWait = 1000;
  
  if (cnt > 86400) { // 86'400 = 24h
    beep(100);
    ESP.restart();
  }
  #ifdef DEBUG
    //Serial.print("Status:");
    Serial.print(static_cast<int>(tState));
    Serial.print(" ");
  #endif
} 
// *************************************************************
//   loop ends  loop ends  loop ends  loop ends  loop ends
// *************************************************************

char dispAlarm(bool isNewState) {
  //  Closing, Closed, Opening, Open1, Alarm1, Open2, Alarm2, Open3, Alarm3, TimerFinished 
  // alarmCount 
  // Closing: schwacher Blitz green every sec for 10 secs 
  // Closed: dispPixelFade alle 
  // Opening: schwacher Blitz blau, 10 mal, 1 sekunde abstand
  // Open1, Open2, Open3: 
  // Alarm1: starker weisser Blitz, 5 mal (1 sekunde Abstand)
  //          Beep 250
  // Open2: starker weisser Blitz, starker roter Blitz, 5 mal (1 sekunde Abstand)
  //          Beep 350
  // Alarm2: starker weisser Blitz, starker blauer Blitz, starker roter Blitz, 5 mal (1 sekunde Abstand)
  //          Beep 500
  // Open3: starker weisser Blitz, starker roter Blitz, 5 mal (1 sekunde Abstand)
  //          Beep 350
  // Alarm3: starker weisser Blitz, starker blauer Blitz, starker roter Blitz, 5 mal (1 sekunde Abstand)
  //          Beep 500
  // 
  // TimerFinished: starker roter Blitz, starker roter Blitz, starker roter Blitz, 10 mal (1 sekunde Abstand)
  //          Beep 500
  int color = 0, ledWait = 0, beepWait = 0;
  bool showPixel = true;
  uint aktMillis = millis();
  //endMillisLed = 0;
  //endMillisBeep = 0;
  // retval: r=running, f=finished, n=nothing
  char retVal = 'r';
  switch (tState)
  {
  case TimerState::Closing:
    if (isNewState) {
      alarmCount = 11;
      newState = false;
    }
    color = greenMid;
    endMillisLed = aktMillis + 100;
    alarmCount--;
    if (alarmCount <= 0)
      retVal = 'f';
    break;

  case TimerState::Closed:
    #ifdef DEBUG
      if (cnt % 10 == 0) // 30
    #else
      if (cnt % 30 == 0) // 30
    #endif
      retVal = dispPixelFade();
    showPixel = false;
    newState = false;
    break;
  case TimerState::Opening:
    if (isNewState) {
      alarmCount = 11;
      newState = false;
      endMillisBeep = aktMillis + 100;
    }
    color = blueMid;
    endMillisLed = aktMillis + 100;
    alarmCount--;
    if (alarmCount <= 0)
      retVal = 'f';
    break;

  case TimerState::Open1:
    if (isNewState) {
      alarmCount = 11;
      newState = false;
			endMillisBeep = aktMillis + 100;  
    }
    else {}
    if (cnt % 10 == 0) {
      color = whiteLow;
      showPixel = true;
      endMillisLed = aktMillis + 100;
    }
    else showPixel = false;
    break;

  case TimerState::Alarm1:

    break;
  case TimerState::Open2:
    break;
  case TimerState::Alarm2:
    break;
  case TimerState::Open3:
    break;
  case TimerState::Alarm3:
    break;
  case TimerState::TimerFinished:
    break;
  default:
    break;
  }

  if (showPixel) {
    pixel.setPixelColor(0, color);
    pixel.show();
    if (endMillisBeep > 0) {
      beepOn(true);
    }
    //delay(ledWait);
    //pixel.clear();
    //pixel.show();
    //if (beepWait > 0)
    //  beep(beepWait);
  }
  return retVal;
}

void prepDisplay(int sec) {
  // variable = (condition) ? expressionTrue : expressionFalse;
  bool isClosed = tState == TimerState::Closed;
  String ms;
  if (tState > TimerState::Closed) {
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
    if (runSeconds > 4) runSeconds = 0;
    if (runSeconds == 0)
      dispOled("-    ", xCol, 64, isClosed);
    else if (runSeconds == 1)
      dispOled(" -   ", xCol, 64, isClosed);
    else if (runSeconds == 2)
      dispOled("  :  ", xCol, 64, isClosed);
    else if (runSeconds == 3)
      dispOled("   - ", xCol, 64, isClosed);
    else if (runSeconds == 4)
      dispOled("    -", xCol, 64, isClosed);
    runSeconds++;
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
  if (toneOff)
    u8g2.drawButtonUTF8(xOffset + 42, 36, U8G2_BTN_INV, 0, 1, 1, "T aus");
  else
    u8g2.drawButtonUTF8(xOffset + 42, 36, U8G2_BTN_BW0, 0, 1, 1, "T ein");

	 int temp = static_cast<int>(tState);
   u8g2.setCursor(xOffset + 32, 36);
   u8g2.print(temp);

  u8g2.sendBuffer();
}

char waitMillis(uint32_t waitMillis) {
  bool actButton = false;
	// retVal: 'n' = nothing, 't' = toneOff pressed, 'p' = plusOne pressed, 'c' = window closed, 'o' = window open
  char retVal = 'n';
  //Serial.print(waitMillis);
	uint32_t actMillis;
  uint32_t maxMillis = millis() + waitMillis;
  while (true) {
    actMillis = millis();
    int readingT = digitalRead(STOP_TONE_PIN);
    if (readingT != lastButtonStateT) {
      lastDebounceTimeT = actMillis; // reset the debouncing timer
    }
    if ((actMillis - lastDebounceTimeT) > DEBOUNCE_DELAY) {
      if (readingT != buttonStateT) {
        buttonStateT = readingT;
        if (buttonStateT == LOW) {
          return 't';
        }
      }
    }
    lastButtonStateT = readingT;
    int readingP = digitalRead(PLUSONE_PIN);
    if (readingP != lastButtonStateP) {
      lastDebounceTimeP = millis(); // reset the debouncing timer
    }
    if ((actMillis - lastDebounceTimeP) > DEBOUNCE_DELAY) {
      if (readingP != buttonStateP) {
        buttonStateP = readingP;
        if (buttonStateP == LOW) {
          return 'p';
        }
      }
    }
    lastButtonStateP = readingP;

    if (mySwitch.available()) {
      unsigned long rv = mySwitch.getReceivedValue();
      //serPrint("switch: " + String(rv));
      if (rv == WINDOW_IS_CLOSED)
        retVal = 'c';
      else if (rv == WINDOW_IS_OPEN)
        retVal = 'o';
      mySwitch.resetAvailable();
      //break;
    }
    if (endMillisBeep > 0 && actMillis > endMillisBeep) {
      beepOn(false);
      endMillisBeep = 0;
    }
    if (endMillisLed > 0 && actMillis > endMillisLed) {
      pixel.clear();
      pixel.show();
      endMillisLed = 0;
		} 
    if (actMillis >= maxMillis)
      break;
  }
  //Serial.print("a ");
  //Serial.print(actMillis);
  //Serial.print("m ");
  //Serial.println(maxMillis);

  return retVal;
}

void dispPixel(char type) {
  // type 
  // t (toneOff pressed)
  // p (plusOne pressed)
  int color;
  if (type == 't') {
    color = pixel.Color(40, 0, 0, 0);
  }
  else {
    color = pixel.Color(0, 0, 40, 0);
  }
  pixel.setPixelColor(0, color);
  pixel.show();
  endMillisLed = millis() + 100;
 
}

void beepOn(bool beepIsOn) {
  if (!toneOff) {
    if (beepIsOn)
      digitalWrite(BEEP_PIN, LOW);
    else
      digitalWrite(BEEP_PIN, HIGH);
  }
}


void beep(int dur) {
  if (!toneOff) {
    digitalWrite(BEEP_PIN, LOW);
    delay(dur);
    digitalWrite(BEEP_PIN, HIGH);
  }
}

void initColors() {
  whiteHigh = pixel.Color(0, 0, 0, 220);
  whiteLow  = pixel.Color(0, 0, 0, 40);
  redHigh   = pixel.Color(220, 0, 0, 0);
  redMid    = pixel.Color(100, 0, 0, 0);
  redLow    = pixel.Color(40, 0, 0, 0);
  greenMid  = pixel.Color(0, 100, 0, 0);
  greenLow  = pixel.Color(0, 40, 0, 0);
  blueMid   = pixel.Color(0, 0, 100, 0);
  blueLow   = pixel.Color(0, 0, 40, 0);
}

char dispPixelFade() {
  char retVal = 'n';
  for (int ii = 2; ii <= 22; ii++) {
    pixel.setPixelColor(0, ii + 2, ii - 2, 0, 0);
    //pixel.setPixelColor(0,1,2,3,4)  n, r, g, b, w
    pixel.show();
    retVal = waitMillis(65);
    if (retVal != 'n') {
			Serial.print("*");
      return retVal;
    }
    //else Serial.print(">");
  }
  retVal = waitMillis(200);
  if (retVal != 'n') {
    return retVal;
  }
  //else Serial.print(":");
  for (int ii = 22; ii > 3; ii--) {
    //for (int ii = 15; ii > 0; ii--) {
    // pixel.setPixelColor(0, pixel.Color(0, max(0, ii - 2), 0, 0));
    pixel.setPixelColor(0, ii + 2, ii - 2, 0, 0);
    pixel.show();
    retVal = waitMillis(65);
    if (retVal != 'n') {
      return retVal;
    }
    //else Serial.print("<");
  }
  pixel.clear();
  pixel.show();
  return retVal;
}

void testPixel() {
  dispPixelFade();
  pixel.setPixelColor(0, pixel.Color(50, 0, 0, 0)); // RED
  pixel.show();
  delay(500);
  pixel.setPixelColor(0, pixel.Color(0, 50, 0, 0)); // GREEN
  pixel.show();
  delay(500);
  pixel.setPixelColor(0, pixel.Color(0, 0, 50, 0)); // BLUE
  pixel.show();
  delay(500);
  pixel.setPixelColor(0, pixel.Color(0, 0, 0, 50)); // WHITE
  pixel.show();
  delay(500);
  pixel.clear();
  pixel.show();
}

void initOpenSeconds() {
  open1Seconds = 90000;
  open2Seconds = 90000;
  open3Seconds = 90000;
}
