# DAC (New) Arduino Uno R4 Software Install Guide

## Open the configured project (recommended)

The repository now contains a root `platformio.ini` for the **Arduino Uno R4
Minima**. Open the **DAC Box repository root** in VS Code, install the recommended
PlatformIO IDE extension, and let its first-time setup finish. Reload the window
if prompted. There is no need to create a new project or copy source files.

The configuration uses the existing `Software/src` and `Software/lib` folders,
Renesas RA platform **1.9.0**, Arduino core **1.6.0**, compiler package
**1.70201.0**, DFU tool **1.11.0**, and a local recovery fork of Ethernet **2.0.2** in
`Software/lib/Ethernet`. PlatformIO installs these pinned packages automatically. Keep the
local Ethernet library: it supplies the physical-button cancellation/reset support. USB upload uses DFU, and
the serial monitor is set to **115200 baud**.

In the PlatformIO sidebar, select `uno_r4_minima` and use **Build**, **Upload**, or
**Monitor**. The equivalent commands in a PlatformIO Core terminal are:

```text
pio run -e uno_r4_minima
pio device list
pio run -e uno_r4_minima -t upload
pio device monitor -b 115200
```

Connect the Minima using a USB data cable before uploading. No COM port is pinned,
so PlatformIO can detect it even if Windows assigns a different port later.
Close any serial monitor that is holding the port before upload. If automatic
bootloader entry fails, double-press the board's reset button and retry Upload.

The current firmware's Ethernet settings are **192.168.1.200**, TCP port **23**,
and MAC **A8:61:0A:AE:F2:01** (`Software/src/main.cpp` and `PhysicalControls.h`). Check these against your
bench network before using the Ethernet connection. The USB connection is for
upload/debug logging; DAC commands are received over Ethernet.

The initial configuration built successfully on 2026-10-13. The 2026-10-14
physical-control firmware also builds successfully; build success does not validate
physical DAC outputs. See [Physical controls](../Physical%20controls.md) for A1/A2
button wiring, 0.01 V/s zeroing, and recovery behavior. The full repair version
uses `RAMP ACCEPTED` plus `STATUS` polling; update Python callers with the helper
in `Software/python/dac_client.py` when uploading that version.

The older manual project-creation walkthrough is preserved below for reference.

## Software assembly guide
This part serves as how to properly install the necessary software.

## Step 0.

To properly set up the software, you need to install **VS Code** first. After that, install **Platform IO**. The installation guide for Platform IO can be found here. 

https://platformio.org/platformio-ide

Once everything is set up, you should see the following welcome page.\
![image.png](4f4845b8-0291-4824-8983-4c1f6342171a.png)

Please proceed to the next step after you see this.

## Step 1.
After that, click **New Project** and set up as the following picture.\
![Screenshot 2025-07-03 005305.png](77ef4c20-cbde-47cf-a2a7-5260a9522a8a.png)\
Note: You can choose a different location if needed.
Note: The initial setup may take some time, as the system needs to download different packages and software. Please be patient.

After a while, you should automatically enter the main panel of the software, it should have a similar layout like the following picture.\
![Screenshot 2025-07-11 213658.png](17cc7fab-d76a-4bef-bf4b-c4f425e638bb.png)

Please proceed once you have achieved the same settings.

## Step 2
Click the "home" button at the bottom left corner.\
![Screenshot 2025-07-11 214239.png](6bc5a0d3-e41f-4bf0-af12-10ec857a843e.png)

Once you open the main page, go to **Libraries** on the left, then search for **Ethernet**, you should locate a library titled ***Ethernet** by Various*, with roughly 40k downloads, and it is written by official Arduino developers, indicated on the top right.\
![Screenshot 2025-07-11 214548.png](7260029f-11f2-4cc8-8ebf-29b0ba53b93b.png)

Click into it, click **Add to Project**, then select our project for DAC, then proceed.\
![Screenshot 2025-07-11 214842.png](4d47c434-81fa-49ba-98f2-9fb7d495b4fa.png)\
![Screenshot 2025-07-11 214858.png](ce4f5769-a990-407b-9519-6132242c0d93.png)

After that, you should be returned to the main page. Now, download the necessary programs from the GitHub repo or Dropbox, put them into the corresponding lib and src folder. Hit yes to replace existing files if needed.\
![Screenshot 2025-07-13 191209.png](b494b439-65ff-4cf3-9adf-233032bd0d4f.png)

## Step 3.
To properly setup, you should change the mac address and the corresponding static IP address for your dac board. Locate and change the following lines in the main.cpp
```cpp
byte mac[] = { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF }; // Change to your MAC address

Ethernet.begin(mac, IPAddress(192,168,1,123)); //Change to a valid static IP address.
```

If you have done everything mentioned above, you can click the compile button located at the bottom left to verify if everything works as expected.\
![Screenshot 2025-07-13 191536.png](e15da07d-9dc0-4e62-b533-b7ddf9b6f6a1.png)

After confirming the compile is successful, you can proceed to plug in the board and upload your program to the Arduino Uno R4.\
![Screenshot 2025-07-14 165823.png](0c3c7ca3-a395-4656-b9b4-ab60f8aa8b55.png)\
After successful uploading, you can disconnect the dev board and proceed to the next step.

Note: You need to install the Arduino Uno R4 driver, so your computer can recognize the Arduino hardware. A screenshot is taken from the *USBTreeView* software
![image.png](49446cca-ae72-43f1-8202-080e94199bec.png)

Detailed driver instructions can be found here.

https://docs.arduino.cc/tutorials/uno-r4-minima/minima-getting-started

https://www.arduino.cc/en/Guide/DriverInstallation
