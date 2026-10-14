#include "DiagnosticLog.h"
#include <SPI.h>
#include <Ethernet.h>
#include <Arduino.h>
#include "ad5780.h"
#include "Communication.h"
#include <string.h>

#define GLOBAL_PWR_EN 9
#define GLOBAL_CLR 8
#define GLOBAL_RST 7
#define VMAX 262143
#define VMIN 0
#define SPI_1MHZ 1000000
#define TELNET_PORT 23

// ================== Global Variables ================== //
byte mac[] = { 0xA8, 0x61, 0x0A, 0xAE, 0xF2, 0x01 }; // MAC address on the DAC box label
EthernetServer telnetServer(TELNET_PORT);
EthernetClient telnetClient;

static int DAC_sync[4] = {2, 3, 5, 6};
SPISettings AD5780_spi_settings;

static char inputLine[128]; // Static buffer for storing input line from client
int inputPos = 0;
bool clientConnected = false;
bool dacBusy = false; // Single-DAC lock: only one DAC operation at a time

const char HELP_MESSAGE[] =
"#Help:\r\n"
"Commands must be uppercase.\r\n"
"Commands:\r\n"
"  INIT [opt:DAC#]\r\n"
"  READ [DAC#]\r\n"
"  SET [DAC#] [BITCODE]\r\n"
"  RAMP [DAC#] [BITCODE] [STEP_SIZE] [STEP_TIME]\r\n"
"  HELP\r\n"
"  ID\r\n"
"All parameters must be integers.\r\n";

// ================== Forward Declarations ================== //
void processCommand(char* cmdLine);
bool isInteger(char* str);
void printHelp();
bool isAllUpperCase(const char* str);
bool isValidChannel(int ch);

// Abort callback for long-running ramp; checked periodically inside the ramp loop
bool rampAbortCheck();

// Handler functions for commands
void func_INIT(int param);
void func_READ(int param);
void func_SET(int p1, int p2);
void func_HELP();
void func_ID();
void func_RAMP(int p1, int p2, int p3, int p4);

#include "PhysicalControls.h"

// ================== Setup and Loop ================== //
void setup() {

  pinMode(NETWORK_RESET_BUTTON, INPUT_PULLUP);
  pinMode(SAFE_ZERO_BUTTON, INPUT_PULLUP);
  // Deselect both other SPI devices before starting the shared bus.
  pinMode(ETHERNET_CS, OUTPUT);
  digitalWrite(ETHERNET_CS, HIGH);
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH); // R4 pinMode(OUTPUT) clears the output latch.
  DacLog.begin(115200);
  DacLog.println("Beginning DacLog...");
  for (size_t i = 0; i < (sizeof(DAC_sync) / sizeof(*DAC_sync)); i++) {
      pinMode(DAC_sync[i], OUTPUT);
      digitalWrite(DAC_sync[i], HIGH);
  }
  DacLog.println("Beginning IO settings...");
  // Init configuration pins.
  pinMode(GLOBAL_PWR_EN, OUTPUT);
  pinMode(GLOBAL_CLR, OUTPUT);
  pinMode(GLOBAL_RST, OUTPUT);
  digitalWrite(GLOBAL_CLR, HIGH);
  digitalWrite(GLOBAL_RST, HIGH);
  digitalWrite(GLOBAL_PWR_EN, HIGH);

  DacLog.println("Beginning SPI bus...");
  SPI.begin();

  DacLog.println("Beginning AD5780 SPI settings...");
  if ((bool)AD5780_SPI_Init(false, SPI_1MHZ, false, true, &AD5780_spi_settings)) {  //1M for stability, SPI Mode 1 because sampling on Falling Edge
      if (AD5780_spi_settings.getBitOrder() == MSBFIRST && AD5780_spi_settings.getClockFreq() == SPI_1MHZ && AD5780_spi_settings.getDataMode() == SPI_MODE1) {
          DacLog.print("\nSPI init Success!\n");
      }
  } else {
      SPISettings AD5780_spi_settings;
      DacLog.print("\nSPI init fail!\n");
  }

  // Static network configuration; failure leaves the local buttons operational.
  restartNetwork();

}

