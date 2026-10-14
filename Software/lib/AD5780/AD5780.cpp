#include "DiagnosticLog.h"
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
    const unsigned long rawControl = AD5780_GetRegisterValue(slaveDeviceId, AD5780_REG_CTRL, 3, settings);
    const unsigned long control = rawControl & 0xFFFFF;
    constexpr unsigned long normal = AD5780_CTRL_BIN2SC | AD5780_CTRL_RBUF;
    if ((rawControl & 0x700000) != AD5780_ADDR_REG(AD5780_REG_CTRL) ||
        (control & ~0x3EUL) || (control & AD5780_CTRL_SDODIS)) return 0;
    uint32_t existing;
    if (!AD5780_ReadDacValueChecked(slaveDeviceId, &existing, settings)) return 0;
    // Repeated INIT is a read-only verification of a normally operating channel.
    if (control == normal) return 1;
    // Changing an active/tristated configuration could reinterpret its DAC code.
    // Only a channel already clamped to ground may be initialized to zero.
    if (!(control & AD5780_CTRL_OPGND)) return 0;
    const unsigned long clamped = normal | AD5780_CTRL_OPGND;
    AD5780_SetRegisterValue(slaveDeviceId, AD5780_REG_CTRL, clamped, 3, settings);
    unsigned long checked = AD5780_GetRegisterValue(slaveDeviceId, AD5780_REG_CTRL, 3, settings);
    if ((checked & 0x7FFFFF) != (AD5780_ADDR_REG(AD5780_REG_CTRL) | clamped)) return 0;
    AD5780_SetDacValue(slaveDeviceId, 131072, settings); // Exact offset-binary zero, while clamped.
    uint32_t zero;
    if (!AD5780_ReadDacValueChecked(slaveDeviceId, &zero, settings) || zero != 131072) return 0;
    AD5780_SetRegisterValue(slaveDeviceId, AD5780_REG_CTRL, normal, 3, settings);
    checked = AD5780_GetRegisterValue(slaveDeviceId, AD5780_REG_CTRL, 3, settings);
    return (checked & 0x7FFFFF) == (AD5780_ADDR_REG(AD5780_REG_CTRL) | normal);
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
    unsigned long oldControl = AD5780_GetRegisterValue(slaveDeviceId, AD5780_REG_CTRL, 3, settings);
    oldControl = (oldControl & 0xFFFFF) & ~AD5780_CTRL_OPGND;
    unsigned long newControl = oldControl | (state ? 0 : AD5780_CTRL_OPGND);
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
    if (bytesNumber != 3) return;
    const uint32_t word = AD5780_ADDR_REG(registerAddress) | (registerValue & 0xFFFFF);
    unsigned char command[3] = {static_cast<unsigned char>(word >> 16),
                                static_cast<unsigned char>(word >> 8),
                                static_cast<unsigned char>(word)};
    unsigned char nop[3] = {};
    _SPI_Write(slaveDeviceId, command, 3, settings);
    delayMicroseconds(10);
    _SPI_Write(slaveDeviceId, nop, 3, settings);
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
    if (bytesNumber != 3) return 0;
    unsigned char command[3] = {
        static_cast<unsigned char>((AD5780_RW_(1) | AD5780_ADDR_REG(registerAddress)) >> 16), 0, 0};
    _SPI_Write(slaveDeviceId, command, 3, settings);
    unsigned char response[3] = {};
    delayMicroseconds(10);
    _SPI_Read(slaveDeviceId, response, 3, settings);
    return (static_cast<uint32_t>(response[0]) << 16) |
           (static_cast<uint32_t>(response[1]) << 8) | response[2];
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
bool AD5780_ReadDacValueChecked(unsigned char slaveDeviceId, uint32_t* value, SPISettings* settings)
{
    const unsigned long raw = AD5780_GetRegisterValue(slaveDeviceId, AD5780_REG_DAC, 3, settings);
    // AD5780 datasheet Table 9: DB22:20 must identify the DAC register (001).
    // R/W and DB1:0 are not voltage data. Do not reject a legitimate full scale.
    // A floating/stuck-high MISO word (0xFFFFFF) must not become +10 V.
    if ((raw & 0x700000UL) != AD5780_ADDR_REG(AD5780_REG_DAC)) {
        DacLog.print("ERR! DAC readback invalid, raw=");
        DacLog.println(raw);
        return false;
    }
    *value = (raw >> 2) & 0x3FFFF;
    return true;
}

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
bool AD5780_ramp(unsigned char slaveDeviceId, unsigned long finval, unsigned long step_size, int del_t, SPISettings* settings, RampAbortCheck abortCheck) {
    if (!step_size || step_size > 0x3FFFF || finval > 0x3FFFF || del_t < 0) return false;
    uint32_t initial;
    if (!AD5780_ReadDacValueChecked(slaveDeviceId, &initial, settings)) return false;
    unsigned long current = initial;

    DacLog.print("current initial value:");
    DacLog.println(current);

    if(current <= finval) {
        while (current < finval) {
            if (abortCheck && abortCheck()) {
                DacLog.println("RAMP aborted.");
                return false;
            }
            uint32_t remainingUs = static_cast<uint32_t>(del_t);
            while (remainingUs) {
                uint32_t slice = remainingUs > 1000 ? 1000 : remainingUs;
                delayMicroseconds(slice);
                remainingUs -= slice;
                if (abortCheck && abortCheck()) return false;
            }
            if (abortCheck && abortCheck()) return false;

            if((current + step_size) > finval) {
                current = finval;
            } else {
                current += step_size;
            }


            AD5780_SetDacValue(slaveDeviceId, current, settings);
        }
    } else {
        while (current > finval) {
            if (abortCheck && abortCheck()) {
                DacLog.println("RAMP aborted.");
                return false;
            }
            uint32_t remainingUs = static_cast<uint32_t>(del_t);
            while (remainingUs) {
                uint32_t slice = remainingUs > 1000 ? 1000 : remainingUs;
                delayMicroseconds(slice);
                remainingUs -= slice;
                if (abortCheck && abortCheck()) return false;
            }
            if (abortCheck && abortCheck()) return false;

            if((current - step_size) < finval) {
                current = finval;
            } else {
                if (current < step_size) {
                    current = finval;
                } else {
                    current -= step_size;
                }
            }

            AD5780_SetDacValue(slaveDeviceId, current, settings);
        }
    }
    return true;
}
