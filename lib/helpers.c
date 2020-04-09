// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * This file is part of libgpiod.
 *
 * Copyright (C) 2017-2018 Bartosz Golaszewski <bartekgola@gmail.com>
 */

/*
 * More specific variants of the core API and misc functions that don't need
 * access to neither the internal library data structures nor the kernel UAPI.
 */

#include <ctype.h>
#include <errno.h>
#include <gpiod.h>
#include <stdio.h>
#include <string.h>

static bool isuint(const char *str)
{
	for (; *str && isdigit(*str); str++)
		;

	return *str == '\0';
}

struct gpiod_chip *gpiod_chip_open_by_name(const char *name)
{
	struct gpiod_chip *chip;
	char *path;
	int rv;

	rv = asprintf(&path, "/dev/%s", name);
	if (rv < 0)
		return NULL;

	chip = gpiod_chip_open(path);
	free(path);

	return chip;
}

struct gpiod_chip *gpiod_chip_open_by_number(unsigned int num)
{
	struct gpiod_chip *chip;
	char *path;
	int rv;

	rv = asprintf(&path, "/dev/gpiochip%u", num);
	if (!rv)
		return NULL;

	chip = gpiod_chip_open(path);
	free(path);

	return chip;
}

struct gpiod_chip *gpiod_chip_open_by_label(const char *label)
{
	struct gpiod_chip_iter *iter;
	struct gpiod_chip *chip;

	iter = gpiod_chip_iter_new();
	if (!iter)
		return NULL;

	gpiod_foreach_chip(iter, chip) {
		if (strcmp(label, gpiod_chip_label(chip)) == 0) {
			gpiod_chip_iter_free_noclose(iter);
			return chip;
		}
	}

	errno = ENOENT;
	gpiod_chip_iter_free(iter);

	return NULL;
}

struct gpiod_chip *gpiod_chip_open_lookup(const char *descr)
{
	struct gpiod_chip *chip;

	if (isuint(descr)) {
		chip = gpiod_chip_open_by_number(strtoul(descr, NULL, 10));
	} else {
		chip = gpiod_chip_open_by_label(descr);
		if (!chip) {
			if (strncmp(descr, "/dev/", 5))
				chip = gpiod_chip_open_by_name(descr);
			else
				chip = gpiod_chip_open(descr);
		}
	}

	return chip;
}

static int chip_get_lines(struct gpiod_chip *chip, unsigned int *offsets,
			  unsigned int num_offsets,
			  struct gpiod_line_bulk *bulk, bool watched)
{
	struct gpiod_line *line;
	unsigned int i;

	gpiod_line_bulk_init(bulk);

	for (i = 0; i < num_offsets; i++) {
		line = watched ? gpiod_chip_get_line_watched(chip, offsets[i])
			       : gpiod_chip_get_line(chip, offsets[i]);
		if (!line) {
			if (watched)
				gpiod_chip_unwatch_all(chip);

			return -1;
		}

		gpiod_line_bulk_add(bulk, line);
	}

	return 0;
}

int gpiod_chip_get_lines(struct gpiod_chip *chip, unsigned int *offsets,
			 unsigned int num_offsets, struct gpiod_line_bulk *bulk)
{
	return chip_get_lines(chip, offsets, num_offsets, bulk, false);
}

int gpiod_chip_get_lines_watched(struct gpiod_chip *chip, unsigned int *offsets,
				 unsigned int num_offsets,
				 struct gpiod_line_bulk *bulk)
{
	return chip_get_lines(chip, offsets, num_offsets, bulk, true);
}

static int chip_get_all_lines(struct gpiod_chip *chip,
			      struct gpiod_line_bulk *bulk, bool watched)
{
	struct gpiod_line_iter *iter;
	struct gpiod_line *line;
	int ret = 0;

	gpiod_line_bulk_init(bulk);

	iter = gpiod_line_iter_new(chip);
	if (!iter)
		return -1;

	gpiod_foreach_line(iter, line) {
		if (watched) {
			ret = gpiod_line_watch(line);
			if (ret) {
				gpiod_chip_unwatch_all(chip);
				break;
			}
		}

		gpiod_line_bulk_add(bulk, line);
	}

	gpiod_line_iter_free(iter);

	return ret;
}

int gpiod_chip_get_all_lines(struct gpiod_chip *chip,
			     struct gpiod_line_bulk *bulk)
{
	return chip_get_all_lines(chip, bulk, false);
}

int gpiod_chip_get_all_lines_watched(struct gpiod_chip *chip,
				     struct gpiod_line_bulk *bulk)
{
	return chip_get_all_lines(chip, bulk, true);
}

static struct gpiod_line *chip_find_line(struct gpiod_chip *chip,
					 const char *name, bool watched)
{
	struct gpiod_line_iter *iter;
	struct gpiod_line *line;
	const char *tmp;
	int ret;

	iter = gpiod_line_iter_new(chip);
	if (!iter)
		return NULL;

	gpiod_foreach_line(iter, line) {
		tmp = gpiod_line_name(line);
		if (tmp && strcmp(tmp, name) == 0) {
			gpiod_line_iter_free(iter);

			if (watched) {
				ret = gpiod_line_watch(line);
				if (ret)
					return NULL;
			}

			return line;
		}
	}

	errno = ENOENT;
	gpiod_line_iter_free(iter);

	return NULL;
}