// Connection housekeeping only: never dispatch commands or start DAC operations.
// Also called between ramp steps to enforce the same single-client policy.
void serviceClientConnections() {
  if (dacNetworkCancelled() || !networkEnabled) return;
  // Check if client is still connected
  if (clientConnected && (!telnetClient || !telnetClient.connected())) {
    DacLog.println("Client disconnected.");
    telnetClient.stop();
    telnetClient = EthernetClient();
    clientConnected = false;
    inputPos = 0;
    inputLine[0] = '\0';
    // Only the DAC handler may release its operation lock, including on abort.
  }

  // accept() transfers each new socket once, even before it sends data.
  // available() can return the active owner again while its command is buffered.
  EthernetClient newClient = telnetServer.accept();
  if (dacNetworkCancelled()) return;
  if (newClient) {
    if (!newClient.connected()) {
      newClient.stop();
    } else if (clientConnected || dacBusy) {
      // Reject: only one client allowed at a time
      DacLog.println("Second client rejected - server busy.");
      newClient.print("ERR! Server busy: only one connection allowed.\r\n");
      newClient.stop();
    } else {
      telnetClient = newClient;
      clientConnected = true;
      DacLog.println("Client connected.");
      telnetClient.print("Welcome to the Telnet server!\r\nType HELP for instructions.\r\n");
      inputPos = 0;
    }
  }
}

void loop() {
  if (servicePhysicalControls()) { serviceNetworkRecovery(); return; }
  serviceNetworkHealth();
  if (servicePhysicalControls()) { serviceNetworkRecovery(); return; }
  // Static IP: maintain() is a no-op, not a link recovery mechanism.
  Ethernet.maintain();
  serviceClientConnections();
  if (servicePhysicalControls()) return;

  // If client is connected, attempt to read incoming data
  if (clientConnected && telnetClient.available()) {
    char c = telnetClient.read();
    if (c == '\r' || c == '\n') {
      // User pressed enter, so we have a command to process
      if (inputPos > 0) {
        inputLine[inputPos] = '\0'; // Null-terminate the string
        processCommand(inputLine);
        inputPos = 0;
      }
    } else {
      // Accumulate characters into inputLine buffer
      if (inputPos < (int)(sizeof(inputLine) - 1)) {
        inputLine[inputPos++] = c;
      }
      // If the buffer is full and no newline, we could handle it by truncation or just ignore overflow.
    }
  }
}

// ================== Command Processing ================== //
void processCommand(char* cmdLine) {
  if (dacNetworkCancelled() || !networkEnabled) return;
  // Break the command line into tokens (command and arguments)
  char* tokens[10];
  int tokenCount = 0;
  char* p = strtok(cmdLine, " ");
  while (p != NULL && tokenCount < 10) {
    tokens[tokenCount++] = p;
    p = strtok(NULL, " ");
  }

  if (tokenCount == 0) {
    // Empty line or no tokens
    printHelp();
    return;
  }

  // Check if the command token is uppercase
  if (!isAllUpperCase(tokens[0])) {
    // If not uppercase, display help and return
    printHelp();
    return;
  }

  const char* command = tokens[0];

  // Debug print the received command and arguments
  DacLog.print("Received Command: ");
  DacLog.print(command);
  for (int i = 1; i < tokenCount; i++) {
    DacLog.print(" ");
    DacLog.print(tokens[i]);
  }
  DacLog.println();

  if (dacNetworkCancelled()) return;
  // Dispatch based on command
  if (strcmp(command, "INIT") == 0) {
    // INIT or INIT [number]
    if (tokenCount == 1) {
      // No parameter
      func_INIT(-1);
    } else if (tokenCount == 2 && isInteger(tokens[1])) {
      int param = atoi(tokens[1]);
      if (param == -1 || isValidChannel(param)) {
        func_INIT(param);
      } else {
        DacLog.println("ERR! Invalid DAC channel.");
        if (clientConnected && telnetClient.connected()) {
          telnetClient.print("ERR! Invalid DAC channel. Must be 1-4.\r\n");
        }
      }
    } else {
      printHelp();
    }
  } else if (strcmp(command, "READ") == 0) {
    // READ [number]
    if (tokenCount == 2 && isInteger(tokens[1])) {
      int param = atoi(tokens[1]);
      if (isValidChannel(param)) {
        func_READ(param);
      } else {
        DacLog.println("ERR! Invalid DAC channel.");
        if (clientConnected && telnetClient.connected()) {
          telnetClient.print("ERR! Invalid DAC channel. Must be 1-4.\r\n");
        }
      }
    } else {
      printHelp();
    }
  } else if (strcmp(command, "SET") == 0) {
    // SET [number] [number]
    if (tokenCount == 3 && isInteger(tokens[1]) && isInteger(tokens[2])) {
      int p1 = atoi(tokens[1]);
      int p2 = atoi(tokens[2]);
      if (isValidChannel(p1)) {
        func_SET(p1, p2);
      } else {
        DacLog.println("ERR! Invalid DAC channel.");
        if (clientConnected && telnetClient.connected()) {
          telnetClient.print("ERR! Invalid DAC channel. Must be 1-4.\r\n");
        }
      }
    } else {
      printHelp();
    }
  } else if (strcmp(command, "HELP") == 0) {
    // HELP
    func_HELP();
  } else if (strcmp(command, "RAMP") == 0) {
    // RAMP [number] [number] [number] [number]
    if (tokenCount == 5 && isInteger(tokens[1]) && isInteger(tokens[2]) &&
                          isInteger(tokens[3]) && isInteger(tokens[4])) {
      int p1 = atoi(tokens[1]);
      int p2 = atoi(tokens[2]);
      int p3 = atoi(tokens[3]);
      int p4 = atoi(tokens[4]);
      if (isValidChannel(p1)) {
        func_RAMP(p1, p2, p3, p4);
      } else {
        DacLog.println("ERR! Invalid DAC channel.");
        if (clientConnected && telnetClient.connected()) {
          telnetClient.print("ERR! Invalid DAC channel. Must be 1-4.\r\n");
        }
      }
    } else {
      printHelp();
    }
  } else if (strcmp(command, "ID") == 0) {
    // ID
    func_ID();
  } else {
    // Unknown command
    printHelp();
  }
}

