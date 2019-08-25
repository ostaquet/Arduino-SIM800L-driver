/********************************************************************************
 * Arduino-SIM800L-driver                                                       *
 * ----------------------                                                       *
 * Arduino driver for GSM module SIM800L                                        *
 * Author: Olivier Staquet                                                      *
 * Last version available on https://github.com/ostaquet/Arduino-SIM800L-driver *
 *******************************************************************************/
#include "SIM800L.h"

/**
 * Constructor; Init the driver, communication with the module and shared
 * buffer used by the driver (to avoid multiples allocation)
 */
SIM800L::SIM800L(int _pinTx, int _pinRx, int _pinRst, unsigned int _internalBufferSize, unsigned int _recvBufferSize, bool _enableDebug) {
  if(enableDebug) Serial.println(F("SIM800L : Active SoftwareSerial"));
  
  // Setup the Software serial
  serial = new SoftwareSerial(_pinTx,_pinRx);
  serial->begin(9600);
  delay(1000);

  // Store local variables
  enableDebug = _enableDebug;
  pinReset = _pinRst;
  pinMode(pinReset, OUTPUT);
  reset();

  // Prepare internal buffers
  if(enableDebug) {
    Serial.print(F("SIM800L : Prepare internal buffer of "));
    Serial.print(_internalBufferSize);
    Serial.println(F(" bytes"));
  }
  internalBufferSize = _internalBufferSize;
  internalBuffer = (char*) malloc(internalBufferSize);
  
  if(enableDebug) {
    Serial.print(F("SIM800L : Prepare reception buffer of "));
    Serial.print(_recvBufferSize);
    Serial.println(F(" bytes"));
  }
  recvBufferSize = _recvBufferSize;
  recvBuffer = (char *) malloc(recvBufferSize);
}

/**
 * Destructor; cleanup the memory allocated by the driver
 */
SIM800L::~SIM800L() {
  free(internalBuffer);
  free(recvBuffer);
}

/**
 * Do HTTP/S POST to a specific URL
 */
