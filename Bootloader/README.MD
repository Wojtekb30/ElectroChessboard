I have wired FT232RL incorrectly, and because of that a custom bootloader is needed. 

The bootloader uses software serial communication instead of the physical UART.

To set it up:

1. Copy the .hex and .lst files from here to (or wherever that directory is in your Arduino IDE version and your OS):
```
C:\Users\[user]\AppData\Local\Arduino15\packages\arduino\hardware\avr\1.8.6\bootloaders\optiboot
```

2. Two directories higher, you will find file boards.txt. Edit it and add in the end:
```
##############################################################
# Mega2560 with Optiboot Bootloader (Soft UART on PD2/PD3 @ 19200)
##############################################################

optiboot2560pd2pd3.name=ATMEGA2560 (Optiboot SoftUART PD2/PD3 @ 19200)

optiboot2560pd2pd3.upload.tool=arduino:avrdude
optiboot2560pd2pd3.upload.tool.serial=arduino:avrdude
optiboot2560pd2pd3.upload.protocol=arduino
optiboot2560pd2pd3.upload.maximum_size=258048
optiboot2560pd2pd3.upload.maximum_data_size=8192
optiboot2560pd2pd3.upload.speed=19200

optiboot2560pd2pd3.bootloader.tool=arduino:avrdude
optiboot2560pd2pd3.bootloader.tool.serial=arduino:avrdude
optiboot2560pd2pd3.bootloader.low_fuses=0xFF
optiboot2560pd2pd3.bootloader.high_fuses=0xDE
optiboot2560pd2pd3.bootloader.extended_fuses=0xFD
optiboot2560pd2pd3.bootloader.unlock_bits=0x3F
optiboot2560pd2pd3.bootloader.lock_bits=0x2F
optiboot2560pd2pd3.bootloader.file=optiboot/optiboot_atmega2560_ElectroChessboard.hex

optiboot2560pd2pd3.build.mcu=atmega2560
optiboot2560pd2pd3.build.f_cpu=16000000L
optiboot2560pd2pd3.build.board=AVR_MEGA2560
optiboot2560pd2pd3.build.core=arduino:arduino
optiboot2560pd2pd3.build.variant=arduino:mega
```

3. Open Arduino IDE (if it was opened while you were editing, close and open it again).

4. Wire ISP programmer to the ElectroChessboard (I used Arduino Uno as ISP).

5. Find and choose the above configuration (it should be on list of Arduino AVR Boards), then choose your programmer, then click "burn bootloader".
I recommend to turn on verbose avrdude (programming) output in Arduino IDE settings to see logs.

6. Now you should be able to upload code to the ElectroChessboard via its USB, as long as the config is chosen in Arduino IDE. Disconnect ISP programmer to enable UART (at least its VCC and GND).

Because of its power consumption, I recommend connecting ElectroChessboard to your computer's USB-C port if it has such with USB-C - USB-C cable. 
Otherwise connect to the latest USB standard port you have, preferably the one that supports power delivery (ElectroChessboard does not use USB-PD, but that will be safer).

To communicate over Serial in your program later, you must use SoftwareSerial. Example:
```c++
#include <SoftwareSerial.h> 

//Define Pin 19 (which is PD2/Pin 45 on the chip) as the TX pin
SoftwareSerial MyFix(10, 19); //RX on PD3, TX on 19 (PD2/Pin 45)
void setup()
{ 
MyFix.begin(9600); 
}
void loop()
{
MyFix.println("Hello world!");
delay(1000);
}
```

Please mind that UART lines are shared with two address connections of demuxes that control LEDs (LED_A2_2 and LED_A2_3) so do not use Serial and these LED adress lines at the same time. 