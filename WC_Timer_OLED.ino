/*
 Name:		WC_Timer_OLED.ino
 Created:	02.09.2026 15:46:18
 Author:	marti

  Received 7051022 closed
  Received 7051018 open
  !
*/

#include <U8g2lib.h>
#include <Wire.h>
#include <RCSwitch.h>
#include <Adafruit_NeoPixel.h> 

// #define DEBUG 
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

constexpr uint16_t secondsToOpen1 = 180;
constexpr uint16_t secondsToOpen2 = 120;
constexpr uint16_t secondsToOpen3 = 60;

constexpr uint16_t beepInterval = 300;
constexpr uint16_t beepInterval2 = 200;
constexpr uint16_t beepInterval3 = 166;

constexpr uint16_t closingCnt = 15; 
constexpr uint16_t openingCnt = 20; 
constexpr uint16_t alarm1Cnt = 15; 
constexpr uint16_t alarm2Cnt = 20; 
constexpr uint16_t alarm3Cnt = 30;

uint8_t alarmCount = 0;
bool ledOn = false;
bool ledOnOld = false;
int whiteHigh, whiteLow, blueMid, blueLow, greenMid, greenLow, redHigh, redMid, redLow;

int seconds = 0;
int runSeconds = 0;
int millisToWait = 1000;
bool toneOff = false;
bool newState = false;

// debouncing
int buttonStateT, buttonStateP;            // the current reading from the input pin
int lastButtonStateT = HIGH, lastButtonStateP = HIGH;  // the previous reading from the input pin
unsigned long lastDebounceTimeT = 0, lastDebounceTimeP = 0;  // the last time the output pin was toggled
unsigned long loopCounter = 0;
long cnt = 0;
char retValFade = 'n';

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
//                         0       1       2       3       4       5      6      7       8           9    

TimerState tState;
TimerState tStateOld;

TimerHandle_t timerCnt;
TimerHandle_t timerLed;
TimerHandle_t timerBeep;
TimerHandle_t timerBeep2;
TimerHandle_t timerBeep3;

Adafruit_NeoPixel pixel(LED_COUNT, LED_PIN, NEO_GRBW + NEO_KHZ800);

void pixelOn(int color = 0) {
  ledOn = true;
  pixel.setPixelColor(0, color);
  pixel.show();
}

void onTimerCnt(TimerHandle_t pxTimer) {
  cnt++;
}
void onTimerLed(TimerHandle_t pxTimer) {
  ledOn = false;
}
void onTimerBeep(TimerHandle_t pxTimer) {
  beepOn(false);
}
void onTimerBeep2(TimerHandle_t pxTimer) {
  beepOn(true);
  xTimerChangePeriod(timerBeep, pdMS_TO_TICKS(beepInterval2), 0);
  //pixelOn(whiteHigh);
  //xTimerChangePeriod(timerLed, pdMS_TO_TICKS(100), 0);
}
void onTimerBeep3(TimerHandle_t pxTimer) {
  beepOn(true);
  xTimerChangePeriod(timerBeep, pdMS_TO_TICKS(beepInterval3), 0);
  xTimerChangePeriod(timerBeep, pdMS_TO_TICKS(beepInterval3), 0);
}

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
  delay(1000);
  serPrint("\nWC_Timer OLED");
  tState = TimerState::Closed;
  tStateOld = tState;  
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

  beep(50);
  initColors();
  pixel.begin();
  pixel.clear();
  pixel.show();

  timerCnt = xTimerCreate("TimerCnt", pdMS_TO_TICKS(1000), pdTRUE, (void*)0, onTimerCnt);
  timerLed = xTimerCreate("TimerLed", pdMS_TO_TICKS(4), pdFALSE, (void*)0, onTimerLed);
  timerBeep = xTimerCreate("TimerBeep", pdMS_TO_TICKS(6), pdFALSE, (void*)0, onTimerBeep);
  timerBeep2 = xTimerCreate("TimerBeep2", pdMS_TO_TICKS(8), pdFALSE, (void*)0, onTimerBeep2);
  timerBeep3 = xTimerCreate("TimerBeep3", pdMS_TO_TICKS(10), pdFALSE, (void*)0, onTimerBeep3);
  if (timerCnt != NULL && timerBeep2 != NULL && timerLed != NULL && timerBeep != NULL) {
    xTimerStart(timerCnt, 0);
  }
  else {
    serPrint("Fehler beim Erstellen des Timers 'timerCnt'!", true);
  }

  delay(500);

  testPixel();
  serPrint("Setup ends");
} // setup ends
// *************************************************************