// ================== Helper Functions ================== //

bool isInteger(char* str) {
  // Check if string represents an integer
  if (*str == '\0') return false;
  if (*str == '-' || *str == '+') str++;  // Allow signed values
  while (*str) {
    if (!isdigit((unsigned char)*str)) return false;
    str++;
  }
  return true;
}

void printHelp() {
  // Print help message to the client if connected
  if (clientConnected && telnetClient.connected()) {
    telnetClient.print(HELP_MESSAGE);
  }
  // Also print debug info to Serial
  DacLog.println("Displayed help message.");
}

bool isAllUpperCase(const char* str) {
  // Validate if the given string is entirely uppercase (A-Z)
  for (; *str; str++) {
    if (*str < 'A' || *str > 'Z') {
      return false;
    }
  }
  return true;
}

bool isValidChannel(int ch) {
  return (ch >= 1 && ch <= (int)(sizeof(DAC_sync) / sizeof(*DAC_sync)));
}

bool rampAbortCheck() {
  if (dacNetworkCancelled() || !networkEnabled) return true;
  serviceNetworkHealth();
  if (dacNetworkCancelled()) return true;
  Ethernet.maintain();
  serviceClientConnections();
  if (dacNetworkCancelled() || !clientConnected || !telnetClient || !telnetClient.connected()) {
    return true;
  }
  return false;
}

// ================== Command Handler Functions ================== //

void func_INIT(int param) {
  if (dacBusy) {
    DacLog.println("ERR! DAC busy, INIT rejected.");
    if (clientConnected && telnetClient.connected()) {
      telnetClient.print("ERR! DAC busy, command rejected.\r\n");
    }
    return;
  }
  dacBusy = true;

  // Print debug info on Serial
  DacLog.print("func_INIT called with param: ");
  DacLog.println(param);

  // Respond to client
  if (clientConnected && telnetClient.connected()) {
    if (param == -1) {
      for (unsigned char i = 0; i < (sizeof(DAC_sync) / sizeof(*DAC_sync)); i++) {
          AD5780_Init(DAC_sync[i], &AD5780_spi_settings);
      }
      telnetClient.print("Initialized all channels. ");      
      telnetClient.print("INIT command received with no parameter.\r\n");
    } else {
      AD5780_Init((unsigned char)DAC_sync[param - 1], &AD5780_spi_settings);
      telnetClient.print("Initialized pin ");
      telnetClient.print(DAC_sync[param - 1]);

      telnetClient.print(". INIT command received with parameter (Channel): ");
      telnetClient.print(param);
      telnetClient.print("\r\n");
    }
  }

  dacBusy = false;
}

