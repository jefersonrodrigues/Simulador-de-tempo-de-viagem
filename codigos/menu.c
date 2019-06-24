#include <stdio.h>
#include <curses.h>
void main()
{

int escolha=1;
printf("\n\n ----------- Simulador de viagem v1.0 ------------ \n\n");
// se a escolha for diferente de 5, ele continua... o que inicialmente é verdade
// pois escolha é igual a 1
while (escolha!=0)
{


    printf("\n Digite o destino desejado\n");
    printf("\n 1 - Santo Amaro ");
    printf("\n 2 - Pinheiros");
    printf("\n 3 - Osasco");
    printf("\n\nEscolha uma opção: ");
    scanf("%d",&escolha);


    // estrutura switch
    switch (escolha) {

      case 1:
          printf("\033c");
          printf("\n\nO destino escolhido foi Santo Amaro \n\n");
          escolha =0;
          break;

      case 2:
          printf("\033c");
          printf("\n\nO destino escolhido foi Pinheiros \n\n");
          escolha =0;
          break;

      case 3:
          printf("\033c");
          printf("\n\nO destino escolhido foi Osasco \n\n");
          escolha =0;
          break;

      default:
          printf("\033c");
          printf("\n\n Escolha inválida, por favor digite uma opção válida!\n\n");
          break;

        }
}

}