// *************************************************************
//        LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP 
// *************************************************************
void loop() {
  // TimerState: Closing, Closed, Opening, Open1, Alarm1, Open2, Alarm2, Open3, Alarm3, TimerFinished 
  prepDisplay(seconds);
  if (tState > TimerState::Closed)
    seconds--;
  
  serPrint(String(seconds) + ", ", false);

  // retVal: 'n' = nothing, 't' = toneOff pressed, 'p' = plusOne pressed, 'c' = window closed, 'o' = window open
  // retval from dispAlarm: r = running, f = finished, n = nothing
  char retVal;
  millisToWait = 1000;

  if (retValFade == 'n')
    retVal = waitMillis(millisToWait);
  else {
    retVal = retValFade;
    retValFade = 'n';
  }

  if (retVal != 'n') {
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
        tState = TimerState::Closing;
        newState = true;
        seconds = 0;
      }
    }
    else if (retVal == 'o') {
      if (tState != TimerState::Opening) {
        serPrint("Window is open");
        tState = TimerState::Opening;
        newState = true;
        seconds = secondsToOpen1;
      }
    }
  }

  switch (tState) {
  case TimerState::Closing:
    if (newState) {
      alarmCount = closingCnt;
      newState = false;
      beepOn(true);
      xTimerChangePeriod(timerBeep, pdMS_TO_TICKS(beepInterval2), 0);
      xTimerChangePeriod(timerBeep2, pdMS_TO_TICKS(beepInterval2 * 3), 0);
    }
    pixelOn(greenMid);
    xTimerChangePeriod(timerLed, pdMS_TO_TICKS(100), 0);
    alarmCount--;
    if (alarmCount <= 0) {
      toneOff = false;
      tState = TimerState::Closed;
      newState = true;
    }
    break;

  case TimerState::Closed:
    if (cnt % 60 == 0) {
      newState = false;
      retValFade = dispPixelFade();
    }
    break;

  case TimerState::Opening:
    if (newState) {
      alarmCount = openingCnt;
      newState = false;
      beepOn(true);
      xTimerChangePeriod(timerBeep, pdMS_TO_TICKS(beepInterval2), 0);
    }
    pixelOn(blueMid);
    xTimerChangePeriod(timerLed, pdMS_TO_TICKS(150), 0);
    alarmCount--;
    if (alarmCount <= 0) {
      tState = TimerState::Open1;
      newState = true;
    }
    break;

  case TimerState::Open1:
    if (newState) {
      newState = false;
			seconds = secondsToOpen1;
		}
    if (seconds <= 0) {
      tState = TimerState::Alarm1;
      newState = true;
		} 
    else if (cnt % 5 == 0) {
      pixelOn(blueMid);
      xTimerChangePeriod(timerLed, pdMS_TO_TICKS(200), 0);
    }
    break;

  case TimerState::Alarm1:
    if (newState) {
      alarmCount = alarm1Cnt;
      newState = false;
    }
    beepOn(true);
    xTimerChangePeriod(timerBeep, pdMS_TO_TICKS(beepInterval), 0);
    pixelOn(whiteHigh);
    xTimerChangePeriod(timerLed, pdMS_TO_TICKS(200), 0);
    alarmCount--;
    if (alarmCount <= 0) {
      tState = TimerState::Open2;
      newState = true;
    }
    break;

  case TimerState::Open2:
    if (newState) {
      newState = false;
      seconds = secondsToOpen2;
    }
    if (seconds <= 0) {
      tState = TimerState::Alarm2;
      newState = true;
    }
    else if (cnt % 2 == 0) {
      pixelOn(whiteLow);
      xTimerChangePeriod(timerLed, pdMS_TO_TICKS(200), 0);
    }
    break;

  case TimerState::Alarm2:
    if (newState) {
      alarmCount = alarm2Cnt;
      newState = false;
    }
		beepOn(true); 
    xTimerChangePeriod(timerBeep, pdMS_TO_TICKS(beepInterval2), 0);
    xTimerChangePeriod(timerBeep2, pdMS_TO_TICKS(beepInterval2 * 2), 0);
    pixelOn(whiteHigh);
    xTimerChangePeriod(timerLed, pdMS_TO_TICKS(200), 0);
    alarmCount--;
    if (alarmCount <= 0) {
      tState = TimerState::Open3;
      newState = true;
    }
    break;

  case TimerState::Open3:
    if (newState) {
      newState = false;
      seconds = secondsToOpen3;
    }
    if (seconds <= 0) {
      tState = TimerState::Alarm3;
      newState = true;
    }
    pixelOn(whiteHigh);
    xTimerChangePeriod(timerLed, pdMS_TO_TICKS(150), 0);
    break;

  case TimerState::Alarm3:
    if (newState) {
      alarmCount = alarm3Cnt;
      newState = false;
    }
    beepOn(true);
    xTimerChangePeriod(timerBeep, pdMS_TO_TICKS(beepInterval3), 0);
    xTimerChangePeriod(timerBeep2, pdMS_TO_TICKS(beepInterval3 * 2), 0);
    xTimerChangePeriod(timerBeep3, pdMS_TO_TICKS(beepInterval3 * 4), 0);
    pixelOn(redHigh);
    xTimerChangePeriod(timerLed, pdMS_TO_TICKS(200), 0);
    alarmCount--;
    if (alarmCount <= 0) {
      tState = TimerState::TimerFinished;
      newState = true;
    }
    break;

  case TimerState::TimerFinished:
    if (cnt % 10 == 0) {
      pixelOn(redHigh);
      xTimerChangePeriod(timerLed, pdMS_TO_TICKS(200), 0);
		}
    break;
  }

  if (cnt > 86400) { // 86'400 = 24h
    beep(100);
    ESP.restart();
  }

