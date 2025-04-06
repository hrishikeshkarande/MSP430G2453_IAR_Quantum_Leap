    /*
    Learning variables and pointers
    */

unsigned long counter = 0;

int main()
{     
    unsigned long *p_int; //this declares the pointer (which is nothing but a container to store addresses) of the type integer
    p_int = &counter; //here and sign is used to assign the address of the counter global variable to the p_int pointer
    while (*p_int <21){ //this is called de-referencing the pointer, putting a star behind the name of the pointer in an expression gives us the value stores in that pointer at that place
          ++(*p_int); //here we are incrementing the value inside the address that the pointer p_int is pointing to, i.e counter's value, we are doing this using de-referencing
    }
    
    p_int = (unsigned long *)0x20000002U;
    *p_int = 0xDEADBEEF;            //changing the pointer to unsigned int worked 
    //this statement leads to misalignment in the memory register, it stores the BEEF before the DEAD
  
  return 0;
}
