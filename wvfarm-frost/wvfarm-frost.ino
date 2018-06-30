/*******************************************************************************
 * Copyright (c) 2015 Thomas Telkamp and Matthijs Kooijman
 * Modified Simon Cozens
 *******************************************************************************/

#include <lmic.h>
#include <hal/hal.h>
#include <SPI.h>
#include <Adafruit_SleepyDog.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#if defined(ARDUINO_SAMD_ZERO) && defined(SERIAL_PORT_USBVIRTUAL)
  #define Serial SERIAL_PORT_USBVIRTUAL
#endif

// Configuration variables

#undef OTAA                             // LoRa association mode
const unsigned SLEEP_SECONDS = 30; // Time to request low power sleep
const unsigned TX_INTERVAL = 5;        // Time to idle between transmissions
const lmic_pinmap lmic_pins = {         // Adafruit LoRa M0 Feather pin mapping
    .nss = 8,
    .rxtx = LMIC_UNUSED_PIN,
    .rst = 4,
    .dio = {3, 5, 6},
};
//const lmic_pinmap lmic_pins = {         // Uno pin mapping
//    .nss = 10,
//    .rxtx = LMIC_UNUSED_PIN,
//    .rst = 9,
//    .dio = {2, 6, 7},
//};

#define ONE_WIRE_POWER 10
#define ONE_WIRE_BUS 11
#define VBATPIN A7

#ifdef OTAA
static const u1_t PROGMEM DEVEUI[8]= { 0xD5, 0xE2, 0x1B, 0x7A, 0xCB, 0x69, 0xB6, 0x00 } ; 
static const u1_t PROGMEM APPEUI[8]= { 0x2A, 0x8E, 0x00, 0xD0, 0x7E, 0xD5, 0xB3, 0x70 } ;
static const u1_t PROGMEM APPKEY[16] = { 0x01, 0x80, 0x9F, 0x04, 0x80, 0xEB, 0x0D, 0x00, 0x9D, 0x0E, 0xF0, 0xA7, 0x08, 0x30, 0x33, 0x0A };

void os_getDevEui (u1_t* buf) { memcpy_P(buf, DEVEUI, 8);}
void os_getArtEui (u1_t* buf) { memcpy_P(buf, APPEUI, 8);}
void os_getDevKey (u1_t* buf) {  memcpy_P(buf, APPKEY, 16);}
#else
static const PROGMEM u1_t NWKSKEY[16] = { 0xED, 0xB1, 0x0D, 0x47, 0xB7, 0x39, 0x6C, 0x32, 0xCB, 0x51, 0x3A, 0xFD, 0xAF, 0x8C, 0xFD, 0x70 };
static const u1_t PROGMEM APPSKEY[16] = { 0x9F, 0xA0, 0x14, 0xAE, 0xC6, 0x40, 0xB8, 0xD6, 0x1C, 0x55, 0xB0, 0xFC, 0x61, 0xAB, 0x69, 0x45 };
static const u4_t DEVADDR = 0x26041453 ; // <-- Change this address for every node!

void os_getArtEui (u1_t* buf) { }
void os_getDevEui (u1_t* buf) { }
void os_getDevKey (u1_t* buf) { }
#endif

int tx_interval;
static osjob_t sendjob;
static osjob_t initjob;
static osjob_t reportjob;

// Some utility funcs
void gotosleep() {
  int targetMS = SLEEP_SECONDS * 1000;
  Serial.print("Going to sleep for ");
  Serial.print(targetMS);
  Serial.println(" ms");

  while (targetMS > 0) {
    Serial.print("Zzzz. ");
    Serial.print(targetMS);
    Serial.println(" ms left.");
    // 16 seconds is the maximum you can sleep using the watchdog
    int sleepMS = Watchdog.sleep(16000);
    targetMS -= sleepMS;
  }
  Serial.println("I'm awake!");
  blink();
  delay(1000); // Time to wake up.
}

void blink() {
  digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
  delay(100);                       // wait for a second
  digitalWrite(LED_BUILTIN, LOW);    // turn the LED off by making the voltage LOW                       // wait for a second
}

float readBattery(){
  float measuredvbat = analogRead(VBATPIN);
  measuredvbat *= 2;    // we divided by 2, so multiply back
  measuredvbat *= 3.3;  // Multiply by 3.3V, our reference voltage
  measuredvbat /= 1024; // convert to voltage
  Serial.print("VBat: " ); Serial.println(measuredvbat);
  return measuredvbat;
}

// Temperature
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature DS18B20(&oneWire);

void powerUpTemp() {
  pinMode(ONE_WIRE_POWER, OUTPUT);
  digitalWrite(ONE_WIRE_POWER, HIGH);
  delay(1000);
}

void powerDownTemp() {
  digitalWrite(ONE_WIRE_POWER, LOW);
}

float readTemperature() {
  float temp;
  int n = 0;
  powerUpTemp();
  do {
    DS18B20.requestTemperatures();
    temp = DS18B20.getTempCByIndex(0);
    delay(50);
    n++;
  } while (n < 20 && (temp == 85.0 || temp == 0.0 || temp < -50.0));
  if (temp < -50.0) temp = 0.0;
  powerDownTemp();
  return temp;
}

