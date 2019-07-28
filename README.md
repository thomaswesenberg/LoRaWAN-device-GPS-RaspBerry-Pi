# LoRaWAN-device-GPS-Raspberry-Pi

This repository contains hardware and software details of a personal project on a LoRaWAN device capable of sending its current location. The intention is to figure out coverage of LoRaWAN.

This LoRaWAN device is based on a RaspBerry Pi Zero with a GPS stick and a RN2483A LoRaWAN module. Power consumption is not an issue here, otherwise choosing a RaspBerry would be a no-go. Power comes from a Powerbank or directly from the car battery via a standard 12V->5V converter.

Beware: don't start developing such a device if there's no gateway around. You definitely need it during development nearly all the time.

![Alt text](pictures/prototype_front-view.jpg?raw=true "prototype")

# Short description
If a LoRaWAN data packet is received by a gateway configured for TTN it is forwarded to the selected TTN server application. Using a payload decoder and a 'TTN Mapper' integration the location data will be considered by ttnmapper.org and shows up in their map. By contributing to the TTN Mapper project a worldwide map of LoRaWAN coverage is build up.
1) Having a LoRaWAN gateway configured for TTN nearby is necessary for first testing purposes.
   https://www.thethingsnetwork.org/ lists known locations of gateways and sometimes also coverage data on the map. At the time this description was written about 7400 gateways were up and running worldwide.
   It is possible to set up an own gateway, configured for TTN or any other server architecture, even a private one.
2) After creating an account at https://www.thethingsnetwork.org/ it is possible to set up an application which would be the receiver for the own LoRaWAN data packets. LoRaWAN data is end-to-end encrypted. To inform ttnmapper.org about sucessfully packet reception by LoRaWAN network, an application plugin is available, called 'TTN Mapper'. *)
3) To deliver the necessary GPS data to the TTN mapper project a 'decoder' converts the raw data into a JSON data structure.
4) The hardware is based on the RN2483A fully-certified LoRa module which has the whole LoRaWAN stack integrated.
5) The software is realized by a C program with autostart functionality. Every 5 minutes a packet is sent out. Alternatively pressing a button sends out a packet at once.
6) A standard USB GPS stick deliveres the GPS location data.

Please see the other files of this project for more details.

*) It should be mentioned here that an alternative would be to use any LoRaWAN device and the smartphone TTN Mapper app. The TTN Mapper app then collects data from the TTN application and uses the GPS of the smartphone to create a valid coverage item. This project does not have such requirements, the LaRaWAN device is stand-alone, without the need of a smartphone nearby running an app.

# Installation notes
The notes how to configure a RaspBerry Pi Zero will be added soon ...

# Operational notes
After powering up LoRaWANmapper is started automatically. <br>
a) Red and green LEDs light up while initializing. This may take several seconds or will continue endlessly (no LoRaWAN gateway nearby and no valid network data available from an earlier joining). <br>
b) The green LED might flash for some time until GPS fix is achieved. <br>
c) Pressing the button will light up the gren LED and after releasing a data packet is sent out. <br>
   If sending was successfull the green LED keeps lighting for 10 seconds. <br>
   If the red LED lights up instead most probably too many packets were sent out and pausing for some minutes is required. <br>
d) Without a button event for a minimum of 5 minutes a data packet is automatically sent out every 5 minutes of the hour.

# Hardware
Wiring LoRa Module RN2483A (Microchip) to the RaspBerry Pi Zero is realized with some cables and this break out module: <br>
https://www.tindie.com/products/drazzy/lorawan-rn2483rn2903-breakout-board-assembled/ <br>
For connecting the GPS stick (here VK-172) a Micro-USB OTG adapter cable is neccessary. <br>
Instead of a RasbBerry Pi Zero also any other RaspBerry could be used too, then the adapter cable is not required.

In the picture is a 3-pin connector to be seen soldered to the serial pins. It is used together with two serial-to-USB adapters (only RX pin used) to monitor both serial data streams. This is a very helpful debugging tool and avoids to insert many printf debug outputs in the code.

More details are coming soon. Please take a look at the high resolution pictures in between.

# Used commands for the RN2483A:
a) initialisation if not joined before, triggers OTAA (requires LoRaWAN network access)

| Command | Response |
| --- | --- |
| [power on] | RN2483 1.0.4 Oct 12 2017 14:59:25 |
| sys get hweui | [hweui] |
| mac set deveui [hweui] | ok |
| mac set appeui [appeui] | ok |
| mac set appkey [appkey] | ok |
| mac set devaddr 00000000 | ok |
| mac set nwkskey 00000000000000000000000000000000 | ok |
| mac set appskey 00000000000000000000000000000000 | ok |
| mac save | ok |
| mac join otaa | ok |
|  | accepted |
| radio set sf sf7 | ok |
| radio set pwr 15 | ok |

b) initialisation if already joined, no LoRaWAN network access required (no LoRa data transmission)

| Command | Response |
| --- | --- |
| [power on] | RN2483 1.0.4 Oct 12 2017 14:59:25 |
| mac get devaddr | [non zero value] |
| mac join abp | ok |
|  | accepted |
| radio set sf sf7 | ok |
| radio set pwr 15 | ok |

c) data

| Command | Response |
| --- | --- |
| mac tx uncnf 1 [data] | ok |
|  | mac_tx_ok |
| mac save | ok |

Example of [data]: cab16f89875600030b00 <br>
with latitude = 0xcab16f => 52.51859 <br>
and longitude = 0x898756 => 13.39968 <br>
and altitude = 0x0003 => 3 <br>
and hdop = 0x0b  => 1.1 <br>
and option = 0 <br>
(interpreted by the individual TTN application decoder)

Hint: If you are in reach of a LoRaWAN gateway but the LoRa module answers with a 'denied' while trying to join the network via command 'mac join otaa', most probably wrong EEPROM content is the cause. Then a 'sys factoryRESET' may be necessary to continue. Before issuing a 'mac save' command the values of devaddr, appeui and appkey have to be updated. The first time before issuing a 'mac join otaa' these values have to be set to zero. After successfully joining the network the module updates these values internally. A following 'mac save' command writes them into EEPROM. This should also be the last command before powering off. Otherwise the frame counter value in the EEPROM ist not updated to the last one used. Continuing later with an old frame counter value may require to reset the frame counter in the application manually. Otherwise all the frames with counter values less or equal to the last one received, will be discarded by the server.
