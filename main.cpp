#include "main.h"
#include "ad5780.h"
#include "Communication.h"

SPISettings AD5780_spi_settings(SPI_CLOCK_DIV128, MSBFIRST, SPI_MODE0);  // Default initialization

/* Main logics begin*/
void setup() {
    for (size_t i = 0; i < (sizeof(DAC) / sizeof(*DAC)); i++) {
        pinMode(DAC[i], OUTPUT);
    }

    if ((bool)AD5780_SPI_Init(false, SPI_35MHZ, false, true, &AD5780_spi_settings)) {
        Serial.println("SPI init success!");
    } else {
        Serial.println("SPI init fail!");
    }

    Serial.begin(115200);
    Serial.println("Setting up server");

    Ethernet.begin(mac);
    server.begin();

    Serial.print("Server is at: ");
    Serial.println(Ethernet.localIP());
}

void loop() {
    EthernetClient client = server.available();
    boolean alreadyConnected = false;

    if (client) {
        if (!alreadyConnected) {
            Serial.print("Client connected on socket ");
            Serial.println(client);
            alreadyConnected = true;
        }
        
        if (client.available() > 0) {
            char received = client.read();
            cmd_str += received;
            
            if (received == '\n') {
                Serial.print(cmd_str);
                process_command(cmd_str, client);
                cmd_str = "";
            }
        }
    }
}
/* Main logics end*/

void process_command(String cmd, EthernetClient client) {
    String arguments_value[10];
    int arguments_count = 0;
    int p1 = 0, p2 = 0;
    int num_handlers = sizeof(handlers) / sizeof(handler_entry);

    while ((arguments_count < 10) && (cmd[p1] != '\n')) {
        while ((cmd[p2] != ' ') && (cmd[p2] != '\n') && (cmd[p2] != '\r')) {
            p2++;
        }
        
        arguments_value[arguments_count] = cmd.substring(p1, p2);
        arguments_count++;

        if (cmd[p2] == '\n') {
            break;
        }

        p2++;
        p1 = p2;
    }

    for (int i = 0; i < num_handlers; i++) {
        if (arguments_value[0] == handlers[i].handler_name) {
            if (arguments_count > handlers[i].num_params) {
                handlers[i].func(arguments_count, arguments_value, client);
            } else {
                handle_ERR(arguments_count, arguments_value, client);
            }
            return;
        }
    }
    handle_ERR(arguments_count, arguments_value, client);
}

int handle_INIT(int arguments_count, String arguments_value[], EthernetClient client) {
    if (arguments_count == 1) {
        for (size_t i = 0; i < (sizeof(DAC) / sizeof(*DAC)); i++) {
            AD5780_Init(DAC[i], &AD5780_spi_settings);
        }
        client.println("Initialized all channels");
    } else if (arguments_count == 2) {
        static int DAC_sync_num = arguments_value[1].toInt() - 1;
        AD5780_Init((unsigned char)DAC[DAC_sync_num], &AD5780_spi_settings);
        client.print("Initialized channel ");
        client.println(DAC_sync_num);
    } else {
        print_help_message(client);
    }
    return 0;
}

int handle_GETCURRVAL(int arguments_count, String arguments_value[], EthernetClient client) {
    return 0; 
}

int handle_SET(int arguments_count, String arguments_value[], EthernetClient client) {
    int dac_num = arguments_value[1].toInt() - 1;
    long dac_val = arguments_value[2].toInt();
    AD5780_SetDacValue(dac_num, dac_val, &AD5780_spi_settings); //todo

    client.println("SET");
    return 0;
}

int handle_RAMP(int arguments_count, String arguments_value[], EthernetClient client) {
    int dac_num = arguments_value[1].toInt() - 1;
    long dac_val = arguments_value[2].toInt();//dec value of -10 to 10 range of register
    long step_size = arguments_value[3].toInt();
    int step_time = arguments_value[4].toInt();
    // long _dac_reg = dacs[dac_num - 1].read_DAC_register();
    AD5780_ramp(dac_num, dac_val, step_size, step_time, &AD5780_spi_settings);
    //instead of reading the register at the end, just convert int to long bitcode string appropriately
    
    //client.println(BITCODE_TO_DAC(rc));
    client.println("RAMP");
    return 0;
}

int handle_READ(int arguments_count, String arguments_value[], EthernetClient client) {
    //client.println("READ");
    int dac_num = arguments_value[1].toInt() - 1;
    //client.println(rc);
    unsigned long current_dac_values = AD5780_GetRegisterValue(dac_num, AD5780_REG_DAC, 3, &AD5780_spi_settings);
    client.println(current_dac_values);
    return 0;
}

int handle_ID(int arguments_count, String arguments_value[], EthernetClient client) {
    client.println("AD5780");
    return 0;
}

int handle_HELP(int arguments_count, String arguments_value[], EthernetClient client) {
    print_help_message(client);
    return 0;
}

int handle_ERR(int arguments_count, String arguments_value[], EthernetClient client) {
    client.println("Invalid command \n");
    print_help_message(client);
    return 0;
}

void print_help_message(EthernetClient client) {
    client.println("Usage: [COMMAND] [PARAM1] [PARAM2] ...");
    client.println("Set voltage: SET [DAC#] [BITCODE]");
    client.println("Ramp voltage: RAMP [DAC#] [BITCODE] [STEP_SIZE] [STEP_TIME]");
    client.println("Read voltage: READ [DAC#]");
    client.println("Initialize dacs: INIT [opt:DAC#]");
    client.println("Help: HELP");
    client.println("1 bit = 38 microVolts");
    client.println("1 milliVolt = 26.2 bits");
}