int SIM800L::doPost(char* url, char* contentType, char* payload, unsigned int clientWriteTimeoutMs, unsigned int serverReadTimeoutMs) {
  // Cleanup the receive buffer
  for(int i = 0; i < recvBufferSize; i++) {
    recvBuffer[i] = 0;
  }
  dataSize = 0;

  // Initiate HTTP/S session with the module
  int initRC = initiateHTTP(url);
  if(initRC > 0) {
    return initRC;
  }

  // Define the content type
  sendCommand("AT+HTTPPARA=\"CONTENT\",", contentType);
  if(!readResponseCheckAnswer(DEFAULT_TIMEOUT, "OK")) {
    if(enableDebug) Serial.println(F("SIM800L : doPost() - Unable to define the content type"));
    return 702;
  }

  // Prepare to send the payload
  char* tmpBuf = (char*)malloc(30);
  sprintf(tmpBuf, "AT+HTTPDATA=%d,%d", strlen(payload), clientWriteTimeoutMs);
  sendCommand(tmpBuf);
  free(tmpBuf);
  if(!readResponseCheckAnswer(DEFAULT_TIMEOUT, "DOWNLOAD")) {
    if(enableDebug) Serial.println(F("SIM800L : doPost() - Unable to send payload to module"));
    return 707;
  }

  // Write the payload on the module
  if(enableDebug) {
    Serial.print(F("SIM800L : doPost() - Payload to send : "));
    Serial.println(payload);
  }
  serial->listen();
  serial->flush();
  readToForget(500);
  serial->write(payload);
  serial->flush();

  // Start HTTP POST action
  sendCommand("AT+HTTPACTION=1");
  if(!readResponseCheckAnswer(DEFAULT_TIMEOUT, "OK")) {
    if(enableDebug) Serial.println(F("SIM800L : doPost() - Unable to initiate POST action"));
    return 703;
  }

  // Wait answer from the server
  if(!readResponse(serverReadTimeoutMs)) {
    if(enableDebug) Serial.println(F("SIM800L : doPost() - Server timeout"));
    return 408;
  }

  // Extract status information
  int idxBase = strIndex(internalBuffer, "+HTTPACTION: 1,");
  if(idxBase < 0) {
    if(enableDebug) Serial.println(F("SIM800L : doPost() - Invalid answer on HTTP POST"));
    return 703;
  }

  // Get the HTTP return code
  int httpRC = 0;
  httpRC += (internalBuffer[idxBase + 15] - '0') * 100;
  httpRC += (internalBuffer[idxBase + 16] - '0') * 10;
  httpRC += (internalBuffer[idxBase + 17] - '0') * 1;

  if(enableDebug) {
    Serial.print(F("SIM800L : doPost() - HTTP status "));
    Serial.println(httpRC);
  }

  if(httpRC == 200) {
    // Get the size of the data to receive
    dataSize = 0;
    for(int i = 0; (internalBuffer[idxBase + 19 + i] - '0') >= 0 && (internalBuffer[idxBase + 19 + i] - '0') <= 9; i++) {
      if(i != 0) {
        dataSize = dataSize * 10;
      }
      dataSize += (internalBuffer[idxBase + 19 + i] - '0');
    }
  
    if(enableDebug) {
      Serial.print(F("SIM800L : doPost() - Data size received of "));
      Serial.print(dataSize);
      Serial.println(F(" bytes"));
    }
  
    // Ask for reading and detect the start of the reading...
    sendCommand("AT+HTTPREAD");
    if(!readResponseCheckAnswer(DEFAULT_TIMEOUT, "+HTTPREAD: ", 2)) {
      return 705;
    }
  
    // Read number of bytes defined in the dataSize
    for(int i = 0; i < dataSize && i < recvBufferSize; i++) {
      while(!serial->available());
      if(serial->available()) {
        // Load the next char
        recvBuffer[i] = serial->read();
        // If the character is CR or LF, ignore it (it's probably part of the module communication schema)
        if((recvBuffer[i] == '\r') || (recvBuffer[i] == '\n')) {
          i--;
        }
      }
    }
  
    if(recvBufferSize < dataSize) {
      dataSize = recvBufferSize;
      if(enableDebug) {
        Serial.println(F("SIM800L : doPost() - Buffer overflow while loading data from HTTP. Keep only first bytes..."));
      }
    }
  
    // We are expecting a final OK
    if(!readResponseCheckAnswer(DEFAULT_TIMEOUT, "OK")) {
      if(enableDebug) Serial.println(F("SIM800L : doPost() - Invalid end of data while reading HTTP result from the module"));
      return 705;
    }
  
    if(enableDebug) {
      Serial.print(F("SIM800L : doPost() - Received from HTTP POST : "));
      Serial.println(recvBuffer);
    }
  }

  // Terminate HTTP/S session
  int termRC = terminateHTTP();
  if(termRC > 0) {
    return termRC;
  }

  return httpRC;
}

/**
 * Do HTTP/S GET on a specific URL
 */
