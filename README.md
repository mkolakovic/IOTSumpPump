# Work in progress!
#
# This will get published in February 2018 issue of Nuts&Volts Magazine


# IOTSumpPump


My campaign against infiltrating water began when I moved to a location with a higher water table that required the assistance of a sump pump to keep water out of the basement. This is particularly worrisome during the snow melts and heavy rains of the spring.  As every sump pump will one day expire, backup systems are needed to prevent basement flooding. The device I created provides a first line of defense against a flood.  It will alert the user via text message that the water level has risen above the sump pump’s maximum level thereby indicating a pump failure. This alert could potentially enable the user to return home and drop in a replacement pump prior to any flooding occurring.
    
Although the device described here will send a text as a sump pump warning, this project applies to pretty much anything with an appropriate sensor. An open door or window, a tripped laser beam, a pressure change on a pressure plate, a proximity sensor, etc. could all be converted into a text message warning. 

#Project Overview

My goal for this project was to design a device that will monitor the sump pump water level and text me if the water level gets too high. The device design parameters included: must be battery powered, have Wi-Fi capability, and have the ability to send an email or text. I chose to go with Texas Instruments CC3200 LaunchPad, which is the evaluation board for CC3200 Wi-Fi wireless MCU. LaunchPad comes with a built in in-circuit debugger, LEDs, switches, sensors, and two 20-pin connectors.

 For water level sensing, I selected a reed switch commonly used in aquariums. The switch is hermetically sealed in the body of the device, while a magnet is located in the float ring. The bracket for the reed switch was cut from 1/8in x 2in x 4ft piece of aluminum from a local hardware store. 

The entire design runs off of two AA batteries. To limit the load current and extend battery life, the device is mostly hibernating. It wakes up immediately if the reed switch closes and it also wakes up every 8 hours to check the battery voltage and read the switch. If it finds that the reed switch is closed or the battery level is low, it will text me to let me know. 


![Alt Text](/img/iot_SumpPump.png)
Figure-1. IoT Sump Pump

#Email to Text

To avoid utilizing third-party servers/gateways to send an email or text, I use CC3200 SDK and my Gmail account to send the text. The way it works is that on CC3200, I run a SMTP client that connects to my Gmail account. Once connected, it sends an email message to my mobile carrier’s SMS gateway, which sits on the mobile network that delivers the message. All you need is a phone number and a SMS gateway domain name. In my case, the Verizon SMS gateway domain is vtext.com.  So I send an email to 1234567890@vtext.com, where 1234567890 is my phone number. All email and Wi-Fi settings for the project are stored in ‘config.h’ file.
















![Alt Text](/img/iot.png)

Figure-2. Project Big Picture


#Hardware Design

CC3200 is a SoC that consists of an ARM M4F core for application software processing, SimpleLink (the Wi-Fi network processor subsystem), 256 kB RAM, and peripherals. SimpleLink has its own dedicated ARM MCU that completely off loads the application MCU. It also includes 802.11bgn Radio and a TCP/IP Stack. This arrangement simplifies development significantly.













Figure-3. CC3200 Hardware Overview


The application MCU runs at 80MHz. User code and user files are stored in an external serial flash (1MB). ROM comes factory programmed with device initialization firmware, a bootloader, and a peripheral driver library. The power up sequence is as follows: after a POR, the device gets initialized, then the bootloader loads the application code from the serial flash into on-chip SRAM and jumps to the application code entry point.
















Figure-4. CC3200 Functional Block Diagram

There are only a few minor modifications needed for LaunchPad:

1 – Add a resistor divider across the battery terminals and connect to ADC Pin#58
2 – Open J2 and J3 so that Yellow and Green LEDs are not ON in Hibernate
3 – Open J13 to supply the board from the J20 battery connector
4 – Depopulate R3 to disable D1 LED
5 – Depopulate R20 to disable D4 LED
6 – Connect the reed switch to Wake-Up input GPIO#13

CC3200 ADC is a 12-bit 8 channel, out of which 4 channels are available for user application. The sampling rate is fixed at 16us per channel and the channels are sampled in a round-robin fashion. The pins tolerate a maximum of 1.8V, but full scale is 1.467V.

To keep it simple, I chose to implement a divide-by-two resistive divider. Since this will be connected across the battery terminals at all times, I went with 1Meg resistors to keep the current consumption low. The drawback to this approach is that the sampling capacitor current will be severely limited thereby affecting A2D readings. To fix this problem, I placed an 100nF capacitor across the lower leg of the resistive divider.  

The CC3200 internal sampling cap is 12pF and it is switched on for 400ns.  So a much larger 100nF external cap will supply enough current for the sampling cap to reach the final voltage value of the divider in time to get correctly sampled. The simulation below shows the voltage on the sampling cap with and without an 100nF external cap. 

 



Figure-5. Voltage on the sampling capacitor with and without external capacitor in the voltage divider

Also, I knowingly compromised the voltage range measurement and bandwidth. This means the A2D input will get full scale when the battery voltage is 2.9V. Also, the voltage change on the pin will be slower. In this application, that did not matter since I was only interested in a low point in the battery voltage range. If the battery voltage dips below 2.4V, a text will go out as a warning to replace the batteries. Assuming two AA batteries, not at full capacity, and average load current about six times higher than expected, the batteries should last well over a year:  2000mAh/0.1mA = 20000h ~ 833 days.

