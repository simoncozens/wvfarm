#include <SPI.h>
#include <RH_RF95.h>

#define RFM95_CS 10
#define RFM95_RST 9
#define RFM95_INT 2
#define RF95_FREQ 915.0

// Singleton instance of the radio driver
RH_RF95 rf95(RFM95_CS, RFM95_INT);

#define PUMP_ON_SIGNAL_PIN  7
#define PUMP_OFF_SIGNAL_PIN 8
#define SIREN_PIN 12
#define RELAY_PULSE_DURATION 1000
// Blinky on receipt
#define LED 13

#define SIREN_DURATION 60000

int pump_status = 255; // Undefined
unsigned long last_change;
unsigned long to_go_on_time = 0;
unsigned long to_go_off_time = 0;


void siren(int amount) {
  digitalWrite(SIREN_PIN, HIGH);
  delay(amount);
  digitalWrite(SIREN_PIN, LOW);
}

void pump_on () {
  Serial.println("Stopping siren, sending pump on pulse");
  digitalWrite(SIREN_PIN, LOW);
  digitalWrite(PUMP_ON_SIGNAL_PIN, HIGH);
  delay(RELAY_PULSE_DURATION);
  digitalWrite(PUMP_ON_SIGNAL_PIN, LOW);
  last_change = millis();
}

void pump_off () {
  Serial.println("Sending pump off pulse");
  pump_status = 0;
  last_change = millis();
  digitalWrite(PUMP_OFF_SIGNAL_PIN, HIGH);
  delay(RELAY_PULSE_DURATION);
  digitalWrite(PUMP_OFF_SIGNAL_PIN, LOW);
  // Just in case we are cancelling an existing order...
  to_go_on_time = 0;
  digitalWrite(SIREN_PIN, LOW);
}

void schedule_pump_on() {
  Serial.print("Sounding siren, will turn on pump at ");
  to_go_on_time = millis();
  to_go_on_time += SIREN_DURATION;
  Serial.println(to_go_on_time);
  digitalWrite(SIREN_PIN, HIGH);
  pump_status = 1;
  last_change = millis();
}

void scheduler() {
  if (to_go_on_time != 0 && millis() >= to_go_on_time) {
    pump_on();
    to_go_on_time = 0;
  }
  if (to_go_on_time !=0 && millis()%1000 == 0) {
    Serial.print("Waiting. To go on is: ");
    Serial.print(to_go_on_time);
    Serial.print(". Time now is ");
    Serial.println(millis());
  }
}

void setup()
{
  pinMode(LED, OUTPUT);
  pinMode(RFM95_RST, OUTPUT);
  pinMode(SIREN_PIN, OUTPUT);
  pinMode(PUMP_OFF_SIGNAL_PIN, OUTPUT);
  pinMode(PUMP_ON_SIGNAL_PIN, OUTPUT);
  last_change = millis();

  digitalWrite(RFM95_RST, HIGH);
  while (!Serial);
  Serial.begin(9600);
  siren(100);
  Serial.println("Pump switch");

  // manual reset
  digitalWrite(RFM95_RST, LOW);
  delay(10);
  digitalWrite(RFM95_RST, HIGH);
  delay(10);

  while (!rf95.init()) {
    Serial.println("LoRa radio init failed");
    while (1);
  }
  Serial.println("LoRa radio init OK!");
  // Defaults after init are 434.0MHz, modulation GFSK_Rb250Fd250, +13dbM
  if (!rf95.setFrequency(RF95_FREQ)) {
    Serial.println("setFrequency failed");
    while (1);
  }
  Serial.print("Set Freq to: "); Serial.println(RF95_FREQ);
  rf95.setTxPower(23, false);
}

void loop()
{
  if (rf95.available())
  {
    // Should be a message for us now   
    uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(buf);
    
    if (rf95.recv(buf, &len))
    {
      digitalWrite(LED, HIGH);
      RH_RF95::printBuffer("Received: ", buf, len);
      buf[len]=0;
      Serial.print("Got: "); Serial.println((char*)buf);
      Serial.print("RSSI: "); Serial.println(rf95.lastRssi(), DEC);
      delay(210);
      if (strncmp("WVFPQSTAT",(char*)buf,9) == 0) {
        Serial.println("Status query");
        uint8_t data[] = "X";
        if (millis() - last_change > 10 * 60 * 1000) {
          pump_status = 255;
        }
        data[0] = pump_status;
        rf95.send(data, 1);
        rf95.waitPacketSent();
        RH_RF95::printBuffer("Status query response sent: ", data, 1);
        digitalWrite(LED, LOW);
      } else if (strncmp("WVFPONXXX",(char*)buf,9) == 0) {
        Serial.println("Turning on");
        uint8_t data[] = "Turning on";
        delay(200);
        rf95.send(data, sizeof(data));
        rf95.waitPacketSent();
        
        digitalWrite(LED, LOW);
        schedule_pump_on();
      } else if (strncmp("WVFPOFFXX",(char*)buf,9) == 0) {
        Serial.println("Turning off");
        uint8_t data[] = "Turning off";
        delay(200);
        rf95.send(data, sizeof(data));
        rf95.waitPacketSent();
        digitalWrite(LED, LOW);
        pump_off();
      }
    }
    else
    {
      Serial.println("Receive failed");
    }
  }
  scheduler();
}