int SIM800L::doGet(char* url, unsigned int serverReadTimeoutMs) {
  // Cleanup the receive buffer
  for(int i = 0; i < recvBufferSize; i++) {
    recvBuffer[i] = 0;
  }
  dataSize = 0;
  
  // Initiate HTTP/S session
  int initRC = initiateHTTP(url);
  if(initRC > 0) {
    return initRC;
  }

  // Start HTTP GET action
  sendCommand("AT+HTTPACTION=0");
  if(!readResponseCheckAnswer(DEFAULT_TIMEOUT, "OK")) {
    if(enableDebug) Serial.println(F("SIM800L : doGet() - Unable to initiate GET action"));
    return 703;
  }

  // Wait answer from the server
  if(!readResponse(serverReadTimeoutMs)) {
    if(enableDebug) Serial.println(F("SIM800L : doGet() - Server timeout"));
    return 408;
  }

  // Extract status information
  int idxBase = strIndex(internalBuffer, "+HTTPACTION: 0,");
  if(idxBase < 0) {
    if(enableDebug) Serial.println(F("SIM800L : doGet() - Invalid answer on HTTP GET"));
    return 703;
  }

  // Get the HTTP return code
  int httpRC = 0;
  httpRC += (internalBuffer[idxBase + 15] - '0') * 100;
  httpRC += (internalBuffer[idxBase + 16] - '0') * 10;
  httpRC += (internalBuffer[idxBase + 17] - '0') * 1;

  if(enableDebug) {
    Serial.print(F("SIM800L : doGet() - HTTP status "));
    Serial.println(httpRC);
  }

  if(httpRC == 200) {
    // Get the size of the data to receive
    dataSize = 0;
    for(int i = 0; (internalBuffer[idxBase + 19 + i] - '0') >= 0 && (internalBuffer[idxBase + 19 + i] - '0') <= 9; i++) {
      if(i != 0) {
        dataSize = dataSize * 10;
      }
      dataSize += (internalBuffer[idxBase + 19 + i] - '0');
    }
  
    if(enableDebug) {
      Serial.print(F("SIM800L : doGet() - Data size received of "));
      Serial.print(dataSize);
      Serial.println(F(" bytes"));
    }
  
    // Ask for reading and detect the start of the reading...
    sendCommand("AT+HTTPREAD");
    if(!readResponseCheckAnswer(DEFAULT_TIMEOUT, "+HTTPREAD: ", 2)) {
      return 705;
    }
  
    // Read number of bytes defined in the dataSize
    for(int i = 0; i < dataSize && i < recvBufferSize; i++) {
      while(!serial->available());
      if(serial->available()) {
        // Load the next char
        recvBuffer[i] = serial->read();
        // If the character is CR or LF, ignore it (it's probably part of the module communication schema)
        if((recvBuffer[i] == '\r') || (recvBuffer[i] == '\n')) {
          i--;
        }
      }
    }
  
    if(recvBufferSize < dataSize) {
      dataSize = recvBufferSize;
      if(enableDebug) {
        Serial.println(F("SIM800L : doGet() - Buffer overflow while loading data from HTTP. Keep only first bytes..."));
      }
    }
  
    // We are expecting a final OK
    if(!readResponseCheckAnswer(DEFAULT_TIMEOUT, "OK")) {
      if(enableDebug) Serial.println(F("SIM800L : doGet() - Invalid end of data while reading HTTP result from the module"));
      return 705;
    }
  
    if(enableDebug) {
      Serial.print(F("SIM800L : doGet() - Received from HTTP GET : "));
      Serial.println(recvBuffer);
    }
  }

  // Terminate HTTP/S session
  int termRC = terminateHTTP();
  if(termRC > 0) {
    return termRC;
  }

  return httpRC;
}

/**
 * Meta method to initiate the HTTP/S session on the module
 */
int SIM800L::initiateHTTP(char* url) {
  // Init HTTP connection
  sendCommand("AT+HTTPINIT");
  if(!readResponseCheckAnswer(DEFAULT_TIMEOUT, "OK")) {
    if(enableDebug) Serial.println(F("SIM800L : initiateHTTP() - Unable to init HTTP"));
    return 701;
  }
  
  // Use the GPRS bearer
  sendCommand("AT+HTTPPARA=\"CID\",1");
  if(!readResponseCheckAnswer(DEFAULT_TIMEOUT, "OK")) {
    if(enableDebug) Serial.println(F("SIM800L : initiateHTTP() - Unable to define bearer"));
    return 702;
  }

  // Define URL to look for
  sendCommand("AT+HTTPPARA=\"URL\",", url);
  if(!readResponseCheckAnswer(DEFAULT_TIMEOUT, "OK")) {
    if(enableDebug) Serial.println(F("SIM800L : initiateHTTP() - Unable to define the URL"));
    return 702;
  }

  // HTTP or HTTPS
  if(strIndex(url, "https://") == 0) {
    sendCommand("AT+HTTPSSL=1");
    if(!readResponseCheckAnswer(DEFAULT_TIMEOUT, "OK")) {
      if(enableDebug) Serial.println(F("SIM800L : initiateHTTP() - Unable to switch to HTTPS"));
      return 702;
    }
  } else {
    sendCommand("AT+HTTPSSL=0");
    if(!readResponseCheckAnswer(DEFAULT_TIMEOUT, "OK")) {
      if(enableDebug) Serial.println(F("SIM800L : initiateHTTP() - Unable to switch to HTTP"));
      return 702;
    }
  }

  return 0;
}

