/*
# We will learn about bit-wise operations in this one
Perform this in the Simulator and set the compiler optimization to NONE.


FIX for Warning
Warning[Pe069]: integer conversion resulted in truncation

# Explanation
0xDEADBEEF is a 32-bit hex literal ? 1101 1110 1010 1101 1011 1110 1110 1111

But on the MSP430 (a 16-bit microcontroller), unsigned int is only 16 bits, so the upper 16 bits get chopped off.

You end up with just 0xBEEF, and the compiler warns you: "Hey, I truncated this."

# Solutions
Declare b as a 32-bit type
If you intended to use all 32 bits:

CODE unsigned long b = 0xDEADBEEF;

*/
int main(void) {
    unsigned int a = 0x5A5A5A5A;
    unsigned long b = 0xDEADBEEF;
    unsigned int c;

    c = a | b;   // OR
    c = a & b;   // AND
    c = a ^ b;   // XOR
    c = ~b;      // NOT
    c = a << 1;  // lef-shift
    c = a << 2;
    c = b >> 1;  // right-shift
    c = b >> 3;

    int x = 1024;
    int y = -1024;
    int z = 0;

    z = x >> 3;
    z = y >> 3;

    //return 0; // unreachable code
}