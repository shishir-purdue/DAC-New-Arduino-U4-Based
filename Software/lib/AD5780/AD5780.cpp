/***************************************************************************//**
 *   @file   AD5780.c
 *   @brief  Implementation of AD5780 Driver.
 *   @author Dan Nechita
********************************************************************************
 * Copyright 2012(c) Analog Devices, Inc.
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *  - Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  - Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  - Neither the name of Analog Devices, Inc. nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *  - The use of this software may or may not infringe the patent rights
 *    of one or more patent holders.  This license does not release you
 *    from the requirement that you obtain separate licenses from these
 *    patent holders to use this software.
 *  - Use of the software either in source or binary form, must be run
 *    on or directly connected to an Analog Devices Inc. component.
 *
 * THIS SOFTWARE IS PROVIDED BY ANALOG DEVICES "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, NON-INFRINGEMENT,
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL ANALOG DEVICES BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, INTELLECTUAL PROPERTY RIGHTS, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
********************************************************************************
 *   SVN Revision: 781
*******************************************************************************/

/******************************************************************************/
/* Include Files                                                              */
/******************************************************************************/
#include "AD5780.h"			    // AD5780 definitions.
#include "Communication.h"		// Communication definitions.
#include <Arduino.h>

/**************************************************************************//**
 * @brief Initializes the communication with the device.
 * @param slaveDeviceId - The ID of the selected slave device.
 * @return 1.
*******************************************************************************/
unsigned char AD5780_Init(unsigned char slaveDeviceId, SPISettings* settings)
{  
    //Trigger hardware reset
    AD5780_SetRegisterValue(slaveDeviceId, AD5780_CMD_WR_SOFT_CTRL, AD5780_SOFT_CTRL_RESET, 3, settings);
    delayMicroseconds(1000);

    //Software init
    AD5780_SetRegisterValue(slaveDeviceId, AD5780_REG_CTRL, AD5780_CTRL_BIN2SC | AD5780_CTRL_OPGND | AD5780_CTRL_RBUF , 3, settings);
    AD5780_GetRegisterValue(slaveDeviceId, AD5780_REG_CTRL, 3, settings);
    AD5780_SetDacValue(slaveDeviceId, 0x1FFFF, settings);
    delayMicroseconds(10);
    AD5780_SetRegisterValue(slaveDeviceId, AD5780_REG_CTRL, AD5780_CTRL_BIN2SC | AD5780_CTRL_RBUF , 3, settings);
    AD5780_GetRegisterValue(slaveDeviceId, AD5780_REG_CTRL, 3, settings);
    AD5780_SetDacValue(slaveDeviceId, 0x1FFFF, settings);

    return(1);    
}

/***************************************************************************//**
 * @brief The part is placed in normal mode or its output is clamped to the 
 *        ground.
 * 
 * @param slaveDeviceId - The ID of the selected slave device.
 * @param state - Enables/disables the output.
 *                Example: 1 - Enables output.
 *                         0 - Disables output.
 *
 * @return none.
*******************************************************************************/
void AD5780_EnableOutput(unsigned char slaveDeviceId, unsigned char state, SPISettings* settings)
{
    unsigned long oldControl = 0;
    unsigned long newControl = 0;
    
    oldControl = AD5780_GetRegisterValue(slaveDeviceId, AD5780_REG_CTRL, 3, settings);
    oldControl = oldControl & ~(AD5780_CTRL_OPGND); // Clears OPGND bit.
    newControl = oldControl | AD5780_CTRL_OPGND * (!state);
    AD5780_SetRegisterValue(slaveDeviceId, AD5780_REG_CTRL, newControl, 3, settings);
}

/***************************************************************************//**
 * @brief Writes data into a register.
 *
 * @param slaveDeviceId - The ID of the selected slave device.
 * @param registerAddress - Address of the register.
 * @param registerValue - Value of the register.
 * @param bytesNumber - Number of bytes to be written.
 * @param settings - SPI configurations used in communication
 * 
 * @return none.
*******************************************************************************/
void AD5780_SetRegisterValue(
                             unsigned char slaveDeviceId,
                             unsigned char registerAddress,
                             unsigned long registerValue,
                             unsigned char bytesNumber,
                             SPISettings* settings)
{
    unsigned char writeCommand[4] = {0, 0, 0, 0};
    unsigned char nullCommand[4] = {0, 0, 0, 0};
    unsigned long spiWord         = 0;
    unsigned char* dataPointer    = (unsigned char*)&spiWord;
    unsigned char bytesNr         = bytesNumber;
    
    spiWord = AD5780_RW_(0) | 
              AD5780_ADDR_REG(registerAddress) |
              registerValue;
    writeCommand[0] = 0x01;
    while(bytesNr != 0)
    {
        writeCommand[bytesNr] = *dataPointer;
        dataPointer ++;
        bytesNr --;
    }    
    _SPI_Write(slaveDeviceId, writeCommand, bytesNumber, settings);
    delayMicroseconds(10);
    _SPI_Write(slaveDeviceId, nullCommand, bytesNumber, settings); //Dummy Byte
}

