/*
 *  A library for Linux serial communication
 *
 *  Author: JoStudio, 2016
 *
 *  Version: 0.5
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "serial.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <termios.h>
#include <sys/select.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

#ifndef CMSPAR
#define CMSPAR   010000000000
#endif



/*
 * convert baud_rate to speed_t
 */
static speed_t  get_speed(unsigned int baud_rate)
{
    switch (baud_rate) {
        case 0:
            return B0;
        case 50:
            return B50;
        case 75:
            return B75;
        case 110:
            return B110;
        case 150:
            return B150;
        case 200:
            return B200;
        case 300:
            return B300;
        case 600:
            return B600;
        case 1200:
            return B1200;
        case 1800:
            return B1800;
        case 2400:
            return B2400;
        case 4800:
            return B4800;
        case 9600:
            return B9600;
        case 19200:
            return B19200;
        case 38400:
            return B38400;
        case 57600:
            return B57600;
        case 115200:
            return B115200;
        case 230400:
            return B230400;
        case 460800:
            return B460800;
        case 500000:
            return B500000;
        case 576000:
            return B576000;
        case 921600:
            return B921600;
        case 1000000:
            return B1000000;
        case 1152000:
            return B1152000;
        case 1500000:
            return B1500000;
        case 2000000:
            return B2000000;
        case 2500000:
            return B2500000;
        case 3000000:
            return B3000000;
        case 3500000:
            return B3500000;
        case 4000000:
            return B4000000;
        default:
            //unsupported baud rate
            return 0;
    }
}


/**
 * set baud rate of  serial port
 *
 * @param file_descriptor file descriptor of serial device
 * @param baud_rate baud rate
 *
 * @return 1 if success
 *  return negative error code if fail
 */
int serial_set_baud_rate(int file_descriptor, int baud_rate) {
	struct termios termio;
	speed_t speed;
	int fd = file_descriptor;

	if ( fd < 0 ) {
		return SERIAL_INVALID_FILE;
	}

	memset(&termio, 0, sizeof(termio));

	//get old attribute
	if (tcgetattr(fd, &termio)) {
		return SERIAL_INVALID_RESOURCE;
	}

	//calculate baud rate
	speed = get_speed(baud_rate);
	if (speed == 0) {
		return SERIAL_ERROR_BAUDRATE;
	}
	cfsetispeed(&termio, speed);
	cfsetospeed(&termio, speed);

	// set baud rate
	if (tcsetattr(fd, TCSAFLUSH, &termio) < 0) {
		return SERIAL_ERROR_BAUDRATE;
	}

	return 1;
}


/**
 * set serial attributes
 *
 * @param file_descriptor file descriptor of serial device
 * @param flow_ctrl
 * @param data_bits how many bits per data, could be 7, 8, 5
 * @param parity which parity method, could be PARITY_NONE, PARITY_ODD, PARITY_EVEN
 * @param stop_bits  how many stop bits
 *
 * @return 1 if success.
 *  return negative error code if fail.
 */
int serial_set_attr(int file_descriptor, int data_bits, char parity, int stop_bits, int flow_ctrl)
{
    struct termios termio;
    int fd = file_descriptor;

    if ( fd < 0 ) {
		return SERIAL_INVALID_FILE;
	}

    memset(&termio, 0, sizeof(termio));

    if (tcgetattr(fd, &termio)) {
        return SERIAL_INVALID_RESOURCE;
    }

    //set flow control
	switch(flow_ctrl) {
	   case FLOW_CONTROL_NONE :
		   termio.c_cflag &= ~CRTSCTS;
		   break;
	   case FLOW_CONTROL_HARDWARE :
		   termio.c_cflag |= CRTSCTS;
		   break;
	   case FLOW_CONTROL_SOFTWARE :
		   termio.c_cflag |= IXON | IXOFF | IXANY;
		   break;
	}

    //set data bit
    termio.c_cflag &= ~CSIZE;
    switch (data_bits) {
        case 8:
            termio.c_cflag |= CS8;
            break;
        case 7:
            termio.c_cflag |= CS7;
            break;
        case 6:
            termio.c_cflag |= CS6;
            break;
        case 5:
            termio.c_cflag |= CS5;
            break;
        default:
            termio.c_cflag |= CS8;
            break;
    }

    //set stop bits
    switch (stop_bits) {
        case 1:
            termio.c_cflag &= ~CSTOPB;
            break;
        case 2:
            termio.c_cflag |= CSTOPB;
            break;
        default:
            break;
    }

    //set parity bit
    switch (parity) {
        case PARITY_NONE:
            termio.c_cflag &= ~(PARENB | PARODD);
            break;
        case PARITY_EVEN:
            termio.c_cflag |= PARENB;
            termio.c_cflag &= ~PARODD;
            break;
        case PARITY_ODD:
            termio.c_cflag |= PARENB | PARODD;
            break;
        case PARITY_MARK:
            termio.c_cflag |= PARENB | CMSPAR | PARODD;
            break;
        case PARITY_SPACE:
            termio.c_cflag |= PARENB | CMSPAR;
            termio.c_cflag &= ~PARODD;
            break;
    }

    if (tcsetattr(fd, TCSAFLUSH, &termio) < 0) {
        return SERIAL_ERROR_SETTING;
    }

    return 1;
}


/**
 * open serial port
 *
 * @param device_filename device file name, such as "/dev/ttyS0"
 * @param baund_rate could be 57600, 38400,  19200,  9600,  4800,  2400,  1200,  300
 *
 * @return file descriptor which could be used in read() or write()
 * return -1 if failed, and errno is set
 */
