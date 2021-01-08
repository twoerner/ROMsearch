/*
 * Copyright (C) 2021  Trevor Woerner <twoerner@gmail.com>
 * SPDX-License-Identifier: OSL-3.0
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <poll.h>
#include <unistd.h>
#include "common.h"

static int toTesterFifoFd_G, fmTesterFifoFd_G;
static DeviceNode_t *listHead_pG = NULL;

static int add_bit (DeviceID_t *device_p, uint8_t bit, bool send);
static int find_one_device (DeviceID_t *device_p);
static int get_next_digits (unsigned *digitsRet_p);

int
main (void)
{
	char sendCh;
	int ret, mainRet=1;
	DeviceNode_t *curNode_p;

	ret = open_fifo(toTesterFifoName_p, &toTesterFifoFd_G);
	if (ret != 0) {
		perror("mkfifo to tester");
		return 1;
	}
	ret = open_fifo(fmTesterFifoName_p, &fmTesterFifoFd_G);
	if (ret != 0) {
		perror ("mkfifo fm tester");
		goto closeToTesterFifo;
	}

	listHead_pG = create_new_node();
	if (listHead_pG == NULL) {
		printf("failed to create head node\n");
		goto exitFail;
	}

	// find_one_device() might add more nodes as it encounters 00
	// therefore search the whole list from the start until no unfinished
	// nodes remain
	while (1) {
		curNode_p = listHead_pG;
		while (1) {
			if (curNode_p == NULL)
				break;
			if (!(curNode_p->device.done))
				break;
			curNode_p = curNode_p->next_p;
		}
		if (curNode_p == NULL)
			break;

		ret = find_one_device(&(curNode_p->device));
		if (ret != 0) {
			printf("failure in find_one_device\n");
			goto cleanupNodes;
		}
		print_id(&(curNode_p->device), (int)curNode_p->device.bitLen);
		printf("\n");
	}

	mainRet = 0;
cleanupNodes:
	free_nodes(listHead_pG);
exitFail:
	sendCh = 'Q';
	write(toTesterFifoFd_G, (void*)&sendCh, 1);
	close(fmTesterFifoFd_G);
	unlink(fmTesterFifoName_p);
closeToTesterFifo:
	close(toTesterFifoFd_G);
	unlink(toTesterFifoName_p);
	return mainRet;
}

/**
 * optionally add the specified bit to the given (non-NULL) device
 * optionally send this bit to the tester
 * bit is specified as a character: '0' or '1'
 *
 * the bits are given to us LSB first, but we put the first bit in bits[0]
 * therefore the bits array stores the bits backwards
 *
 * return:
 *  0: ok
 * -1: failure
 */
static int
add_bit (DeviceID_t *device_p, uint8_t bit, bool send)
{
	/* preconds */
	if ((bit != '0') && (bit != '1'))
		return -1;

	if (device_p != NULL) {
		if (device_p->bitLen < sizeof(device_p->bits))
			device_p->bits[device_p->bitLen++] = bit;
		else {
			printf("bitfield full\n");
			return -1;
		}
	}
	if (send)
		write(toTesterFifoFd_G, (void*)&bit, sizeof(bit));

	return 0;
}

/**
 * interrogate the testing device over the fifo to find 1 device/serial number
 * (see the "Example of a ROM Search" section of the datasheet for the DS18B20
 * for an explanation of the algorithm)
 *
 * if a fork is found along the way, create a new node that is a copy of
 * the current device up to this point with "the other path" already added
 * to the ID of the new device
 */
static int
find_one_device (DeviceID_t *device_p)
{
	int ret;
	char sendCh;
	unsigned nextDigits;
	DeviceNode_t *newNode_p;

	/* preconds */
	if (device_p == NULL)
		return -1;

	// send reset
	sendCh = 'R';
	write(toTesterFifoFd_G, (void*)&sendCh, 1);

	// send ROM search command
	sendCh = 'S';
	write(toTesterFifoFd_G, (void*)&sendCh, 1);

	// check if this is an already-started ID
	// if it is, twiddle the tester with the current bits up to now so
	// that all devices are at the correct state before continuing
	if (device_p->bitLen > 0) {
		size_t i;

		for (i=0; i<device_p->bitLen; ++i) {
			ret = get_next_digits(&nextDigits);
			if (ret != 0)
				return -1;
			ret = add_bit(NULL, device_p->bits[i], true);
			if (ret != 0)
				return -1;
		}
	}

	while (!(device_p->done)) {
		ret = get_next_digits(&nextDigits);
		if (ret != 0) {
			printf("error fetching next digits\n");
			return -1;
		}
		switch (nextDigits) {
			case 0: // 00
				// send someone off to do the '1' case
				newNode_p = create_node_copy_device(device_p);
				if (newNode_p != NULL) {
					add_bit(&(newNode_p->device), '1', false);
					ret = add_node_to_list(listHead_pG, newNode_p);
					if (ret != 0)
						free_nodes(newNode_p);
				}

				// we'll do the '0' case here
				ret = add_bit(device_p, '0', true);
				if (ret != 0)
					return ret;
				break;

			case 1: // 01
				ret = add_bit(device_p, '0', true);
				if (ret != 0)
					return ret;
				break;

			case 2: // 10
				ret = add_bit(device_p, '1', true);
				if (ret != 0)
					return ret;
				break;

			case 3: // 11
				device_p->done = true;
				break;

			default:
				printf("unhandled reply from tester: %c (0x%02x)\n", nextDigits, nextDigits);
				return -1;
		}
	}

	return 0;
}

/**
 * perform 2 "reads" on the "device" (i.e. send two 'r' commands down the
 * fifo) to obtain the all the current bits of all devices and the complement
 * of all the current bits of all devices
 *
 * the first received bit is the MSB
 *
 * the return value indicates error:
 * 0  → okay
 * -1 → error
 *
 * the digitsRet_p is set to the value from the tester:
 * 0b00
 * 0b01
 * 0b10
 * 0b11
 */
static int
get_next_digits (unsigned *digitsRet_p)
{
	int i, ret;
	char sendCh, recvCh;
	struct pollfd pollFd[1];
	ssize_t retRead;

	/* preconds */
	if (digitsRet_p == NULL)
		return -1;

	pollFd[0].fd = fmTesterFifoFd_G;
	pollFd[0].events = POLLIN;

	*digitsRet_p = 0;
	for (i=1; i>-1; --i) {
		// send 'r'
		sendCh = 'r';
		write(toTesterFifoFd_G, (void*)&sendCh, 1);

		ret = poll(pollFd, 1, -1);
		if ((ret == 1) && (pollFd[0].revents == POLLIN)) {
			retRead = read(pollFd[0].fd, &recvCh, sizeof(recvCh));
			if (retRead <= 0)
				return -1;
			switch (recvCh) {
				case '0':
					break;
				case '1':
					*digitsRet_p += (unsigned)(1 << i);
					break;
				default:
					return 0;
			}

		}
		else {
			return -1;
		}
	}

	return 0;
}
