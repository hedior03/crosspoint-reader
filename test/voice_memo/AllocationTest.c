// Compile the real C encoder with a failing allocator to exercise its null guard.
#include <assert.h>
#include <stdlib.h>
static void* failed_malloc(size_t size) {
  (void)size;
  return NULL;
}
#define malloc failed_malloc
#include "../../lib/AdpcmXq/adpcm-lib.c"
#undef malloc
int main(void) {
  assert(adpcm_create_context(1, 16000, 0, NOISE_SHAPING_OFF) == NULL);
  return 0;
}