Whenever the reed switch input is sampled, it is also debounced. The pin is pooled every 10ms, 100 consecutive times.  Only if it reads “1” every single time will it return “success.” This will prevent a false alarm.

To preserve the batteries, the device is kept in Hibernate mode. In this state, most of the SoC is powered down except the RTC and 2x32-bit OCR registers. Before hibernating, the software enables two wake up sources: RTC (every 8 hours) and GPIO#13. The reed switch and LaunchPad switch SW3 are both connected to GPIO#13.  Upon wakeup, the core resumes its execution in the ROM bootloader. While hibernating, the measured current out of the battery was close to 8uA.

#Software Design

The software development was done using CCS - Code Composer Studio, a free IDE with compiler/debugger. CCS is a very powerful tool, but it takes some time to get to know it. There are lots of videos on CCS on the web, so I won’t go into details on how to use it here.

CC3200 has three SOP (Sense-On-Power) pins. The state of these pins defines what mode CC3200 will be in after power up. SOP[2:0]=0b000 instructs the bootloader to load the application from the serial flash to the internal MCU RAM. SOP[2:0]=0b100 instructs the bootloader to enter “download” mode. This mode is used to program the application to the serial flash. This is done with the help of a UniFlash tool. Once the image is programmed, J15 can be removed and the board reset. At that point, the application code is going to get loaded from the serial flash to SRAM and executed. During development/debugging, I have J15 on (SOP[2:0]=0b100), which will keep the bootloader in wait mode and the application won’t be automatically running.

The application is non-OS based. The logic is very simple and robust, which keeps code to the minimum but still accomplishes the objective. Also, I used CC3200 DriverLib as much as possible and all the DriverLib calls are linked to ROM (functions with “MAP” in prefix). The main function initializes the board, reads the sensors, sends an email if needed, and goes into hibernation when done. Every time the device wakes up, it goes through the same routine. 

The first thing the application will do is initialize the board. This takes care of setting up the ports and pins, UART, and hibernate mode. Next it will check the battery voltage level.  If the battery voltage level is low, the application will send an email warning. It will also read the reed switch. If the switch is Off, the board goes into hibernation.  The board will again wake up in 8 hours (or on a closed switch). However, if the switch is On, the application will send an email, disable wake up input, and set the timer to wake up in 60 minutes. This will avoid email bombardment. 

If the sump pump gets fixed by the time the board wakes up, then everything continues as if nothing happened, and the board goes into hibernation for the next 8 hours. But if the sump pump reed switch is still On, an email goes out again. If everything goes right with the AP connection and an email gets sent, all three LEDs will turn on before hibernation. If there is a problem with the connection to the AP, an orange LED will flash. If there is a problem with sending an email, a green LED will flash.





















Figure-6. Code Flow


#Additional Considerations

A SMTP client is part of CC3200 SDK and it will create a socket, connect, authenticate, create and send packets to a SMTP server for you. I won’t go into details on how the SMTP protocol works here, but there is an excellent “Email” example included with SDK. Also, you should check out the original SMTP spec created in 1982, RFC 821.

Needless to say, you will need a Gmail account. You must set your username, password, phone number, and your AP access parameters in “config.h” file. In addition, you may also have to change your Gmail account settings and enable access for less secure apps. Also, I have my AP on a battery backup unit in case of a power outage so that I can still get warnings.

This project was done using CCS 7 and CC3200 SDK 1.2. The project is available for download at https://github.com/mkolakovic/IOTSumpPump.  You may need to change project settings and redefine paths to your CCS and SDK libraries. All development software is free and available for download and without limitations. 

Now for the real-world problems: try not to place the board too close to the pump power cable, because EMI generated by the pump motor and radiated from the power cable could interfere with the board. If you must install it closer, the input debouncing is robust enough to function.  The worst that can happen are false wake ups affecting battery life.  This assumes you have no issue with Wi-Fi in the installation location. You could try putting the board in the metal enclosure, but then you’ll need a whip antenna and that would be just fine. 

Another issue is running the reed switch cable in parallel and next to the power cable. This could cause high voltage transients from the pump power cable to couple to the reed switch cable. Since there is no TVS on the CC3200 reed switch input, transient could potentially damage the SoC. A TVS on the reed switch input is something I am definitely planning on adding to this project.


Figure-7. Code Composer Studio 7 IDE Debug Session


So go ahead and see a movie, or meet a friend for coffee.  Even if its pouring outside. Because if your pump decides to dump, you’ll get a text warning. And since you can save the day from there on out, there will be no flood. And all of the awesome stuff that you’ve got in the basement will be just fine. 





#References:

http://www.ti.com/tool/cc3200modlaunchxl
http://www.ti.com/lit/ds/swrs166/swrs166.pdf
http://www.ti.com/lit/ug/swru367c/swru367c.pdf
http://www.ti.com/product/CC3200/datasheet/detailed_description
https://en.wikipedia.org/wiki/SMS_gateway
http://processors.wiki.ti.com/index.php/CC32xx_Email_Demo_Application
https://www.ietf.org/rfc/rfc2821.txt
