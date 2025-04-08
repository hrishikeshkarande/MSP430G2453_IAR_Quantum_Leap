/*
### What's Happening Here:
We're bypassing the DEFC macros from the #include <msp430.h> (which takes you to #include "msp430g2453.h" file) and using #define to specify the actual addresses of P1DIR and P1OUT.

((volatile unsigned char *) 0x0022) creates a pointer to the hardware register at address 0x0022, which is P1DIR.

Dereferencing the pointer using * allows direct access to that memory-mapped register.

We use bitwise OR (|=) to set bits and XOR (^=) to toggle them.

### volatile
This tells the compiler:

"This value may change at any time, outside of the program’s control — don’t optimize access to it."

Perfect for hardware registers that can change due to hardware events (like button presses, timer ticks, etc.).

*/


#define LED1_BIT    0x01    // P1.0
#define LED2_BIT    0x40    // P1.6

// Direct memory addresses from the datasheet / header file
#define P1DIR_ADDR  ((volatile unsigned char *) 0x0022)
#define P1OUT_ADDR  ((volatile unsigned char *) 0x0021)

int main(void)
{
    volatile unsigned int i;

    *P1DIR_ADDR |= (LED1_BIT | LED2_BIT);  // Set P1.0 and P1.6 as outputs

    while (1)
    {
        *P1OUT_ADDR ^= (LED1_BIT | LED2_BIT);  // Toggle P1.0 and P1.6

        for (i = 10000; i > 0; i--);           // Simple delay loop
    }
}
