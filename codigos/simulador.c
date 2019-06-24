/***************************************************************************************
 ******************                  SIMULATOR                        ******************
 * MAIN:                                                                               *
 *      Efetua comunicação com monitor.                                                *
 *      Criação das Threads.                                                           *
 *                                                                                     *
 * RECEIVER:                                                                           *
 *          Conversão e atribuição dos valores para                                    *
 *          as variáveis globais.                                                      *
 *                                                                                     *
 * IMC:                                                                                *
 *      Efetua o calculo do IMC e atualiza variáves globais.                           *
 *      Conversão para string.                                                         *
 *                                                                                     *
 * MEDIDAS MEDIAS:                                                                     *
 *              Efetua o calculo das médias, atraves das variáveis globais que         *
 *              são sempre atualizadas.                                                *
 *              Conversão para string.                                                 *
 *                                                                                     *
 * TENDENCIA:                                                                          *
 *          Efetua o calculo da tendencia, considerando 10% o nível do IMC.            *
 *          Necessita do valor do IMC para fazer estimativa.                           *
 *          Conversão para string.                                                     *
 *                                                                                     *
 * SEND RESULT:                                                                        *
 *          Percorre buffer até encontrar comando específico.                          *
 *          Atualiza buffer.                                                           *
 *          Faz a comunicação com o monitor, retornando o resultado.                   *
 ***************************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <locale.h>
#include <time.h>
#include <signal.h>

//time of execution of periodic tasks
#define TIME_IMC 9000000
#define TIME_MED 6000000
#define TIME_TEND 3000000

#define T_buffers 256   //size of informations buffer

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t protect = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

int newsockfd;
int id_comando[100], linhas = 0, id_co_destino_alt = 0, n_destinos = 0, n_meio_transportes = 0, id_send = 0, find = 0, i_save = 0, vel_com_vel_atraso = 0;
double destino[100], meio_transporte[100], med_destino = 0, med_meio_transporte = 0, vel_carro = 0, vel_onibus = 0, vel_metro = 0, send_med_destino = 0, send_med_meio_transporte = 0, vel_atraso_carro=0, vel_atraso_onibus=0, vel_atraso_metro=0, melhor_op=0, tempo_carro=0, tempo_onibus=10, tempo_metro=20, menor_tempo=0;
char save[T_buffers][5];
int vazio = 0;

// Periodic threads using POSIX timers
struct periodic_info
{
	int sig;
	sigset_t alarm_sig;
};

// periodic functions
void* calcula_velocidade(void *arg);
void* best_opt(void *arg);
void* atraso(void *arg);

void* send_results(void *arg);
static int make_periodic(int unsigned period, struct periodic_info *info);
static void wait_period(struct periodic_info *info);

//make periodic
static int make_periodic(int unsigned period, struct periodic_info *info)
{ //(intervalo, referência pra estrutura)
	static int next_sig;
	int ret;
	unsigned int ns;
	unsigned int sec;
	struct sigevent sigev;
	timer_t timer_id;
	struct itimerspec itval;
	/* Initialise next_sig first time through. We can't use static
	   initialisation because SIGRTMIN is a function call, not a constant */
	if (next_sig == 0)
		next_sig = SIGRTMIN;
	/* Check that we have not run out of signals */
	if (next_sig > SIGRTMAX)
		return -1;
	info->sig = next_sig;
	next_sig++;
	/* Create the signal mask that will be used in wait_period */
	sigemptyset(&(info->alarm_sig));
	sigaddset(&(info->alarm_sig), info->sig);
	/* Create a timer that will generate the signal we have chosen */
	sigev.sigev_notify = SIGEV_SIGNAL;
	sigev.sigev_signo = info->sig;
	sigev.sigev_value.sival_ptr = (void *)&timer_id;
	ret = timer_create(CLOCK_MONOTONIC, &sigev, &timer_id);
	if (ret == -1)
		return ret;
	/* Make the timer periodic */
	sec = period / 1000000;
	ns = (period - (sec * 1000000)) * 1000;
	itval.it_interval.tv_sec = sec;
	itval.it_interval.tv_nsec = ns;
	itval.it_value.tv_sec = sec;
	itval.it_value.tv_nsec = ns;
	ret = timer_settime(timer_id, 0, &itval, NULL);
	return ret;
}
static void wait_period(struct periodic_info *info)
{
	int sig;
	sigwait(&(info->alarm_sig), &sig);
}


