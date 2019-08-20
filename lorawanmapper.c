/*****************************/
/* file: lorawanmapper.c     */
/* date: 2019-08-20          */
/* autor: Thomas Wesenberg   */
/*****************************/

#include <stdio.h>		// for printf()
#include <fcntl.h>		// for open()
#include <termios.h>	// for termios
#include <unistd.h>		// for write()
#include <string.h>		// for strlen()
#include <time.h>		// for nanosleep()
#include <stdlib.h>		// for exit()
#include <stdbool.h>	// for bool
#include <sys/time.h>	// for struct timeval and gettimeofday()
#include <gps.h>
#include <unistd.h>
#include <math.h>
#include <wiringPi.h>

#define GPIO_LED_RED   19
#define GPIO_LED_GREEN 26
#define GPIO_RF_RESET  25
#define BUFLEN 64
#define HWEUI_LEN 20
#define SENT_EVERY_X_MINUTES 2	/* 3 minimum for SF12 (5 recommended), 1 minimum for SF7 (2 recommended) */
#define APP_EUI_DEFAULT "0123456789012345" /* private data, refer TTN console */
#define APP_EUI_PRIVATE APP_EUI_DEFAULT
#define APP_KEY_DEFAULT "01234567890123456789012345678901" /* private data, refer TTN console */
#define APP_KEY_PRIVATE APP_KEY_DEFAULT

int button_check(void);

enum LED_ {LED_OFF, LED_RED, LED_GREEN, LED_YELLOW, LED_RESTORE};

int ledStatusSaved = LED_YELLOW;
int ser_fd;
struct gps_data_t my_gps_data;
char message[BUFLEN];
int buttonRecognized = 0;
bool otaaForced = false;

