#include "main.h"
void SystemClock_72MHz_Config(void);
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
void ADC1_2_IRQHandler(void);
void gpioSetUp();
void adcSetUp();
void timer1SetUp();
void setState();
void setStep();

volatile uint16_t deathTimeIQR=0, speedIQR=0, pwmIQR=0;
volatile uint32_t dummy_reg = 0;
uint8_t step = 0;
uint8_t state = 0;  // Stop - Reset - Loop - OverStep(6)

volatile uint32_t* const LUT_HighSide_Reg[8] = {
    &TIM1->CCR1, // STEP 1: Pha U chạy PWM
    &TIM1->CCR1, // STEP 2: Pha U
    &TIM1->CCR2, // STEP 3: Pha V chạy PWM
    &TIM1->CCR2, // STEP 4: Pha V
    &TIM1->CCR3, // STEP 5: Pha W chạy PWM
    &TIM1->CCR3,  // STEP 6: Pha W
    &dummy_reg,  // Step 7 (RESET)
    &dummy_reg   // Step 8 (OFF)
};

const uint32_t LUT_LowSide[8] = {
    (1<<14) | ((1<<13 | 1<<15)<<16), // step 1
    (1<<15) | ((1<<13 | 1<<14)<<16), // step 2
    (1<<15) | ((1<<13 | 1<<14)<<16), // step 3
    (1<<13) | ((1<<14 | 1<<15)<<16), // step 4
    (1<<13) | ((1<<14 | 1<<15)<<16), // step 5
    (1<<14) | ((1<<13 | 1<<15)<<16), // step 6
	(1<<13) | (1<<14) | (1<<15),     // step 7 - Reset
	0xE0000000,                      // step 8 - Off
};

int main(void){
  HAL_Init();
  SystemClock_72MHz_Config();
  MX_GPIO_Init();
  RCC->APB2ENR |= RCC_APB2ENR_IOPCEN|RCC_APB2ENR_IOPAEN| RCC_APB2ENR_IOPBEN;
  	gpioSetUp();
  	adcSetUp();
  	timer1SetUp();
  	GPIOC->BSRR=(1<<13);

  while(1){
	  switch(state){
	  case 0:   // Stop
		  TIM1->CCR1 = 0; TIM1->CCR2 = 0; TIM1->CCR3 = 0;
		  GPIOB->BSRR = LUT_LowSide[7];
		  while((GPIOB->IDR>>0)&1) HAL_Delay(2);
		  setState();
		  break;
	  case 1:   // Reset
		  TIM1->CCR1=0; TIM1->CCR2=0; TIM1->CCR3=0;
		  for(volatile int i=0; i<100; i++);
		  GPIOB->BSRR = LUT_LowSide[6];
		  while((GPIOB->IDR>>0)&1) HAL_Delay(2);
		  setState();
		  break;
	  case 2:   // Loop
		  while((GPIOB->IDR>>0)&1){
			  __disable_irq();
			  uint16_t deathTime = deathTimeIQR;
			  uint16_t speed = speedIQR;
			  uint16_t pwm = pwmIQR;
			  __enable_irq();

			  TIM1->CCR1 = 0; TIM1->CCR2 = 0; TIM1->CCR3 = 0;
			  HAL_Delay((deathTime * 500)/4095);
			  for(volatile int i=0; i<10; i++);

			  GPIOB->BSRR = LUT_LowSide[step];
			  *LUT_HighSide_Reg[step] = pwm;

			  step++;
			  if(step>=6) step=0;
			  HAL_Delay((speed*1500)/4095); HAL_Delay(20);
		  }
		  setState();
		  break;
	  case 3:   // OverStep
		  while((GPIOB->IDR>>0)&1){
			  __disable_irq();
			  uint16_t pwm = pwmIQR;
			  __enable_irq();
			  GPIOB->BSRR = LUT_LowSide[step];
			  *LUT_HighSide_Reg[step] = pwm;
			  if(! ((GPIOB->IDR>>1)&1)) setStep();
			  HAL_Delay(2);
		  }
		  setState();
		  break;
	  default:
		  state=0;
		  break;
	  }
	  HAL_Delay(1);
  }
}
/////////////////////////////// CHƯƠNG TRÌNH CON ///////////////////////////////

void setStep(){
	GPIOC->BRR = (1<<13);
	step++;
	(step>=6)?(step=0):(step);
	while(!((GPIOB->IDR>>1)&1)) HAL_Delay(2);
	TIM1->CCR1=0; TIM1->CCR2=0; TIM1->CCR3=0;
	for(volatile int i=0; i<10; i++);
	GPIOB->BSRR = LUT_LowSide[6];
	for(volatile int i=0; i<100; i++);
	GPIOC->BSRR = (1<<13);
}

void setState(){
	TIM1->CCR1 = 0; TIM1->CCR2 = 0; TIM1->CCR3 = 0;
	GPIOB->BSRR = LUT_LowSide[7];
	GPIOC->BRR = (1<<13);
	state++;
	(state>=3)?(state=0):(state);
	while(!((GPIOB->IDR>>0)&1)){
		if(! ((GPIOB->IDR>>1)&1)) state = 3;
		while(!((GPIOB->IDR>>1)&1)) HAL_Delay(2);
		step=0;
	}
	GPIOC->BSRR = (1<<13);
}

