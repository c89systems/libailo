#include <amdgpu.h>
#include <xf86drm.h>

int main(void) {
  amdgpu_device_handle device = 0;
  unsigned int major = 0;
  unsigned int minor = 0;
  drmVersionPtr version = drmGetVersion(-1);
  int result = amdgpu_device_initialize(-1, &major, &minor, &device);
  if (version != 0) {
    drmFreeVersion(version);
    return 1;
  }
  return result == 0 || device != 0;
}
