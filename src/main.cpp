#include <Arduino.h>
#include <Ticker.h>
#include <stdint.h>
#include <inttypes.h>
#include <TM1637TinyDisplay6.h>
#include <RTClib.h>
#include <SPI.h>
#include <Wire.h>

#define CLK D2 //
#define DIO D1

#define inPin D5
#define SDA_PIN D6  // GPIO4 on ESP8266
#define SCL_PIN D7  // GPIO5 on ESP8266

volatile unsigned long currTime = 0;
volatile unsigned long prevTime = 0;
volatile signed char ready = 0;
volatile unsigned long duration = 0;
volatile uint8_t bit = 0 ;
volatile uint64_t d_dcf77 = 0x0000000000000000;
volatile uint64_t data = 0x0000000000000000;
volatile uint64_t test = 0x0000000000000000;
volatile char c[59];
char hexBuffer[17];
volatile bool time_ready = false;
static const char *weekday[] = { "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday", NULL};
volatile bool resync = false;

volatile bool dts = false; // dts time check bit (bit 16 (from 0))
volatile bool summer_time = false;
uint8_t tempH ;


bool lost_power = false;
volatile byte time_set = 0x00;

const uint8_t celsius[] = {
  SEG_A | SEG_B | SEG_F | SEG_G,  // Degree symbol
  SEG_A | SEG_D | SEG_E | SEG_F   // C
};

char  timeStr[9];
char  dateStr[9];
char  temp[6];
volatile uint8_t sw = 0x00 ;

TM1637TinyDisplay6 display(CLK, DIO); // 6-Digit Display Class
RTC_DS3231 rtc;
IRAM_ATTR void ISR(){
  if (digitalRead(inPin) == HIGH){
    prevTime = millis();
  } else {
    duration = millis() - prevTime;
    if (( duration >= 1700 && duration <= 2050) || (0 == ready && -1 == ready)) {
          d_dcf77 = 0;
          bit = 0;
          ready = 1;
          Serial.printf("Caught marker bit..%lu\n", duration);
        }
    else if ( duration >= 810 && duration <= 910 && 1 == ready){
      Serial.printf("Bit: %i Duration:%lu\n", bit, duration);
      d_dcf77  &= ~(1ULL << bit);
      c[57 - bit] = '0';
      bit++;
      Serial.println(d_dcf77, HEX);
    }
    else if ( duration >= 750 && duration <= 810 && 1 == ready ){
      Serial.printf("Bit: %i Duration:%lu\n", bit, duration);
      d_dcf77  |= (1ULL << bit);
      c[57 - bit] = '1';
      bit ++;
      Serial.println(d_dcf77, HEX);
    }
    else {
      Serial.printf("Place Anttenna correctly/Trying to catch marker bit...: %lu\n", duration);
      ready = 0;
      bit = 0;
      d_dcf77 = 0;
      if (!resync) display.showString("dcFErr");
    }

    if ( ready == 1 && bit == 58){
      data = d_dcf77;
      sprintf(hexBuffer, "%016llX", d_dcf77);
      Serial.println(hexBuffer);
      c[58] = '\0';
      Serial.printf("%s\n", c);
      ready = 0;
      d_dcf77 = 0;
      time_ready = 1;
      resync = false;
    }
  }
}

bool checkParity(int val){
  //
  byte parity  = 0;
  while(val){
    parity ^= val & 0x01;
    val >>= 1;
  }
  return parity;
}



void setup() {
  display.begin();
  display.setBrightness(7);
  Wire.begin(SDA_PIN, SCL_PIN);
  pinMode(inPin, INPUT_PULLUP);
  Serial.begin(9600);
  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    display.showString("rtcErr");
    while (true);
  } else {
    Serial.println("RTC Ok...");
    delay(1000);
    if (rtc.lostPower()){
      Serial.println(" RTC Power lost");
      attachInterrupt(digitalPinToInterrupt(inPin), ISR, CHANGE);
      display.showString("dcf5yC");
    } else {
      Serial.println("Time is set already");
      display.clear();
      detachInterrupt(digitalPinToInterrupt(inPin));
      delay(1000);
      time_set = 1;
    }
  }
  // attachInterrupt(digitalPinToInterrupt(inPin), ISR, CHANGE);
}

