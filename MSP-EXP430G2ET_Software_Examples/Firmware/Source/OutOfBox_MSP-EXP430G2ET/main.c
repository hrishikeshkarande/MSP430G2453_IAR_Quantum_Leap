/*
 * Copyright (C) 2011-2018 Texas Instruments Incorporated - http://www.ti.com/
 *
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *    Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *    Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 *    Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
*/


/******************************************************************************
 *                  MSP-EXP430G2ET - LaunchPad Out Of Box Demo
 *
 * 1. Device starts up in LPM3 + blinking LED to indicate device is alive
 *    + Upon first button press, device transitions to application mode
 * 2. Application Mode
 *    + Continuously sample ADC Temp Sensor channel, compare result against
 *      initial value
 *    + Set PWM based on measured ADC offset: Red LED for positive offset, Green
 *      LED for negative offset
 *    + Transmit temperature value via TimerA UART to PC
 *    + Button Press --> Calibrate using current temperature
 *                       Send character '�' via UART, notifying PC
 *
 * Changes:
 * 1.3  + Re-released for the new MSP-EXP430G2ET LaunchPad (replaced old EZ-430 debugger
 *        with the new eZ-FET debugger)
 *      + Changed UART speed to 9600 baud rate
 * 1.2  + Updated register naming conventions to reflect latest standard by TI
 *           e.g.: CCR0 --> TACCR0, CCTL0 --> TACCTL0
 *      + Changed method to capture TAR value into TACCR0 by using capture a
 *        SW-triggered event. [Changing TACCR input from GND to VCC]
 * 1.1  + LED1 & LED2 labels changed so that Green LED(LED2) indicates sampled
 *        temperature colder than calibrated temperature and vice versa
 *        with Red LED (LED1).
 *      + Turn off peripheral function of TXD after transmitting byte to
 *        eliminate the extra glitch at the end of UART transmission
 * 1.0  Initial Release Version
 *
 * Texas Instruments, Inc.
 ******************************************************************************/

#include  "msp430g2553.h"

#define     LED1                  BIT0
#define     LED2                  BIT6
#define     LED_DIR               P1DIR
#define     LED_OUT               P1OUT



#define     BUTTON                BIT3
#define     BUTTON_OUT            P1OUT
#define     BUTTON_DIR            P1DIR
#define     BUTTON_IN             P1IN
#define     BUTTON_IE             P1IE
#define     BUTTON_IES            P1IES
#define     BUTTON_IFG            P1IFG
#define     BUTTON_REN            P1REN

#define     TXD                   BIT1                      // TXD on P1.1
#define     RXD                   BIT2                      // RXD on P1.2

#define     APP_STANDBY_MODE      0
#define     APP_APPLICATION_MODE  1

#define     TIMER_PWM_MODE        0
#define     TIMER_UART_MODE       1
#define     TIMER_PWM_PERIOD      500
#define     TIMER_PWM_OFFSET      20

#define     TEMP_SAME             0
#define     TEMP_HOT              1
#define     TEMP_COLD             2

#define     TEMP_THRESHOLD        5

//   Conditions for 9600 Baud SW UART, SMCLK = 1MHz
#define     Bitime_5              0x05                      // ~ 0.5 bit length + small adjustment
#define     Bitime                13//0x0D

#define     UART_UPDATE_INTERVAL  1000


unsigned char BitCnt;


unsigned char applicationMode = APP_STANDBY_MODE;
unsigned char timerMode = TIMER_PWM_MODE;

unsigned char tempMode;
unsigned char calibrateUpdate = 0;
unsigned char tempPolarity = TEMP_SAME;
unsigned int TXByte;

/* Using an 8-value moving average filter on sampled ADC values */
long tempMeasured[8];
unsigned char tempMeasuredPosition=0;
long tempAverage;

long tempCalibrated, tempDifference;


void InitializeLeds(void);
void InitializeButton(void);
void PreApplicationMode(void);                     // Blinks LED, waits for button press
void ConfigureAdcTempSensor(void);
void ConfigureTimerPwm(void);
void ConfigureTimerUart(void);
void Transmit(void);
void InitializeClocks(void);


