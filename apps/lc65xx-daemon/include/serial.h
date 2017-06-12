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

#ifndef __SERIAL_H__
#define __SERIAL_H__


#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>

/* serial error code */
#define SERIAL_ERROR_OPEN        -1
#define SERIAL_ERROR_READ        -2
#define SERIAL_ERROR_WRITE       -3
#define SERIAL_ERROR_BAUDRATE    -4
#define SERIAL_ERROR_SETTING     -5
#define SERIAL_INVALID_RESOURCE  -6
#define SERIAL_INVALID_FILE      -7


/* flow control */
#define FLOW_CONTROL_NONE         0
#define FLOW_CONTROL_HARDWARE     1
#define FLOW_CONTROL_SOFTWARE     2

/* parity */
#define PARITY_NONE  'N'
#define PARITY_EVEN  'E'
#define PARITY_ODD   'O'
#define PARITY_SPACE 'S'
#define PARITY_MARK  'M'


/**
 * open serial port
 *
 * @param port number of port, could be 0, 1 ... , the device file is /etc/ttyS0, /etc/ttyS1 respectively
 * @param baund_rate could be 57600, 38400,  19200,  9600,  4800,  2400,  1200,  300
 *
 * @return file descriptor which could be used in read() or write()
 * return -1 if failed, and errno is set
 */
int serial_open(int port, int baud_rate);


/**
 * close serial port
 *
 * @param file_descriptor file descriptor of serial port device file
 *
 * @return 0 if success.
 *  return -1 if failed, and errno is set.
 */
int serial_close(int file_descriptor);

/**
 * open serial port
 *
 * @param device_filename device file name, such as "/etc/ttyS0"
 * @param baund_rate could be 57600, 38400,  19200,  9600,  4800,  2400,  1200,  300
 *
 * @return file descriptor which could be used in read() or write()
 * return -1 if failed, and errno is set
 */
int serial_open_file(char *device_filename, int baud_rate);


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
int serial_set_attr(int file_descriptor, int data_bits, char parity, int stop_bits, int flow_ctrl);


/**
 * open serial port
 *
 * @param file_descriptor file descriptor of serial device
 * @param timeout timeout in second
 *
 * @return 1 if success
 *  return negative error code if fail
 */
int serial_set_timeout(int file_descriptor, int timeout);


/**
 * set baud rate of  serial port
 *
 * @param file_descriptor file descriptor of serial device
 * @param baud_rate baud rate
 *
 * @return 1 if success
 *  return negative error code if fail
 */
int serial_set_baud_rate(int file_descriptor, int baud_rate) ;




/**
 * The function waits until all queued output to the terminal filedes has been transmitted.
 *
 * tcdrain() is called.

 * @param file_descriptor file descriptor of serial port device file
 *
 * @return 0 if success.
 *  return -1 if fail, and errno is set.
 */
int serial_flush(int file_descriptor);

/**
 * whether data is available
 *
 * @param file_descriptor file descriptor of serial port device file
 * @param timeout_millisec timeout in millisecond
 *
 * @return 1 if data is available
 *  return 0 if data is not available
 */
int serial_data_available(int file_descriptor, unsigned int timeout_millisec);


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
int serial_send(int file_descriptor, char *buffer, size_t data_len);



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
int serial_receive(int file_descriptor, char *buffer,size_t data_len);



#ifdef __cplusplus
}
#endif


#endif /* __SERIAL_H__ */
