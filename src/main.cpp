/*
 * Copyright (c) 2016 Intel Corporation.
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

#include <iostream>
#include<stdio.h> //printf
#include<string.h> //memset
#include<stdlib.h> //exit(0);
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h> //close(s);
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/mapped_region.hpp>

#include "./../include/json.hpp"

#define BUFLEN 512  //Max length of buffer
#define PORT 4444   //The port on which to listen for incoming data

using json = nlohmann::json;

int nSocket = -1;
char buffer[BUFLEN];
char buffer_old[BUFLEN];
boost::interprocess::mapped_region* regionRX;
boost::interprocess::mapped_region* regionTX;

void _SIG_HANDLER_ (int sig) {
	std::cout << "[UDP RUNTIME] Interrupt signal received " << sig << std::endl;

	if(nSocket != -1) {
		std::cout << "Closing socket..." << std::endl;
		close(nSocket);
	}

	std::cout << "Clearing buffer... " << std::endl;
	std::memset(buffer, '\0', BUFLEN); // clear buffer

	std::cout << "Clearing buffer copy..." << std::endl;
	std::memset(buffer_old, '\0', BUFLEN); // clear buffer

	if(regionRX) {
		std::cout << "Memset '0' for SHARED_RX_MEM..." << std::endl;
		std::memset(regionRX->get_address(), '\0', regionRX->get_size());
		delete regionRX;
	}

	if(regionTX) {
		std::cout << "Memset '0' for SHARED_TX_MEM..." << std::endl;
		std::memset(regionTX->get_address(), '\0', regionTX->get_size());
		delete regionTX;
	}

	exit(sig);
}
 
void die(char *s)
{
    perror(s);
    exit(1);
}

int main(void)
{
	std::cout << "Starting UDP Server..." << std::endl;
    struct sockaddr_in si_me, si_other;

    int recv_len, send_len;
    socklen_t slen = sizeof(si_other);
    send_len = BUFLEN; // TX messages have same length as buffer

    //create a UDP socket
    if ((nSocket=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    {
        std::cout << "Failed to create UDP Socket";
        exit(-1);
    }

    // zero out the structure
    memset((char *) &si_me, 0, sizeof(si_me));

    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(PORT);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);

    //bind socket to port
    if( bind(nSocket , (struct sockaddr*)&si_me, sizeof(si_me) ) == -1)
    {
    	std::cout << "Failed to bind socket to port";
    	exit(-2);
    }

	// create and clear space for shared memory - RECIEVER
	boost::interprocess::shared_memory_object sharedMemory_UdpRX(
			boost::interprocess::open_or_create,
			"shared_mem_udp_RX",
			boost::interprocess::read_write
	);
	sharedMemory_UdpRX.truncate(BUFLEN);
	regionRX = new boost::interprocess::mapped_region(sharedMemory_UdpRX, boost::interprocess::read_write);
	std::memset(regionRX->get_address(), '\0', regionRX->get_size());

	// create and clear space for shared memory - SENDER
	boost::interprocess::shared_memory_object sharedMemory_UdpTX(
			boost::interprocess::open_or_create,
			"shared_mem_udp_TX",
			boost::interprocess::read_write
	);
	sharedMemory_UdpTX.truncate(BUFLEN);
	regionTX = new boost::interprocess::mapped_region(sharedMemory_UdpTX, boost::interprocess::read_write);
	std::memset(regionTX->get_address(), '\0', regionTX->get_size());

	std::cout << "Waiting for data..." << std::endl;
    //keep listening for data

    while(1)
    {
        fflush(stdout);
        std::memset(buffer, '\0', BUFLEN); // clear buffer
        //try to receive some data, this is a blocking call
        if ((recv_len = recvfrom(nSocket, buffer, BUFLEN, 0, (struct sockaddr *) &si_other, &slen)) == -1)
        {
        	std::cout << "Failed to receive from socket";
        	exit(-1);
        }

        if(strcmp(buffer_old, buffer) != 0) {
        	std::cout << "Received: " << buffer << std::endl;
        	strncpy(buffer_old, buffer, BUFLEN);
            strncpy((char*)regionRX->get_address(), buffer, BUFLEN); //copy RX buffered message to shared mem
            std::memset(buffer, '\0', BUFLEN); // clear buffer
        }

        strncpy(buffer, (char*)regionTX->get_address(), BUFLEN); //copy shared memory TX message to buffer

        //now reply the client with the same data
        if (sendto(nSocket, buffer, send_len, 0, (struct sockaddr*) &si_other, slen) == -1)
        {
        	std::cout << "Failed to send through socket";
        	exit(-1);
        }
    }

    close(nSocket);
    return 0;
}
