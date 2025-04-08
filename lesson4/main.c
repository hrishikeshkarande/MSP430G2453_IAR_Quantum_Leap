#include <msp430.h> // You can open this file (Then open #include "msp430g2453.h" file) to look at P1DIR and P1OUT definitions, This is port 1 and the pin 0 and pin 6 are connected to the LEDS 
#define b00000001 0x0001 //Here we first need to define the binary bits of the LEDs from the Hex values
#define b01000000 0x0040 //#define is used to define the thing we will be using next its definition is in front of it


int main(void)
{
    volatile unsigned int i;
    WDTCTL = WDTPW + WDTHOLD;                 // Stop watchdog timer
    P1DIR |= b00000001;                            // Set P1.0 to output direction
    P1DIR |= b01000000;                            // Set P1.6 to output direction

    while(1)
    {
        P1OUT ^= b00000001;                        // Toggle P1.0 using exclusive-OR
        P1OUT ^= b01000000;                        // Toggle P1.6 using exclusive-OR

        for (i=10000; i>0; i--);
  }
}