void main(void)
{
  unsigned int uartUpdateTimer = UART_UPDATE_INTERVAL;
  unsigned char i;

  WDTCTL = WDTPW + WDTHOLD;                 // Stop WDT

  InitializeClocks();
  InitializeButton();
  InitializeLeds();
  PreApplicationMode();                     // Blinks LEDs, waits for button press

  /* Application Mode begins */
  applicationMode = APP_APPLICATION_MODE;
  ConfigureAdcTempSensor();
  ConfigureTimerPwm();

  __enable_interrupt();                     // Enable interrupts.

  /* Main Application Loop */
  while(1)
  {
    ADC10CTL0 |= ENC + ADC10SC;             // Sampling and conversion start
    __bis_SR_register(CPUOFF + GIE);        // LPM0 with interrupts enabled


    /* Moving average filter out of 8 values to somewhat stabilize sampled ADC */
    tempMeasured[tempMeasuredPosition++] = ADC10MEM;
    if (tempMeasuredPosition == 8)
      tempMeasuredPosition = 0;
    tempAverage = 0;
    for (i = 0; i < 8; i++)
      tempAverage += tempMeasured[i];
    tempAverage >>= 3;                      // Divide by 8 to get average

    if ((--uartUpdateTimer == 0) || calibrateUpdate )
    {
      ConfigureTimerUart();
      if (calibrateUpdate)
      {
        TXByte = 248;                       // A character with high value, outside of temp range
        Transmit();
        calibrateUpdate = 0;
      }
      TXByte = (unsigned char)( ((tempAverage - 630) * 761) / 1024 );

      Transmit();

      uartUpdateTimer = UART_UPDATE_INTERVAL;
      ConfigureTimerPwm();
    }

    tempDifference = tempAverage - tempCalibrated;
    if (tempDifference < -TEMP_THRESHOLD)
    {
      tempDifference = -tempDifference;
      tempPolarity = TEMP_COLD;
      LED_OUT &= ~ LED1;
    }
    else
    if (tempDifference > TEMP_THRESHOLD)
    {
      tempPolarity = TEMP_HOT;
      LED_OUT &= ~ LED2;
    }
    else
    {
      tempPolarity = TEMP_SAME;
      TACCTL0 &= ~CCIE;
      TACCTL1 &= ~CCIE;
      LED_OUT &= ~(LED1 + LED2);
    }

    if (tempPolarity != TEMP_SAME)
    {
      tempDifference <<= 3;
      tempDifference += TIMER_PWM_OFFSET;
      TACCR1 = ( (tempDifference) < (TIMER_PWM_PERIOD-1) ? (tempDifference) : (TIMER_PWM_PERIOD-1) );
      TACCTL0 |= CCIE;
      TACCTL1 |= CCIE;
    }
  }
}

void PreApplicationMode(void)
{
  LED_DIR |= LED1 + LED2;
  LED_OUT |= LED1;                          // To enable the LED toggling effect
  LED_OUT &= ~LED2;

  BCSCTL1 |= DIVA_1;                        // ACLK/2
  BCSCTL3 |= LFXT1S_2;                      // ACLK = VLO

  TACCR0 = 1200;                             //
  TACTL = TASSEL_1 | MC_1;                  // TACLK = SMCLK, Up mode.
  TACCTL1 = CCIE + OUTMOD_3;                // TACCTL1 Capture Compare
  TACCR1 = 600;
  __bis_SR_register(LPM3_bits + GIE);          // LPM0 with interrupts enabled
}

void ConfigureAdcTempSensor(void)
{
  unsigned char i;
  /* Configure ADC Temp Sensor Channel */
  ADC10CTL1 = INCH_10 + ADC10DIV_3;         // Temp Sensor ADC10CLK/4
  ADC10CTL0 = SREF_1 + ADC10SHT_3 + REFON + ADC10ON + ADC10IE;
  __delay_cycles(1000);                     // Wait for ADC Ref to settle
  ADC10CTL0 |= ENC + ADC10SC;               // Sampling and conversion start
  __bis_SR_register(CPUOFF + GIE);          // LPM0 with interrupts enabled
  tempCalibrated = ADC10MEM;
  for (i=0; i < 8; i++)
    tempMeasured[i] = tempCalibrated;
  tempAverage = tempCalibrated;
}


void ConfigureTimerPwm(void)
{
  timerMode = TIMER_PWM_MODE;

  TACCR0 = TIMER_PWM_PERIOD;                              //
  TACTL = TASSEL_2 | MC_1;                  // TACLK = SMCLK, Up mode.
  TACCTL0 = CCIE;
  TACCTL1 = CCIE + OUTMOD_3;                // TACCTL1 Capture Compare
  TACCR1 = 1;
}

void ConfigureTimerUart(void)
{
  timerMode = TIMER_UART_MODE;              // Configure TimerA0 UART TX

  TACCTL0 = OUT;                              // TXD Idle as Mark
  TACTL = TASSEL_2 + MC_2 + ID_3;           // SMCLK/8, continuous mode
  P1SEL |= TXD + RXD;                       //
  P1DIR |= TXD;                             //
}

