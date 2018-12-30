#include "SIM800L.h"
 
#define SIM800_TX_PIN 10
#define SIM800_RX_PIN 9
#define SIM800_RST_PIN 8

SIM800L* sim800l;

void setup() {
  // Initialize Serial Monitor for debugging
  Serial.begin(9600);
  while(!Serial);
   
  // Initialize SIM800L driver with a reception buffer of 512 bytes, debug disabled
  sim800l = new SIM800L(SIM800_TX_PIN,SIM800_RX_PIN, SIM800_RST_PIN, 512, false);

  // Setup module for GPRS communication
  setupModule();
}
 
void loop() {
  // Establish GPRS connectivity (5 trials)
  bool connected = false;
  for(int i = 0; i < 5 && !connected; i++) {
    delay(1000);
    connected = sim800l->connectGPRS();
  }

  // Check if connected, if not reset the module and setup the config again
  if(connected) {
    Serial.println("GPRS connected !");
  } else {
    Serial.println("GPRS not connected !");
    Serial.println("Reset the module.");
    sim800l->reset();
    setupModule();
    return;
  }

  // Do HTTP POST communication with 10s for the timeout (read and write)
  int rc = sim800l->doPost("https://postman-echo.com/post", "application/json", "{\"name\": \"morpheus\", \"job\": \"leader\"}", 10000, 10000);
   if(rc == 200) {
    // Success, output the data received on the serial
    Serial.print("HTTP POST successful (");
    Serial.print(sim800l->getDataSizeReceived());
    Serial.println(" bytes)");
    Serial.print("Received : ");
    Serial.println(sim800l->getDataReceived());
  } else {
    // Failed...
    Serial.print("HTTP POST error ");
    Serial.println(rc);
  }

  // Close GPRS connectivity (5 trials)
  bool disconnected = sim800l->disconnectGPRS();
  for(int i = 0; i < 5 && !connected; i++) {
    delay(1000);
    disconnected = sim800l->disconnectGPRS();
  }
  
  if(disconnected) {
    Serial.println("GPRS disconnected !");
  } else {
    Serial.println("GPRS still connected !");
  }

  // Go into low power mode
  bool lowPowerMode = sim800l->setPowerMode(MINIMUM);
  if(lowPowerMode) {
    Serial.println("Module in low power mode");
  } else {
    Serial.println("Failed to switch module to low power mode");
  }

  // End of program... wait...
  while(1);
}

void setupModule() {
    // Wait until the module is ready to accept AT commands
  while(!sim800l->isReady()) {
    Serial.println("Problem to initialize AT command, retry in 1 sec");
    delay(1000);
  }
  Serial.println("Setup Complete!");

  // Wait for the GSM signal
  int signal = sim800l->getSignal();
  while(signal <= 0) {
    delay(1000);
    signal = sim800l->getSignal();
  }
  Serial.print("Signal OK (strenght: ");
  Serial.print(signal);
  Serial.println(")");
  delay(1000);

  // Wait for operator network registration (national or roaming network)
  NetworkRegistration network = sim800l->getRegistrationStatus();
  while(network != REGISTERED_HOME && network != REGISTERED_ROAMING) {
    delay(1000);
    network = sim800l->getRegistrationStatus();
  }
  Serial.println("Network registration OK");
  delay(1000);

  // Setup APN for GPRS configuration
  bool success = sim800l->setupGPRS("Internet.be");
  while(!success) {
    success = sim800l->setupGPRS("Internet.be");
    delay(5000);
  }
  Serial.println("GPRS config OK");
}
