# DAC (New) Arduino Uno R4 Hardware Install Guide

## Hardware assembly guide
This part serves as how to properly assemble the necessary parts.

## Step 0.
Refer to https://github.com/ma-quantumlab/DAC-New-Arduino-U4-Based/tree/main/Hardware

It requires 3 different customized PCB boards to be manufactured. You need to pick the corresponding GerBer file (used for manufacturing) and BOM file (used to purchase necessary parts). You can use a variety of factories like JLCPCB or OSHPark, and Digikey or Mouser to source the necessary files.

Note: For PCB manufacturing, no specific processing technology is needed. Generic FR-6 with lead-free coating is considered enough. Components can be sourced elsewhere, as long as they provide the same value and packaging.

Once everything is correctly assembled, you can process to the next step.

## Step 1.
Make sure you have set up all necessary hardware. Check your soldering and wiring, make sure no short circuits, and check if the insulations are properly installed. All the necessary electronic apparatus is shown below.
![IMG_8964.jpg](3195786a-d5e2-46b6-a991-719db76c9e20.jpg)\
The top four from left to right are 5*2 parallel cable, AD5780 4CH eXtension Board, Arduino Ethernet Shield, Arduino Uno R4.\
The bottom, from left to right, is DAC eXtension Signal Board, DAC eXtension Power Board, AD5780 * 4, and cables needed for wiring.

![IMG_8966.jpg](0d9b9131-0a5b-4127-a7bf-a5b90ca727c2.jpg)\
The box assembly is shown in the above picture. It has 4 * BNC to SMB connectors used to connect to DAC, an Ethernet extension cord used to connect to Ethernet shield, and an aviation connector used to power-up the whole system.\
Note: For Ethernet shield, you may need to de-solder the old SPI headers first, then re-solder a new header with enough length of the pins.

**WARN: Check your aviation connector solder point, it has orientation and must match the following picture.**
![IMG20250630161218.jpg](eb67ccdd-0676-4c13-9ac9-88f9c2c1c051.jpg)
With the top pin points up and looking from the back, the LHS is VSS (blue, $-15V$), the MIDDLE is GND (black, $0V$), and the RHS is VDD (red, $+15V$).

## Step 2. 

Make sure the extension board and power board look like the following picture.
![IMG_8967.jpg](6eabc532-2712-4775-ba99-da26e93b4ab6.jpg)\
![IMG_8968.jpg](e6738f04-1787-4859-9c3b-5d6b6707a305.jpg)\
![IMG_8969.jpg](416a7c1f-5f58-4b12-bdef-0cb87b3456d5.jpg)\
The above picture shows how the signal board is assembled.

![IMG_8972.jpg](b948a456-ce19-42c1-96e6-6a9b6dfd926f.jpg)\
The foam tape is acting as an insulation layer, preventing the solder joint from shorting on the metallic box.

![IMG_8971.jpg](e6da7abf-4091-41b8-8044-080a37fcd8d3.jpg)\
![IMG_8970.jpg](02bae0ec-e7ac-4a08-a004-158366ed85d8.jpg)\
**WARN: Direction matters! You must check the orientation of the headers and the solder to make sure they match exactly as the picture shows. Otherwise your DAC may be permanently damaged.**

## Step 2.
Connect the power board and the output cord to the DAC.
![IMG_8978.jpg](a19b1f01-efd0-470c-a7e9-70c79f086b3c.jpg)\
![IMG_8979.jpg](b0f0ef52-45e9-4ddf-9f03-19146c23fdeb.jpg)



After that, wiring up all the necessary parts.\
![IMG_8980.jpg](59f3f344-43f0-43e8-a3f0-637ab47c8218.jpg)
Remember to check the cable polarity before wiring. Corresponding polarity is labeled on the PCB board. They must match.


```python

```