struct gpiod_line *
gpiod_chip_find_line(struct gpiod_chip *chip, const char *name)
{
	return chip_find_line(chip, name, false);
}

struct gpiod_line *
gpiod_chip_find_line_watched(struct gpiod_chip *chip, const char *name)
{
	return chip_find_line(chip, name, true);
}

int chip_find_lines(struct gpiod_chip *chip, const char **names,
		    struct gpiod_line_bulk *bulk, bool watched)
{
	struct gpiod_line *line;
	int i;

	gpiod_line_bulk_init(bulk);

	for (i = 0; names[i]; i++) {
		line = chip_find_line(chip, names[i], watched);
		if (!line) {
			if (watched)
				gpiod_chip_unwatch_all(chip);

			return -1;
		}

		gpiod_line_bulk_add(bulk, line);
	}

	return 0;
}

int gpiod_chip_find_lines(struct gpiod_chip *chip,
			  const char **names, struct gpiod_line_bulk *bulk)
{
	return chip_find_lines(chip, names, bulk, false);
}

int gpiod_chip_find_lines_watched(struct gpiod_chip *chip, const char **names,
				  struct gpiod_line_bulk *bulk)
{
	return chip_find_lines(chip, names, bulk, true);
}

int gpiod_chip_unwatch_all(struct gpiod_chip *chip)
{
	struct gpiod_line_iter *iter;
	struct gpiod_line *line;
	int ret = 0;

	iter = gpiod_line_iter_new(chip);
	if (!iter)
		return -1;

	gpiod_foreach_line(iter, line) {
		ret = gpiod_line_unwatch(line);
		/* EBUSY means this line is not watched. */
		if (ret && errno != EBUSY)
			break;
	}

	gpiod_line_iter_free(iter);

	return ret;
}

int gpiod_line_request_input(struct gpiod_line *line, const char *consumer)
{
	struct gpiod_line_request_config config = {
		.consumer = consumer,
		.request_type = GPIOD_LINE_REQUEST_DIRECTION_INPUT,
	};

	return gpiod_line_request(line, &config, 0);
}

int gpiod_line_request_output(struct gpiod_line *line,
			      const char *consumer, int default_val)
{
	struct gpiod_line_request_config config = {
		.consumer = consumer,
		.request_type = GPIOD_LINE_REQUEST_DIRECTION_OUTPUT,
	};

	return gpiod_line_request(line, &config, default_val);
}

int gpiod_line_request_input_flags(struct gpiod_line *line,
				   const char *consumer, int flags)
{
	struct gpiod_line_request_config config = {
		.consumer = consumer,
		.request_type = GPIOD_LINE_REQUEST_DIRECTION_INPUT,
		.flags = flags,
	};

	return gpiod_line_request(line, &config, 0);
}

int gpiod_line_request_output_flags(struct gpiod_line *line,
				    const char *consumer, int flags,
				    int default_val)
{
	struct gpiod_line_request_config config = {
		.consumer = consumer,
		.request_type = GPIOD_LINE_REQUEST_DIRECTION_OUTPUT,
		.flags = flags,
	};

	return gpiod_line_request(line, &config, default_val);
}

static int line_event_request_type(struct gpiod_line *line,
				   const char *consumer, int flags, int type)
{
	struct gpiod_line_request_config config = {
		.consumer = consumer,
		.request_type = type,
		.flags = flags,
	};

	return gpiod_line_request(line, &config, 0);
}

int gpiod_line_request_rising_edge_events(struct gpiod_line *line,
					  const char *consumer)
{
	return line_event_request_type(line, consumer, 0,
				       GPIOD_LINE_REQUEST_EVENT_RISING_EDGE);
}

int gpiod_line_request_falling_edge_events(struct gpiod_line *line,
					   const char *consumer)
{
	return line_event_request_type(line, consumer, 0,
				       GPIOD_LINE_REQUEST_EVENT_FALLING_EDGE);
}

int gpiod_line_request_both_edges_events(struct gpiod_line *line,
					 const char *consumer)
{
	return line_event_request_type(line, consumer, 0,
				       GPIOD_LINE_REQUEST_EVENT_BOTH_EDGES);
}

int gpiod_line_request_rising_edge_events_flags(struct gpiod_line *line,
						const char *consumer,
						int flags)
{
	return line_event_request_type(line, consumer, flags,
				       GPIOD_LINE_REQUEST_EVENT_RISING_EDGE);
}

int gpiod_line_request_falling_edge_events_flags(struct gpiod_line *line,
						 const char *consumer,
						 int flags)
{
	return line_event_request_type(line, consumer, flags,
				       GPIOD_LINE_REQUEST_EVENT_FALLING_EDGE);
}