//RECEIVER
void *receiver(void *arg) {  /* Recebe dados e identifica comandos */
	int n, ctrl;
  char buffer[T_buffers], *aux;
  while (1) {
  	bzero(buffer,sizeof(buffer));  /* zera buffer para receber dados */
    n = read(newsockfd,buffer,50);

    if (n <= 0) {           /* condição para fechar = 0 */
        printf("Erro lendo do socket!\n");
        exit(1);
    }

  //Atribui valores recebidos para as variáveis, já convertidos para que as operações sejam efetuadas

    pthread_mutex_lock(&protect);
    ctrl = 0;
    aux = strtok(buffer, " ");
    id_comando[id_co_destino_alt] = atoi(aux);
    while (aux != NULL) {
    	aux = strtok(NULL, " ");
      if (ctrl == 0){         // Controle para salvar o primeiro - meio_transporte
      	meio_transporte[id_co_destino_alt] = atof(aux);
        ctrl = 1;
      }else if (ctrl == 1){   // Controle para salvar o segundo - destino
        destino[id_co_destino_alt] = atof(aux);
        ctrl = 2;           // Não salva mais
      }
    }
    if(id_co_destino_alt == T_buffers)       /* Começa a sobreescrever */
    	id_co_destino_alt = 0;
    else
      id_co_destino_alt++;
    pthread_mutex_unlock(&protect);
   }
}


// CALCULA VELOCIDADE MÉDIA (TEÓRICA)
void*  calcula_velocidade(void *arg){
	char imc_char[50];
  struct periodic_info info;
	make_periodic(TIME_IMC, &info);
  	while(1){
      pthread_mutex_lock(&protect);
			if(id_comando[linhas] == 0 && destino[linhas] != 0 && meio_transporte[linhas] == 1   ){   /* Só efetua o calculo se não forem zero */
      	vel_carro = 18.1;
        sprintf(imc_char, "%f", vel_carro);       /* conversão para char para poder colocar no buffer */
        if(id_comando[linhas] == 0){
        	//Começa a salvar no buffer
          save[linhas][0] = '0';
          save[linhas][1] = ' ';
        	for(int i = 0; i < strlen(imc_char); ++i){
              save[linhas][i+2] = imc_char[i];
          }
        if(linhas == T_buffers)   /* Sobrescreve */
        	linhas = 0;
        else
          linhas++;
        }
        vazio = 1;
        pthread_cond_signal(&cond);
				}else if(id_comando[linhas] == 0 && destino[linhas] != 0 && meio_transporte[linhas] == 2){
				vel_onibus = 20.8;
				sprintf(imc_char, "%f", vel_onibus); //Conversão para char para poder colocar no buffer
				if(id_comando[linhas] == 0){
					//Começa a salvar no buffer
					save[linhas][0] = '0';
					save[linhas][1] = ' ';
					for(int i = 0; i < strlen(imc_char); ++i){
						save[linhas][i+2] = imc_char[i];
						}
					if(linhas == T_buffers)   // Sobrescreve
						linhas = 0;
					else
						linhas++;
					}
					vazio = 1;
					pthread_cond_signal(&cond);
				}else if(id_comando[linhas] == 0 && destino[linhas] != 0 && meio_transporte[linhas] == 3){
					vel_metro = 30;
					sprintf(imc_char, "%f", vel_metro);       /* conversão para char para poder colocar no buffer */
					if(id_comando[linhas] == 0){
						//Começa a salvar no buffer
						save[linhas][0] = '0';
						save[linhas][1] = ' ';
						for(int i = 0; i < strlen(imc_char); ++i){
						save[linhas][i+2] = imc_char[i];
						}
					if(linhas == T_buffers)   /* Sobrescreve */
						linhas = 0;
					else
						linhas++;
					}
					vazio = 1;
					pthread_cond_signal(&cond);
				}
    pthread_mutex_unlock(&protect);
		wait_period(&info);
    }
}