void func_READ(int param) {
  if (dacBusy) {
    DacLog.println("ERR! DAC busy, READ rejected.");
    if (clientConnected && telnetClient.connected()) {
      telnetClient.print("ERR! DAC busy, command rejected.\r\n");
    }
    return;
  }
  dacBusy = true;

  // Print debug info on Serial
  DacLog.print("func_READ called with param: ");
  DacLog.print(param);

  uint32_t current_dac_values;
  if (!AD5780_ReadDacValueChecked((unsigned char)DAC_sync[param - 1],
                                 &current_dac_values, &AD5780_spi_settings)) {
    if (clientConnected && telnetClient.connected())
      telnetClient.print("ERR! DAC readback invalid; voltage unknown.\r\n");
    dacBusy = false;
    return;
  }
  
  DacLog.print(", DAC read value is: ");
  DacLog.println(current_dac_values);

  // Respond to client
  if (clientConnected && telnetClient.connected()) {
    // telnetClient.print("READ command received with parameter: ");
    // telnetClient.print(param);
    // telnetClient.print(", DAC read value is: ");
    telnetClient.print(current_dac_values);
    telnetClient.print("\r\n");
  }

  dacBusy = false;
}

void func_SET(int p1, int p2) {
  if (dacBusy) {
    DacLog.println("ERR! DAC busy, SET rejected.");
    if (clientConnected && telnetClient.connected()) {
      telnetClient.print("ERR! DAC busy, command rejected.\r\n");
    }
    return;
  }
  dacBusy = true;

  // Called when SET command is received
  // Print debug info on Serial
  DacLog.print("func_SET called with params: ");
  DacLog.print(p1);
  DacLog.print(", ");
  DacLog.println(p2);

  if(p2 > VMAX || p2 < VMIN) {
    DacLog.println("WARN! Value out of bound! Auto corrected to Maximum / Minimum bound!");
    if(p2 > VMAX) {
      p2 = VMAX;
    } else {
      p2 = VMIN;
    }
  }

  AD5780_SetDacValue((unsigned char)DAC_sync[p1 - 1], p2, &AD5780_spi_settings);

  // Respond to client
  if (clientConnected && telnetClient.connected()) {
    telnetClient.print("SET command received with parameters: ");
    telnetClient.print(p1);
    telnetClient.print(" ");
    telnetClient.print(p2);
    telnetClient.print("\r\n");
  }

  dacBusy = false;
}

void func_HELP() {
  // Called when HELP command is received
  // Print debug info on Serial
  DacLog.println("func_HELP called.");
  printHelp();
}

void func_RAMP(int p1, int p2, int p3, int p4) {
  if (dacBusy) {
    DacLog.println("ERR! DAC busy, RAMP rejected.");
    if (clientConnected && telnetClient.connected()) {
      telnetClient.print("ERR! DAC busy, command rejected.\r\n");
    }
    return;
  }
  dacBusy = true;

  // Called when RAMP command is received
  // Print debug info on Serial
  if(p2 > VMAX || p2 < VMIN) {
    DacLog.println("WARN! Value out of bound! Auto corrected to Maximum / Minimum bound!");
    if(p2 > VMAX) {
      p2 = VMAX;
    } else {
      p2 = VMIN;
    }
  }

  if(p3 != 0) {
    bool completed = AD5780_ramp((unsigned char)DAC_sync[p1 - 1], p2, p3, p4, &AD5780_spi_settings, rampAbortCheck);

    if (completed) {
      DacLog.print("func_RAMP completed with params: ");
      DacLog.print(p1); DacLog.print(", ");
      DacLog.print(p2); DacLog.print(", ");
      DacLog.print(p3); DacLog.print(", ");
      DacLog.println(p4);
      if (clientConnected && telnetClient.connected()) {
        telnetClient.print("RAMP command received with parameters: ");
        telnetClient.print(p1); telnetClient.print(" ");
        telnetClient.print(p2); telnetClient.print(" ");
        telnetClient.print(p3); telnetClient.print(" ");
        telnetClient.print(p4);
        telnetClient.print("\r\n");
      }
    } else {
      DacLog.println("ERR! RAMP failed: readback or interrupted operation.");
      if (clientConnected && telnetClient.connected())
        telnetClient.print("ERR! RAMP failed: invalid readback or interrupted operation.\r\n");
    }
  } else {
    DacLog.println("ERR! step_size = 0, RAMP not executed.");

    //Error respond
    if (clientConnected && telnetClient.connected()) {
      telnetClient.print("ERR! step_size = 0, RAMP not executed.");
      telnetClient.print("\r\n");
    }
  }

  dacBusy = false;
}

void func_ID() {
  //No actual use, used for pure compatibility.
  DacLog.println("func_ID called.");

  if (clientConnected && telnetClient.connected()) {
    telnetClient.print("AD5780");
    telnetClient.print("\r\n");
  }
}
