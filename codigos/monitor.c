#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>		// inet_aton
#include <pthread.h>
#include <locale.h>
#include <time.h>


int sockfd;

    //Função para leitura do socket: Lê informação e salva em buffer

void *leitura(void *arg) {
    char buffer[256];
    int n, op;
    char *velocidade_media, *id_command, *velocity, *best_opt;
    int ctrl_print = 0;
    while (1) {
        bzero(buffer,sizeof(buffer));
        n = recv(sockfd,buffer,50,0);

        if (n <= 0) {
            printf("Erro lendo do socket!\n");
            exit(1);
        }

        /*
         *  Identificação dos comandos para amostragem de dados:
         * id_command recebe o comando.
         * velocidade_media recebe o calculo do velocidade_media.
         */
        if (buffer[0] == '0'){
            id_command = strtok(buffer, " ");
            velocidade_media = strtok(NULL, " ");
            printf("\n\nA VELOCIDADE MEDIA TEÓRICA DO TRAJETO= %.4s\n",velocidade_media );
        }else if (buffer[0] == '1'){
            velocity = strtok(buffer, " ");
            id_command = strtok(NULL, " ");
            printf("\n\nA VELOCIDADE MEDIA REAL DO TRAJETO (COM ATRASOS) = %.4s\n",id_command);
        }else{
            id_command = strtok(buffer, " ");
            best_opt = strtok(NULL, " ");
            op = atoi(best_opt);
            printf("%d", op);
            if(op == 10)
                printf("\nA OPÇÃO MAIS RÁPIDA É IR DE CARRO E O TEMPO DE VIAGEM SERÁ DE %s\n", best_opt);
            else if(best_opt == 20)
                printf("\nA OPÇÃO MAIS RÁPIDA É IR DE ÔNIBUS E O TEMPO DE VIAGEM SERÁ DE  %.4s\n", best_opt);
            else if(best_opt == 30)
                printf("\nA OPÇÃO MAIS RÁPIDA É IR DE METRO E O TEMPO DE VIAGEM SERÁ DE  %.4s\n", best_opt);
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
    printf("\n\t\t\tSIMULADOR DE VIAGEM - v1.0"); //capturar comandos aqui
    printf("\n\t\t\tSÃO PAULO, %i/%i/%i\n",myTime->tm_mday, myTime->tm_mon +1, 1900+myTime->tm_year);
    printf("\t\t\tESTAÇÃO GRAJAÚ - %ih%imin\n", myTime->tm_hour, myTime->tm_min);
    printf("[+]\n\n\tInforme os comandos:\n");


    do {
        bzero(buffer,sizeof(buffer));   //Limpa buffer
        fgets(buffer,50,stdin);
        if (strcmp(buffer,"exit\n") == 0) {
            break;
        }
        // Condição para comandos aceitos
        if (buffer[0] == '0' || buffer[0] == '1' || buffer[0] == '2'){
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