/***************************************************************************//**
 * @brief Reads the value of a register.
 * 
 * @param slaveDeviceId - The ID of the selected slave device.
 * @param registerAddress - Address of the register.
 * @param bytesNumber - Number of bytes that will be read.
 * @param settings - SPI configurations used in communication
 * 
 * @return dataRead - Value of the register.
*******************************************************************************/
unsigned long AD5780_GetRegisterValue(
                                        unsigned char slaveDeviceId,
                                        unsigned char registerAddress,
                                        unsigned char bytesNumber,
                                        SPISettings* settings)
{
    unsigned char registerWord[4] = {0, 0, 0, 0}; 
    unsigned long dataRead        = 0x0;
    unsigned char i               = 0;
    registerWord[0] = 0x01;
    registerWord[1] = (AD5780_RW_(1) | AD5780_ADDR_REG(registerAddress)) >> 16;
    _SPI_Write(slaveDeviceId, registerWord, 3, settings);  
    registerWord[0] = 0x01;
    registerWord[1] = 0x00;
    registerWord[2] = 0x00;
    registerWord[3] = 0x00;
    delayMicroseconds(10);
    _SPI_Read(slaveDeviceId, registerWord, bytesNumber, settings);

    // // Combine the bytes into a single 24-bit value (explicit handling)
    // dataRead = (registerWord[0] << 16) | (registerWord[1] << 8) | (registerWord[2]);
    
    // // Extract the middle 18 bits (bits [18:1])
    // dataRead = (dataRead >> 2) & 0x3FFFF;  // Shift by 2 to remove reserved bits and mask to get 18 bits

    for(i = 0;i < bytesNumber;i ++) 
    {
        dataRead = (dataRead << 8) + registerWord[i + 1];
    }
    return(dataRead);
}

/***************************************************************************//**
 * @brief Writes to the DAC register.
 *
 * @param slaveDeviceId - The ID of the selected slave device.
 * @param value - The value to be written to DAC.
 * @param settings - SPI configurations used in communication
 *
 * @return none.
*******************************************************************************/
void AD5780_SetDacValue(unsigned char slaveDeviceId, unsigned long value, SPISettings* settings)
{
    AD5780_SetRegisterValue(slaveDeviceId,
                            AD5780_REG_DAC, 
                            AD5780_DAC_DATA(value), 
                            3, 
                            settings);

    //Dummy Byte, although does not spesifically know why it is needed.
    // AD5780_SetRegisterValue(slaveDeviceId,
    //                         AD5780_NOP, 
    //                         AD5780_NOP, 
    //                         3, 
    //                         settings);
}

/***************************************************************************//**
 * @brief Asserts RESET, CLR and LDAC in a software manner.
 *
 * @param slaveDeviceId - The ID of the selected slave device.
 * @param instructionBit - A Software Control Register bit.
 *                         Example: AD5780_SOFT_CTRL_LDAC  - Load DAC
 *                                  AD5780_SOFT_CTRL_CLR   - Clear
 *                                  AD5780_SOFT_CTRL_RESET - Reset
 * @param settings - SPI configurations used in communication
 *
 * @return none.
*******************************************************************************/
void AD5780_SoftInstruction(unsigned char slaveDeviceId, unsigned char instructionBit, SPISettings* settings)
{
   
    AD5780_SetRegisterValue(slaveDeviceId, AD5780_CMD_WR_SOFT_CTRL, instructionBit, 3, settings);
    // Waits for the instruction to take effect.
    delayMicroseconds(10);
}

/***************************************************************************//**
 * @brief Writes to Control Register.
 *
 * @param slaveDeviceId - The ID of the selected slave device.
 * @param setupWord - Is a 24-bit value that sets or clears the Control Register
 *                    bits.
 *                    Example: AD5780_CTRL_BIN2SC | AD5780_CTRL_SDODIS - sets
 *                             the DAC register to use offset binary coding and 
 *                             disables SDO pin(tristated).
 * @param settings - SPI configurations used in communication  
 *
 * @return none.
*******************************************************************************/
void AD5780_Setup(unsigned char slaveDeviceId, unsigned long setupWord, SPISettings* settings)
{
    AD5780_SetRegisterValue(slaveDeviceId, AD5780_REG_CTRL, setupWord, 3, settings);
}

////////////////////////////////////////////////////////////////////////////////
//USER DEFINED FUNCTIONS
////////////////////////////////////////////////////////////////////////////////
/***************************************************************************//**
 * @brief Perform a ramp of the DAC value, incrementing or decrementing the 
 *        DAC output in steps over a defined time period.
 *
 * @param slaveDeviceId - The ID of the selected slave device.
 * @param finval - The final value tha will ramped to. The range is (0 ~ 262143), mapped to (-10V ~ 10V)
 * @param step_size - The step (voltage) for each unit of ramp function.
 * @param del_t - Delay time after each step is complete.
 * @param settings - SPI configurations used in communication  
 *
 * @return none.
*******************************************************************************/
void AD5780_ramp(unsigned char slaveDeviceId, unsigned long finval, unsigned long step_size, int del_t, SPISettings* settings) {
    unsigned long raw_value = AD5780_GetRegisterValue(slaveDeviceId, AD5780_REG_DAC, 3, settings);
    unsigned long current = (raw_value >> 2) & 0x3FFFF;

    Serial.print("current initial value:");
    Serial.println(current);

    if(current <= finval) {
        while (current < finval) {
            delayMicroseconds(del_t);

            if((current + step_size) > finval) {
                current = finval;
            } else {
                current += step_size;
            }

            Serial.print("current ramp value:");
            Serial.println(current);

            AD5780_SetDacValue(slaveDeviceId, current, settings);
        }
    } else {
        while (current > finval) {
            delayMicroseconds(del_t);

            if((current - step_size) < finval) {
                current = finval;
            } else {
                if (current < step_size) {
                    current = finval;
                } else {
                    current -= step_size;
                }
            }
            Serial.print("current ramp value:");
            Serial.println(current);

            AD5780_SetDacValue(slaveDeviceId, current, settings);
        }
    }
}