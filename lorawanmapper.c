/*****************************/
/* file: lorawanmapper.c     */
/* date: 2019-07-28          */
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

#define DELAY_1MS     1000
#define DELAY_10MS   10000
#define DELAY_100MS 100000
#define DELAY_200MS 200000
#define DELAY_500MS 500000
#define DELAY_1S   1000000
#define DELAY_2S   2000000
#define DELAY_10S 10000000
#define BUFLEN 64
#define HWEUI_LEN 20
#define SENT_EVERY_X_MINUTES 5
#define APP_EUI "0123456789012345" /* private data, refer TTN console */
#define APP_KEY "01234567890123456789012345678901" /* private data, refer TTN console */

int ser_fd;
struct gps_data_t my_gps_data;
char message[BUFLEN];

int delay(unsigned long us)
{
  struct timespec ts;
  int err;

  ts.tv_sec = us / 1000000L;
  ts.tv_nsec = (us % 1000000L) * 1000L;
  err = nanosleep(&ts, (struct timespec *)NULL);
  return(err);
}

void led_green(bool on)
{
	FILE *fp;
	if (on)
		fp = popen("echo 0 > /sys/class/gpio/gpio2/value", "r");
	else
		fp = popen("echo 1 > /sys/class/gpio/gpio2/value", "r");
	pclose(fp);
}

void led_red(bool on)
{
	FILE *fp;
	if (on)
		fp = popen("echo 0 > /sys/class/gpio/gpio3/value", "r");
	else
		fp = popen("echo 1 > /sys/class/gpio/gpio3/value", "r");
	pclose(fp);
}

