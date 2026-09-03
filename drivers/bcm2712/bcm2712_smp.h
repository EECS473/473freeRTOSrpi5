#ifndef BCM2712_SMP_H_
#define BCM2712_SMP_H_

#include <stdint.h>

void bcm2712SmpBringupSecondaryCpus(void);
void vPortYieldCore(uint32_t ulCoreID);

#endif
