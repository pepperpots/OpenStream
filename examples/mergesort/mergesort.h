/**
 * Definitions and functions used both in the sequential and the
 * parallel version of mergesort.
 *
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

#ifndef MERGESORT_H
#define MERGESORT_H

#include <stdint.h>
#include <inttypes.h>
#include <string.h>

typedef uint64_t key_t;
#define KEY_FORMAT "%"PRIu64

#define DEFAULT_NUM_KEYS_LOG 20
#define DEFAULT_BLOCK_SIZE_LOG 14

/*
 * Initializes an array of keys with random values
 */
static inline void init_sequence(key_t* keys, long num_keys)
{
  key_t rands = 77777;

  for(long i = 0; i < num_keys; i++) {
    rands = rands * 1103515245 + 12345;
    keys[i] = rands;
  }
}

/* Merges two sorted arrays keys_a and keys_b of equal length
 * and writes the result to keys_out. Num_keys specifies the
 * number of keys in keys_a and keys_b respectively.
 */
static inline void merge(key_t* keys_a, key_t* keys_b, key_t* out, long num_keys)
{
  long pos_a = 0;
  long pos_b = 0;
  long pos_out = 0;

  while(pos_out < 2*num_keys) {
    if(pos_a < num_keys && (pos_b >= num_keys || keys_a[pos_a] <= keys_b[pos_b]))
      out[pos_out++] = keys_a[pos_a++];
    else
      out[pos_out++] = keys_b[pos_b++];
  }
}

/* Performs a sequential merge sort on the keys in keys_in and
 * writes the result to keys_out.
 */
static inline void sort_block(key_t* keys_in, key_t* keys_out, long num_keys)
{
  key_t* iki = keys_in;
  key_t* iko = keys_out;
  key_t* tmp;


  for(long ibs = 1; ibs < num_keys; ibs = ibs << 1) {
    for(long ib = 0; ib < num_keys / ibs; ib += 2) {
      merge(&iki[ib*ibs], &iki[(ib+1)*ibs], &iko[ib*ibs], ibs);
    }

    tmp = iki;
    iki = iko;
    iko = tmp;
  }

  /* If the number of iterations of the main loop is even,
   * the result is is the wrong array. Copy data to the
   * output array.
   */
  if(iki == keys_in)
    memcpy(keys_out, keys_in, num_keys*sizeof(key_t));
}

/* Check if keys are in ascending order */
static inline int check_ascending(key_t* keys, long num_keys)
{
  for(long i = 1; i < num_keys; i++) {
    if(keys[i-1] > keys[i])
      return 0;
  }

  return 1;
}

#endif
