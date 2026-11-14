#include "darray.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

typedef struct darr_hdr_t {
  size_t size;
  size_t length;
  size_t stride;
  char data[];
} darr_hdr_t;

darr_hdr_t *hdr_from_darr(darray darr) {
  return (darr_hdr_t *)((char *)darr - sizeof(darr_hdr_t));
}

darray darrnew(size_t size, size_t stride) {
  size_t data_size = size * stride;
  darr_hdr_t *darr_hdr = malloc(sizeof(darr_hdr_t) + data_size);
  darr_hdr->stride = stride;
  darr_hdr->size = size;
  darr_hdr->length = 0;

  memset(darr_hdr->data, 0, data_size);

  return darr_hdr->data;
}

void darrfree(darray darr) { free(hdr_from_darr(darr)); }

size_t darrlen(darray darr) { return hdr_from_darr(darr)->length; }

darr_hdr_t *darr_grow(darr_hdr_t *darr_hdr, size_t new_size) {
  darr_hdr =
      realloc(darr_hdr, sizeof(darr_hdr_t) + ((new_size * darr_hdr->stride)));
  darr_hdr->size = new_size;
  memset(darr_hdr->data + sizeof(darr_hdr_t) +
             (darr_hdr->stride * darr_hdr->length),
         0, darr_hdr->size - darr_hdr->length);
  return darr_hdr;
}

darray darradd(darray darr, void *element) {
  darr_hdr_t *darr_hdr = hdr_from_darr(darr);

  if (darr_hdr->length + 1 > darr_hdr->size) {
    darr_hdr = darr_grow(darr_hdr, darr_hdr->size * 2);
  }

  memcpy(element, darr_hdr->data + (darr_hdr->length * darr_hdr->stride),
         darr_hdr->stride);
  darr_hdr->length++;
  return darr_hdr->data;
}