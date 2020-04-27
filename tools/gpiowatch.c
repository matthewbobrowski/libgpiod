// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * This file is part of libgpiod.
 *
 * Copyright (C) 2020 Bartosz Golaszewski <bgolaszewski@baylibre.com>
 */

#include <getopt.h>
#include <gpiod.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "tools-common.h"

static const struct option longopts[] = {
	{ "help",	no_argument,	NULL,	'h' },
	{ "version",	no_argument,	NULL,	'v' },
	{ GETOPT_NULL_LONGOPT },
};

static const char *const shortopts = "+hv";

static void print_help(void)
{
	printf("Usage: %s [OPTIONS] <chip name/number> <offset 1> <offset2> ...\n",
	       get_progname());
	printf("\n");
	printf("Monitor state changes of GPIO lines (request, release and config operations).\n");
	printf("\n");
	printf("Options:\n");
	printf("  -h, --help:\t\tdisplay this message and exit\n");
	printf("  -v, --version:\tdisplay the version and exit\n");
}

int main(int argc, char **argv)
{
	struct gpiod_line_bulk lines = GPIOD_LINE_BULK_INITIALIZER;
	struct gpiod_chip *chip;
	unsigned int *offsets;
	int optc, opti, num_lines, i, ret;
	char *device, *end;
	struct timespec timeout = { 10, 0 };
	struct gpiod_watch_event events[32];

	for (;;) {
		optc = getopt_long(argc, argv, shortopts, longopts, &opti);
		if (optc < 0)
			break;

		switch (optc) {
		case 'h':
			print_help();
			return EXIT_SUCCESS;
		case 'v':
			print_version();
			return EXIT_SUCCESS;
		case '?':
			die("try %s --help", get_progname());
		default:
			abort();
		}
	}

	argc -= optind;
	argv += optind;

	if (argc < 1)
		die("gpiochip must be specified");

	if (argc < 2)
		die("at least one GPIO line offset must be specified");

	device = argv[0];
	argc--;
	argv++;
	num_lines = argc;

	offsets = calloc(num_lines, sizeof(*offsets));
	if (!offsets)
		die("out of memory");

	for (i = 0; i < num_lines; i++) {
		offsets[i] = strtoul(argv[i], &end, 10);
		if (*end != '\0' || offsets[i] > INT_MAX)
			die("invalid GPIO offset: %s", argv[i]);
	}

	chip = gpiod_chip_open_lookup(device);
	if (!chip)
		die_perror("unable to access the GPIO chip %s", device);

	ret = gpiod_chip_get_lines_watched(chip, offsets, num_lines, &lines);
	if (ret)
		die_perror("unable to retrieve GPIO lines");

	for (;;) {
		ret = gpiod_chip_watch_event_wait(chip, &timeout);
		if (ret < 0)
			die_perror("error watching the GPIO chip for line state change events");
		else if (ret == 0)
			continue;

		ret = gpiod_chip_watch_event_read_multiple(chip, events,
							   ARRAY_SIZE(events));
		if (ret < 0)
			die_perror("error reading line state change events");

		
	}

	return EXIT_SUCCESS;
}
