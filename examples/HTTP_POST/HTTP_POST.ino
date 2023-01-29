/********************************************************************************
Based on

 * Example of HTTPS POST with Serial1 (Mega2560) and Arduino-SIM800L-driver     *
 * HTTPS_POST_HardwareSerial.ino                                                *
 * Author: Olivier Staquet                                                      *
 * Last version available on https://github.com/ostaquet/Arduino-SIM800L-driver *
 ********************************************************************************
 * MIT License
 *
 * Copyright (c) 2019 Olivier Staquet
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *******************************************************************************/
#include "SIM800L.h"

//#define SIM800_RST_PIN 6
#define SIM800L_RX 27
#define SIM800L_TX 26
#define SIM800L_PWRKEY 4
#define SIM800_RST_PIN 5
#define SIM800L_POWER 23


const char APN[] = "internet.t-mobile.cz";
const char URL[] = "http://fathome.zam.slu.cz/iot/add.php";
const char CONTENT_TYPE[] = "application/x-www-form-urlencoded";
const char PAYLOAD[] = "m=popo&i=9991";
//onst char PAYLOAD2[] = "&i=9999";
//const char CONTENT_TYPE[] = "application/json";
//const char PAYLOAD[] = "{\"name\": \"morpheus\", \"job\": \"leader\"}";

SIM800L* sim800l;

void setup() {
  // Initialize Serial Monitor for debugging
  Serial.begin(115200);


  // Initialize the GSM module and its AT Serial
  Serial.println("ESP32+SIM800L begins");
  pinMode(SIM800L_POWER, OUTPUT);
  digitalWrite(SIM800L_POWER, HIGH);
  Serial2.begin(9600, SERIAL_8N1, SIM800L_TX, SIM800L_RX);
  delay(5000);
  while (Serial2.available()) {
    Serial.write(Serial2.read());
    delay(500);
  }

  // Initialize SIM800L driver with an internal buffer of 200 bytes and a reception buffer of 512 bytes, debug disabled
  // sim800l = new SIM800L((Stream*)&Serial2, SIM800_RST_PIN, 200, 512);
  // Equivalent line with the debug enabled on the Serial
  sim800l = new SIM800L((Stream*)&Serial2, SIM800_RST_PIN, 200, 512, (Stream*)&Serial);
  //sim800l->enableEchoMode();

  // Setup module for GPRS communication
  setupModule();
}


void setupModule() {
  // Wait until the module is ready to accept AT commands
  while (!sim800l->isReady()) {
    Serial.println(F("Problem to initialize AT command, retry in 1 sec"));
    delay(1000);
  }
  Serial.println(F("Setup Complete!"));

  // Wait for the GSM signal
  uint8_t signal = sim800l->getSignal();
  int i = 0;
  while (signal <= 0 && i++ < 10) {
    delay(2000);
    signal = sim800l->getSignal();
  }
  Serial.print(F("Signal strenght: "));
  Serial.print(signal);
  Serial.println(F(")"));
  delay(1000);



  // Wait for operator network registration (national or roaming network)
  NetworkRegistration network = sim800l->getRegistrationStatus();
  while (network != REGISTERED_HOME && network != REGISTERED_ROAMING) {
    Serial.print(".");
    delay(1000);
    network = sim800l->getRegistrationStatus();
  }
  Serial.println(F("Network registration OK"));
  delay(1000);

  // Setup APN for GPRS configuration
  bool success = sim800l->setupGPRS(APN);
  while (!success) {
    success = sim800l->setupGPRS(APN);
    delay(5000);
  }
  Serial.println(F("GPRS config OK"));
}

/*
uint16_t mydoPost(const char* url, const char* contentType, const char* payload, uint16_t clientWriteTimeoutMs, uint16_t serverReadTimeoutMs) {
  uint16_t resultCode;
  resultCode = sim800l->doPost_Init(url, NULL, contentType);
  if (resultCode != 0) return resultCode;

  sim800l->doPost_AppendPayload(payload, clientWriteTimeoutMs);

  sim800l->doPost_AppendPayload(PAYLOAD2, clientWriteTimeoutMs);



  return sim800l->doPost_Send(serverReadTimeoutMs);
}
*/

void loop() {
  // Establish GPRS connectivity (5 trials)
  bool connected = false;
  Serial.print("Connecting to GPRS ");
  for (uint8_t i = 0; i < 60 && !connected; i++) {
    delay(2000);
    Serial.print(".");
    connected = sim800l->connectGPRS();
  }
  Serial.println(" .");

  // Check if connected, if not reset the module and setup the config again
  if (connected) {
    Serial.print(F("GPRS connected with IP "));
    Serial.println(sim800l->getIP());
  } else {
    Serial.println(F("GPRS not connected !"));
    Serial.println(F("Reset the module."));
    sim800l->reset();
    setupModule();
    return;
  }

  Serial.println(F("Start HTTP POST..."));

  // Do HTTP POST communication with 10s for the timeout (read and write)

   uint16_t rc = sim800l->doPost(URL, CONTENT_TYPE, PAYLOAD, 10000, 10000);

  // uint16_t rc = mydoPost(URL, CONTENT_TYPE, PAYLOAD, 10000, 10000);

  if (rc == 200) {
    // Success, output the data received on the serial
    Serial.print(F("HTTP POST successful ("));
    Serial.print(sim800l->getDataSizeReceived());
    Serial.println(F(" bytes)"));
    Serial.print(F("Received : "));
    Serial.println(sim800l->getDataReceived());
  } else {
    // Failed...
    Serial.print(F("HTTP POST error "));
    Serial.println(rc);
  }

  // Close GPRS connectivity (5 trials)
  bool disconnected = sim800l->disconnectGPRS();
  for (uint8_t i = 0; i < 5 && !connected; i++) {
    delay(1000);
    disconnected = sim800l->disconnectGPRS();
  }

  if (disconnected) {
    Serial.println(F("GPRS disconnected !"));
  } else {
    Serial.println(F("GPRS still connected !"));
  }

  // Go into low power mode
  bool lowPowerMode = sim800l->setPowerMode(MINIMUM);
  if (lowPowerMode) {
    Serial.println(F("Module in low power mode"));
  } else {
    Serial.println(F("Failed to switch module to low power mode"));
  }

  // End of program... wait...
  while (1)
    ;
}
