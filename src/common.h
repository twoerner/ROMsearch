/*
 * Copyright (C) 2021  Trevor Woerner <twoerner@gmail.com>
 * SPDX-License-Identifier: OSL-3.0
 */

#ifndef ROM_SEARCH_COMMON__H
#define ROM_SEARCH_COMMON__H

#include <stdint.h>
#include <stdbool.h>

// device
typedef struct {
	uint8_t bits[64];
	size_t bitLen;
	bool done;
} DeviceID_t;

// fifos
#define toTesterFifoName_p "toTesterFifoFd"
#define fmTesterFifoName_p "fmTesterFifoFd"
int open_fifo (const char *name_p, int *fdOut_p);

// misc
void print_id (DeviceID_t *device_p, int maxbits);
void print_bits (uint64_t val, int startPos, int cnt);
int dwidth (int maxbits);

// single linked list
typedef struct _devicenode {
	struct _devicenode *next_p;
	DeviceID_t device;
} DeviceNode_t;
void free_nodes (DeviceNode_t *startNode_p);
DeviceNode_t *create_new_node (void);
DeviceNode_t *create_node_copy_device (DeviceID_t *deviceToCopy_p);
int add_node_to_list (DeviceNode_t *startNode_p, DeviceNode_t *nodeToAdd_p);

#endif