void logMsg(const char *message)
{
	int fd = open("/var/log/lorawanmapper.log", O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if (fd == -1)
		printf("error opening log file\n");
	else {
		printf(message);
		write(fd, message, strlen(message));
		if (fsync(fd) == -1) {
			printf("fsync error\n");
		}
		close(fd);
		if (close(fd) == -1) {
			printf("close error\n");
		}
	}
}

void check_lorawan_parameter(void)
{
	if (APP_EUI_PRIVATE == APP_EUI_DEFAULT) {
		printf("- Please change the APP EUI to connect to TTN network!\r\n");
	}
	if (APP_KEY_PRIVATE == APP_KEY_DEFAULT) {
		printf("- Please change the APP KEY to connect to TTN network!\r\n");
	}
}

void delay_ms(unsigned int ms)
{
	struct timespec ts;
	ts.tv_sec = 0;
	if (ms >= 200) {
		int counter = ms / 200;
		ts.tv_nsec = 100000000L;
		for (int i = 0; i < counter; i++) {
			nanosleep(&ts, (struct timespec *)NULL); // wait 100ms
			button_check(); // takes 100ms too
		}
	} else {
		ts.tv_nsec = ms * 1000000L;
		nanosleep(&ts, (struct timespec *)NULL);
	}
}

void led(int led_status)
{
	if ((led_status == LED_GREEN) || ((led_status == LED_RESTORE) && (ledStatusSaved == LED_GREEN))) {
		digitalWrite(GPIO_LED_GREEN, 0);
		digitalWrite(GPIO_LED_RED, 1);
	} else if ((led_status == LED_RED) || ((led_status == LED_RESTORE) && (ledStatusSaved == LED_RED))) {
		digitalWrite(GPIO_LED_GREEN, 1);
		digitalWrite(GPIO_LED_RED, 0);
	} else if ((led_status == LED_YELLOW) || ((led_status == LED_RESTORE) && (ledStatusSaved == LED_YELLOW))) {
		digitalWrite(GPIO_LED_GREEN, 0);
		digitalWrite(GPIO_LED_RED, 0);
	} else {
		digitalWrite(GPIO_LED_GREEN, 1);
		digitalWrite(GPIO_LED_RED, 1);
	}
	if (led_status != LED_RESTORE) {
		ledStatusSaved = led_status;
	}
}

void init(void)
{
	int rxCount;

	pinMode(GPIO_LED_GREEN, OUTPUT);
	pinMode(GPIO_LED_RED, OUTPUT);
	pinMode(GPIO_RF_RESET, OUTPUT);
	digitalWrite(GPIO_LED_GREEN, 0);
	digitalWrite(GPIO_LED_RED, 0);
	digitalWrite(GPIO_RF_RESET, 0);

	ser_fd = open("/dev/serial0", O_RDWR | O_NOCTTY);
	if (ser_fd == -1) {
		logMsg("could not open serial interface\r\n");
		exit(EXIT_FAILURE);
	} else {
		struct termios options;
		tcgetattr(ser_fd, &options);
		cfsetspeed(&options, B57600);
		cfmakeraw(&options);
		options.c_cflag &= ~CSTOPB;
		options.c_cflag |= CLOCAL;
		options.c_cflag |= CREAD;
		options.c_cc[VTIME]=0;
		options.c_cc[VMIN]=0;
		tcsetattr(ser_fd, TCSANOW, &options);
		tcflush(ser_fd, TCIOFLUSH);
	}

	digitalWrite(GPIO_RF_RESET, 1);
	delay_ms(1000);  // wait for power up of RF chip
	rxCount = read(ser_fd, message, BUFLEN);
	if (rxCount < 1) {
		close(ser_fd);
		logMsg("nothing received while waiting for version info\r\n");
		exit(EXIT_FAILURE);
	}
}

bool join(void)
{
	// let's check if we've got usable data out of eeprom from saved previous activities
	if (otaaForced) {
		return false;
	}
	strncpy(message, "mac get devaddr\r\n", BUFLEN);
	write(ser_fd, message, strlen(message));
	delay_ms(100);
	int rxCount = read(ser_fd, message, BUFLEN);
	if (rxCount < 1) {
		printf("nothing received while waiting for GET DEVADDR reply\r\n");
		return false;
	} else {
		//message[rxCount] = 0;
		//printf("devaddr: %s", message);
		if (strncmp(message, "00000000\r\n", 10) != 0) {
			// ok to reuse data from eeprom
			strncpy(message, "mac join abp\r\n", BUFLEN);
			write(ser_fd, message, strlen(message));
			delay_ms(1000);
			rxCount = read(ser_fd, message, BUFLEN);	// ok
			if (rxCount < 1) {
				return false;
			}
			//message[rxCount] = 0;
			//printf("join abp result: %s", message);
			if (strstr(message, "accepted\r\n")) {
				strncpy(message, "sys get hweui\r\n", BUFLEN);
				write(ser_fd, message, strlen(message));
				delay_ms(100);
				rxCount = read(ser_fd, &message, HWEUI_LEN);
				if (rxCount < 1) {
					return false;
				}
				printf("This HW EUI is %s", message);
				check_lorawan_parameter();
				return true;
			}
		}
	}
	return false;
}

bool activate(void)
{
	int rxCount;
	char hweui[HWEUI_LEN];
	strncpy(message, "sys factoryRESET\r\n", BUFLEN);
	write(ser_fd, message, strlen(message));
	delay_ms(5000);
	rxCount = read(ser_fd, message, BUFLEN);
	if (rxCount < 1) {
		return false;
	} else {
		message[rxCount] = 0;
		printf("sys factoryRESET reply: %s", message);
	}
	strncpy(message, "sys get hweui\r\n", BUFLEN);
	write(ser_fd, message, strlen(message));
	delay_ms(100);
	rxCount = read(ser_fd, &hweui, HWEUI_LEN);
	if (rxCount < 1) {
		return false;
	}
	printf("This HW EUI is %s", hweui);
	check_lorawan_parameter();

	strncpy(message, "mac set deveui ", BUFLEN);
	strncat(message, hweui, BUFLEN - strlen(hweui));
	write(ser_fd, message, strlen(message));
	delay_ms(100);
	rxCount = read(ser_fd, message, BUFLEN);
	if (rxCount < 1) {
		return false;
	}
	strncpy(message, "mac set appeui " APP_EUI_PRIVATE "\r\n", BUFLEN);
	write(ser_fd, message, strlen(message));
	delay_ms(100);
	rxCount = read(ser_fd, message, BUFLEN);
	if (rxCount < 1) {
		return false;
	}
    strncpy(message, "mac set appkey " APP_KEY_PRIVATE "\r\n", BUFLEN); // app: de-elmshorn-1-mapping
	write(ser_fd, message, strlen(message));
	delay_ms(100);
	rxCount = read(ser_fd, message, BUFLEN);
	if (rxCount < 1) {
		return false;
	}
    strncpy(message, "mac set devaddr 00000000\r\n", BUFLEN);	// blanc credentials part 1, necessary for saving eeprom content later
	write(ser_fd, message, strlen(message));
	delay_ms(100);
	rxCount = read(ser_fd, message, BUFLEN);
	if (rxCount < 1) {
		return false;
	}
    strncpy(message, "mac set nwkskey 00000000000000000000000000000000\r\n", BUFLEN);	// blanc credentials part 2, necessary for saving eeprom content later
	write(ser_fd, message, strlen(message));
	delay_ms(100);
	rxCount = read(ser_fd, message, BUFLEN);
	if (rxCount < 1) {
		return false;
	}
    strncpy(message, "mac set appskey 00000000000000000000000000000000\r\n", BUFLEN);	// blanc credentials part 3, necessary for saving eeprom content later
	write(ser_fd, message, strlen(message));
	delay_ms(100);
	rxCount = read(ser_fd, message, BUFLEN);
	if (rxCount < 1) {
		return false;
	}
    strncpy(message, "mac save\r\n", BUFLEN);
	write(ser_fd, message, strlen(message));
	delay_ms(3000);
	rxCount = read(ser_fd, message, BUFLEN);
	if (rxCount < 1) {
		return false;
	}
	strncpy(message, "mac join otaa\r\n", BUFLEN);
	write(ser_fd, message, strlen(message));
	delay_ms(100);
	rxCount = read(ser_fd, message, BUFLEN);
	if (rxCount < 1) {
		return false;
	}
	delay_ms(10000);

	rxCount = read(ser_fd, message, BUFLEN); // accepted or denied
	if (rxCount < 1) {
		printf("nothing received while waiting for JOIN OTAA result\r\n");
		return false;
	} else {
		message[rxCount] = 0;
		printf("join otaa result: %s", message);
		if (strncmp(message, "denied\r\n", 8) == 0) {
			logMsg("OTAA denied\r\n");
			return false;
		}
	}
	return true;
}

void set_parameter(void)
{
	strncpy(message, "radio set sf sf7\r\n", BUFLEN);
	write(ser_fd, message, strlen(message));
	delay_ms(100);
	int rxCount = read(ser_fd, message, BUFLEN);
	if (rxCount < 1) {
		close(ser_fd);
		exit(EXIT_FAILURE);
	}

	strncpy(message, "radio set pwr 15\r\n", BUFLEN);
	write(ser_fd, message, strlen(message));
	delay_ms(100);
	rxCount = read(ser_fd, message, BUFLEN);
	if (rxCount < 1) {
		close(ser_fd);
		exit(EXIT_FAILURE);
	}
}

bool loraSend(int opt)
{
	int LatitudeBinary = ((my_gps_data.fix.latitude + 90) / 180.0) * 16777215;
	int LongitudeBinary = ((my_gps_data.fix.longitude + 180) / 360.0) * 16777215;
	int AltitudeBinary = my_gps_data.fix.altitude;
	int HdopBinary = my_gps_data.dop.hdop * 10.0;
	char message[BUFLEN];
	sprintf(message, "mac tx uncnf 1 %06x%06x%04x%02x%02x\r\n", LatitudeBinary & 0xFFFFFF, LongitudeBinary & 0xFFFFFF, AltitudeBinary & 0xFFFF, HdopBinary & 0xFF, opt);
	tcflush(ser_fd, TCIFLUSH);
	write(ser_fd, message, strlen(message));
	delay_ms(100);
	if (read(ser_fd, message, BUFLEN) < 1) {
		return false;
	}
	if (strncmp(message, "ok\r\n", 4) != 0) {
		char * stringEnd = strstr(message, "\r\n");
		if (stringEnd != NULL) {
			*stringEnd = 0;
		}
		return false;
	}
	delay_ms(5000);
	if (read(ser_fd, message, BUFLEN) < 1) {
		return false;
	}
	if (strncmp(message, "mac_tx_ok\r\n", 11) != 0) {
		char * stringEnd = strstr(message, "\r\n");
		if (stringEnd != NULL) {
			*stringEnd = 0;
		}
		printf("transmit error: [%s]\r\n", message);
		return false;
	}
	return true;
}

void saveLoraParameter(void)
{
	tcflush(ser_fd, TCIOFLUSH);
	strncpy(message, "mac save\r\n", BUFLEN);
	write(ser_fd, message, strlen(message));
	delay_ms(3000);
	int rxCount = read(ser_fd, message, BUFLEN);
	if (rxCount < 1) {
		close(ser_fd);
		exit(EXIT_FAILURE);
	}
	message[rxCount] = 0;
	printf("mac save reply: %s", message);
}

void deinit(void)
{
	digitalWrite(GPIO_RF_RESET, 0);
	gps_stream(&my_gps_data, WATCH_DISABLE, NULL);
	gps_close (&my_gps_data);
}

int button_check(void)
{
	// The buttons are in parallel to the LEDs. To check button status we have to switch LEDs off.
	// Hardware circuitry takes care that switching LEDs off for less than 250ms will have no visual effect.
	static bool valueButtonGreenSaved = false, valueButtonRedSaved = false;
	int valueButtonGreen, valueButtonRed;
	int button = 0;
	struct timespec ts;
	ts.tv_sec = 0;
	// set to input and wait a very short time
	pinMode(GPIO_LED_GREEN, INPUT);
	pinMode(GPIO_LED_RED, INPUT);
	ts.tv_nsec = 10000000L;
	nanosleep(&ts, (struct timespec *)NULL);
	valueButtonGreen = digitalRead(GPIO_LED_GREEN);
	valueButtonRed = digitalRead(GPIO_LED_RED);
	// set back to output and wait for debouncing functionality (read status two times)
	pinMode(GPIO_LED_GREEN, OUTPUT);
	pinMode(GPIO_LED_RED, OUTPUT);
	led(LED_RESTORE);
	ts.tv_nsec = 90000000L;
	nanosleep(&ts, (struct timespec *)NULL);
	// set to input and wait a very short time
	pinMode(GPIO_LED_GREEN, INPUT);
	pinMode(GPIO_LED_RED, INPUT);
	ts.tv_nsec = 10000000L;
	nanosleep(&ts, (struct timespec *)NULL);
	valueButtonGreen += digitalRead(GPIO_LED_GREEN);
	valueButtonRed += digitalRead(GPIO_LED_RED);
	// now we have all the information we need, so set back to output for controlling LED status
	pinMode(GPIO_LED_GREEN, OUTPUT);
	pinMode(GPIO_LED_RED, OUTPUT);
	led(LED_RESTORE);
	if (valueButtonGreen == 0) {
		valueButtonGreenSaved = true;
	}
	if (valueButtonRed == 0) {
		valueButtonRedSaved = true;
	}
	if (valueButtonGreen == 2) {
		valueButtonGreenSaved = false;
	}
	if (valueButtonRed == 2) {
		valueButtonRedSaved = false;
	}
	if (valueButtonGreenSaved) {
		button = 1;
	} else if (valueButtonRedSaved) {
		button = 2;
	}
	if (buttonRecognized < button) {
		buttonRecognized = button;
	}
	return button;
}

int button_released(void)
{
	if (button_check() == 0) {
		int value = buttonRecognized;
		if (buttonRecognized > 0) {
			buttonRecognized = 0;
			return value;
		}
	}
	return 0;
}

bool init_gps(void)
{
	int rc;
	for (int i = 0; i < 10; i++) {
		rc = gps_open("localhost", "2947", &my_gps_data);
		if (rc != -1) {
			gps_stream(&my_gps_data, WATCH_ENABLE | WATCH_JSON, NULL);
			return true;
		}
		sleep(3);
	}
	printf("code: %d, reason: %s\n", rc, gps_errstr(rc));
	return false;
}

void flush_gps_data(void)
{
	while (gps_read(&my_gps_data) > 0) {
		;
	}	
}

bool fetch_gps(void)
{
	for (int i = 0; i < 3; i++) {
		int rc = gps_read(&my_gps_data);
		if (rc > 0) {
			while (rc > 0) {
				// flush old data
				rc = gps_read(&my_gps_data);
			}
			if ((my_gps_data.status == STATUS_FIX) &&
				(my_gps_data.fix.mode == MODE_3D) &&
				!isnan(my_gps_data.fix.time) &&
				!isnan(my_gps_data.fix.latitude) &&
				!isnan(my_gps_data.fix.longitude) &&
				!isnan(my_gps_data.fix.altitude) &&
				!isnan(my_gps_data.dop.hdop)) {
					return true;
				}
		}
		delay_ms(500);
	}
	return false;
}

int main(int argc, char** argv)
{
	bool automode = true;
	if (argc == 2) {
		if (strcmp(argv[1], "OTAA") == 0) {
			otaaForced = true;
			logMsg("> forcing OTAA join <\r\n");
		}
	} else if (argc > 2) {
		logMsg("Too many arguments supplied.\n");
		return EXIT_FAILURE;
	}
	printf("Now initializing WiringPi ...\r\n");
	wiringPiSetupGpio();
	pullUpDnControl(GPIO_LED_GREEN, PUD_UP);
	pullUpDnControl(GPIO_LED_RED, PUD_UP);
	printf("WiringPi Setup finished\r\n");
	init();
	printf("Now initializing GPS ...\r\n");
	if (init_gps() == false) {
		logMsg("GPS init failed\r\n");
		return EXIT_FAILURE;
	}
	printf("GPS init finished\r\n");
	// set up lorawan
	if (! join()) {
		// communicate with network, wait as long as necessary
		while (! activate()) {
			delay_ms(SENT_EVERY_X_MINUTES * 60 * 1000);
		}
	}
	led(LED_OFF);
	set_parameter();
	flush_gps_data();
	printf("successfully initialized, now waiting for an event (time, button) ...\n");
	while (1) {
		static int option = 0;
		static long gpsSecondNow = 0;
		static long lastDataSentSecond = 0;
		static bool gpsFixMsgPrinted = false;
		if (fetch_gps()) {
			if (!gpsFixMsgPrinted) {
				gpsFixMsgPrinted = true;
				printf("GPS fix achieved\r\n");
			}
			bool sendDataNow = false;
			int buttonReleased = button_released();
			if (buttonReleased == 1) {
				option = 1;
				sendDataNow = true;
				gpsSecondNow = (long)my_gps_data.fix.time;
			} else if (buttonReleased == 2) {
				// toggle automatically transmission
				if (automode) {
					printf("automode off\n");
					automode = false;
					led(LED_RED);
					delay_ms(2000);
					led(LED_YELLOW);
					delay_ms(2000);
					led(LED_RED);
					delay_ms(2000);
					led(LED_OFF);
				} else {
					printf("automode on\n");
					automode = true;
					led(LED_GREEN);
					delay_ms(2000);
					led(LED_YELLOW);
					delay_ms(2000);
					led(LED_GREEN);
					delay_ms(2000);
					led(LED_OFF);
				}
			} else if (automode) {
				if (gpsSecondNow != (long)my_gps_data.fix.time) {
					gpsSecondNow = (long)my_gps_data.fix.time;
					if ((gpsSecondNow % (SENT_EVERY_X_MINUTES * 60)) == 0) {
						if (gpsSecondNow >= (lastDataSentSecond + SENT_EVERY_X_MINUTES * 60 - 3)){
							sendDataNow = true;
						}
					}
				}
			}
			if (sendDataNow) {
				led(LED_GREEN);
				if (loraSend(option)) {
					// be prepared for poweroff
					saveLoraParameter();	// takes about 3 seconds to finish //
					led(LED_OFF);
				} else {
					led(LED_RED);
					delay_ms(15000);
					led(LED_OFF);
				}
				printf("lat: %f, lon: %f, alt: %f, hdop: %f\n", my_gps_data.fix.latitude, my_gps_data.fix.longitude, my_gps_data.fix.altitude, my_gps_data.dop.hdop);
				lastDataSentSecond = gpsSecondNow;
			}
			delay_ms(100);
		} else {
			led(LED_YELLOW);
			delay_ms(2000);
			led(LED_OFF);
			delay_ms(1000);
		}
	}
	deinit();
	return EXIT_SUCCESS;
}
