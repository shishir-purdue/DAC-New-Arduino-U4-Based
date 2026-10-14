/***************************************************************************//**
 *   @file   AD5780.h
 *   @brief  Header file of AD5780 Driver.
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
#ifndef __AD5780_H__
#define __AD5780_H__

/******************************************************************************/
/* Include Files                                                              */
/******************************************************************************/
#include <SPI.h>

/*****************************************************************************/
/* GPIO                                                                      */
/*****************************************************************************/
#define AD5780_LDAC_OUT        GPIO1_PIN_OUT
#define AD5780_LDAC_LOW        GPIO1_PIN_LOW
#define AD5780_LDAC_HIGH       GPIO1_PIN_HIGH
#define AD5780_CLR_OUT         GPIO2_PIN_OUT
#define AD5780_CLR_LOW         GPIO2_PIN_LOW
#define AD5780_CLR_HIGH	       GPIO2_PIN_HIGH
#define AD5780_RESET_OUT       GPIO3_PIN_OUT
#define AD5780_RESET_LOW       GPIO3_PIN_LOW
#define AD5780_RESET_HIGH      GPIO3_PIN_HIGH

/*****************************************************************************/
/* AD5780                                                                    */
/*****************************************************************************/
/* AD5780 Register Map */
#define AD5780_NOP                 0  // No operation (NOP).
#define AD5780_REG_DAC             1  // DAC register.
#define AD5780_REG_CTRL            2  // Control register.
#define AD5780_REG_CLR_CODE        3  // Clearcode register.
#define AD5780_CMD_WR_SOFT_CTRL    4  // Software control register.
/* Input Shift Register Bit Designations */
#define AD5780_RW_(x)              ((x) << 23)
#define AD5780_ADDR_REG(x)         ((x) << 20)
/* DAC Register Bit Designations */
#define AD5780_DAC_DATA(x)         ((x) << 2)
/* Clearcode Register Bit Designations */
#define AD5780_CLR_CODE_DATA(x)    ((x) << 2)
/* Control Register Bit Designations */
#define AD5780_CTRL_SDODIS   (1 << 5) // SDO pin enable/disable control.
#define AD5780_CTRL_BIN2SC   (1 << 4) // DAC register coding selection.
#define AD5780_CTRL_DACTRI   (1 << 3) // DAC tristate control.
#define AD5780_CTRL_OPGND    (1 << 2) // Output ground clamp control.
#define AD5780_CTRL_RBUF     (1 << 1) // Output amplifier configuration control.
/* Software Control Register Bit Designations */
#define AD5780_SOFT_CTRL_RESET      (1 << 2) // RESET function.
#define AD5780_SOFT_CTRL_CLR        (1 << 1) // CLR function.
#define AD5780_SOFT_CTRL_LDAC       (1 << 0) // LDAC function.

/******************************************************************************/
/* Functions Prototypes                                                       */
/******************************************************************************/

/* Initializes the communication with the device. */
unsigned char AD5780_Init(unsigned char slaveDeviceId, SPISettings* settings);

/* The part is placed in normal mode or its output is clamped to the ground. */
void AD5780_EnableOutput(unsigned char slaveDeviceId, unsigned char state, SPISettings* settings);

/* Writes data into a register. */
void AD5780_SetRegisterValue(
                             unsigned char slaveDeviceId,
                             unsigned char registerAddress,
                             unsigned long registerValue,
                             unsigned char bytesNumber,
                             SPISettings* settings);

/* Reads the value of a register. */
unsigned long AD5780_GetRegisterValue(
                                      unsigned char slaveDeviceId,
                                      unsigned char registerAddress,
                                      unsigned char bytesNumber,
                                      SPISettings* settings);

/* Validates the returned register address before decoding an output code.
 * Leaves *value unchanged on failure; performs no output writes. */
bool AD5780_ReadDacValueChecked(unsigned char slaveDeviceId, uint32_t* value, SPISettings* settings);

/* Writes to the DAC register. */
void AD5780_SetDacValue(unsigned char slaveDeviceId, unsigned long value, SPISettings* settings);

/* Asserts RESET, CLR and LDAC in a software manner. */
void AD5780_SoftInstruction(unsigned char slaveDeviceId, unsigned char instructionBit, SPISettings* settings);

/* Writes to Control Register. */
void AD5780_Setup(unsigned char slaveDeviceId, unsigned long setupWord, SPISettings* settings);

/* Abort callback for ramp; returns true if ramp should abort. */
typedef bool (*RampAbortCheck)();

/* Steps (ramp) to desired voltage. Returns true if completed, false if aborted. */
bool AD5780_ramp(unsigned char slaveDeviceId, unsigned long finval, unsigned long step_size, int del_t, SPISettings* settings, RampAbortCheck abortCheck);

#endif	// AD5780_H_
