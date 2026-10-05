#include <stdio.h>

#ifndef __STRICT_ANSI__
#error Project C must use strict ANSI mode
#endif
#ifdef __STDC_VERSION__
#error Project C must use C89, not C99 or later
#endif

int main(void) {
  puts("project C policy: -std=c89");
  return 0;
}
