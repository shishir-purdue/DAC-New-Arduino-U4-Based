#ifndef MAIN_H
#define MAIN_H

#include <SPI.h>
#include <Ethernet.h>
#include "AD5780.h"
#include "Communication.h"

// SPI PARAMETERS
#define SPI_35MHZ 35000000
extern SPISettings AD5780_spi_settings;

// Ethernet configuration variables
#define TELNET_PORT 23
EthernetServer server(TELNET_PORT);
byte mac[] = { 0x90, 0xA2, 0xDA, 0x10, 0xD0, 0xA8 };
// IPAddress ip(192, 168, 1, 50);
// IPAddress mydns(192, 168, 1, 1);
// IPAddress gateway(192, 168, 1, 1);
// IPAddress subnet(255, 255, 0, 0);

// Define user input global variable, probably can be localized.
String cmd_str = "";

// Define DAC_X SYNC_PINS
static int DAC[4] = {2, 3, 4, 5};

// Define handler entry macro
#define HANDLER_ENTRY(NAME, np) { #NAME, handle_ ## NAME, np }

// Forward declaration of handler functions
int handle_INIT(int argc, String argv[], EthernetClient client);
int handle_SET(int argc, String argv[], EthernetClient client);
int handle_READ(int argc, String argv[], EthernetClient client);
int handle_RAMP(int argc, String argv[], EthernetClient client);
int handle_ID(int argc, String argv[], EthernetClient client);
int handle_HELP(int argc, String argv[], EthernetClient client);
int handle_ERR(int argc, String argv[], EthernetClient client);
int handle_GETCURRVAL(int argc, String argv[], EthernetClient client);

// Function to process the command received
void process_command(String cmd, EthernetClient client);

// Help message function
void print_help_message(EthernetClient client);

// Handler entry structure and definition
typedef int (*handler_function)(int argc, String argv[], EthernetClient client);

typedef struct handler_entry {
    String handler_name;
    handler_function func;
    int num_params;
} handler_entry;

// Declare handlers array
handler_entry handlers[] = {
    HANDLER_ENTRY(INIT, 0), 
    HANDLER_ENTRY(SET, 2), 
    HANDLER_ENTRY(READ, 1), 
    HANDLER_ENTRY(RAMP, 4),
    HANDLER_ENTRY(ID, 0),
    HANDLER_ENTRY(HELP, 0),
    HANDLER_ENTRY(ERR, 0),
    HANDLER_ENTRY(GETCURRVAL, 0),
};

#endif  // MAIN_H
