/*  Copyright 2003-2013 Ciaran Anscomb
 *
 *  This file is part of XRoar.
 *
 *  XRoar is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  XRoar is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XRoar.  If not, see <http://www.gnu.org/licenses/>.
 */

/* Support the so-called "becker port", an IP version of the usually-serial
 * DriveWire protocol. */

#define _POSIX_C_SOURCE 200112L

#include "config.h"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef WINDOWS32
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#else
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#endif

#include "becker.h"
#include "logging.h"
#include "xroar.h"

/* In theory no reponse should be longer than this (though it doesn't actually
 * matter, this only constrains how much is read at a time). */
#define INPUT_BUFFER_SIZE 262
#define OUTPUT_BUFFER_SIZE 16

static int sockfd = -1;
static char input_buf[INPUT_BUFFER_SIZE];
static int input_buf_ptr = 0;
static int input_buf_length = 0;
static char output_buf[OUTPUT_BUFFER_SIZE];
static int output_buf_ptr = 0;
static int output_buf_length = 0;

_Bool becker_open(void) {

#ifdef WINDOWS32
	// Windows needs this to load the appropriate DLL, it seems
	WORD wVersionRequested;
	WSADATA wsaData;
	wVersionRequested = MAKEWORD(2, 2);
	if (WSAStartup(wVersionRequested, &wsaData) != 0) {
		LOG_WARN("becker: WSAStartup failed\n");
		return 0;
	}
#endif

	struct addrinfo hints, *info;
	char *hostname = xroar_opt_becker_ip ? xroar_opt_becker_ip : "localhost";
	char *portname = xroar_opt_becker_port ? xroar_opt_becker_port : "65504";

	// Find the server
	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_family = AF_UNSPEC;
	if (getaddrinfo(hostname, portname, &hints, &info) < 0) {
		LOG_WARN("becker: getaddrinfo %s:%s failed\n", hostname, portname);
		goto failed;
	}

	// Create a socket...
	sockfd = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
	if (sockfd < 0) {
		LOG_WARN("becker: socket not created\n");
		goto failed;
	}

	// ... and connect it to the requested server
	if (connect(sockfd, info->ai_addr, info->ai_addrlen) < 0) {
		LOG_WARN("becker: connect %s:%s failed\n", hostname, portname);
		goto failed;
	}

	// Set the socket to non-blocking
#ifndef WINDOWS32
	int flags = fcntl(sockfd, F_GETFL, 0);
	fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
#else
	u_long iMode = 1;
	if (ioctlsocket(sockfd, FIONBIO, &iMode) != NO_ERROR) {
		LOG_WARN("becker: couldn't set non-blocking mode on socket\n");
		goto failed;
	}
#endif

	return 1;

failed:
	if (sockfd != -1) {
		close(sockfd);
		sockfd = -1;
	}
#ifdef WINDOWS32
	WSACleanup();
#endif
	return 0;
}

void becker_close(void) {
	close(sockfd);
	sockfd = -1;
}

static void fetch_input(void) {
	if (input_buf_ptr == 0) {
		ssize_t new = recv(sockfd, input_buf, INPUT_BUFFER_SIZE, 0);
		if (new > 0) {
			input_buf_length = new;
		}
	}
}

static void write_output(void) {
	if (output_buf_length > 0) {
		ssize_t sent = send(sockfd, output_buf + output_buf_ptr, output_buf_length - output_buf_ptr, 0);
		if (sent > 0) {
			output_buf_ptr += sent;
			if (output_buf_ptr >= output_buf_length) {
				output_buf_ptr = output_buf_length = 0;
			}
		}
	}
}

uint8_t becker_read_status(void) {
	fetch_input();
	if (input_buf_length > 0)
		return 0x02;
	return 0x00;
}

uint8_t becker_read_data(void) {
	fetch_input();
	if (input_buf_length == 0)
		return 0x00;
	uint8_t r = input_buf[input_buf_ptr++];
	if (input_buf_ptr == input_buf_length) {
		input_buf_ptr = input_buf_length = 0;
	}
	return r;
}

void becker_write_data(uint8_t D) {
	if (output_buf_length < OUTPUT_BUFFER_SIZE) {
		output_buf[output_buf_length++] = D;
	}
	write_output();
}
