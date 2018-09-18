#ifndef _EDGEX_DEVICE_MAP_H_
#define _EDGEX_DEVICE_MAP_H_ 1

/* Based on rxi's type-safe hashmap implementation */

/**
 * Copyright (c) 2014 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

struct edgex_map_node;
typedef struct edgex_map_node edgex_map_node;

typedef struct
{
  edgex_map_node **buckets;
  unsigned nbuckets;
  unsigned nnodes;
} edgex_map_base;

typedef struct
{
  unsigned bucketidx;
  edgex_map_node *node;
} edgex_map_iter;

#define edgex_map(T) \
struct \
{ \
  edgex_map_base base; \
  T * ref; \
  T tmp; \
}

#define edgex_map_init(m) memset(m, 0, sizeof(*(m)))

#define edgex_map_deinit(m) edgex_map_deinit_(&(m)->base)

#define edgex_map_get(m, key) ((m)->ref = edgex_map_get_(&(m)->base, key))

#define edgex_map_set(m, key, value) \
  ((m)->tmp = (value), \
  edgex_map_set_(&(m)->base, key, &(m)->tmp, sizeof((m)->tmp)) )

#define edgex_map_remove(m, key) edgex_map_remove_ (&(m)->base, key)

#define edgex_map_iter(m) edgex_map_iter_ ()

#define edgex_map_next(m, iter) edgex_map_next_ (&(m)->base, iter)

extern void edgex_map_deinit_ (edgex_map_base *m);

extern void *edgex_map_get_ (edgex_map_base *m, const char *key);

extern int edgex_map_set_
  (edgex_map_base *m, const char *key, void *value, int vsize);

extern void edgex_map_remove_ (edgex_map_base *m, const char *key);

extern edgex_map_iter edgex_map_iter_ (void);

extern const char *edgex_map_next_ (edgex_map_base *m, edgex_map_iter *iter);

typedef edgex_map(void*) edgex_map_void;
typedef edgex_map(char*) edgex_map_string;
typedef edgex_map(int) edgex_map_int;
typedef edgex_map(char) edgex_map_char;
typedef edgex_map(float) edgex_map_float;
typedef edgex_map(double) edgex_map_double;

#endif
