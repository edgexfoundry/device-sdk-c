#include "map.h"

#include <stdlib.h>
#include <string.h>

/* Based on rxi's type-safe hashmap implementation */

/**
 * Copyright (c) 2014 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

typedef struct edgex_map_node
{
  unsigned hash;
  void *value;
  edgex_map_node *next;
} edgex_map_node;

static unsigned edgex_hash (const char *str)
{
  unsigned hash = 5381u;
  while (*str)
  {
    hash = ((hash << 5) + hash) ^ *str++;
  }
  return hash;
}

static edgex_map_node *edgex_map_newnode
  (const char *key, void *value, int vsize)
{
  edgex_map_node *node;
  int ksize = strlen (key) + 1;
  int voffset = ksize + ((sizeof (void *) - ksize) % sizeof (void *));
  node = malloc (sizeof (*node) + voffset + vsize);
  if (!node)
  { return NULL; }
  memcpy (node + 1, key, ksize);
  node->hash = edgex_hash (key);
  node->value = ((char *) (node + 1)) + voffset;
  memcpy (node->value, value, vsize);
  return node;
}

static int edgex_map_bucketidx (edgex_map_base *m, unsigned hash)
{
  /* If the implementation is changed to allow a non-power-of-2 bucket count,
   * the line below should be changed to use mod instead of AND
   */
  return hash & (m->nbuckets - 1);
}

static void edgex_map_addnode (edgex_map_base *m, edgex_map_node *node)
{
  int n = edgex_map_bucketidx (m, node->hash);
  node->next = m->buckets[n];
  m->buckets[n] = node;
}

static int edgex_map_resize (edgex_map_base *m, int nbuckets)
{
  edgex_map_node *nodes, *node, *next;
  edgex_map_node **buckets;
  int i;
  /* Chain all nodes together */
  nodes = NULL;
  i = m->nbuckets;
  while (i--)
  {
    node = (m->buckets)[i];
    while (node)
    {
      next = node->next;
      node->next = nodes;
      nodes = node;
      node = next;
    }
  }
  /* Reset buckets */
  buckets = realloc (m->buckets, sizeof (*m->buckets) * nbuckets);
  if (buckets != NULL)
  {
    m->buckets = buckets;
    m->nbuckets = nbuckets;
  }
  if (m->buckets)
  {
    memset (m->buckets, 0, sizeof (*m->buckets) * m->nbuckets);
    /* Re-add nodes to buckets */
    node = nodes;
    while (node)
    {
      next = node->next;
      edgex_map_addnode (m, node);
      node = next;
    }
  }
  /* Return error code if realloc() failed */
  return (buckets == NULL) ? -1 : 0;
}

static edgex_map_node **edgex_map_getref (edgex_map_base *m, const char *key)
{
  unsigned hash = edgex_hash (key);
  if (m->nbuckets > 0)
  {
    edgex_map_node **next = &m->buckets[edgex_map_bucketidx (m, hash)];
    while (*next)
    {
      if ((*next)->hash == hash && !strcmp ((char *) (*next + 1), key))
      {
        return next;
      }
      next = &(*next)->next;
    }
  }
  return NULL;
}

void edgex_map_deinit_ (edgex_map_base *m)
{
  edgex_map_node *next, *node;
  int i;
  i = m->nbuckets;
  while (i--)
  {
    node = m->buckets[i];
    while (node)
    {
      next = node->next;
      free (node);
      node = next;
    }
  }
  free (m->buckets);
}

void *edgex_map_get_ (edgex_map_base *m, const char *key)
{
  edgex_map_node **next = edgex_map_getref (m, key);
  return next ? (*next)->value : NULL;
}

int edgex_map_set_ (edgex_map_base *m, const char *key, void *value, int vsize)
{
  int n, err;
  edgex_map_node **next, *node;
  /* Find & replace existing node */
  next = edgex_map_getref (m, key);
  if (next)
  {
    memcpy ((*next)->value, value, vsize);
    return 0;
  }
  /* Add new node */
  node = edgex_map_newnode (key, value, vsize);
  if (node == NULL)
  { goto fail; }
  if (m->nnodes >= m->nbuckets)
  {
    n = (m->nbuckets > 0) ? (m->nbuckets << 1) : 1;
    err = edgex_map_resize (m, n);
    if (err)
    { goto fail; }
  }
  edgex_map_addnode (m, node);
  m->nnodes++;
  return 0;
  fail:
  if (node)
  { free (node); }
  return -1;
}

void edgex_map_remove_ (edgex_map_base *m, const char *key)
{
  edgex_map_node **next = edgex_map_getref (m, key);
  if (next)
  {
    edgex_map_node *node = *next;
    *next = (*next)->next;
    free (node);
    m->nnodes--;
  }
}

edgex_map_iter edgex_map_iter_ (void)
{
  edgex_map_iter iter;
  iter.bucketidx = -1;
  iter.node = NULL;
  return iter;
}

const char *edgex_map_next_ (edgex_map_base *m, edgex_map_iter *iter)
{
  if (iter->node)
  {
    iter->node = iter->node->next;
    if (iter->node == NULL)
    { goto nextBucket; }
  }
  else
  {
    nextBucket:
    do
    {
      if (++iter->bucketidx >= m->nbuckets)
      {
        return NULL;
      }
      iter->node = m->buckets[iter->bucketidx];
    } while (iter->node == NULL);
  }
  return (char *) (iter->node + 1);
}
