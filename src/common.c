/*
 * Copyright (C) 2021  Trevor Woerner <twoerner@gmail.com>
 * SPDX-License-Identifier: OSL-3.0
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "common.h"

// fifos
int
open_fifo (const char *name_p, int *fdOut_p)
{
        mode_t fifoMode;
        int flags;
        int ret;

        /* preconds */
        if (name_p == NULL)
        	return -1;
        if (fdOut_p == NULL)
                return -1; 

        // create the fifo
        fifoMode = S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH;
        ret = mkfifo (name_p, fifoMode);
        if (ret != 0) {
                if (errno != EEXIST) {
                        return -1; 
                }
        }

        // open the fifo for read/write
        flags = O_RDWR | O_NONBLOCK;
        *fdOut_p = open (name_p, flags);
        if (*fdOut_p == -1) {
                return -1; 
        }

        return 0;
}

// misc
/**
 * NOTE: the bits are stored in the array backwards
 * since they are given to us LSB first
 * i.e. LSB is in bits[0]
 */
void
print_id (DeviceID_t *device_p, int maxbits)
{
	int i;
	size_t j;
	size_t pos;
	uint64_t val = 0;

	/* preconds */
	if (device_p == NULL)
		return;
	if (maxbits <= 0)
		return;
	if (maxbits > (int)device_p->bitLen)
		return;

	// reverse-print bits
	for (i=0,pos=device_p->bitLen; i<(int)device_p->bitLen; ++i,--pos)
		if (device_p->bits[pos] == '1')
			printf("1");
		else
			printf("0");
	printf("...");

	// convert and print value
	for (j=0; j<device_p->bitLen; ++j)
		val += ((uint64_t)(device_p->bits[j] - 0x30) << j);
	printf("%0*"PRIu64, dwidth(maxbits), val);
}

void
print_bits (uint64_t val, int startPos, int cnt)
{
	int i;
	int pos;

	/* preconds */
	if ((startPos - cnt + 1) < 0)
		return;
	if (startPos < 0)
		return;
	if (cnt <= 0)
		return;

	for (i=0,pos=startPos; i<cnt; ++i,--pos) {
		if (val & (uint64_t)(1llu<<pos))
			printf("1");
		else
			printf("0");
	}
}

/**
 * given a width in bits
 * translate to the max width for decimal digits
 */
int 
dwidth (int maxbits)
{
	/* preconds */
	if (maxbits <= 0)
		return 0;

	if (maxbits < 5)  // 0xf
		return 2;
	if (maxbits < 9)  // 0xff
		return 3;
	if (maxbits < 13) // 0xfff
		return 4;
	if (maxbits < 17) // 0xffff
		return 5;
	if (maxbits < 21) // 0xffff f
		return 7;
	if (maxbits < 25) // 0xffff ff
		return 8;
	if (maxbits < 29) // 0xffff fff
		return 9;
	if (maxbits < 33) // 0xffff ffff
		return 10;
	if (maxbits < 37)
		return 11;
	if (maxbits < 41)
		return 13;
	if (maxbits < 45)
		return 14;
	if (maxbits < 49)
		return 15;
	if (maxbits < 53)
		return 16;
	if (maxbits < 57)
		return 17;
	if (maxbits < 61)
		return 19;
	return 20;

}

// single linked list
void
free_nodes (DeviceNode_t *startNode_p)
{
	DeviceNode_t *next_p;

	/* preconds */
	if (startNode_p == NULL)
		return;

	next_p = startNode_p->next_p;
	free(startNode_p);

	if (next_p != NULL)
		free_nodes(next_p);
}

DeviceNode_t *
create_new_node (void)
{
	DeviceNode_t *newNode_p;

	/* preconds */
	// none

	// node
	newNode_p = (DeviceNode_t*)malloc(sizeof(DeviceNode_t));
	if (newNode_p == NULL) {
		perror("malloc");
		return NULL;
	}

	// device
	newNode_p->device.bitLen = 0;
	newNode_p->device.done = false;

	// list
	newNode_p->next_p = NULL;

	return newNode_p;
}

DeviceNode_t *
create_node_copy_device (DeviceID_t *deviceToCopy_p)
{
	DeviceNode_t *newNode_p;

	/* preconds */
	if (deviceToCopy_p == NULL)
		return NULL;

	newNode_p = create_new_node();
	if (newNode_p == NULL)
		return NULL;

	newNode_p->device.bitLen = deviceToCopy_p->bitLen;
	if (deviceToCopy_p->bitLen > 0)
		memcpy(&(newNode_p->device.bits), &(deviceToCopy_p->bits), deviceToCopy_p->bitLen);
	newNode_p->device.done = deviceToCopy_p->done;

	return newNode_p;
}

int
add_node_to_list (DeviceNode_t *startNode_p, DeviceNode_t *nodeToAdd_p)
{
	DeviceNode_t *curNode_p;

	/* preconds */
	if (startNode_p == NULL)
		return -1;
	if (nodeToAdd_p == NULL)
		return -1;

	curNode_p = startNode_p;
	while (curNode_p->next_p != NULL)
		curNode_p = curNode_p->next_p;
	curNode_p->next_p = nodeToAdd_p;

	return 0;
}
