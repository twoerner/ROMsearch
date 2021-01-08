/*
 * Copyright (C) 2021  Trevor Woerner <twoerner@gmail.com>
 * SPDX-License-Identifier: OSL-3.0
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h>
#include <getopt.h>
#include <string.h>
#include <signal.h>
#include <setjmp.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "common.h"
#include "config.h"

typedef enum {
	ROMsearch,
	ROMnone,
} RomFunction_e;
typedef struct {
	uint64_t deviceID;
	bool inSearch;
} Devices_t;
static Devices_t *devices_pG;
static int numEntries_G;
static bool verbose_G = false;
#define DEFAULT_BIT_SIZE 8
static int bitSize_G = DEFAULT_BIT_SIZE;
#define DEFAULT_MAX_ENTRIES 8;
static int maxEntries_G = DEFAULT_MAX_ENTRIES;
static jmp_buf env_G;
static RomFunction_e function_G = ROMnone;

static int process_cmdline_args (int argc, char *argv[]);
static void setup_signal_handler (void);

int
main (int argc, char *argv[])
{
	int i;
	int ret;
	int toTesterFifoFd;
	int fmTesterFifoFd;
	struct pollfd pollFd[1];
	ssize_t retRead;
	volatile bool runLoop;
	char readBuf;
	volatile int bitPos;
	volatile int readState;
	int ANDbit;
	char writeBuf;
	volatile bool firstRun;
	uint64_t currentBit;

	ret = process_cmdline_args(argc, argv);
	if (ret != 0)
		return 1;
	printf("devices: %d\n", numEntries_G);
	printf("bitsize: %d", bitSize_G);

	ret = open_fifo(toTesterFifoName_p, &toTesterFifoFd);
	if (ret != 0) {
		perror("mkfifo to tester");
		goto fifoFail;
	}
	ret = open_fifo(fmTesterFifoName_p, &fmTesterFifoFd);
	if (ret != 0) {
		perror("mkfifo fm tester");
		goto closeToFifo;
	}

	pollFd[0].fd = toTesterFifoFd;
	pollFd[0].events = POLLIN;

	bitPos = 0;
	readState = 0;
	firstRun = true;
	runLoop = true;

	setup_signal_handler();
	if (setjmp(env_G) != 0)
		runLoop = false;

	while (runLoop) {
		if (verbose_G || firstRun) {
			firstRun = false;
			printf("\n");
			for (i=0; i<numEntries_G; ++i) {
				if (devices_pG[i].inSearch) {
					printf("devices_pG[%02d] = %0*"PRIu64" (0b", i, dwidth(bitSize_G), devices_pG[i].deviceID);
					print_bits(devices_pG[i].deviceID, bitSize_G-1, bitSize_G);
					printf(")  current bit pos:%02d → ", bitPos);
					print_bits(devices_pG[i].deviceID, bitPos, 1);
					printf("\n");
				}
			}
		}

		ret = poll(pollFd, 1, -1);
		if ((ret != 1) || (pollFd[0].revents != POLLIN))
			continue;

		retRead = read(pollFd[0].fd, &readBuf, sizeof(readBuf));
		if (retRead == -1) {
			perror("read fifo");
			goto allocFail;
		}
		if (retRead == 0) {
			runLoop = false;
			continue;
		}
		if (verbose_G)
			printf("fifo: 0x%02x (%c) cnt:%zd bitPos:%d\n", readBuf, readBuf, retRead, bitPos);

		switch (readBuf) {
			case 'Q': // quit
				runLoop = false;
				break;

			case 'R': // reset
				function_G = ROMnone;
				readState = 0;
				bitPos = 0;
				for (i=0; i<numEntries_G; ++i)
					devices_pG[i].inSearch = true;
				break;

			case 'S': // ROM search function
				function_G = ROMsearch;
				break;

			case 'V': // verbose
				verbose_G = !verbose_G;
				break;

			case 'r': // read
				if (function_G != ROMsearch)
					break;
				if ((readState != 0) && (readState != 1))
					break;

				if (verbose_G)
					printf(" readState:%d bitPos:%d\n", readState, bitPos);

				ANDbit = 1; // the default is pull-up
				for (i=0; i<numEntries_G; ++i) {
					currentBit = 1;
					if (devices_pG[i].inSearch) {
						currentBit = (1llu << bitPos) & devices_pG[i].deviceID;
						if (readState == 1)
							currentBit = currentBit? 0 : 1;

						if (verbose_G)
							printf("  in search [%02d] %cbit:%d\n", i, (readState==0? ' ' : '~'), (currentBit? 1 : 0));
					}
					ANDbit &= (currentBit? 1 : 0);
				}

				writeBuf = (char)(ANDbit + '0');
				if (verbose_G)
					printf("  <= %c\n", writeBuf);
				write(fmTesterFifoFd, (void*)&writeBuf, 1);
				++readState;
				break;

			case '0':
			case '1':
				if (function_G != ROMsearch)
					break;
				if (readState != 2)
					break;
				if (verbose_G)
					printf(" readState:%d bitPos:%d\n", readState, bitPos);

				for (i=0; i<numEntries_G; ++i) {
					if (devices_pG[i].inSearch) {
						currentBit = (1llu << bitPos) & devices_pG[i].deviceID;
						if (((readBuf == '0') && currentBit) || ((readBuf == '1') && !currentBit)) {
							if (verbose_G)
								printf("   removing: %02d\n", i);
							devices_pG[i].inSearch = false;
						}
					}
				}

				++bitPos;
				if (bitPos >= bitSize_G)
					for (i=0; i<numEntries_G; ++i)
						devices_pG[i].inSearch = false;
				readState = 0;
				break;

			default:
				break;
		}
	}

allocFail:
	free(devices_pG);
	close(fmTesterFifoFd);
	unlink("fmTesterFifoFd");
closeToFifo:
	close(toTesterFifoFd);
	unlink("toTesterFifoFd");
fifoFail:
	return 0;
}

static void
signal_handler (int signo)
{
	/* preconds */
	// none

	longjmp(env_G, signo);
}