//CALCULA TEMPOS E MELHOR OPÇÃO DE ESCOLHA
void* best_opt(void *arg){
		char char_menortempo[50];
		struct periodic_info info;
		make_periodic(TIME_TEND, &info);
	  while(1){
			pthread_mutex_lock(&protect);
			if(id_comando[linhas] == 2 && destino[linhas] == 0){   /* Só efetua o calculo se não forem zero */
				tempo_carro = 18.2/vel_atraso_carro;
				tempo_onibus = 18.2/vel_atraso_onibus;
				tempo_metro = 18.2/vel_atraso_metro;

				if(tempo_carro < tempo_onibus && tempo_carro < tempo_metro){
					menor_tempo = tempo_carro;
					melhor_op = 10;
				}else if(tempo_onibus < tempo_carro && tempo_onibus <tempo_metro){
					menor_tempo = tempo_onibus;
					melhor_op = 20;
				}else if(tempo_metro < tempo_carro && tempo_metro <tempo_onibus){
					menor_tempo = tempo_metro;
					melhor_op = 30;
				}
				sprintf(char_menortempo, "%f", melhor_op);
				save[linhas][0] = '2';
		    save[linhas][1] = ' ';
				for(int i = 0; i < strlen(char_menortempo); ++i){
		     save[linhas][i+2] = char_menortempo[i];
		    }
				if(linhas == T_buffers)   /* Sobrescreve */
		     linhas = 0;
		    else
		     linhas++;
      	vazio = 1;
			  pthread_cond_signal(&cond);
      }else if(id_comando[linhas] == 2 && destino[linhas] == 1){   /* Só efetua o calculo se não forem zero */
				tempo_carro = 31.4/vel_atraso_carro;
				tempo_onibus = 31.4/vel_atraso_onibus;
				tempo_metro = 31.4/vel_atraso_metro;
				if(tempo_carro < tempo_onibus && tempo_carro < tempo_metro){
					menor_tempo = tempo_carro;
					melhor_op = 10;
				}else if(tempo_onibus < tempo_carro && tempo_onibus <tempo_metro){
					menor_tempo = tempo_onibus;
					melhor_op = 20;
				}else if(tempo_metro < tempo_carro && tempo_metro <tempo_onibus){
					menor_tempo = tempo_metro;
					melhor_op = 30;
				}
				sprintf(char_menortempo, "%f", melhor_op);
				save[linhas][0] = '2';
		    save[linhas][1] = ' ';
				for(int i = 0; i < strlen(char_menortempo); ++i){
		    save[linhas][i+2] = char_menortempo[i];
		    }
				if(linhas == T_buffers)   /* Sobrescreve */
		     linhas = 0;
		    else
		     linhas++;

			  vazio = 1;
			  pthread_cond_signal(&cond);
        }else	if(id_comando[linhas] == 2 && destino[linhas] == 2){   /* Só efetua o calculo se não forem zero */
					tempo_carro = 41.7/vel_atraso_carro;
					tempo_onibus = 41.7/vel_atraso_onibus;
					tempo_metro = 41.7/vel_atraso_metro;

				if(tempo_carro < tempo_onibus && tempo_carro < tempo_metro){
					menor_tempo = tempo_carro;
					melhor_op = 10;
				}else if(tempo_onibus < tempo_carro && tempo_onibus <tempo_metro){
					menor_tempo = tempo_onibus;
					melhor_op = 20;
				}else if(tempo_metro < tempo_carro && tempo_metro <tempo_onibus){
					menor_tempo = tempo_metro;
					melhor_op = 30;
				}
				sprintf(char_menortempo, "%f", melhor_op);
				save[linhas][0] = '2';
		    save[linhas][1] = ' ';
				for(int i = 0; i < strlen(char_menortempo); ++i){
		     save[linhas][i+2] = char_menortempo[i];
		    }
				if(linhas == T_buffers)   /* Sobrescreve */
		     linhas = 0;
		    else
		    linhas++;

			  vazio = 1;
			  pthread_cond_signal(&cond);
        }
		pthread_mutex_unlock(&protect);
		wait_period(&info);
		}
	}