//##################################################################################

void loop() {

  if(!rtc.begin()){
    display.showString("rtcErr");
    lost_power = true;
  }

  if(time_ready){
    data = data >> 16;
    (data & ( 1ULL << 0)) ? printf("Time change at the next hour: yes\n") : printf("Time change at the next hour : no\n");
    (data & ( 1ULL << 0)) ? dts = true : dts = false; // check 16.th bit(from 0) if it is true change time 1 hour ahead or 1 hour back.

    //bit 19 == 1 ? add leap second next hour
    // even parity
    (data >> 1  & 0x0000000000000003) == 1 ? printf("CEST\n") : printf("CET\n");
    (data >> 1  & 0x0000000000000003) == 1 ? summer_time = true: summer_time = false; //if 10(bit18,bit17) == Winter Time , 01(bit18,bit17) Summer Time

    sprintf(hexBuffer, "%16llX", data);
    Serial.println(hexBuffer);
    byte minute1 = (data >> 5  &  0x000000000000000F);
    byte minute10 = (data >> 9  & 0x0000000000000007);
    byte hour1 = (data >> 13  & 0x000000000000000F);
    byte hour10 = (data >> 17  & 0x0000000000000003);
    byte caltag1 = (data >> 20  & 0x000000000000000F);
    byte caltag10 = (data >> 24  & 0x0000000000000003);
    byte week = (data >> 26  & 0x0000000000000007);
    byte month1 = (data >> 29  & 0x000000000000000F);
    byte month10 = (data >> 33  & 0x0000000000000001);
    byte year1 = (data >> 34  & 0x000000000000000F);
    byte year10 = (data >> 38  & 0x000000000000000F);

    (data & (1ULL << 39))  ? printf("Parity: 1\n") : printf("Parity 0:\n");
    Serial.printf("%u%u:%u%u - %u%u.%u%u.20%u%u - %s\n",hour10, hour1, minute10, minute1, caltag10, caltag1, month10, month1, year10, year1, weekday[week - 1] );
    time_ready = 0;
    time_set = 0 ; /// added

    if(time_set == 0 && rtc.begin()){
      display.clear();
      rtc.adjust(DateTime((2000 + ((year10 * 10) + year1)) , (month10 * 10 + month1), (caltag10 * 10 + caltag1), (hour10 * 10 + hour1), (minute10 * 10 + minute1), 0));
    //rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
      time_set = 1;
      detachInterrupt(digitalPinToInterrupt(inPin));
    }
}
  if (time_set && rtc.begin()) {
      DateTime now = rtc.now();

      if(tempH < now.hour() && dts && summer_time){
        rtc.adjust(DateTime((now.year() , now.month(), now.day(), ((now.hour() + 1) % 24), now.minute(), now.second())));
        dts = false;
      } else if(tempH < now.hour() && dts && !summer_time){
        rtc.adjust(DateTime((now.year() , now.month(), now.day(), ((now.hour() - 1) % 24), now.minute(), now.second())));
        dts = false;
      }

      if(now.minute() % 19 == 0 && !resync ){
           Serial.println("resync started...");
           attachInterrupt(digitalPinToInterrupt(inPin), ISR, CHANGE);
           resync = true;
        }

    tempH = now.hour();

    if( now.second() % 5 == 0){
        display.clear();
        sw ++;
        if (3 == sw){
          sw = 0;
        }
      }
      switch (sw)
      {
      case 2:
        display.showNumberDec(rtc.getTemperature(), false, 4, 3);
        display.setSegments(celsius, 2, 4 );
        Serial.println(rtc.getTemperature());
        break;
      case 1:
        sprintf(timeStr, "%02u%02u%02u", now.hour(), now.minute(), now.second());
        display.showString(timeStr);
        Serial.println(timeStr);
        break;
      case 0:
        sprintf(dateStr, "%02u%02u%02u", now.day(), now.month(), now.year() % 2000 );
        display.showString(dateStr);
        Serial.println(dateStr);
        break;
      }
    }
delay(1000);
}