static void
setup_signal_handler (void)
{
	struct sigaction sig;

	/* preconds */
	// none

	memset(&sig, 0, sizeof(sig));
	sig.sa_handler = signal_handler;
	sigaction(SIGINT, &sig, NULL);
}

static void
usage (const char *cmdline_p)
{
	/* preconds */
	//none

	if (cmdline_p == NULL) {
		printf("bad usage\n");
		return;
	}

	printf("usage: %s [<options>] [<testfile>]\n", cmdline_p);
	printf("  where:\n");
	printf("    <testfile>              a file from which to get serial ID data\n");
	printf("                            (otherwise the data is generated randomly)\n");
	printf("    <options>\n");
	printf("      -h|--help             print information about this program and exit successfully\n");
	printf("      -b|--bitsize <b>      set the number of bits in the serial ID to <b> (MIN:2 default:8 MAX:64)\n");
	printf("      -m|--max-devices <m>  set the maximum number of devices (MIN:1 default:8)\n");
}

/**
 * input file format:
 * <number of entries>
 * <number of bits>
 * <unique serial id>… * N
 */
static int
get_data_from_file (const char *fileName_p)
{
	int i, ret;
	int fnRtn = -1;
	FILE *dataFile_p = NULL;
	char dataBuf[32];

	/* preconds */
	if (fileName_p == NULL)
		return -1;

	dataFile_p = fopen(fileName_p, "r");
	if (dataFile_p == NULL) {
		perror("open data file");
		return -1;
	}

	// 1st line → entries
	if (fgets(dataBuf, sizeof(dataBuf), dataFile_p) == NULL) {
		perror("reading 1st line");
		goto closeDataFile;
	}
	if (sscanf(dataBuf, "%i", &numEntries_G) != 1) {
		perror("parsing numEntries");
		goto closeDataFile;
	}

	// 2nd line → bit size
	if (fgets(dataBuf, sizeof(dataBuf), dataFile_p) == NULL) {
		perror("reading 2nd line");
		goto closeDataFile;
	}
	if (sscanf(dataBuf, "%i", &bitSize_G) != 1) {
		perror("parsing bitSize");
		goto closeDataFile;
	}

	devices_pG = (Devices_t*)malloc((size_t)numEntries_G * sizeof(Devices_t));
	if (devices_pG == NULL) {
		printf("can't allocate memory\n");
		goto closeDataFile;
	}
	for (i=0; i<numEntries_G; ++i) {
		if (fgets(dataBuf, sizeof(dataBuf), dataFile_p) == NULL) {
			printf("error getting entry %i from data file\n", i);
			goto postAllocFail;
		}
		if (sscanf(dataBuf, "%d", &ret) != 1) {
			printf("error converting entry %i from data file\n", i);
			goto postAllocFail;
		}
		devices_pG[i].deviceID = (uint64_t)ret;
		devices_pG[i].inSearch = true;
	}

	if (dataFile_p != NULL)
		fclose(dataFile_p);
	return 0;

postAllocFail:
	free(devices_pG);
closeDataFile:
	if (dataFile_p != NULL)
		fclose(dataFile_p);
	return fnRtn;
}

