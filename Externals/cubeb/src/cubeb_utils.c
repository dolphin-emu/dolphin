/*
 * Copyright © 2016 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */

#include "cubeb_utils.h"
#include "cubeb_assert.h"

#include <stdlib.h>

static void
device_info_destroy(cubeb_device_info * info)
{
  XASSERT(info);

  free((void *) info->device_id);
  free((void *) info->friendly_name);
  free((void *) info->group_id);
  free((void *) info->vendor_name);
}

int
cubeb_utils_default_device_collection_destroy(cubeb * context,
                                              cubeb_device_collection * collection)
{
  uint32_t i;
  XASSERT(collection);

  (void) context;

  for (i = 0; i < collection->count; i++)
    device_info_destroy(&collection->device[i]);

  free(collection->device);
  return CUBEB_OK;
}
