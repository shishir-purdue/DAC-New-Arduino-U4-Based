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
    //Software Init
    AD5780_Setup(slaveDeviceId, (AD5780_CTRL_DACTRI | AD5780_CTRL_OPGND), settings); //TODO
    AD5780_SoftInstruction(slaveDeviceId, AD5780_SOFT_CTRL_CLR, settings);
    AD5780_SoftInstruction(slaveDeviceId, AD5780_SOFT_CTRL_LDAC, settings);
    AD5780_SoftInstruction(slaveDeviceId, AD5780_SOFT_CTRL_RESET, settings);
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
    _SPI_Read(slaveDeviceId, registerWord, bytesNumber, settings);
    for(i = 0;i < bytesNumber;i ++) 
    {
        dataRead = (dataRead << 8) + registerWord[i];
    }
    
    return(dataRead);
}

/***************************************************************************//**
 * @brief Writes to the DAC register.
 *
 * @param slaveDeviceId - The ID of the selected slave device.
 * @param value - The value to be written to DAC.
 *
 * @return none.
*******************************************************************************/
void AD5780_SetDacValue(unsigned char slaveDeviceId, unsigned long value, SPISettings* settings)
{
    //AD5780_LDAC_LOW;
    AD5780_SoftInstruction(slaveDeviceId, AD5780_SOFT_CTRL_LDAC, settings);
    AD5780_SetRegisterValue(slaveDeviceId,
                            AD5780_REG_DAC, 
                            AD5780_DAC_DATA(value), 
                            3, 
                            settings);
    AD5780_SoftInstruction(slaveDeviceId, AD5780_SOFT_CTRL_LDAC, settings);
    //AD5780_LDAC_HIGH;
}

/***************************************************************************//**
 * @brief Asserts RESET, CLR and LDAC in a software manner.
 *
 * @param slaveDeviceId - The ID of the selected slave device.
 * @param instructionBit - A Software Control Register bit.
 *                         Example: AD5780_SOFT_CTRL_LDAC  - Load DAC
 *                                  AD5780_SOFT_CTRL_CLR   - Clear
 *                                  AD5780_SOFT_CTRL_RESET - Reset
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
 * @param finval - Is a 24-bit value that sets or clears the Control Register
 *                    bits.
 *                    Example: AD5780_CTRL_BIN2SC | AD5780_CTRL_SDODIS - sets
 *                             the DAC register to use offset binary coding and 
 *                             disables SDO pin(tristated).     
 *
 * @return none.
*******************************************************************************/
void AD5780_ramp(unsigned char slaveDeviceId, long finval, long step_size, int del_t, SPISettings* settings) {
    // _dac_reg is supposed to be the current value of the DAC register
    // this could be the issue if _dac_reg is not being updated properly by other functions 
    // _dac_reg is currently updated by set_value() and read_DAC_register() (lines 86 & 130)
    long currval = AD5780_GetRegisterValue(slaveDeviceId, AD5780_REG_DAC, 3, settings);
    //long currval = (20/(262144-1))*gcurrval-10; //send global currentvalue to local currentvalue, convert from bit value to voltage value
    //long currval = gcurrval; //conversion of finval into bit value is done in python script
    long n_steps = abs(currval-finval)/abs(step_size);
    if (finval > currval) {
        step_size = abs(step_size);
    } else {
        step_size = -abs(step_size);
    }
    for(int j= 0; abs(currval-finval) > step_size; j++) {
        if (j > n_steps) break;
        unsigned long timer = millis();
        long newval = (currval + j*step_size)&0x3FFFF;
        // long newval = (currval + j*step_size);
        AD5780_SetDacValue(slaveDeviceId, newval, settings);
        while(millis() <= timer + del_t);
    }
    //last step to get to the actual value you want
    AD5780_SetDacValue(slaveDeviceId, finval, settings);
}