static int
use_random_data (void)
{
	int i, j;
	bool duplicate;
	uint64_t mask;
	uint64_t nextRandVal;

	/* preconds */
	if (bitSize_G > 64)
		return -1;

	// create mask
	mask = 0;
	for (i=0; i<bitSize_G; ++i)
		mask |= (uint64_t)(1llu << i);

	// make sure the max number of entries is sensible wrt the chosen bit size
	// a quick and easy way is to ensure the max entries is a magnitude
	// smaller than the number of bits
	if ((uint64_t)maxEntries_G > (mask>>1)) {
		printf("the given bit size (%d) is not high enough to randomly\n", bitSize_G);
		printf("generate the requested number of max entries (%d)\n\n", maxEntries_G);
		printf("please either increase the bit size\n");
		printf("or reduce the number of max entries to %lu or less\n", (mask>>1));
		return -1;
	}

	srandom((unsigned)time(NULL));
	numEntries_G = ((int)random() % (maxEntries_G)) + 1;

	devices_pG = (Devices_t*)malloc((size_t)numEntries_G * sizeof(Devices_t));
	if (devices_pG == NULL) {
		printf("can't allocate memory\n");
		return -1;
	}

	// generate unique entries
	for (i=0; i<numEntries_G; ++i) {
		nextRandVal = ((uint64_t)random() * (uint64_t)random()) & mask;
		duplicate = false;
		for (j=0; j<i; ++j)
			if (devices_pG[j].deviceID == nextRandVal) {
				duplicate = true;
				break;
			}

		if (duplicate) {
			--i;
			continue;
		}

		devices_pG[i].deviceID = nextRandVal;
		devices_pG[i].inSearch = true;
	}

	return 0;
}

static int
process_cmdline_args (int argc, char *argv[])
{
	int c, ret;
	bool bitSizeSpecified = false;
	bool maxEntriesSpecified = false;
	struct option longOpts[] = {
		{"help", no_argument, NULL, 'h'},
		{"bitsize", required_argument, NULL, 'b'},
		{"max-devices", required_argument, NULL, 'm'},
		{NULL, 0, NULL, 0},
	};

	while (1) {
		c = getopt_long(argc, argv, "hb:m:", longOpts, 0);
		if (c == -1)
			break;
		switch (c) {
			case 'h':
				printf("%s\n", PACKAGE_STRING);
				usage(argv[0]);
				exit(0);

			case 'b':
				if (sscanf(optarg, "%i", &ret) != 1) {
					usage(argv[0]);
					return -1;
				}
				if ((ret > 64) || (ret < 2)) {
					usage(argv[0]);
					return -1;
				}
				bitSize_G = ret;
				bitSizeSpecified = true;
				break;

			case 'm':
				if (sscanf(optarg, "%i", &ret) != 1) {
					usage(argv[0]);
					return -1;
				}
				if (ret < 1) {
					usage(argv[0]);
					return -1;
				}
				maxEntries_G = ret;
				maxEntriesSpecified = true;
				break;

			default:
				printf("cmdline arg error: %c (0x%02x)\n", c, c);
		}
	}

	ret = 0;
	if (argc == optind)
		ret = use_random_data();
	else if (argc == (optind + 1)) {
		if (bitSizeSpecified || maxEntriesSpecified) {
			printf("WARNING: specifying the bit size and/or max entries on the cmdline\n");
			printf("         is not compatible with using pre-generated data from a file\n");
			printf("these cmdline options will be ignored in favour of the values from the datafile\n");
		}
		ret = get_data_from_file(argv[optind]);
	}
	else {
		usage(argv[0]);
		return -1;
	}
	if (ret != 0)
		return -1;

	return 0;
}
