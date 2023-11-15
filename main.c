// 
// IMU Visualizer
// Reads IMU tracking data over virtual serial port
// Displays position and orientation using a virtual object
//
// 2023, Jonathan Tainer
//

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <unistd.h>

#define BAUDRATE B38400
#define _POSIX_SOURCE 1 // POSIX compliant source

volatile int stop = 0;

int main(int argc, char** argv) {
	if (argc < 2) {
		printf("No serial port indicated\n");
		return 0;
	}

	const char* modem_dev = argv[1];
	int modem_fd = open(modem_dev, O_RDWR | O_NOCTTY);
	if (modem_fd < 0) {
		printf("Failed to open modem device: %s\n", modem_dev);
		return 1;
	}

	struct termios oldtio = { 0 }, newtio = { 0 };
	tcgetattr(modem_fd, &oldtio); // Save current serial port settings
	newtio.c_cflag = BAUDRATE | CRTSCTS | CS8 | CLOCAL | CREAD;
	newtio.c_iflag = IGNPAR | ICRNL;
	newtio.c_oflag = 0;
	newtio.c_lflag = ICANON;
	newtio.c_cc[VINTR]	= 0;	/* Ctrl-c */ 
	newtio.c_cc[VQUIT]	= 0;	/* Ctrl-\ */
	newtio.c_cc[VERASE]	= 0;	/* del */
	newtio.c_cc[VKILL]	= 0;	/* @ */
	newtio.c_cc[VEOF]	= 4;	/* Ctrl-d */
	newtio.c_cc[VTIME]	= 0;	/* inter-character timer unused */
	newtio.c_cc[VMIN]	= 1;	/* blocking read until 1 character arrives */
	newtio.c_cc[VSWTC]	= 0;	/* '\0' */
	newtio.c_cc[VSTART]	= 0;	/* Ctrl-q */ 
	newtio.c_cc[VSTOP]	= 0;	/* Ctrl-s */
	newtio.c_cc[VSUSP]	= 0;	/* Ctrl-z */
	newtio.c_cc[VEOL]	= 0;	/* '\0' */
	newtio.c_cc[VREPRINT]	= 0;	/* Ctrl-r */
	newtio.c_cc[VDISCARD]	= 0;	/* Ctrl-u */
	newtio.c_cc[VWERASE]	= 0;	/* Ctrl-w */
	newtio.c_cc[VLNEXT]	= 0;	/* Ctrl-v */
	newtio.c_cc[VEOL2]	= 0;	/* '\0' */

	// Clear modem line and activate new port settings
	tcflush(modem_fd, TCIFLUSH);
	tcsetattr(modem_fd, TCSANOW, &newtio);

	const int buflen = 1024;
	char buf[buflen];
	int res = 0;

	int count = 0;
	while (count++ < 100) {
		res = read(modem_fd, buf, buflen);
		buf[res] = 0;
		int x = 0, y = 0;
		int conv = sscanf(buf, "Ang.x = %d\t\tAng.y = %d", &x, &y);
//		printf("%s", buf);
		if (conv > 0)
			printf("x = %d\ty = %d\n", x, y);
		if (buf[0] == 'z') stop = 1;
	}

	// Restore old port settings
	tcsetattr(modem_fd, TCSANOW, &oldtio);

	close(modem_fd);

	return 0;
}
