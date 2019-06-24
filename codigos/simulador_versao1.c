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

//definição das constantes e variáveis globais

//time of execution of periodic tasks
#define TIME_SPEED 1000000
#define TIME_DELAY 2000000
#define TIME_B_OPT 3000000
#define BUFFER_SIZE 256 //size of informations buffer

int newsockfd;
int comm_id[50], bl = 0, send_data = 0, find = 0, _save = 0, function_id=0;
double vehicle[50], car_speed = 0, bus_speed = 0, sub_speed = 0, car_delay=0, bus_delay=0, sub_delay=0, best_op=0, car_time=0, bus_time=10, sub_time=20, small_size=0;
char data[BUFFER_SIZE][5];
int empty = 0;

//define of mutexes
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t protect = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

// Periodic threads using POSIX timers
struct periodic_info
{
	int sig;
	sigset_t alarm_sig;
};

// definition of threads and
void* average_speed(void *arg);
void* best_opt(void *arg);
void* delay(void *arg);
void* send_results(void *arg);

//static functions to make periodic
static void wait_period(struct periodic_info *info);
static int make_periodic(int unsigned period, struct periodic_info *info)

 //(ntervalo, referência pra estrutura
{
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
void *receiver(void *arg) {            									//receive data and indentifies the command
	int n, ctrl;
  char buffer[BUFFER_SIZE], *aux;
  while (1) {
  	bzero(buffer,sizeof(buffer));    									  // zera buffer para receber dados
    n = read(newsockfd,buffer,50);
    if (n <= 0) {                     									//condição para fechar = 0
        printf("Erro lendo do socket!\n");
        exit(1);
    }

	//Atribui valores recebidos para as variáveis, já convertidos para que as operações sejam efetuadas

    pthread_mutex_lock(&protect);
    ctrl = 0;
    aux = strtok(buffer, " "); 													//aux = buffer(sem espaços)
    comm_id[function_id] = atoi(aux); 									//converte (comm_id[0] == 0) em inteiro
    while (aux != NULL) {
    	aux = strtok(NULL, " ");
      if (ctrl == 0){         													//control to save the first data
      	vehicle[function_id] = atof(aux);
        ctrl = 1;           												// don't save anymore
      }
    }
    if(function_id == BUFFER_SIZE)       								//Começa a sobreescrever
    	function_id = 0;
    else
      function_id++;
    pthread_mutex_unlock(&protect);
   }
}


//saving informations on buffer
void save_b(char *arg){
	data[bl][1] = ' ';
	for(int i = 0; i < strlen(arg); ++i){
			data[bl][i+2] = arg[i];
	}
	if(bl == BUFFER_SIZE)
		bl = 0;
	else
		bl++;
	empty = 1;
	pthread_cond_signal(&cond);
	return;
}

// FIND AVERAGE VELOCITY (THEORICAL)
void *average_speed(void *arg){
	char speed_c[50];
  struct periodic_info info;
	make_periodic(TIME_SPEED, &info);
  	while(1){
      pthread_mutex_lock(&protect);
			if(comm_id[bl] == 0 && vehicle[bl] == 1){    				//VEHICLE = CAR AND COMMAND = 0
      	car_speed = 20.6;
        sprintf(speed_c, "%f", car_speed);
				data[bl][0] = '0';     					//conversão para char para poder colocar no buffer
        save_b(speed_c);
      }else if(comm_id[bl] == 0 && vehicle[bl] == 2){   	//VEHICLE = BUS AND COMMAND = 0
				bus_speed = 21.8;
				sprintf(speed_c, "%f", bus_speed);
				data[bl][0] = '0';					//Conversão para char para poder colocar no buffer
				save_b(speed_c);
			}else if(comm_id[bl] == 0 && vehicle[bl] == 3){    	//VEHICLE = METRO AND COMMAND = 0
				sub_speed = 26.1;
				sprintf(speed_c, "%f", sub_speed);
				data[bl][0] = '0';
				save_b(speed_c);
			}
    pthread_mutex_unlock(&protect);
		wait_period(&info);
    }
}

void* delay(void *arg){
		char delay_c[50];
		int i, j;
		double car_aux, bus_aux, sub_aux;
		time_t currentTime;
		time(&currentTime);
		struct tm *myTime = localtime(&currentTime);
		struct periodic_info info;
		make_periodic(TIME_DELAY, &info);
		while(1){
				pthread_mutex_lock(&protect);
						if(comm_id[bl] == 1 && vehicle[bl] == 0){   /* Só efetua o calculo se não forem zero */
							if(myTime->tm_hour != 18 && myTime->tm_hour != 19){
									car_aux = car_speed;
									car_delay = 41.7/car_aux;
									sprintf(delay_c, "%f", car_delay);
									data[bl][0] = '1';
									save_b(delay_c);

							}else{
										car_aux = car_speed - car_speed * 0.1;
										car_delay = 41.7/car_aux;
										sprintf(delay_c, "%f", car_delay);
										data[bl][0] = '1';
										save_b(delay_c);

							}
						}else if(comm_id[bl] == 1 && vehicle[bl] == 1){
							if(myTime->tm_hour != 18 && myTime->tm_hour != 19){
									if(myTime->tm_min > 0 && myTime->tm_min <= 15){
										bus_aux = 15 - myTime->tm_min;
									}else if(myTime->tm_min > 15 && myTime->tm_min <= 30){
										bus_aux = 30 - myTime->tm_min;
									}else if(myTime->tm_min > 30 && myTime->tm_min <= 45){
										bus_aux = 45 - myTime->tm_min;
									}else if(myTime->tm_min > 45 && myTime->tm_min <= 59){
										bus_aux = 59 - myTime->tm_min;
									}else{
										bus_aux = 0;
									}
									bus_delay = 41.7/bus_speed + bus_aux/60; 		//bus_delay = tempo sem atraso + tempo de espera do onibus + tempo do horário de pico
									sprintf(delay_c, "%f", bus_delay);
									data[bl][0] = '1';
									save_b(delay_c);
							}else{
										if(myTime->tm_min > 0 && myTime->tm_min <= 15){
											bus_aux = 15 - myTime->tm_min;
										}else if(myTime->tm_min > 15 && myTime->tm_min <= 30){
											bus_aux = 30 - myTime->tm_min;
										}else if(myTime->tm_min > 30 && myTime->tm_min <= 45){
											bus_aux = 45 - myTime->tm_min;
										}else if(myTime->tm_min > 45 && myTime->tm_min <= 59){
											bus_aux = 59 - myTime->tm_min;
										}else{
											bus_aux = 0;
										}
										bus_delay = 41.7/bus_speed + bus_aux/60 + 10/60;
										sprintf(delay_c, "%f", bus_delay);
										data[bl][0] = '1';
										save_b(delay_c);
							}
						}else if(comm_id[bl] == 1 && vehicle[bl] == 2){
							if(myTime->tm_hour != 18 || myTime->tm_hour != 19){
									if(myTime->tm_min > 0 && myTime->tm_min <= 30){
										sub_aux = 30 - myTime->tm_min;
									}else if(myTime->tm_min > 30 && myTime->tm_min <= 59){
										sub_aux = 59 - myTime->tm_min;
									}else{
										sub_aux = 0;
									}
									sub_delay = 41.7/sub_speed + sub_aux/60;
									sprintf(delay_c, "%f", sub_delay);
									data[bl][0] = '1';
									save_b(delay_c);

							}else{
										if(myTime->tm_min > 0 && myTime->tm_min <= 30){
											sub_aux = 30 - myTime->tm_min;
										}else if(myTime->tm_min > 30 && myTime->tm_min <= 59){
											sub_aux = 59 - myTime->tm_min;
										}else{
											sub_aux = 0;
										}
										sub_delay = 41.7/sub_speed + sub_aux/60 + 30/60;
										printf("subdelay: %f", sub_delay);
										sprintf(delay_c, "%f", sub_delay);
										data[bl][0] = '1';
										save_b(delay_c);
							}
						}
			pthread_mutex_unlock(&protect);
			wait_period(&info);
		}
}


void* best_opt(void *arg){
		char best_c[50];
		struct periodic_info info;
		make_periodic(TIME_B_OPT, &info);
	  while(1){
			pthread_mutex_lock(&protect);
			if(comm_id[bl] == 2){
				if(car_delay < bus_delay && car_delay < sub_delay){
					best_op = 1;
					sprintf(best_c, "%f", best_op);
					data[bl][0] = '2';
					save_b(best_c);
				}else if(bus_delay < car_delay && bus_delay < sub_delay){
					best_op = 2;
					sprintf(best_c, "%f", best_op);
					data[bl][0] = '2';
					save_b(best_c);
				}else if(sub_delay < car_delay && sub_delay < bus_delay){
					best_op = 3;
					sprintf(best_c, "%f", best_op);
					data[bl][0] = '2';
					save_b(best_c);
				}
			}
			pthread_mutex_unlock(&protect);
			wait_period(&info);
		}
}

//SEND THE RESULTS
void* send_results(void *arg){
    int n, j;
    char buffer[BUFFER_SIZE], comm_id_char[2];
    while(1){
        pthread_mutex_lock(&protect);     							//while buffer is empty wait
            while(empty <= 0){
                pthread_cond_wait(&cond, &protect);
            }
            /**
             *  Percorre vetor com informações até encontrar comando específico
             *  Comando é convertido para char, para que a comparação aconteça
             */
            _save = 0;
            while(_save <= bl-1){
                sprintf(comm_id_char, "%d", comm_id[_save]);
                if(comm_id_char[0] == data[_save][0]){
                  bzero(buffer,sizeof(buffer));  		// zera buffer para receber dados
                  	for(j = 0; data[_save][j] != '\0'; ++j){
                        buffer[j] = data[_save][j];
                    }
                    send_data++;
                    find = 1;
                    data[_save][0] = '3';     // Inutiliza a linha do buffer
                    break;
                /* Se não encontra, procura na outra linha */
                }else{
                  find = 0;
                  _save++;
                }
            }
            // Se informação não for encontrada ou todas já foram enviadas, buffer esta vazio
            if(!find){
                empty = 0;
            }else{
            // Se elemento for encontrado, envia para monitor
            pthread_mutex_lock(&mutex);
                n = write(newsockfd,buffer,50);
                if (n < 0) {
                    printf("Error escrevendo no socket!\n");
                    exit(1);
                }
                printf("[>>] \tMensagem enviada\n");
            pthread_mutex_unlock(&mutex);
            }
        pthread_mutex_unlock(&protect);
    }
}

//MAIN FUNCTION
int main(int argc, char *argv[]) {
   // Função para utilizar Acentos no Printf
    setlocale(LC_ALL, "Portuguese");

    //Estrutura para servidor e para cliente (guarda IP)
    struct sockaddr_in serv_addr, cli_addr;

    socklen_t clilen;
    int sockfd, portno, i;        // descritor socket e porta - passada pelo argv
    pthread_t t, calc_speed, calc_delay, best_time, send_d;

    sigset_t alarm_sig; //conjunto de sinais "sinais de alarme
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

    // Zera para receber dados
    bzero((char *) &serv_addr, sizeof(serv_addr));
    portno = atoi(argv[1]);

    //Funções para conversões em endereço de REDE;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

     //Associa o socket criado a porta local do sistema operacional. Nesta associação é verificado
     //se a porta já não está sendo utilizada por algum outro processo

    if(bind(sockfd, (struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) {
        printf("Erro fazendo bind!\n");
        exit(1);
    }

    listen(sockfd,1);  // Coloca porta em escuta - 1 conexões aceitáveis

    while (1) {
        // Comunicação com o cliente
    	newsockfd = accept(sockfd,(struct sockaddr *) &cli_addr,&clilen);
			pthread_mutex_lock(&mutex);
     	if (newsockfd < 0) {
        	printf("Erro no accept!\n");
         	exit(1);
    	}
        pthread_create(&t, NULL, receiver, NULL);
        pthread_create(&calc_speed, NULL, average_speed, NULL);
      	pthread_create(&calc_delay, NULL, delay, NULL);
        pthread_create(&best_time, NULL, best_opt, NULL);
        pthread_create(&send_d, NULL, send_results, NULL);
        printf("[>>]\tConexao estabelecida. Esperando comandos...\n\n");
				pthread_mutex_unlock(&mutex);
    }
    return 0;
}