//CALCULA VELOCIDADE MÉDIA (REAL)
void* atraso(void *arg){
		char char_atraso[50];
		int i, j;
		time_t currentTime;
		time(&currentTime);
		struct tm *myTime = localtime(&currentTime);
		struct periodic_info info;
		make_periodic(TIME_MED, &info);
		while(1){
				pthread_mutex_lock(&protect);
						if(id_comando[linhas] == 1 && meio_transporte[linhas] == 1){   /* Só efetua o calculo se não forem zero */
							if(myTime->tm_hour != 16){
									vel_atraso_carro = vel_carro - vel_carro*0.22;
									sprintf(char_atraso, "%f", vel_atraso_carro);       /* conversão para char para poder colocar no buffer */
									//Começa a salvar no buffer
									save[linhas][0] = '1';
									save[linhas][1] = ' ';
									for(int i = 0; i < strlen(char_atraso); ++i){
											save[linhas][i+2] = char_atraso[i];
									}
									if(linhas == T_buffers)   /* Sobrescreve */
											linhas = 0;
									else
											linhas++;

									vazio = 1;
									pthread_cond_signal(&cond);
							}else{
										vel_atraso_carro = vel_carro - vel_carro * 0.42;
										sprintf(char_atraso, "%f", vel_atraso_carro);       /* conversão para char para poder colocar no buffer */
										//Começa a salvar no buffer
										save[linhas][0] = '1';
										save[linhas][1] = ' ';
										for(int i = 0; i < strlen(char_atraso); ++i){
												save[linhas][i+2] = char_atraso[i];
										}
										if(linhas == T_buffers)   /* Sobrescreve */
												linhas = 0;
										else
												linhas++;

										vazio = 1;
										pthread_cond_signal(&cond);
							}
						}else if(id_comando[linhas] == 1 && meio_transporte[linhas] == 2){
							if(myTime->tm_hour != 18){
									vel_atraso_onibus = vel_onibus;
									sprintf(char_atraso, "%f", vel_atraso_onibus);       /* conversão para char para poder colocar no buffer */
									//Começa a salvar no buffer
									save[linhas][0] = '1';
									save[linhas][1] = ' ';
									for(int i = 0; i < strlen(char_atraso); ++i){
											save[linhas][i+2] = char_atraso[i];
									}
									if(linhas == T_buffers)   /* Sobrescreve */
											linhas = 0;
									else
											linhas++;

									vazio = 1;
									pthread_cond_signal(&cond);
							}else{
										vel_atraso_onibus += vel_onibus * 0.9;
										sprintf(char_atraso, "%f", vel_atraso_onibus);       /* conversão para char para poder colocar no buffer */
										//Começa a salvar no buffer
										save[linhas][0] = '1';
										save[linhas][1] = ' ';
										for(int i = 0; i < strlen(char_atraso); ++i){
												save[linhas][i+2] = char_atraso[i];
										}
										if(linhas == T_buffers)   /* Sobrescreve */
												linhas = 0;
										else
												linhas++;

										vazio = 1;
										pthread_cond_signal(&cond);
							}
						}else if(id_comando[linhas] == 1 && meio_transporte[linhas] == 3){
							if(myTime->tm_hour != 18){
									vel_atraso_metro = vel_metro;
									sprintf(char_atraso, "%f", vel_atraso_metro);       /* conversão para char para poder colocar no buffer */
									//Começa a salvar no buffer
									save[linhas][0] = '1';
									save[linhas][1] = ' ';
									for(int i = 0; i < strlen(char_atraso); ++i){
											save[linhas][i+2] = char_atraso[i];
									}
									if(linhas == T_buffers)   /* Sobrescreve */
											linhas = 0;
									else
											linhas++;

									vazio = 1;
									pthread_cond_signal(&cond);
							}else{
										vel_atraso_metro = vel_metro - vel_metro * 0.9;
										sprintf(char_atraso, "%f", vel_atraso_metro);       /* conversão para char para poder colocar no buffer */
										//Começa a salvar no buffer
										save[linhas][0] = '1';
										save[linhas][1] = ' ';
										for(int i = 0; i < strlen(char_atraso); ++i){
												save[linhas][i+2] = char_atraso[i];
										}
										if(linhas == T_buffers)   /* Sobrescreve */
												linhas = 0;
										else
												linhas++;

										vazio = 1;
										pthread_cond_signal(&cond);
							}
						}
			pthread_mutex_unlock(&protect);
			wait_period(&info);
		}
}

