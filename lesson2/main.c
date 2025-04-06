int main()
{       
    /*
    here we observed that in the id else the prog gives new register the declared variables.
    it then increments them based on the PC.
    while uses the compare pnemonics instruction. 
    */
    int counter = 0;
    int odd_var = 0;
    int even_var = 0;
    while (counter <21){
          ++counter;
          if ((counter & 1) != 0){
            //do something when counter is odd
            odd_var++;
          }
          else{
            //do something when counter is even
             even_var++;
          }
    }
  
  return 0;
}
