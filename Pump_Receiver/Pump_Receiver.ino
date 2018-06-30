#include <SPI.h>
#include <LoRa.h>

#define PUMP_ON_SIGNAL_PIN  8
#define PUMP_OFF_SIGNAL_PIN 10
#define SIREN_PIN 12
#define RELAY_PULSE_DURATION 100

#define LORA_MAX_MESSAGE 1024

// Blinky on receipt
#define LED 13

int pump_status = 255; // Undefined
unsigned long last_change;

void siren(int amount) {
  digitalWrite(SIREN_PIN, HIGH);
  delay(amount);
  digitalWrite(SIREN_PIN, LOW);
}

void pump_on () {
  // Turn on siren for a minute
  pump_status = 1;
  siren(1000 * 60);
  last_change = millis();
  digitalWrite(PUMP_ON_SIGNAL_PIN, HIGH);
  delay(RELAY_PULSE_DURATION);
  digitalWrite(PUMP_ON_SIGNAL_PIN, LOW);
}

void pump_off () {
  pump_status = 0;
  last_change = millis();
  digitalWrite(PUMP_OFF_SIGNAL_PIN, HIGH);
  delay(RELAY_PULSE_DURATION);
  digitalWrite(PUMP_OFF_SIGNAL_PIN, LOW);
}

void setup() {
  pinMode(LED, OUTPUT);
  pinMode(SIREN_PIN, OUTPUT);
  pinMode(PUMP_OFF_SIGNAL_PIN, OUTPUT);
  pinMode(PUMP_ON_SIGNAL_PIN, OUTPUT);
  last_change = millis();
  siren(100);
  Serial.begin(9600);
  while (!Serial);

  Serial.println("Pump receiver, version 2");

  if (!LoRa.begin(915E6)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }

  lora_send("Hello world",12);
  // register the receive callback
  LoRa.onReceive(onReceive);

  // put the radio into receive mode
  LoRa.receive();
}

void loop() {
  // do nothing
}

void onReceive(int len) {
  // received a packet
  uint8_t buf[LORA_MAX_MESSAGE];
  if (len > LORA_MAX_MESSAGE) len = LORA_MAX_MESSAGE;

  digitalWrite(LED, HIGH);

  // read packet
  Serial.print("Received packet '");
  for (int i = 0; i < len; i++) {
    char m = (char)LoRa.read();
    if (i>=4) {
      Serial.print(m);
      buf[i-4]=m;
    }
  }
  // print RSSI of packet
  Serial.print("' with RSSI ");
  Serial.println(LoRa.packetRssi());
  delay(210);

  if (strncmp("WVFPQSTAT",(char*)buf,9) == 0) {
        uint8_t data[] = " ";
        Serial.print("Status query, returning: ");
        if (millis() - last_change > 10 * 60 * 1000) {
          pump_status = 255;
        }
        data[0] = pump_status;
        Serial.println(pump_status);
        lora_send(data, sizeof(data));
        digitalWrite(LED, LOW);
//  } else if (strncmp("WVFPONXXX",(char*)buf,9) == 0) {
//        uint8_t data[] = "Turning on";
//        lora_send(data, sizeof(data));
//        digitalWrite(LED, LOW);
//        pump_on();
//  } else if (strncmp("WVFPOFFXX",(char*)buf,9) == 0) {
//    uint8_t data[] = "Turning off";
//    lora_send(data, sizeof(data));
//    digitalWrite(LED, LOW);
//    pump_off();
  } else {
    Serial.println("Unknown packet");
  }
}

void lora_send(char* buff, int len) {
  LoRa.beginPacket(false);
  LoRa.write(buff, len);
  LoRa.endPacket();
  Serial.println("Sent!");
  // Go back to listening.
  LoRa.receive();
}