/**
 * Meta method to terminate the HTTP/S session on the module
 */
int SIM800L::terminateHTTP() {
  // Close HTTP connection
  sendCommand("AT+HTTPTERM");
  if(!readResponseCheckAnswer(DEFAULT_TIMEOUT, "OK")) {
    if(enableDebug) Serial.println(F("SIM800L : terminateHTTP() - Unable to close HTTP session"));
    return 706;
  }
  return 0;
}

/**
 * Force a reset of the module
 */
void SIM800L::reset() {
  if(enableDebug) Serial.println(F("SIM800L : Reset"));
  
  // Reset the device
  digitalWrite(pinReset, HIGH);
  delay(1000);
  digitalWrite(pinReset, LOW);
  delay(2000);
  digitalWrite(pinReset, HIGH);
  delay(5000);

  // Purge the serial
  serial->flush();
  while (serial->available()) {
    serial->read();
  }
  
}

/**
 * Return the size of data received after the last successful HTTP connection
 */
unsigned int SIM800L::getDataSizeReceived() {
  return dataSize;
}

/**
 * Return the buffer of data received after the last successful HTTP connection
 */
char* SIM800L::getDataReceived() {
  return recvBuffer;
}

/**
 * Status function: Check if AT command works
 */
bool SIM800L::isReady() {
  sendCommand("AT");
  return readResponseCheckAnswer(DEFAULT_TIMEOUT, "OK");
}

/**
 * Status function: Check the power mode
 */
PowerMode SIM800L::getPowerMode() {
  sendCommand("AT+CFUN?");
  if(readResponse(DEFAULT_TIMEOUT)) {
    // Check if there is an error
    int errIdx = strIndex(internalBuffer, "ERROR");
    if(errIdx > 0) {
      return POW_ERROR;
    }

    // Extract the value
    int idx = strIndex(internalBuffer, "+CFUN: ");
    char value = internalBuffer[idx + 7];

    // Prepare the clear output
    switch(value) {
      case '0' : return MINIMUM;
      case '1' : return NORMAL;
      case '4' : return SLEEP;
      default  : return POW_UNKNOWN;
    }
  }
  return POW_ERROR;
}

/**
 * Status function: Check if the module is registered on the network
 */
NetworkRegistration SIM800L::getRegistrationStatus() {
  sendCommand("AT+CREG?");
  if(readResponse(DEFAULT_TIMEOUT)) {
    // Check if there is an error
    int errIdx = strIndex(internalBuffer, "ERROR");
    if(errIdx > 0) {
      return NET_ERROR;
    }

    // Extract the value
    int idx = strIndex(internalBuffer, "+CREG: ");
    char value = internalBuffer[idx + 9];
  
    // Prepare the clear output
    switch(value) {
      case '0' : return NOT_REGISTERED;
      case '1' : return REGISTERED_HOME;
      case '2' : return SEARCHING;
      case '3' : return DENIED;
      case '5' : return REGISTERED_ROAMING;
      default  : return NET_UNKNOWN;
    }
  }
  
  return NET_ERROR;
}

/**
 * Setup the GPRS connectivity
 * As input, give the APN string of the operator
 */
bool SIM800L::setupGPRS(char* apn) {
  // Prepare the GPRS connection as the bearer
  sendCommand("AT+SAPBR=3,1,\"Contype\",\"GPRS\"");
  if(!readResponseCheckAnswer(20000, "OK")) {
    return false;
  }

  // Set the config of the bearer with the APN
  sendCommand("AT+SAPBR=3,1,\"APN\",", apn);
  return readResponseCheckAnswer(20000, "OK");
}

