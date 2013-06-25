/**
 * Copyright (C) 2013 Andi Drebes <andi.drebes@lip6.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef TRACE_FILE_H
#define TRACE_FILE_H

#include <stdio.h>
#include <stdint.h>

/**
 * OpenStream trace format:
 *
 * +----------------------------+
 * | trace_header               |
 * +----------------------------+
 * | Arbitrary number of:       |
 * | - trace_state_events       |
 * | - trace_comm_events        |
 * | - trace_single_events      |
 * +----------------------------+
 *
 * Each of these structs contains a header that specifies its type and
 * that contains common fields used in every trace sample.
 *
 * The on-disk byte order for every integer fiels is little endian
 */

/* OSTV in ASCII */
#define TRACE_MAGIC 0x5654534f
#define TRACE_VERSION 1

enum event_type {
	EVENT_TYPE_STATE = 0,
	EVENT_TYPE_COMM = 1,
	EVENT_TYPE_SINGLE = 2
};

enum worker_state {
	WORKER_STATE_SEEKING = 0,
	WORKER_STATE_TASKEXEC = 1,
	WORKER_STATE_RT_TCREATE = 2,
	WORKER_STATE_RT_RESDEP = 3,
	WORKER_STATE_RT_TDEC = 4,
	WORKER_STATE_RT_BCAST = 5,
	WORKER_STATE_RT_INIT = 6,
	WORKER_STATE_RT_ESTIMATE_COSTS = 7,
	WORKER_STATE_RT_REORDER = 8,
	WORKER_STATE_MAX = 9
};

enum comm_event_type {
	COMM_TYPE_UNKNOWN = 0,
	COMM_TYPE_STEAL = 1,
	COMM_TYPE_PUSH = 2,
};

enum single_event_type {
	SINGLE_TYPE_TCREATE = 0
};


/* File header */
struct trace_header {
	/* Magic number */
	uint32_t magic;

	/* Version of the file format used on disk */
	uint32_t version;

	/* Day of month in [1;31] */
	uint8_t day;

	/* Month in [1;12] */
	uint8_t month;

	/* Year as plain integer e.g. 2013 */
	uint16_t year;

	/* Hour in [0;23] */
	uint8_t hour;

	/* Minute in [0;59] */
	uint8_t minute;
} __attribute__((packed));

extern int trace_header_conversion_table[];

/* A trace event header is included in every data structure
 * used in the trace file in order to identify the type of
 * the sample.
 */
struct trace_event_header {
	/* Struct type */
	uint32_t type;

	/* Timestamp of the event in [0;UINT64_MAX] */
	uint64_t time;

	/* CPU in [0;UINT32_MAX] */
	uint32_t cpu;

	/* Worker in [0;UINT32_MAX] */
	uint32_t worker;

	/* Task that was being executed when the event occured
	 * (value of the pointer to the task's frame) */
	uint64_t active_task;
} __attribute__((packed));;

extern int trace_event_header_conversion_table[];

/* Describes a state in which the worker was during a period of time.
 * Per-worker states might not overlap.
 */
struct trace_state_event {
	/* Start time of the state is stored in header.time */
	struct trace_event_header header;

	/* Indicates when the worker left the state */
	uint64_t end_time;

	/* State identifier */
	uint32_t state;
} __attribute__((packed));

extern int trace_state_event_conversion_table[];

/* Communication events can be any events that involves point-to-point
 * communication between workers.
 */
struct trace_comm_event {
	/* header.cpu / header.worker represents the source
	 * of the communication */
	struct trace_event_header header;

	/* Communication type, e.g. steal or push */
	uint32_t type;

	/* Destination CPU */
	uint32_t dst_cpu;

	/* Destination worker */
	uint32_t dst_worker;

	/* Indicates how much data in bytes was transferred */
	uint64_t size;

	/* What was transferred, e.g. value of the pointer to the data */
	uint64_t what;
} __attribute__((packed));

extern int trace_comm_event_conversion_table[];

/* Single events are events that only involve one worker
 * and whose duration is not important, e.g. task creation.
 */
struct trace_single_event {
	struct trace_event_header header;

	/* Single event type */
	uint32_t type;
} __attribute__((packed));

extern int trace_single_event_conversion_table[];

enum conversion_direction {
	CONVERT_DSK_TO_HOST = 0,
	CONVERT_HOST_TO_DSK,
};

/* Converts a structure either from or to on-disk format */
void convert_struct(void* ptr, int* conversion_table, int offset, enum conversion_direction dir);

/* Read a data structure from disk and convert it to host format */
int read_struct_convert(FILE* fp, void* out, int size, int* conversion_table, int offset);

/* Write a data structure to disk and convert it to on-disk format */
int write_struct_convert(FILE* fp, void* out, int size, int* conversion_table, int offset);

/* Performs an integrity check on a header in host format */
int trace_verify_header(struct trace_header* header);

#endif