//SEND_RESULTS
void* send_results(void *arg){
    int n, j;
    char buffer[T_buffers], id_comando_char[2];
    while(1){
        pthread_mutex_lock(&protect);
            /* Enquanto buffer com informações estiver vazio, espera */
            while(vazio <= 0){
                pthread_cond_wait(&cond, &protect);
            }

            /**
             *  Percorre vetor com informações até encontrar comando específico
             *  Comando é convertido para char, para que a comparação aconteça
             */
            i_save = 0;
            while(i_save <= linhas-1){
                sprintf(id_comando_char, "%d", id_comando[i_save]);
                if(id_comando_char[0] == save[i_save][0]){
                    bzero(buffer,sizeof(buffer));  /* zera buffer para receber dados */
                        for(j = 0; save[i_save][j] != '\0'; ++j){
                            buffer[j] = save[i_save][j];
                        }

                        id_send++;
                        find = 1;
                        save[i_save][0] = '3';     // Inutiliza a linha do buffer
                        break;
                /* Se não encontra, procura na outra linha */
                }else{
                    find = 0;
                    i_save++;
                }
            }
            /* Se informação não for encontrada ou todas já foram enviadas, buffer esta vazio */
            if(!find){
                vazio = 0;
            }else{
            /* Se elemento for encontrado, envia para monitor */
            pthread_mutex_lock(&mutex);
                n = write(newsockfd,buffer,50);
                if (n < 0) {
                    printf("Erro escrevendo no socket!\n");
                    exit(1);
                }
                printf("<!>\tMensagem enviada\n");
            pthread_mutex_unlock(&mutex);
            }
        pthread_mutex_unlock(&protect);
    }
}


//MAIN FUNCTION
int main(int argc, char *argv[]) {
   /* Função para utilizar Acentos no Printf */
    setlocale(LC_ALL, "Portuguese");

    /*
     * Estrutura para servidor e para cliente (guarda IP)
     */
    struct sockaddr_in serv_addr, cli_addr;

    socklen_t clilen;
    int sockfd, portno, i;        /* descritor socket e porta - passada pelo argv */
    pthread_t t, calcula_vel, calc_atraso, best_time, envia;

    sigset_t alarm_sig; //conjunto de sinais "sinais de alarme"

	sigemptyset(&alarm_sig); //limpa o conjunto
	for (i = SIGRTMIN; i <= SIGRTMAX; i++)
		sigaddset(&alarm_sig, i);			  //adiciona sinais de SIGRTMIN até SIGRTMAX (32 sinais)
	sigprocmask(SIG_BLOCK, &alarm_sig, NULL); //"dado esses 32 sinais eu quero bloquear todos eles, se eles ocorrerem em algum momento"

    if (argc < 2) {
        printf("Erro, porta nao definida!\n");
        exit(1);
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);  /* definição do socket - TCPIP*/
    if (sockfd < 0) {
        printf("Erro abrindo o socket!\n");
        exit(1);
    }

    /* Zera para receber dados */
    bzero((char *) &serv_addr, sizeof(serv_addr));
    portno = atoi(argv[1]);

    /*
     *   Funções para conversões em endereço de REDE;
     */
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    /*
    * Associa o socket criado a porta
    * local do sistema operacional. Nesta associação é verificado
    * se a porta já não está sendo utilizada por algum outro processo
    */
    if (bind(sockfd, (struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) {
        printf("Erro fazendo bind!\n");
        exit(1);
    }

    listen(sockfd,1);  /* Coloca porta em escuta - 1 conexões aceitáveis */

    while (1) {
        // Comunicação com o cliente
    	newsockfd = accept(sockfd,(struct sockaddr *) &cli_addr,&clilen);
	pthread_mutex_lock(&mutex);
     	if (newsockfd < 0) {
        	printf("Erro no accept!\n");
         	exit(1);
    	}
        pthread_create(&t, NULL, receiver, NULL);
        pthread_create(&calcula_vel, NULL, calcula_velocidade, NULL);
        pthread_create(&calc_atraso, NULL, atraso, NULL);
        pthread_create(&best_time, NULL, best_opt, NULL);
        pthread_create(&envia, NULL, send_results, NULL);
        printf("<!>\tConexao estabelecida. Esperando comandos...\n\n");
	pthread_mutex_unlock(&mutex);
    }
    return 0;
}