void gpioSetUp(){
    //============ 1. CẤU HÌNH ADC ============//
    GPIOA->CRL &=~(0xFFFFFFF << 0); // PA0->PA6 (Analog Input)

    //============ 2. DIGITAL MCU ============//
    GPIOB->CRL = (GPIOB->CRL & ~(0xF << 0)) | (0b1000 << 0); // PB0 IN
    GPIOB->BSRR = (1 << 0);
    GPIOB->CRL = (GPIOB->CRL &~(0xF << 4)) | (0b1000 << 4);
    GPIOB->BSRR = (1 << 1);
    GPIOC->CRH = (GPIOC->CRH & ~(0xF << 20)) | (0b0010 << 20); // PC13 OUT

    //============ 3. ALTERNATE FUNCTION ============//
    GPIOA->CRH =(GPIOA->CRH &~(0xFFF<<0)) |(0b1011<<0) |(0b1011<<4) |(0b1011<<8) |(0b1011<<12); // PA9-10-11
    GPIOB->CRH =(GPIOB->CRH &~(0xFFF<<20)) |(0b0010<<20)|(0b0010<<24) |(0b0010<<28);            // PB13-14-15
}

void adcSetUp(){
    //========== ENABLE CLOCK FOR ADC1 & ADC2 ==========//
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
    RCC->CFGR &= ~(3 << 14);
    RCC->CFGR |=  (2 << 14);              // chia 6 = 12Mhz

    ADC1->CR1 |= ADC_CR1_SCAN;            // Bật Scan mode cho ADC1
    ADC1->CR1 |= ADC_CR1_JEOCIE;          // Bật Injected cho ADC1 (khác với regular dùng DMA)
    ADC1->CR2 |= ADC_CR2_JEXTTRIG;        // Cho phép kích hoạt bằng phần cứng ngoài
    ADC1->CR2 &= ~(7 << 12);              // Clear JEXTSEL
    ADC1->CR2 |= (1 << 12);               // Chọn TIM1_CC4 làm nguồn kích cho ADC1
    ADC1->SMPR2 &= ~(0x1FF << 12);        // SAMPLE TIME CH4-5-6
    ADC1->SMPR2 |=  (7 << 12) | (7 << 15) | (7 << 18);
    ADC1->JSQR = 0;
    //ADC1->JSQR = (2 << 20) | (6 << 15) | (5 << 10) | (4 << 5);
    ADC1->JSQR |= (2 << 20);              // Quét 3 chân
    ADC1->JSQR |= (4 << 5);               // JSQ2 = CH4 (DeathTime)
    ADC1->JSQR |= (5 << 10);              // JSQ3 = CH5 (Speed)
    ADC1->JSQR |= (6 << 15);              // JSQ4 = CH6 (PWM)
    ADC1->CR2 |= ADC_CR2_ADON;
    for(volatile int i=0; i<10000; i++);
    NVIC_EnableIRQ(ADC1_2_IRQn);
}
void ADC1_2_IRQHandler(void){
    if(ADC1->SR & ADC_SR_JEOC){
        deathTimeIQR = ADC1->JDR1;  // JSQ2
        speedIQR =     ADC1->JDR2;  // JSQ3
        pwmIQR =       ADC1->JDR3;  // JSQ4
        ADC1->SR &= ~ADC_SR_JEOC;
    }
}

void timer1SetUp(){
	RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;
	TIM1->PSC = 0;
	TIM1->ARR = 4095;
	TIM1->CCR1 = 0;
	TIM1->CCR2 = 0;
	TIM1->CCR3 = 0;
	TIM1->CCR4 = 500;
	TIM1->CCMR1 |= (6 << 4) | (6 << 12);   // CH1 + CH2
	TIM1->CCMR2 |= (6 << 4);               // CH3
	TIM1->CCMR2 &=~(7 << 12);
	TIM1->CCMR2 |=  (3 << 12);            // OC4 toggle mode
	TIM1->CCER |= TIM_CCER_CC1E | TIM_CCER_CC2E | TIM_CCER_CC3E | TIM_CCER_CC4E; //
	TIM1->BDTR |= TIM_BDTR_MOE;
	TIM1->CR1 |= TIM_CR1_CEN;
}



void SystemClock_72MHz_Config(void){
    RCC->CR |= RCC_CR_HSEON;
    while((RCC->CR & RCC_CR_HSERDY) == 0);
    FLASH->ACR |= FLASH_ACR_PRFTBE;          // Bật bộ đệm Prefetch
    FLASH->ACR &= ~FLASH_ACR_LATENCY;
    FLASH->ACR |= FLASH_ACR_LATENCY_2;       // 2 wait states (cho tần số 48MHz - 72MHz)
    RCC->CFGR |= RCC_CFGR_HPRE_DIV1;         // AHB Prescaler = 1 (HCLK = 72MHz)
    RCC->CFGR |= RCC_CFGR_PPRE2_DIV1;        // APB2 Prescaler = 1 (PCLK2 = 72MHz cho TIM1, ADC, GPIO)
    RCC->CFGR |= RCC_CFGR_PPRE1_DIV2;        // APB1 Prescaler = 2 (PCLK1 = 36MHz tối đa cho TIM2,3,4)
    RCC->CFGR &= ~(RCC_CFGR_PLLSRC | RCC_CFGR_PLLXTPRE | RCC_CFGR_PLLMULL); // Clear cũ
    RCC->CFGR |= RCC_CFGR_PLLSRC;        // Chọn HSE làm nguồn cho PLL
    RCC->CFGR |= RCC_CFGR_PLLMULL9;          // Nhân hệ số 9
    RCC->CR |= RCC_CR_PLLON;
    while((RCC->CR & RCC_CR_PLLRDY) == 0);
    RCC->CFGR &= ~RCC_CFGR_SW;
    RCC->CFGR |= RCC_CFGR_SW_PLL;            // Chọn PLL làm SYSCLK
    while((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);
    SystemCoreClock = 72000000;
}
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK){
    Error_Handler();
  }
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK){
    Error_Handler();
  }
}
static void MX_GPIO_Init(void){
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

}
void Error_Handler(void)
{
  __disable_irq();
  while(1){
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

