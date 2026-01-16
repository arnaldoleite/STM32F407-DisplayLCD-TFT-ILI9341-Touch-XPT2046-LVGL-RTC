#ifndef XPT2046_H
#define XPT2046_H

#include <stdint.h>

uint8_t XPT2046_Pressed(void);
void XPT2046_Read(uint16_t *x, uint16_t *y);

#endif