// Radio protocol things

void scheduleNextJob () {
  Serial.println("Scheduling next transmission");
#ifdef OTAA
    os_setTimedCallback(j, os_getTime()+sec2osticks(tx_interval), reportfunc);
#else
    // I find myself having to reset the radio every transmission...
    os_setTimedCallback(&initjob, os_getTime()+sec2osticks(tx_interval), initfunc);
#endif
}

void onEvent (ev_t ev) {
    Serial.print(os_getTime());
    Serial.print(": ");
    switch(ev) {
        case EV_SCAN_TIMEOUT:
            Serial.println(F("EV_SCAN_TIMEOUT"));
            break;
        case EV_BEACON_FOUND:
            Serial.println(F("EV_BEACON_FOUND"));
            break;
        case EV_BEACON_MISSED:
            Serial.println(F("EV_BEACON_MISSED"));
            break;
        case EV_BEACON_TRACKED:
            Serial.println(F("EV_BEACON_TRACKED"));
            break;
        case EV_JOINING:
            Serial.println(F("EV_JOINING"));
            digitalWrite(LED_BUILTIN, HIGH);
            break;
        case EV_JOINED:
            Serial.println(F("EV_JOINED"));
            LMIC_setLinkCheckMode(0);
            digitalWrite(LED_BUILTIN, LOW);
#ifdef OTAA
            reportfunc(&reportjob);
#endif
            break;
        case EV_RFU1:
            Serial.println(F("EV_RFU1"));
            break;
        case EV_JOIN_FAILED:
            Serial.println(F("EV_JOIN_FAILED"));
            digitalWrite(LED_BUILTIN, HIGH);
            break;
        case EV_REJOIN_FAILED:
            Serial.println(F("EV_REJOIN_FAILED"));
            digitalWrite(LED_BUILTIN, HIGH);
            break;
        case EV_TXCOMPLETE:
            Serial.println(F("EV_TXCOMPLETE (includes waiting for RX windows)"));
            if (LMIC.txrxFlags & TXRX_ACK)
              Serial.println(F("Received ack"));
            gotosleep();
            scheduleNextJob();
            break;
        case EV_LOST_TSYNC:
            Serial.println(F("EV_LOST_TSYNC"));
            break;
        case EV_RESET:
            Serial.println(F("EV_RESET"));
            break;
        case EV_RXCOMPLETE:
            Serial.println(F("EV_RXCOMPLETE"));
            break;
        case EV_LINK_DEAD:
            Serial.println(F("EV_LINK_DEAD"));
            break;
        case EV_LINK_ALIVE:
            Serial.println(F("EV_LINK_ALIVE"));
            break;
         default:
            Serial.println(F("Unknown event"));
            break;
    }
}

static void initfunc (osjob_t* j) {
    // reset MAC state
    Serial.println("Reset called");
    LMIC_reset();
#ifdef OTAA
    LMIC_setLinkCheckMode(1);
    LMIC_startJoining();
#else
#ifdef PROGMEM
    // On AVR, these values are stored in flash and only copied to RAM
    // once. Copy them to a temporary buffer here, LMIC_setSession will
    // copy them into a buffer of its own again.
    uint8_t appskey[sizeof(APPSKEY)];
    uint8_t nwkskey[sizeof(NWKSKEY)];
    memcpy_P(appskey, APPSKEY, sizeof(APPSKEY));
    memcpy_P(nwkskey, NWKSKEY, sizeof(NWKSKEY));
    LMIC_setSession (0x1, DEVADDR, nwkskey, appskey);
#else
    // If not running an AVR with PROGMEM, just use the arrays directly
    LMIC_setSession (0x1, DEVADDR, NWKSKEY, APPSKEY);
#endif

    // Disable link check validation
    LMIC_setLinkCheckMode(0);
    reportfunc(&reportjob);
#endif
}

static void reportfunc (osjob_t* j) {
    float t = readTemperature();
    float v = readBattery();
    char buf[16];
    Serial.println(t);
    String s = String(t);
    s.concat(",");
    s.concat(String(v));
    String m;
    if (s.length() > 16) m = s.substring(0,15);
    else m = s;
    m.toCharArray(buf, sizeof(buf));
    Serial.println(buf);
    for (int i = 0; i < m.length(); ++i)
      LMIC.frame[i] = (unsigned char) buf[i];
    LMIC_setTxData2(1, LMIC.frame, s.length(), 1); // (port 1, length() bytes , unconfirmed)
    blink();
}


void setup() {
    int wait = 0;
    digitalWrite(LED_BUILTIN, HIGH);
    while ( ! Serial && wait < 1200) { delay( 10 ); wait += 10;}
    delay(30);
    if (Serial) {
      Serial.begin(9600);
      Serial.println(F("Starting"));
    }
    DS18B20.begin();
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
    tx_interval = TX_INTERVAL; // ideally OTA update!
    // initialize runtime env
    Serial.println("Calling OS init");
    os_init();
    Serial.println("Calling init func");
#ifdef OTAA
    os_setCallback(&initjob, initfunc);
#else
    initfunc(&initjob);
#endif
}

void loop() {
   os_runloop();
}
