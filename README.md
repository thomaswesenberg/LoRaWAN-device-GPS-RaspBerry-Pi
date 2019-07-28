# LoRaWAN-device-GPS-Raspberry-Pi

This repository contains hardware and software details of a personal project on a LoRaWAN device capable of sending its current location. The intention is to figure out coverage of LoRaWAN.

This LoRaWAN device is based on a RaspBerry Pi Zero with a GPS stick and a RN2483A LoRaWAN module. Power consumption is not an issue here, otherwise choosing a RaspBerry would be a no-go. Power comes from a Powerbank or directly from the car battery via a standard 12V->5V converter.

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

*) It should be mentioned here that an alternative would be to use any LoRaWAN device and the smartphone TTN Mapper app. The TTN Mapper app then collects data from the TTN application and uses the GPS of the smartphone to create a valid coverage item. This project does not have such requirements, the LaRaWAN device is completely stand-alone.

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
