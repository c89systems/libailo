#include <amdgpu_drm.h>

#include "amdgpu_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int check(unsigned int device, unsigned int revision,
                 const char *expected) {
  struct amdgpu_device dev;
  int result;
  memset(&dev, 0, sizeof(dev));
  dev.info.asic_id = device;
  dev.info.pci_rev_id = revision;
  amdgpu_parse_asic_ids(&dev);
  result = strcmp(amdgpu_get_marketing_name(&dev), expected);
  if (result != 0) {
    fprintf(stderr, "AMDGPU %04x:%02x: expected %s, got %s\n", device, revision,
            expected, amdgpu_get_marketing_name(&dev));
  }
  free(dev.marketing_name);
  return result != 0;
}

int main(void) {
  /* First row, a later revision, and upstream's duplicate last-row ordering. */
  if (check(0x1114, 0xc2, "AMD Radeon 860M Graphics") ||
      check(0x1114, 0xc3, "AMD Radeon 840M Graphics") ||
      check(0x98e4, 0xeb, "AMD Radeon R3 Graphics")) {
    return 1;
  }
  /* An unmatched revision/device keeps the upstream public fallback name. */
  return check(0x1114, 0xc1, "AMD Radeon Graphics") ||
         check(0xffff, 0xff, "AMD Radeon Graphics");
}
