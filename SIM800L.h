/********************************************************************************
 * Arduino-SIM800L-driver                                                       *
 * ----------------------                                                       *
 * Arduino driver for GSM module SIM800L                                        *
 * Author: Olivier Staquet                                                      *
 * Last version available on https://github.com/ostaquet/Arduino-SIM800L-driver *
 *******************************************************************************/
#ifndef _SIM800L_H_
#define _SIM800L_H_

#include <Arduino.h>
#include <SoftwareSerial.h>

#define SIM800L_INTERNAL_BUFFER_SIZE 100

enum PowerMode {MINIMUM, NORMAL, POW_UNKNOWN, SLEEP, POW_ERROR};
enum NetworkRegistration {NOT_REGISTERED, REGISTERED_HOME, SEARCHING, DENIED, NET_UNKNOWN, REGISTERED_ROAMING, NET_ERROR};

class SIM800L {
  public:
    // Initialize the driver
    SIM800L(int _pinTx, int _pinRx, int _pinRst, unsigned int _recvBufferSize = 256, bool _enableDebug = false);
    ~SIM800L();

    // Force a reset of the module
    void reset();

    // Status functions
    bool isReady();
    int getSignal();
    PowerMode getPowerMode();
    NetworkRegistration getRegistrationStatus();

    // Define the power mode
    bool setPowerMode(PowerMode powerMode);

    // Enable/disable GPRS
    bool setupGPRS(char *apn);
    bool connectGPRS();
    bool disconnectGPRS();

    // Do HTTP methods
    int doGet(char* url, unsigned int serverReadTimeoutMs);
    int doPost(char* url, char* contentType, char* payload, unsigned int clientWriteTimeoutMs, unsigned int serverReadTimeoutMs);

    // Obtain results from HTTP connections
    unsigned int getDataSizeReceived();
    char* getDataReceived();

  protected:
    // Send command
    void sendCommand(char* command);
    // Send command with parameter within quotes (template : command"parameter")
    void sendCommand(char* command, char* parameter);
    // Read from module (timeout in millisec)
    bool readResponse(unsigned int timeout, unsigned int crlfToWait = 2);
    // Read from module and expect a specific answer (timeout in millisec)
    bool readResponseCheckAnswer(unsigned int timeout, char* expectedAnswer, unsigned int crlfToWait = 2);
    // Read from module but forget what is received
    void readToForget(unsigned int timeout);
    
    // Find string in another string
    int strIndex(char* str, char* findStr, int startIdx = 0);

    // Manage internal buffers
    void initInternalBuffer();

    // Initiate HTTP/S connection
    int initiateHTTP(char* url);
    int terminateHTTP();

  private:
    // Serial line with SIM800L
    SoftwareSerial* serial;
    unsigned int DEFAULT_TIMEOUT = 5000;

    // Details about the circuit: pins
    int pinReset = -1;

    // Internal memory for the shared buffer
    // Used for all reception of message from the module
    char *internalBuffer;
    char *recvBuffer;
    unsigned int recvBufferSize = 0;
    unsigned int dataSize = 0;

    // Enable debug mode
    bool enableDebug = false;
};

#endif // _SIM800L_H_
