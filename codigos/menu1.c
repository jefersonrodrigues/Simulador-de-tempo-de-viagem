#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <locale.h>


int main(){
    //Função para utilizar Acentos no Printf
    setlocale(LC_ALL, "Portuguese");
    time_t currentTime;
		time(&currentTime);
		struct tm *myTime = localtime(&currentTime);

    printf("\n|------------------------ SÃO PAULO/SP | ESTAÇÃO GRAJAÚ --------------------|"); //capturar comandos aqui
    printf("\n                             %i/%i/%i - %ih%imin                            ",myTime->tm_mday, myTime->tm_mon +1, 1900+myTime->tm_year, myTime->tm_hour, myTime->tm_min);
    printf("\n\n[+]\t Informe os comandos:\n");


    return 0;
}