/**
 * Open the GPRS connectivity
 */
bool SIM800L::connectGPRS() {
  sendCommand("AT+SAPBR=1,1");
  // Timout is max 85 seconds according to SIM800 specifications
  return readResponseCheckAnswer(85000, "OK");
}

/**
 * Close the GPRS connectivity
 */
bool SIM800L::disconnectGPRS() {
  sendCommand("AT+SAPBR=0,1");
  // Timout is max 65 seconds according to SIM800 specifications
  return readResponseCheckAnswer(65000, "OK");
}

/**
 * Define the power mode
 * Available : MINIMUM, NORMAL, SLEEP
 * Return true is the mode is correctly switched
 */
bool SIM800L::setPowerMode(PowerMode powerMode) {
  // Check if the power mode requested is not ERROR or UNKNOWN
  if(powerMode == POW_ERROR || powerMode == POW_UNKNOWN) {
    return false;
  }
  
  // Check the current power mode
  PowerMode currentPowerMode = getPowerMode();

  // If the current power mode is undefined, abord
  if(currentPowerMode == POW_ERROR || currentPowerMode == POW_UNKNOWN) {
    return false;
  }

  // If the current power mode is the same that the requested power mode, say it's OK
  if(currentPowerMode == powerMode) {
    return true;
  }
  
  // If SLEEP or MINIMUM, only NORMAL is allowed
  if((currentPowerMode == SLEEP || currentPowerMode == MINIMUM) && (powerMode != NORMAL)) {
    return false;
  }

  // Send the command
  char value;
  switch(powerMode) {
    case MINIMUM : 
      sendCommand("AT+CFUN=0");
      break;
    case SLEEP :
      sendCommand("AT+CFUN=4");
      break;
    case NORMAL :
    default :
      sendCommand("AT+CFUN=1");
  }

  // Read but don't care about the result
  readToForget(10000);

  // Check the current power mode
  currentPowerMode = getPowerMode();
  
  // If the current power mode is the same that the requested power mode, say it's OK
  return currentPowerMode == powerMode;
}

/**
 * Status function: Check the strengh of the signal
 */
int SIM800L::getSignal() {
  sendCommand("AT+CSQ");
  if(readResponse(DEFAULT_TIMEOUT)) {
    int idxBase = strIndex(internalBuffer, "AT+CSQ");
    if(idxBase != 0) {
      return -1;
    }
    int idxEnd = strIndex(internalBuffer, ",", idxBase);
    int value = internalBuffer[idxEnd - 1] - '0';
    if(internalBuffer[idxEnd - 2] != ' ') {
      value += (internalBuffer[idxEnd - 2] - '0') * 10;
    }
    if(value > 31) {
      return -1;
    }
    return value;
  }
  return -1;
}

/*****************************************************************************************
 * HELPERS
 *****************************************************************************************/
/**
 * Find string "findStr" in another string "str"
 * Returns true if found, false elsewhere
 */
int SIM800L::strIndex(char* str, char* findStr, int startIdx) {
  int firstIndex = -1;
  int sizeMatch = 0;
  for(int i = startIdx; i < strlen(str); i++) {
    if(sizeMatch >= strlen(findStr)) {
      break;
    }
    if(str[i] == findStr[sizeMatch]) {
      if(firstIndex < 0) {
        firstIndex = i;
      }
      sizeMatch++;
    } else {
      firstIndex = -1;
      sizeMatch = 0;
    }
  }

  if(sizeMatch >= strlen(findStr)) {
    return firstIndex;
  } else {
    return -1;
  }
}

/**
 * Init internal buffer
 */
void SIM800L::initInternalBuffer() {
  for(int i = 0; i < internalBufferSize; i++) {
    internalBuffer[i] = '\0';
  }
}

/*****************************************************************************************
 * LOW LEVEL FUNCTIONS TO COMMUNICATE WITH THE SIM800L MODULE
 *****************************************************************************************/
/**
 * Send AT command to the module
 */
