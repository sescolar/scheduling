#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#define max(a,b) ((a)>=(b)?(a):(b))
#define min(a,b) ((a)<=(b)?(a):(b))
#define DESKTOP

unsigned long millis(void)
{
  return (unsigned long)(clock()/CLOCKS_PER_SEC);
}

#include "FinalBatterySchedulingSimulator/FinalBatterySchedulingSimulator.ino"



int main(int argc, char *argv[])
{
  setup();
  while (1) loop();

  return 1;
}
