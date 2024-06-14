# grbl_controller_esp32 modded code By Docitchy

This code made by mstrens are ported for ESP32-3224s028r Dual usb port only !

(Original code is here https://github.com/mstrens/grbl_controller_esp32)

This project allows to control a CNC running GRBL without having to use a pc.<br>

This is an alternative to Marlin or Repetier for CNC.

Note: GRBL has to run on a separate micro computer (e.g. an Arduino Uno, Mega or a STM32 blue pill).<br>
This project only intends to replace the PC by an ESP32 board, not to replace GRBL.

This application allows to:
- select a file on the SD card (with Gcode) and send it to GRBL for execution
- pause/resume/cancel sending the Gcode
- send predefined set of GRBL commands (= macros)
- unlock grbl alarm
- ask grbl for homing the CNC
- move X,Y, Z axis by 0.01, 0.1, 1, 10 mm steps
- set X, Y, Z Work position to 0 (based on the current position)
- forward GRBL commands from the PC (so you can still control your CNC using your pc with e.g. Universal Gcode sender).<br>
   This is e.g. useful to configure GRBL from the PC with GRBL "$" commands.<br>
   The Gcode/commands can be sent using or <br>
   - the USB (serial) interface that exist on the ESP32 developement board.<br>
   - a telnet protocol over Wifi (in this case, best is to use BCNC software on pc side because it support telnet connection)
- let a browser session running on pc connect to the ESP32 over Wifi in order to upload/download files between the pc and the SD card.
   So you can avoid physical manipulation of SD card.
   This works currently only with the root directory on the SD card (not with subdirectory) 

This application displays some useful GRBL informations like
- the source of the Gcode being sent to GRBL (e.g. SD card, USB, Telnet)
- the GRBL status (Idle, Run, Alarm,...)
- the work position (Wpos) and the machine position (Mpos)
- the last error and alert message.

!
not working , i2c nunchuck pin 21 are used by lcd backlight pwm command ! fonction désactived for prevent uncontrolled action !
Optionnally you can connect a Nunchuk (kind of joystick) in order to move the X, Y, Z, axis.<br>
To move the axis, you have to move the joystick (up/down/left/right) and simultaneously 
- press the C button to move X/Y axis,
- press the Z button to move Z axis
Nunchuk is automatically disconnected when the source of Gcode USB or Telnet.

!

## Hardware

To implement this project you need:
- an ESP32-3224s028r tft board
- modding board required for serial usage (not for BT or wifi )
- remove RGB led in back of pcb
- in the 6 pad present , ignore the 2 central pad (or soldering simple led in place if you have )
- the other 4 pad , left sold jump the upper and lower pad , and same to the right
- the pad left are io16 pin and pulled up = RX pin to Grbl
- the pad right are io17 pin and pulled up = TX pin to Grbl
- Important NOTE
- TX and RX pin are 3.3V pin , do NOT use TTL 5V tx rx directly , please use spécific vcc rs232 converter before connecting to arduino or other controller rs232 !!
- if your connect directly , esp32 burn't quickly !
- for exemple , use this product for safe TX and RX usage https://fr.aliexpress.com/item/1005003731129946.html?spm=a2g0o.order_list.order_list_main.470.60335e5b9tsz3S&gatewayAdapt=glo2fra

## Software

This project compiles in Arduino IDE but it requires:
- to add in Arduino IDE the software that support ESP32 dev Module. The process is explained e.g. in this link
	https://randomnerdtutorials.com/installing-the-esp32-board-in-arduino-ide-windows-instructions/
- to edit the file config.h from this project in order to specify if you plan to use the WiFi and if so if you are using the ESP32 in station mode or in access point mode.
   When using the WiFi, you must also specify the SSID (= name of the access point) and the password (in access point mode, it must contain at least 8 char)<br>
Note: the WiFi parameters can also be changed using the SD card. So you do not have to recompile and reflash the firmware.
To change the WiFi parameter with the SD card, you have to put a file named "wifi.cfg" (respect the case) on the root of SD card.
The file must contain 3 lines with:<br>
WIFI="ESP32_ACT_AS_STATION"<br>
PASSWORD="your password"<br>
SSID="your access point"<br>
You can only change: "ESP32_ACT_AS_STATION" (change by "NO_WIFI" or "ESP32_ACT_AS_AP") , "your password" and "your access point"<br>
Once uploaded from config.h or from file wifi.cfg, the parameters are saved inside the ESP32 and are reused until new parameters are uploaded via SD card.<br>
So you can remove the "wifi.cfg" file from SD card once the program runs once.

   
Take care that if you do not use my configuration, you can have:<br>
-  to edit the file User_setup.h included in the TFT_eSPI_ms folder.<br>
   This file specify the type of display controller and the pins being used by the SPI, the display and the touch screen chip select.
-  to edit the file config.h from this project in order to specify some pins being used (e.g. the chip select for the SD card reader)
  
Note: the pins used for the SPI signals (MOSI, MISO, SCLK) are currently hardcoded.<br>
Please note that many ESP32 pins are reserved and can't ne used (reserved for bootup, for internal flash, input only,...).
See doc on ESP for more details.

Note: the first time you let ESP32 run this program, the program should execute a calibration of the touch panel.<br>
The screen should first display an arrow in a corner. You have to click on the corner.<br>
When done, an arrow should also be displayed sucessively in the 3 others corners. Click each time on the arrow.<br>
This should normally be done only once. The calibration data are stored automatically in the ESP32.<br>   
If you want to recalibrate your screen, you should replace, in the "config.h" file, "#define REPEAT_CAL false" by "#define REPEAT_CAL true".<br>
Change it back to "#define REPEAT_CAL false" afterwards.<br>
Alternatively you can force a recalibration from the SD card. To do so, you have to put a file named "calibrate.txt" in the root of the SD card.
The content of the file does not matter (only the file name).
Remove the file from SD card once calibration is done in order to avoid to do it at each power on.

#### Between ESP32 and GRBL computer
- Gpio16 is the Serial port Rx from ESP32; It has to be connected to TX pin from GRBL
- Gpio17 is the Serial port Tx from ESP32; It has to be connected to RX pin from GRBL
- Warning this is 3.3V pin only , please use ttl converter before connecting 
Note : Grnd has to be common between ESP32 and GRBL computer.


#### Between ESP32 and Nunchuk
- 3.3V => Vcc
- Grnd => Grnd
- Gpio21 => SDA Warning the pin21 are used for tft backlight try to replace to io 27 for connecting to cn connector ! 
- Gpio22 => SCL 


## Control of GRBL from the PC

In order to control GRBL from a PC, you can use the software bCNC on the PC.\
It allows you to connect ESP32 using a com port (USB) or using wifi.\
To install bCNC, follow the instructions from this site: https://github.com/vlachoudis/bCNC/wiki/Installation \
For Windows, do not use the exe file (at march 2019 it did not work) but start intalling python2.7 and then install bCNC using pip.\
At march 2019, this does not install the latest version of pyserial software. \
So, you have to enter also (in the command windows): pip install --upgrade pyserial \
Then, you can launch bCNC with the command: python.exe -m bCNC.\
Note: if you already have a version 3 of python running on your PC, it could be that you have to type: python2.exe -m bCNC.\

In bCNC, select the "File" tab, and in the Serial part, in field Port, enter:\
- for a USB connection : comX    (where X is the com port assigned to ESP32 e.g. with Arduino)
- for a Wifi (telnet) connection : socket://192.168.1.5:23 (where 192.168.1.5 has to be replaced by the IP adress assign to your ESP32)

In bCNC, in the serial part, in field Controller, enter the type of GRBL you are using, so probably it will be GRBL1.\

Then click on the "Open" button in bCNC.\
And finally, on ESP32, select the Print button and then "USB" or "Telnet".\
Once USB or Telnet is activated, you can't anymore use the ESP32 options (Setup, ...).\
The only thing you can do on ESP32 side is to select the button "Stop PC" which will stop the flow of data exchanged with the PC.\


## Control of SD card content from the PC

In order to manage the files on the SD card from the PC, you have to:
- open a browser session (chrome, ...)
- fill in the URL of your ESP32 e.g. //192.168.1.5 (the value is displayed on the setup screen on ESP32)
- then click on the buttons Download, Upload , Delete, Directory
Note: you can only interact with the root of the SD card (not the sub directories).
 

## Informations on the TFT layout and functions
### Info screen:
This is the first and main screen of the application.
It displays:
- line 1 left: a status saying if you are printing from SD card (and % of printing), in Pause or if GRBL is controlled via PC+USB or via PC+wifi
- line 1 right: the GRBL status (Idle, Run, Alarm , Hold, Jog, ...) or "??" when GRBL is not responding (e.g. GRBL in alarm:1) 
- line 2: the last message (error or alarm) sent by GRBL. This message is automatically cleared when you go back from Setup screen to Info screen.
- line 3: a wifi icon; it is red when there are no PC connected with telnet, green when a pc is connected with telnet
- next lines: the X, Y, Z positions in Work coordinate and in Machine coordinate
- last line: the current feed rate and spindle rpm.
- there are also 2 buttons that depends on the current state (e.g. Setup, Print, Pause, Resume, Cancel)
### Setup screen
It displays:
- The IP address to be used if you want to
  - upload/download/delete/list files from the root directory on the SD. This IP has to be entered in a PC browsing session
  - use a telnet (=wifi) connection to control GRBL
  - The GRBL status
- The last message from GRBL
- some buttons to request from GRBL a Homing, an unlock or a soft reset
- some buttons to access other screens
### Print screen
It displays some buttons to select between
- printing a file from the SD card; you will then get a screen with a list of files/directories on the SD card
- printing/controlling GRBL from the PC using the USB connection.
- printing/controlling GRBL from the PC using a telnet connection.
- direct links to other screens (Setup, Cmd, Info)
Note: with USB and Telnet, the ESP32 works in "passthrough". It means that it works just like the PC was directly connected to GRBL.
So, Once USB or Telnet is activated, you can't anymore use the ESP32 options (Setup, ...).\
The only thing you can do on ESP32 side is to view the current position and status and to select the button "Stop PC" which will stop the flow of data exchanged with the PC.\
### File list screen
It allows to select a file in a directory from the Sd card
- line 1 left: index of the first file name being displayed followed by "/" and the total number of files in the current directory
- line 2 right: name of the current directory (/ for the root)
- up to 4 buttons with file/directory names; text is blue on white background for a directory and the opposite for a file.
 Pressing one of those buttons goes into the directory (one level down) or start printing the file (= sending the Gcode to GRBL).
 Still printing does not start immediately because the controller is first set in Pause mode in order to let you eventually cancel the job.
 So, in order to really start printing you have to confirm pressing the Resume button on Info screen
- button UP to move one level up in the directory hierarchy
- button <- and -> to scroll in the list

### Move screen
It allows to move the motors in the X, Y and Z axis.
- It displays the current work positions and the distances moved since this screen has been entered.
- there are 2 kinds of moves
  - when the button "Auto" is displayed, it means that the distances to move depends on the duration of pressing a X,Y,R button. When you keep pressing, the speed increases.  When you releases, the motor stops)
  - If you press the "Auto" button, the button will be renamed "0.01". It means that each press on X, Y,Z will move by 0.01 mm (or inch depending on the GRBL setting). Button X,Y,Z has to be released and pressed again to move a new step.
   Pressing 0.01 button changes it to 0.1. Pressing again, it becomes 1, 10 and finally back to Auto.
