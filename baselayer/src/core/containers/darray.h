#pragma once

#include <stddef.h>

typedef void *darray;

// ==== Lifetime ================================
darray darrnew(size_t size, size_t stride);
void darrfree(darray darr);
size_t darrlen(darray darr);
darray darradd(darray darr, size_t item_count, void *item);
#define darradddarr(darr, addition) darr = darradd(darr, darrlen(addition), addition)

#define darr_foreach(darr, elem)                                               \
  for (typeof(darr) elem = (darr), elem##_end = elem + darrlen(darr);          \
       elem != elem##_end; elem++)