#ifdef DEBUG
  if (tStateOld != tState) {
    tStateOld = tState;
    Serial.println(static_cast<int>(tState));
  }
#endif
}
// *************************************************************
//   loop ends  loop ends  loop ends  loop ends  loop ends
// *************************************************************

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
      dispOled("  -  ", xCol, 64, isClosed); // :
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

// return: 'n' = nothing, 't' = toneOff pressed, 'p' = plusOne pressed, 'c' = window closed, 'o' = window open
char waitMillis(uint32_t waitMillis) {
  bool actButton = false;
  char retVal = 'n';
	uint32_t actMillis;
  uint32_t maxMillis = millis() + waitMillis;
  uint32_t beepWait = 0;
  while (true) {  // (cnt == cntOld)
    actMillis = millis();
		// led ausschalten; geht nur ausserhalb der interruptfunktion, sonst wird der ledTimer nicht getriggert
    if (ledOn != ledOnOld) {
      ledOn = false;
      ledOnOld = false;
      pixelOff();
      #ifdef DEBUG
      Serial.print("*");
      #endif
    }

    // Buttons
    int readingT = digitalRead(STOP_TONE_PIN);
    if (readingT != lastButtonStateT) {
      lastDebounceTimeT = actMillis; // reset the debouncing timer
    }
    if ((actMillis - lastDebounceTimeT) > DEBOUNCE_DELAY) {
      if (readingT != buttonStateT) {
        buttonStateT = readingT;
        if (buttonStateT == LOW) {
          // return 't';
          retVal = 't';
          actMillis = maxMillis;
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
          // return 'p';
          retVal = 'p';
          actMillis = maxMillis;
        }
      }
    }
    lastButtonStateP = readingP;

    // Window Switch
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

    // maxMillis 
    if (actMillis >= maxMillis) {
      // reset beep and led
      beepOn(false);
      pixelOff();
      break;
    }
  }
  //cntOld = cnt
  return retVal;
}

void pixelOff() {
  pixel.clear();
  pixel.show();
  ledOn = false;
}

void dispPixel(char type) {
  // type 
  // t (toneOff pressed)
  // p (plusOne pressed)
  int color;
  if (type == 't') {
    color = greenLow;
  }
  else {
    color = blueLow;
  }
  pixelOn(color);
  xTimerChangePeriod(timerLed, pdMS_TO_TICKS(100), 0);
}

void beepOn(bool beepIsOn) {
  if (!toneOff) {
    if (beepIsOn)
      digitalWrite(BEEP_PIN, LOW);
    else
      digitalWrite(BEEP_PIN, HIGH);
  }
#ifdef DEBUG
  else if (beepIsOn)
   Serial.print(" beep ");
#endif
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
  int bRed = 2;
  while (bRed < 50) {
    pixel.setPixelColor(0, pixel.Color(bRed, 0, 0, 0));
    pixel.show();
    retVal = waitMillis(55);
    if (retVal != 'n') {
      bRed = -1;
      break;
    }
    bRed += 1;
  }
  while (bRed > 0) {
    pixel.setPixelColor(0, pixel.Color(bRed, 0, 0, 0));
    pixel.show();
    retVal = waitMillis(55);
    if (retVal != 'n') {
      break;
    }
    bRed -= 1;
  }
  pixel.clear();
  pixel.show();
  return retVal;
}

void testPixel() {
  //dispPixelFade();
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