// Function Transmits Character from TXByte
void Transmit()
{
  BitCnt = 0xA;                             // Load Bit counter, 8data + ST/SP

  /* Simulate a timer capture event to obtain the value of TAR into the TACCR0 register */
  TACCTL0 = CM_1 + CCIS_2  + SCS + CAP + OUTMOD0;       //capture on rising edge, initially set to GND as input // clear CCIFG flag
  TACCTL0 |= CCIS_3;                        //change input to Vcc, effectively rising the edge, triggering the capture action

  while (!(TACCTL0 & CCIFG));               //allowing for the capturing//updating TACCR0.

  TACCR0 += Bitime ;                           // Some time till first bit
  TXByte |= 0x100;                          // Add mark stop bit to TXByte
  TXByte = TXByte << 1;                     // Add space start bit
  TACCTL0 =  CCIS0 + OUTMOD0 + CCIE;          // TXD = mark = idle

  while ( TACCTL0 & CCIE );                   // Wait for TX completion
}



// Timer A0 interrupt service routine
#pragma vector=TIMER0_A0_VECTOR
__interrupt void Timer_A (void)
{
  if (timerMode == TIMER_UART_MODE)
  {
    TACCR0 += Bitime;                         // Add Offset to TACCR0
    if (TACCTL0 & CCIS0)                      // TX on CCI0B?
    {
      if ( BitCnt == 0)
      {
        P1OUT |= TXD;
        P1SEL &= ~(TXD+RXD);
        TACCTL0 &= ~ CCIE ;                   // All bits TXed, disable interrupt
      }

      else
      {
        TACCTL0 |=  OUTMOD2;                  // TX Space
        if (TXByte & 0x01)
        TACCTL0 &= ~ OUTMOD2;                 // TX Mark
        TXByte = TXByte >> 1;
        BitCnt --;
      }
    }
  }
  else
  {
    if (tempPolarity == TEMP_HOT)
      LED_OUT |= LED1;
    if (tempPolarity == TEMP_COLD)
      LED_OUT |= LED2;
    TACCTL0 &= ~CCIFG;
  }
}

#pragma vector=TIMER0_A1_VECTOR
__interrupt void ta1_isr(void)
{
  TACCTL1 &= ~CCIFG;
  if (applicationMode == APP_APPLICATION_MODE)
    LED_OUT &= ~(LED1 + LED2);
  else
    LED_OUT ^= (LED1 + LED2);

}

void InitializeClocks(void)
{

  BCSCTL1 = CALBC1_1MHZ;                    // Set range
  DCOCTL = CALDCO_1MHZ;
  BCSCTL2 &= ~(DIVS_3);                     // SMCLK = DCO = 1MHz
}

void InitializeButton(void)                 // Configure Push Button
{
  BUTTON_DIR &= ~BUTTON;
  BUTTON_OUT |= BUTTON;
  BUTTON_REN |= BUTTON;
  BUTTON_IES |= BUTTON;
  BUTTON_IFG &= ~BUTTON;
  BUTTON_IE |= BUTTON;
}


void InitializeLeds(void)
{
  LED_DIR |= LED1 + LED2;
  LED_OUT &= ~(LED1 + LED2);
}

/* *************************************************************
 * Port Interrupt for Button Press
 * 1. During standby mode: to exit and enter application mode
 * 2. During application mode: to recalibrate temp sensor
 * *********************************************************** */
#pragma vector=PORT1_VECTOR
__interrupt void PORT1_ISR(void)
{
  BUTTON_IFG = 0;
  BUTTON_IE &= ~BUTTON;            /* Debounce */
  WDTCTL = WDT_ADLY_250;
  IFG1 &= ~WDTIFG;                 /* clear interrupt flag */
  IE1 |= WDTIE;

  if (applicationMode == APP_APPLICATION_MODE)
  {
    tempCalibrated = tempAverage;
    calibrateUpdate  = 1;
  }
  else
  {
    applicationMode = APP_APPLICATION_MODE; // Switch from STANDBY to APPLICATION MODE
    __bic_SR_register_on_exit(LPM3_bits);
  }
}

// WDT Interrupt Service Routine used to de-bounce button press
#pragma vector=WDT_VECTOR
__interrupt void WDT_ISR(void)
{
    IE1 &= ~WDTIE;                   /* disable interrupt */
    IFG1 &= ~WDTIFG;                 /* clear interrupt flag */
    WDTCTL = WDTPW + WDTHOLD;        /* put WDT back in hold state */
    BUTTON_IE |= BUTTON;             /* Debouncing complete */
}

// ADC10 interrupt service routine
#pragma vector=ADC10_VECTOR
__interrupt void ADC10_ISR (void)
{
  __bic_SR_register_on_exit(CPUOFF);        // Return to active mode
}



