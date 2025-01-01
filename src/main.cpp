#include <Arduino.h>
#include <Ticker.h>
#include <stdint.h>
#include <inttypes.h>

const int inPin = D8;
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
    }

    if ( ready == 1 && bit == 58){
      data = d_dcf77;
      sprintf(hexBuffer, "%016llX", d_dcf77);
      Serial.println(hexBuffer);
      c[58] = '\0';
      Serial.printf("%s\n", c);
      ready = 0;
      d_dcf77 = 0;
      time_ready = true;
    }
  }
}

void setup() {
  pinMode(inPin, INPUT_PULLUP);
  Serial.begin(9600);
  attachInterrupt(digitalPinToInterrupt(inPin), ISR, CHANGE);
}

void loop() {
  if(time_ready){
    data = data >> 16;
    (data & ( 1ULL << 0)) ? printf("change summer: yes\n") : printf("change summer : no\n");
    (data >> 1  & 0x0000000000000003) == 1 ? printf("CEST\n") : printf("CET\n");
    sprintf(hexBuffer, "%16llX", data);
    Serial.println(hexBuffer);

    uint8_t minute1 = (data >> 5  &  0x000000000000000F);
    uint8_t minute10 = (data >> 9  & 0x0000000000000007);
    uint8_t hour1 = (data >> 13  & 0x000000000000000F);
    uint8_t hour10 = (data >> 17  & 0x0000000000000003);
    uint8_t caltag1 = (data >> 20  & 0x000000000000000F);
    uint8_t caltag10 = (data >> 24  & 0x0000000000000003);
    uint8_t week = (data >> 26  & 0x0000000000000007);
    uint8_t month1 = (data >> 29  & 0x000000000000000F);
    uint8_t month10 = (data >> 33  & 0x0000000000000001);
    uint8_t year1 = (data >> 34  & 0x000000000000000F);
    uint8_t year10 = (data >> 38  & 0x000000000000000F);
    Serial.printf("%i%i:%i%i - %i%i.%i%i.20%i%i - %s\n",hour10, hour1, minute10, minute1, caltag10, caltag1, month10, month1, year10, year1, weekday[week - 1] );
    time_ready = false;
  }
}