int serial_open_file(char *device_filename, int baud_rate) {
	int fd;
	struct termios termio;

	if ((fd = open(device_filename, O_RDWR  | O_NOCTTY )) == -1) {
	   return -1;
	}

	memset(&termio, 0, sizeof(termio));

	// setup the tty and the selected baud rate
	// get current modes
	if (tcgetattr(fd, &termio)) {
		close(fd);
		return -1;
	}

	// setup initial mode:   8 bit data, No parity, 1 stop bit, no echo, no flow control
	cfmakeraw(&termio);
	if (tcsetattr(fd, TCSAFLUSH, &termio) < 0) {
		close(fd);
		return -1;
	}

	if (serial_set_baud_rate(fd, baud_rate) != 1) {
		close(fd);
		return -1;
	}

	return fd;
}

/**
 * open serial port
 *
 * @param file_descriptor file descriptor of serial device
 * @param timeout timeout in second
 *
 * @return 1 if success
 *  return negative error code if fail
 */
int serial_set_timeout(int file_descriptor, int timeout)
{
	int fd = file_descriptor;
	struct termios termio;

	if (fd < 0) {
	   return SERIAL_INVALID_FILE;
	}


    // get current attributes
    if (tcgetattr(fd, &termio)) {
        return SERIAL_INVALID_RESOURCE;
    }

    //set read timeout
    if (timeout > 0) {
    	timeout = timeout / 100;
        if (timeout == 0)
        	timeout = 1;
    }
    termio.c_lflag &= ~(ICANON|ECHO|ECHOE|ISIG); /* Set non-canonical mode */
	termio.c_oflag &= ~(OPOST|ONLCR|OCRNL);
	termio.c_iflag &= ~(INLCR|ICRNL|IGNCR);
	
    termio.c_cc[VTIME] = timeout; /* Set timeout in tenth seconds */
    if (tcsetattr(fd, TCSANOW, &termio) < 0) {
        return SERIAL_ERROR_SETTING;
    }

    return 1;
}


/**
 * open serial port
 *
 * @param port number of port, could be 0, 1 ... , the device file is /dev/ttyS0, /dev/ttyS1 respectively
 * @param baund_rate could be 57600, 38400,  19200,  9600,  4800,  2400,  1200,  300
 *
 * @return file descriptor which could be used in read() or write()
 * return -1 if failed, and errno is set
 */
int serial_open(int port, int baud_rate) {
	char buf[128];

	snprintf(buf, sizeof(buf), "/dev/ttyS%d", port);
	return serial_open_file(buf, baud_rate);
}


/**
 * close serial port
 *
 * @param file_descriptor file descriptor of serial port device file
 *
 * @return 0 if success.
 *  return -1 if failed, and errno is set.
 */
int serial_close(int file_descriptor) {
	return close(file_descriptor);
}


/**
 * The function waits until all queued output to the terminal filedes has been transmitted.
 *
 * tcdrain() is called.

 * @param file_descriptor file descriptor of serial port device file
 *
 * @return 0 if success.
 *  return -1 if fail, and errno is set.
 */
int serial_flush(int file_descriptor)
{
    return tcdrain(file_descriptor);
}

/**
 * whether data is available
 *
 * @param file_descriptor file descriptor of serial port device file
 * @param timeout_millisec timeout in millisecond
 *
 * @return 1 if data is available
 *  return 0 if data is not available
 */
int serial_data_available(int file_descriptor, unsigned int timeout_millisec)
{
	int fd = file_descriptor;
	struct timeval timeout;
	fd_set readfds;

    if (fd < 0) {
        return SERIAL_INVALID_FILE;
    }

    if (timeout_millisec == 0) {
    	// no waiting
        timeout.tv_sec = 0;
        timeout.tv_usec = 0;
    } else {
        timeout.tv_sec = timeout_millisec / 1000;
        timeout.tv_usec = (timeout_millisec % 1000) * 1000;
    }

    FD_ZERO(&readfds);
    FD_SET(fd, &readfds);

    if (select(fd + 1, &readfds, NULL, NULL, &timeout) > 0) {
        return 1; // data is ready
    } else {
        return 0; // no data
    }
}


/**
 * send data
 *
 * @param file_descriptor file descriptor of serial port device file
 * @param buffer   buffer of data to send
 * @param data_len data length
 *
 * @return positive integer of bytes sent if success.
 *  return -1 if failed.
 */
int serial_send(int file_descriptor, char *buffer, size_t data_len)
{
    ssize_t len = 0;

    len = write(file_descriptor, buffer, data_len);
    if ( len == data_len ) {
    	return len;
    } else {
		tcflush(file_descriptor, TCOFLUSH);
		return -1;
    }

}

/**
 * receive data
 *
 * @param file_descriptor file descriptor of serial port device file
 * @param buffer   buffer to receive data
 * @param data_len max data length
 *
 * @return positive integer of bytes received if success.
 *  return -1 if failed.
 */
int serial_receive(int file_descriptor, char *buffer,size_t data_len)
{
    int len,fs_sel;
    fd_set fs_read;
    struct timeval time;
    int fd = file_descriptor;

    FD_ZERO(&fs_read);
    FD_SET(fd, &fs_read);

    time.tv_sec = 10;
    time.tv_usec = 0;

    //use select() to achieve multiple channel communication
    fs_sel = select(fd + 1, &fs_read, NULL, NULL, &time);
    if ( fs_sel ) {
    	len = read(fd, buffer, data_len);
        return len;
    } else {
    	return -1;
    }
}


