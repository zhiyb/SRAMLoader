#ifndef USART1_H
#define USART1_H

#include <stdint.h>
#include "stm32f1xx.h"

#ifdef __cplusplus
extern "C" {
#endif

void usart1Init();
void usart1WriteChar(char c);
void usart1WriteString(const char *str);
void usart1DumpHex(uint32_t v);
static inline uint32_t usart1Ready() {return USART1->SR & USART_SR_TXE;}

#ifdef __cplusplus
}
#endif

#endif // USART1_H