void init(void)
{
	FILE *fp;
	int rxCount;

	// prepare GPIO access
	// LED green
	fp = popen("echo 2 > /sys/class/gpio/export 2>/dev/null", "r");
	pclose(fp);
	// LED red
	fp = popen("echo 3 > /sys/class/gpio/export 2>/dev/null", "r");
	pclose(fp);
	// RF chip reset pin
	fp = popen("echo 25 > /sys/class/gpio/export 2>/dev/null", "r");
	pclose(fp);
	// button
	fp = popen("echo 16 > /sys/class/gpio/export 2>/dev/null", "r");
	pclose(fp);
	ser_fd = open("/dev/serial0", O_RDWR | O_NOCTTY);
	if (ser_fd == -1) {
		printf("could not open serial interface\r\n");
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
	// set GPIO direction and default value
	fp = popen("echo out > /sys/class/gpio/gpio2/direction", "r");
	pclose(fp);
	fp = popen("echo 1 > /sys/class/gpio/gpio2/value", "r");
	pclose(fp);
	fp = popen("echo out > /sys/class/gpio/gpio3/direction", "r");
	pclose(fp);
	fp = popen("echo 1 > /sys/class/gpio/gpio3/value", "r");
	pclose(fp);
	fp = popen("echo out > /sys/class/gpio/gpio25/direction", "r");
	pclose(fp);
	fp = popen("echo 0 > /sys/class/gpio/gpio25/value", "r");   // reset chip
	pclose(fp);
	delay(DELAY_1MS);

	// start up RF chip
	fp = popen("echo 1 > /sys/class/gpio/gpio25/value", "r");
	pclose(fp);
	delay(DELAY_1S);  // wait for power up of RF chip
	rxCount = read(ser_fd, message, BUFLEN);
	if (rxCount < 1) {
		close(ser_fd);
		printf("nothing received while waiting for version info\r\n");
		exit(EXIT_FAILURE);
	}
	//message[rxCount] = 0;
	//printf("chip version: %s", message);
}

bool join(void)
{
	// let's check if we've got usable data out of eeprom from saved previous activities
	strncpy(message, "mac get devaddr\r\n", BUFLEN);
	write(ser_fd, message, strlen(message));
	delay(DELAY_100MS);
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
			delay(DELAY_1S);
			rxCount = read(ser_fd, message, BUFLEN);	// ok
			if (rxCount < 1) {
				return false;
			}
			//message[rxCount] = 0;
			//printf("join abp result: %s", message);
			if (strstr(message, "accepted\r\n")) {
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
	sleep(5);
	rxCount = read(ser_fd, message, BUFLEN);
	if (rxCount < 1) {
		return false;
	} else {
		message[rxCount] = 0;
		printf("sys factoryRESET reply: %s", message);
	}
	strncpy(message, "sys get hweui\r\n", BUFLEN);
	write(ser_fd, message, strlen(message));
	delay(DELAY_100MS);
	rxCount = read(ser_fd, &hweui, HWEUI_LEN);
	if (rxCount < 1) {
		return false;
	}
	strncpy(message, "mac set deveui ", BUFLEN);
	strncat(message, hweui, BUFLEN - strlen(hweui));
	write(ser_fd, message, strlen(message));
	delay(DELAY_100MS);
	rxCount = read(ser_fd, message, BUFLEN);
	if (rxCount < 1) {
		return false;
	}
	strncpy(message, "mac set appeui " APP_EUI "\r\n", BUFLEN);
	write(ser_fd, message, strlen(message));
	delay(DELAY_100MS);
	rxCount = read(ser_fd, message, BUFLEN);
	if (rxCount < 1) {
		return false;
	}
    strncpy(message, "mac set appkey " APP_KEY "\r\n", BUFLEN); // app: de-elmshorn-1-mapping
	write(ser_fd, message, strlen(message));
	delay(DELAY_100MS);
	rxCount = read(ser_fd, message, BUFLEN);
	if (rxCount < 1) {
		return false;
	}
    strncpy(message, "mac set devaddr 00000000\r\n", BUFLEN);	// blanc credentials part 1, necessary for saving eeprom content later
	write(ser_fd, message, strlen(message));
	delay(DELAY_100MS);
	rxCount = read(ser_fd, message, BUFLEN);
	if (rxCount < 1) {
		return false;
	}
    strncpy(message, "mac set nwkskey 00000000000000000000000000000000\r\n", BUFLEN);	// blanc credentials part 2, necessary for saving eeprom content later
	write(ser_fd, message, strlen(message));
	delay(DELAY_100MS);
	rxCount = read(ser_fd, message, BUFLEN);
	if (rxCount < 1) {
		return false;
	}
    strncpy(message, "mac set appskey 00000000000000000000000000000000\r\n", BUFLEN);	// blanc credentials part 3, necessary for saving eeprom content later
	write(ser_fd, message, strlen(message));
	delay(DELAY_100MS);
	rxCount = read(ser_fd, message, BUFLEN);
	if (rxCount < 1) {
		return false;
	}
    strncpy(message, "mac save\r\n", BUFLEN);
	write(ser_fd, message, strlen(message));
	sleep(3);
	rxCount = read(ser_fd, message, BUFLEN);
	if (rxCount < 1) {
		return false;
	}
	strncpy(message, "mac join otaa\r\n", BUFLEN);
	write(ser_fd, message, strlen(message));
	delay(DELAY_100MS);
	rxCount = read(ser_fd, message, BUFLEN);
	if (rxCount < 1) {
		return false;
	}
	delay(DELAY_10S);

	rxCount = read(ser_fd, message, BUFLEN); // accepted or denied
	if (rxCount < 1) {
		printf("nothing received while waiting for JOIN OTAA result\r\n");
		return false;
	} else {
		message[rxCount] = 0;
		printf("join otaa result: %s", message);
		if (strncmp(message, "denied\r\n", 8) == 0) {
			return false;
		}
	}
	return true;
}

void set_parameter(void)
{
	strncpy(message, "radio set sf sf7\r\n", BUFLEN);
	write(ser_fd, message, strlen(message));
	delay(DELAY_100MS);
	int rxCount = read(ser_fd, message, BUFLEN);
	if (rxCount < 1) {
		close(ser_fd);
		exit(EXIT_FAILURE);
	}

	strncpy(message, "radio set pwr 15\r\n", BUFLEN);
	write(ser_fd, message, strlen(message));
	delay(DELAY_100MS);
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
	delay(DELAY_100MS);
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
	sleep(5);
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
	sleep(3);
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
	// RF chip reset
	FILE *fp = popen("echo 0 > /sys/class/gpio/gpio25/value", "r");
	pclose(fp);
	gps_stream(&my_gps_data, WATCH_DISABLE, NULL);
	gps_close (&my_gps_data);
}

bool button(void)
{
	FILE *fp;
	int value = 0;
	fp = popen("cat /sys/class/gpio/gpio16/value", "r");
	if (fp != NULL) {
   	value = fgetc(fp);
		pclose(fp);
	}
	if (value == '0')
		return true;
	return false;
}

bool button_pressed(void)
{
	// called every 100ms
	static struct timeval lastTimeEvent;
	static bool buttonSaved = false;
	static bool buttonStatus = false;
	int elapsedTime;
	if (button() != buttonSaved) {
		gettimeofday(&lastTimeEvent, NULL);
		buttonSaved = ! buttonSaved;
	}
	else {
		if (buttonStatus != buttonSaved) {
			buttonStatus = buttonSaved;
			//printf("button status changed\n");
			if (buttonStatus) {
				led_green(true);
			} else {
				led_green(false);
			}
		}
#if 0
		else if (buttonStatus == true) {
			// check if we have a long pressing event
			struct timeval timeNowVal; 
			gettimeofday(&timeNowVal, NULL);
			elapsedTime = timeNowVal.tv_sec - lastTimeEvent.tv_sec;
			if (elapsedTime > 5) {
				// button pressing for 5 seconds means "power down"
				led_green(true);
				led_red(true);
				deinit();
				sleep(3);
				//popen("sudo /sbin/shutdown -a -P now", "r");
				popen("poweroff", "r");
			}
		}
#endif
	}
	return buttonStatus;
}

bool button_released(void)
{
	static bool eventAnnounced = true;
	if (button_pressed()) {
		eventAnnounced = false;
	}
	else {
		if (! eventAnnounced) {
			eventAnnounced = true;
			return true;
		}
	}
	return false;
}

bool init_gps(void)
{
	int rc;
	if ((rc = gps_open("localhost", "2947", &my_gps_data)) == -1) {
		printf("code: %d, reason: %s\n", rc, gps_errstr(rc));
		return false;
	}
	gps_stream(&my_gps_data, WATCH_ENABLE | WATCH_JSON, NULL);
	return true;
}

void flush_gps_data(void)
{
	while (gps_read(&my_gps_data) > 0) {
		;
	}	
}

bool fetch_gps(void)
{
	if (gps_waiting(&my_gps_data, 1000000)) {
		int rc = gps_read(&my_gps_data);
		if (rc > 0) {
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
	}
	return false;
}

int main(int argc, char** argv)
{
	init();
	if (init_gps() == false) {
		return EXIT_FAILURE;
	}
	led_green(true);
	led_red(true);
	// set up lorawan
	if (!button() && join()) {
		// saved data is valid, we continue without any additional interaction
		led_green(false);
		led_red(false);
	} else {
		// communicate with network, wait as long as necessary
		while (! activate()) {
			sleep(SENT_EVERY_X_MINUTES * 60);
		}
	}
	led_green(false);
	led_red(false);
	set_parameter();
	flush_gps_data();
	printf("sucessfully initialized\n");
	while (1) {
		static int option = 0;
		static long gpsSecondNow = 0;
		static long lastDataSentSecond = 0;
		if (fetch_gps()) {
			bool sendDataNow = false;
			if (button_released()) {
				option = 1;
				sendDataNow = true;
				gpsSecondNow = (long)my_gps_data.fix.time;
			} else {
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
				led_red(false);
				led_green(true);
				if (loraSend(option)) {
					// be prepared for sudden poweroff event
					saveLoraParameter();
					sleep(10);
					led_green(false);
				} else {
					led_green(false);
					led_red(true);
					sleep(10);
					led_red(false);
				}
				printf("lat: %f, lon: %f, alt: %f, hdop: %f\n", my_gps_data.fix.latitude, my_gps_data.fix.longitude, my_gps_data.fix.altitude, my_gps_data.dop.hdop);
				lastDataSentSecond = gpsSecondNow;
			}
			delay(DELAY_100MS);
		} else {
			led_red(true);
			delay(DELAY_200MS);
			led_red(false);
			delay(DELAY_500MS);
		}
	}
	deinit();
	return EXIT_SUCCESS;
}