int gpiod_line_request_both_edges_events_flags(struct gpiod_line *line,
					       const char *consumer, int flags)
{
	return line_event_request_type(line, consumer, flags,
				       GPIOD_LINE_REQUEST_EVENT_BOTH_EDGES);
}

int gpiod_line_request_bulk_input(struct gpiod_line_bulk *bulk,
				  const char *consumer)
{
	struct gpiod_line_request_config config = {
		.consumer = consumer,
		.request_type = GPIOD_LINE_REQUEST_DIRECTION_INPUT,
	};

	return gpiod_line_request_bulk(bulk, &config, 0);
}

int gpiod_line_request_bulk_output(struct gpiod_line_bulk *bulk,
				   const char *consumer,
				   const int *default_vals)
{
	struct gpiod_line_request_config config = {
		.consumer = consumer,
		.request_type = GPIOD_LINE_REQUEST_DIRECTION_OUTPUT,
	};

	return gpiod_line_request_bulk(bulk, &config, default_vals);
}

static int line_event_request_type_bulk(struct gpiod_line_bulk *bulk,
					const char *consumer,
					int flags, int type)
{
	struct gpiod_line_request_config config = {
		.consumer = consumer,
		.request_type = type,
		.flags = flags,
	};

	return gpiod_line_request_bulk(bulk, &config, 0);
}

int gpiod_line_request_bulk_rising_edge_events(struct gpiod_line_bulk *bulk,
					       const char *consumer)
{
	return line_event_request_type_bulk(bulk, consumer, 0,
					GPIOD_LINE_REQUEST_EVENT_RISING_EDGE);
}

int gpiod_line_request_bulk_falling_edge_events(struct gpiod_line_bulk *bulk,
						const char *consumer)
{
	return line_event_request_type_bulk(bulk, consumer, 0,
					GPIOD_LINE_REQUEST_EVENT_FALLING_EDGE);
}

int gpiod_line_request_bulk_both_edges_events(struct gpiod_line_bulk *bulk,
					      const char *consumer)
{
	return line_event_request_type_bulk(bulk, consumer, 0,
					GPIOD_LINE_REQUEST_EVENT_BOTH_EDGES);
}

int gpiod_line_request_bulk_input_flags(struct gpiod_line_bulk *bulk,
					const char *consumer, int flags)
{
	struct gpiod_line_request_config config = {
		.consumer = consumer,
		.request_type = GPIOD_LINE_REQUEST_DIRECTION_INPUT,
		.flags = flags,
	};

	return gpiod_line_request_bulk(bulk, &config, 0);
}

int gpiod_line_request_bulk_output_flags(struct gpiod_line_bulk *bulk,
					 const char *consumer, int flags,
					 const int *default_vals)
{
	struct gpiod_line_request_config config = {
		.consumer = consumer,
		.request_type = GPIOD_LINE_REQUEST_DIRECTION_OUTPUT,
		.flags = flags,
	};

	return gpiod_line_request_bulk(bulk, &config, default_vals);
}

int gpiod_line_request_bulk_rising_edge_events_flags(
					struct gpiod_line_bulk *bulk,
					const char *consumer, int flags)
{
	return line_event_request_type_bulk(bulk, consumer, flags,
					GPIOD_LINE_REQUEST_EVENT_RISING_EDGE);
}

int gpiod_line_request_bulk_falling_edge_events_flags(
					struct gpiod_line_bulk *bulk,
					const char *consumer, int flags)
{
	return line_event_request_type_bulk(bulk, consumer, flags,
					GPIOD_LINE_REQUEST_EVENT_FALLING_EDGE);
}

int gpiod_line_request_bulk_both_edges_events_flags(
					struct gpiod_line_bulk *bulk,
					const char *consumer, int flags)
{
	return line_event_request_type_bulk(bulk, consumer, flags,
					GPIOD_LINE_REQUEST_EVENT_BOTH_EDGES);
}

struct gpiod_line *gpiod_line_get(const char *device, unsigned int offset)
{
	struct gpiod_chip *chip;
	struct gpiod_line *line;

	chip = gpiod_chip_open_lookup(device);
	if (!chip)
		return NULL;

	line = gpiod_chip_get_line(chip, offset);
	if (!line) {
		gpiod_chip_close(chip);
		return NULL;
	}

	return line;
}

struct gpiod_line *gpiod_line_find(const char *name)
{
	struct gpiod_chip_iter *iter;
	struct gpiod_chip *chip;
	struct gpiod_line *line;

	iter = gpiod_chip_iter_new();
	if (!iter)
		return NULL;

	gpiod_foreach_chip(iter, chip) {
		line = gpiod_chip_find_line(chip, name);
		if (line) {
			gpiod_chip_iter_free_noclose(iter);
			return line;
		}

		if (errno != ENOENT)
			goto out;
	}

	errno = ENOENT;

out:
	gpiod_chip_iter_free(iter);

	return NULL;
}

void gpiod_line_close_chip(struct gpiod_line *line)
{
	struct gpiod_chip *chip = gpiod_line_get_chip(line);

	gpiod_chip_close(chip);
}
