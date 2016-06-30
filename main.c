#include <string.h>
#include "stm32f1xx.h"
#include "usart1.h"
#include "debug.h"

void rccInit()
{
	// Enable external crystal oscillator
	RCC->CR |= RCC_CR_HSEON;
	while (!(RCC->CR & RCC_CR_HSERDY));
	RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_HSE;
	while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSE);

	// Disable internal oscillator, enable PLL
	// Clock from 8MHz HSE, x9 to 72MHz system clock
	// System timer	@ 72 MHz
	// APB1		@ 36 MHz
	// APB2		@ 72 MHz
	// USB		@ 48 MHz
	RCC->CFGR = RCC_CFGR_PLLMULL9 | RCC_CFGR_PLLSRC |
			RCC_CFGR_PPRE1_DIV2 /*| RCC_CFGR_PPRE2_DIV4*/ |
			RCC_CFGR_SW_HSE;
	RCC->CR = RCC_CR_HSEON | RCC_CR_PLLON;
	while (!(RCC->CR & RCC_CR_PLLRDY));
	// 2 flash wait states for SYSCLK >= 48 MHz
	FLASH->ACR = FLASH_ACR_PRFTBS | FLASH_ACR_PRFTBE | 2;
	RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;
	while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);
}

static inline void flashReset()
{
	// Check if read protection is already disabled
	if (!(FLASH->OBR & FLASH_OBR_RDPRT))
		return;

	while (FLASH->SR & FLASH_SR_BSY);
	usart1WriteString("\n************\nRead protection reset process started.\n");

	while (FLASH->CR & FLASH_CR_LOCK) {
		usart1WriteString("Unlocking FPEC...\n");
		FLASH->KEYR = FLASH_KEY1;
		FLASH->KEYR = FLASH_KEY2;
	}
	while (!(FLASH->CR & FLASH_CR_OPTWRE)) {
		usart1WriteString("Disabling option bytes write protection...\n");
		FLASH->OPTKEYR = FLASH_KEY1;
		FLASH->OPTKEYR = FLASH_KEY2;
	}
	while (FLASH->SR & FLASH_SR_BSY);

	FLASH->CR = FLASH_CR_OPTWRE | FLASH_CR_STRT | FLASH_CR_OPTER;
	usart1WriteString("Erasing option bytes...\n");
	while (FLASH->SR & FLASH_SR_BSY);

	FLASH->CR = FLASH_CR_OPTWRE | FLASH_CR_OPTPG;
	OB->RDP = 0x00a5;
	usart1WriteString("Programming option bytes...\n");
	while (FLASH->SR & FLASH_SR_BSY);

	usart1WriteString("Finished, standby.\n");
	dbbkpt();
}

int main()
{
	// Jump to SRAM in case of software reset
	if (RCC->CSR & RCC_CSR_SFTRSTF) {
		RCC->CSR = RCC_CSR_RMVF;
		asm ("bx %0" : : "r" (*(uint32_t *)(SRAM_BASE + 4UL)));
	}

	// Vector Table Relocation in Internal FLASH
	SCB->VTOR = FLASH_BASE;

	rccInit();
	//usart1Init();
	//usart1WriteString("Initialised.\n");
	//flashReset();

	// Initialise RAM content from FLASH_RAM using DMA
	__disable_irq();
	SCB->SCR |= SCB_SCR_SEVONPEND;
	RCC->AHBENR |= RCC_AHBENR_DMA1EN;
	DMA1_Channel1->CNDTR = 20U * 1024U / 4U;
	DMA1_Channel1->CPAR = FLASH_BASE + 2U * 1024U;
	DMA1_Channel1->CMAR = SRAM_BASE;
	DMA1_Channel1->CCR = DMA_CCR_MEM2MEM | DMA_CCR_PL |
			DMA_CCR_MSIZE_1 | DMA_CCR_PSIZE_1 |
			DMA_CCR_MINC | DMA_CCR_PINC |
			DMA_CCR_TCIE | DMA_CCR_EN;
	do
		__WFE();
	while (!(DMA1->ISR & DMA_ISR_TCIF1));

	// Vector Table Relocation in Internal SRAM
	SCB->VTOR = SRAM_BASE;

	//usart1WriteString("SRAM copied, resetting...\n");
	//while (!usart1Ready());
	//*(uint32_t *)(SRAM_BASE + 4UL) = *(uint32_t *)(FLASH_BASE + 4UL);
	// Software reset
	NVIC_SystemReset();
	for (;;);
}