void SIM800L::sendCommand(char* command) {
  if(enableDebug) {
    Serial.print(F("SIM800L : Send \""));
    Serial.print(command);
    Serial.println(F("\""));
  }
  
  serial->listen();
  serial->flush();
  readToForget(500);
  serial->write(command);
  serial->write("\r\n");
  serial->flush();
}

/**
 * Send AT command to the module with a parameter
 */
void SIM800L::sendCommand(char* command, char* parameter) {
  if(enableDebug) {
    Serial.print(F("SIM800L : Send \""));
    Serial.print(command);
    Serial.print(F("\""));
    Serial.print(parameter);
    Serial.print(F("\""));
    Serial.println(F("\""));
  }
  
  serial->listen();
  serial->flush();
  readToForget(500);
  serial->write(command);
  serial->write("\"");
  serial->write(parameter);
  serial->write("\"");
  serial->write("\r\n");
  serial->flush();
}

/**
 * Read from module and forget the data
 */
void SIM800L::readToForget(unsigned int timeout) {
  int currentSizeResponse = 0;

  // Initialize internal buffer
  initInternalBuffer();

  unsigned long timerStart = millis();

  while (1) {
    // While there is data available on the buffer, read it until the max size of the response
    if(serial->available()) {
      // Load the next char
      internalBuffer[currentSizeResponse] = serial->read();
      currentSizeResponse++;

      // Avoid buffer overflow
      if(currentSizeResponse == internalBufferSize) {
        if(enableDebug) Serial.println(F("SIM800L : Received to forget maximum buffer size"));
        break;
      }
    }

    // If timeout, abord the reading
    if(millis() - timerStart > timeout) {
      if(enableDebug) Serial.println(F("SIM800L : Receive to forget timeout"));
      return;
    }
  }

  if(enableDebug) {
    Serial.print(F("SIM800L : Receive to forget \""));
    Serial.print(internalBuffer);
    Serial.println(F("\""));
  }
}

/**
 * Read from module and expect a specific answer (timeout in millisec)
 */
bool SIM800L::readResponseCheckAnswer(unsigned int timeout, char* expectedAnswer, unsigned int crlfToWait) {
  if(readResponse(timeout, crlfToWait)) {
    // Check if it's the expected answer
    int idx = strIndex(internalBuffer, expectedAnswer);
    if(idx > 0) {
      return true;
    }
  }
  return false;
}

/**
 * Read from the module for a specific number of CRLF
 * True if we have some data
 */
bool SIM800L::readResponse(unsigned int timeout, unsigned int crlfToWait) {
  int currentSizeResponse = 0;
  bool seenCR = false;
  int countCRLF = 0;

  // First of all, cleanup the buffer
  initInternalBuffer();
  
  unsigned long timerStart = millis();

  while(1) {
    // While there is data available on the buffer, read it until the max size of the response
    if(serial->available()) {
      // Load the next char
      internalBuffer[currentSizeResponse] = serial->read();

      // Detect end of transmission (CRLF)
      if(internalBuffer[currentSizeResponse] == '\r') {
        seenCR = true;
      } else if (internalBuffer[currentSizeResponse] == '\n' && seenCR) {
        countCRLF++;
        if(countCRLF == crlfToWait) {
          if(enableDebug) Serial.println(F("SIM800L : End of transmission"));
          break;
        }
      } else {
        seenCR = false;
      }

      // Prepare for next read
      currentSizeResponse++;

      // Avoid buffer overflow
      if(currentSizeResponse == internalBufferSize) {
        if(enableDebug) Serial.println(F("SIM800L : Received maximum buffer size"));
        break;
      }
    }

    // If timeout, abord the reading
    if(millis() - timerStart > timeout) {
      if(enableDebug) Serial.println(F("SIM800L : Receive timeout"));
      // Timeout, return false to parent function
      return false;
    }
  }

  if(enableDebug) {
    Serial.print(F("SIM800L : Receive \""));
    Serial.print(internalBuffer);
    Serial.println(F("\""));
  }

  // If we are here, it's OK ;-)
  return true;
}
