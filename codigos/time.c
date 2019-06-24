#include <stdio.h>
#include <stdlib.h>
#include <time.h>


int main(void){
  time_t currentTime;
  time(&currentTime);
  int atraso = 0;
  struct tm *myTime = localtime(&currentTime);
  printf("%i\n",myTime->tm_hour);

  if(myTime->tm_hour == 18 || myTime->tm_hour == 22){
    atraso = 18;
  }else{
    atraso = 10;
  }
  printf("transito atual: %ikm/h\n",atraso);

}