### Set XYZ screen
It allows to set the work coordinates X, Y, Z (or all 3) to zero at the current machine position.
### Cmd screen
It allows you to execute some set of predefined Gcode commands. It is a kind of macros that you can define yourself (e.g. for probing, changing coordinates system, ...)
- you can define up to 7 commands (the name and the Gcode commands)
- to define/update a Cmd button, you have to fill in a file on your pc with one set of Gcode commands and save it under a name like Cmd5_name.xxx where
  - Cmd and _ are fixed
  - 5 is replaced by the digit of the Cmd (must be between 1 and 7)
  - name is replaced by the name given to the button (must be less than 16 char and begin with a letter a...z or A...Z)
  - xxx is the file extension and is discarded
- this file has to be copied/uploaded on the SD card
- you have to select this file with the Print from SD Card option.
 The file (due to the structure of the name) will not be printed but the Cmd button will be created and the Gcode commands will be saved in the ESP32.
- So, later on, you can use the Cmd buttons even if the SD card is not present.
Notes:
- A button being added will be displayed only after the next reset of ESP32.
- "Printing" a file having the same Cmd digit as an existing Cmd button will replace as well the name as the Gcode of the button 
- to delete a Cmd button, "Print" a file having a name equal to "delete" So, e.g. "Cmd2_delete.xxx" will delete the second button.
	
