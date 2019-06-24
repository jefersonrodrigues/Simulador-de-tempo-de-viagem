#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <locale.h>
#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <locale.h>
#include <time.h>


int sockfd;

    //Função para leitura do socket, lê informação e salva em buffer

void *leitura(void *arg) {
    char buffer[256];
    int n, command_i, op;
    float command;
    char *velocidade_media, *id_command, *velocity, *best_opt;
    int ctrl_print = 0;
    while (1) {
        bzero(buffer,sizeof(buffer));
        n = recv(sockfd,buffer,50,0);

        if (n <= 0) {
            printf("Erro lendo do socket!\n");
            exit(1);
        }

//Identificação dos comandos para amostragem de dados:
//id_command recebe o comando.
//velocidade_media recebe o calculo do velocidade_media.
//best_opt recebe o valor da melhor opção, sendo 1 = carro, 2= ônibus e 3 = metro.

        if (buffer[0] == '0'){
            id_command = strtok(buffer, " ");
            velocidade_media = strtok(NULL, " ");
            printf("\n\t[>>] Velocidade média: %.4s km/h\n",velocidade_media );
        }else if (buffer[0] == '1'){
            velocity = strtok(buffer, " ");
            id_command = strtok(NULL, " ");
            command = atof(id_command);
            command_i = atoi(id_command);
            printf("\n\t[>>] Tempo de duração: %.2d horas e %.0f minutos\n", command_i, (command-command_i)*60);
        }else if(buffer[0] == '2'){
            id_command = strtok(buffer, " ");
            best_opt = strtok(NULL, " ");
            op = atoi(best_opt);
            if(op == 1){
                printf("\n\t[>>] É melhor ir de: Carro. \n\t[>>] Tempo até chegar ao destino: %.2d horas e %.0f minutos\n", command_i, (command-command_i)*60);
            }else if(op == 2){
              printf("\n\t[>>] É melhor ir de: Ônibus. \n\t[>>] Tempo até chegar ao destino: %.2d horas e %.0f minutos\n", command_i, (command-command_i)*60);
            }else if(op == 3){
              printf("\n\t[>>] É melhor ir de: Metrô. \n\t[>>] Tempo até chegar ao destino: %.2d horas e %.0f minutos\n", command_i, (command-command_i)*60);
            }
        }
    }
  }


int main(int argc, char *argv[]) {
    //Função para utilizar Acentos no Printf
    setlocale(LC_ALL, "Portuguese");
    time_t currentTime;
		time(&currentTime);
		struct tm *myTime = localtime(&currentTime);

    int portno, n;
    struct sockaddr_in serv_addr;
    pthread_t t;
    char buffer[256];

    if (argc < 3) {
       fprintf(stderr,"Uso: %s nomehost porta\n", argv[0]);
       exit(0);
    }

    portno = atoi(argv[2]);     // Pega numero da porta

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        printf("Erro criando socket!\n");
        return -1;
    }

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    inet_aton(argv[1], &serv_addr.sin_addr);
    serv_addr.sin_port = htons(portno);
    if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) {
        printf("Erro conectando!\n");
        return -1;
    }

    pthread_create(&t, NULL, leitura, NULL);
    printf("\n|---------------------------- [SÃO PAULO/SP | ESTAÇÃO GRAJAÚ] -------------------------|"); //capturar comandos aqui
    printf("\n                                  [ %i/%i/%i - %ih%imin ]                               ",myTime->tm_mday, myTime->tm_mon +1, 1900+myTime->tm_year, myTime->tm_hour, myTime->tm_min);
    printf("\nDESTINO: Estação Osasco\n");
    printf("\n\n>> Comandos\n \t0 - Calcula velocidade média do trajeto de acordo com o meio de transporte selecionado \n \t1 - Calcula o tempo de deslocamento de acordo com o meio de transporte indicado\n \t2 - Calcula a melhor opção disponível, baseado no menor tempo de viagem\n \texit - Para encerra o programa");
    printf("\n\n>> Meios de transporte\n \t1 - Carro\n \t2 - Ônibus\n \t3 - Metrô");
    printf("\n\n>> insira os comandos seguindo o modelo: [comando][tecla espaço][meio de transporte]\n\n");

    printf("\n\n[?] Informe os comandos:\n");


    do {
        bzero(buffer,sizeof(buffer));   //Limpa buffer
        fgets(buffer,50,stdin);
        if (strcmp(buffer,"exit\n") == 0) {
            break;
        }
        // Condição para comandos aceitos
        if ((buffer[0] == '0' || buffer[0] == '1' || buffer[0] == '2') && (buffer[2] == '0' || buffer[2] == '1' || buffer[2] == '2') || buffer[2] == '3'){
            n = send(sockfd,buffer,50,0);
            if (n == -1) {
                printf("Erro escrevendo no socket!\n");
                return -1;
            }
        }else {
            printf("\n[+]\tComando -> %s\tInválido, informe o comando novamente.\n\n", buffer);
        }
    } while (1);
    close(sockfd);

    return 0;
}
