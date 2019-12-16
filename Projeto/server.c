#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h> //Para ler pastas
#include <arpa/inet.h>
#include <ctype.h>


#define BUF_SIZE	1024
#define DEBUG 			//comment this line to remove debug comments

//criar lista ligada para guardar os clientes que estão ativos ara o comando show
//adicionar node a cada cliente
//*!!!fazer antes do fork();!!!*


void erro(char *msg);
void process_client(int fd, int client, struct sockaddr_in client_info, int server_port);
void enviaStringBytesTCP(char *file, int client_fd);
void send_int_TCP(int num, int fd);
int receive_int_TCP (int fd);
void handleUDP(char *file, char*type, int port);
void send_int_UDP(int num, int fd, struct sockaddr_in addr);
int receive_int_UDP(int fd, struct sockaddr_in addr);

int main(int argc, char **argv) {

	if (argc != 3) {
		printf("server {port} {max number of clients} \n");
		exit(-1);
	}

	//Server Port Max_Clients
	int server_port=atoi(argv[1]);
	int max_clients=atoi(argv[2]);
	int curr_clients=0;

	int fd, client;
	struct sockaddr_in addr, client_addr;
	int client_addr_size;

	memset((void *) &addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(server_port);//comunicacoes de rede utilizam big endian - conversão de inteiro short no host para tipo de inteiro a guardar com formato rede

	if ( (fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)//cria um socket - devolve -1 se falhar. SOCK_STREAM para TCP
		erro("na funcao socket");
	if ( bind(fd,(struct sockaddr*)&addr,sizeof(addr)) < 0)//associa um socket a um endereço, neste caso aquele que foi especificado na estrutura addr - devolve -1 se falhar
		erro("na funcao bind");
	if( listen(fd, 5) < 0)//aguarda ligacoes no socket, 5 sao o numero de clientes que podem estar numa queue de espera - devolve -1 se falhar
		erro("na funcao listen");
	client_addr_size = sizeof(client_addr);
	while (1) {

		while(curr_clients<max_clients){
			//quando finalmente chega um cliente temos de guardar os seus dados numa estrutura(ip, ...). A funcao accept escreve nos seus parametros os dados dos clientes. Devolve um descritor de socket(mesmo tipo de fd) ou -1 se falhar
			if ((client = accept(fd,(struct sockaddr *)&client_addr,(socklen_t *)&client_addr_size))>0) {
				curr_clients++;
				if (fork() == 0) {//depois do accept faço um fork para continuar à escuta no socket principal
					close(fd);//processo filho fecha o socket fd
					process_client(client, curr_clients, (struct sockaddr_in) client_addr, server_port);//processa os dados do cliente
					curr_clients--;
					exit(0);
				}
				//add client to lista ligada
				close(client); //fecha a ligação com o cliente na parent
			}
		}
		//foi atingido o numero maximo de client_addr_size
		wait(NULL);				//espera pela saída de 1 cliente
	}
	return 0;
}

void process_client(int client_fd, int client, struct sockaddr_in client_info, int server_port){

	char client_ip_address[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &client_info.sin_addr,client_ip_address, INET_ADDRSTRLEN);
	#ifdef DEBUG
		printf("From (IP:port): %s:%d\n", client_ip_address, client_info.sin_port);
	#endif

	char buffer[BUF_SIZE];
	while(1){
		memset(buffer, 0, BUF_SIZE);
		read(client_fd, buffer, BUF_SIZE);
		//buffer[nread] = '\0';				last line: nread=read(..., BUF_SIZE-1);
		#ifdef DEBUG
			printf("RECEBI %s\n", buffer);//DEBUG
		#endif
		fflush(stdout);

		if (strcmp(buffer,"LIST")==0){
			char dir[256];
			struct dirent *dptr;
			DIR *dp = NULL;
			if(getcwd(dir, sizeof(dir)) == NULL){					//get current path
				erro("Couldn't get current directory");
			}
			strcat(dir,"/server_files/");
			if ((dp = opendir(dir))==NULL){
				perror("Cannot open the given directory");
			}

			int n_files=0;
			while((dptr = readdir(dp))!=NULL){
				if(dptr->d_name[0]!='.')  //Files never begin with '.'
					n_files++;;
				//Ver como e que vamos ver os tamanhos a enviar
				//Se os dados a enviar excederem o tamanho do pacote vamos ter problemas
				//Os dados podem não chegar por ordem
			}
			send_int_TCP(n_files, client_fd);
			if ((dp = opendir(dir))==NULL){
				perror("Cannot open the given directory");
			}
			while((dptr = readdir(dp))!=NULL){
				if(dptr->d_name[0]!='.'){  //Files never begin with '.'
					memset(buffer, 0, sizeof(buffer));
					strcat(buffer,dptr->d_name);
					#ifdef DEBUG
						printf("%s\n", dptr->d_name);
					#endif
					write(client_fd, buffer, strlen(buffer));
					usleep(100);
				}
			}
		}
		else if(strcmp(buffer,"QUIT")==0){
			close(client_fd);//fecha a ligaçao com o cliente
			break;
		}
		else{
			char aux_com[BUF_SIZE];
			strcpy(aux_com,buffer);
			if(strcmp(strtok(aux_com, " "),"DOWNLOAD")==0){
				char down_comm [3][BUF_SIZE/4];		//[protocol, encripted, name]
				char* token;
				int count =-1;
				while( (token = strtok(NULL, " ")) ) {
					count++;
					printf("%d\t", count);
					printf("%s\n", token);
					if (count>2) {
						count++;
						break;}
					strcpy (down_comm [count],token);
				}
				if (count==2){

					char dir[256];
					struct dirent *dptr;
					DIR *dp = NULL;
					if(getcwd(dir, sizeof(dir)) == NULL){					//get current path
						erro("Couldn't get current directory");
					}
					strcat(dir,"/server_files/");
					if ((dp = opendir(dir))==NULL){
						perror("Cannot open the given directory");
					}

					while((dptr = readdir(dp))!=NULL){
						//if it finds the file with same name
						#ifdef DEBUG
							printf("%s\t %s\n",dptr->d_name, down_comm[2]);
						#endif
						if(strcmp(dptr->d_name, down_comm[2])==0){
							//tcp
							if ((strcmp("TCP", down_comm[0]))==0){
								#ifdef DEBUG
									printf("TCP\n");
								#endif
								//not encripted
								if (strcmp("NOR", down_comm[1])==0){
									#ifdef DEBUG
										printf("Not encripted\n");
									#endif
									send_int_TCP(1,client_fd);
									strcat(dir,dptr->d_name);
									enviaStringBytesTCP(dir, client_fd);
									break;
								}
								//encripted
								else if (strcmp("ENC", down_comm[1])==0){
									#ifdef DEBUG
										printf("Encripted\n");
									#endif
									//TODO: Function
								}
								else printf("Nem ENC nem NOR\n");
							}
							//udp
							else if (strcmp("UDP", down_comm[0])==0){
								//not encripted
								#ifdef DEBUG
									printf("UDP\n");
								#endif
								if (strcmp("NOR", down_comm[1])==0){
									#ifdef DEBUG
										printf("Not encripted\n");
									#endif

									send_int_TCP(1,client_fd);
									strcat(dir,dptr->d_name);
									handleUDP(dir,down_comm[1],server_port);
								}
								//encripted
								else if (strcmp("ENC", down_comm[1])==0){
									#ifdef DEBUG
										printf("Encripted\n");
									#endif

										handleUDP(dir,down_comm[1],server_port);
								}
								else printf("Nem ENC nem NOR\n");
							}
							else printf("Nem TCP nem UDP\n");
						}
					}
					//if it doesn't find the file
					if(dptr==NULL){
						send_int_TCP(0,client_fd);
						printf("File doesnt exist\n");
					}
				}
				//else //wrong download command /number of argumetns
				else{
					send_int_TCP(0,client_fd);
					printf("Wrong download command. Invalid number of args\n");
				}
			}
			//else //wrong command
			else{
				send_int_TCP(0,client_fd);
				printf("Wrong command\n");
			}
		}
	}
}

void erro(char *msg){
	perror(msg);
	exit(-1);
}

void send_int_TCP(int num, int fd){
	num = htonl(num);
	write(fd, &num, sizeof(num));
}

int receive_int_TCP(int fd){
	int aux=0;
  read(fd, &aux, sizeof(aux));
  return ntohl(aux);
}

void enviaStringBytesTCP(char *file, int client_fd){
	FILE *read_ptr;
	read_ptr = fopen(file,"rb");

	fseek(read_ptr, 0, SEEK_END);
	unsigned int filesize = ftell(read_ptr);
	#ifdef DEBUG
		printf("Tamanho do ficheiro %d\n", filesize);
	#endif
	fseek(read_ptr, 0, SEEK_SET);
	unsigned char stream[BUF_SIZE];
	memset(stream, 0, sizeof(stream));
	//Send filesize
	send_int_TCP(filesize, client_fd);
	int n_sent,  n_received, left_size=filesize;
	while(left_size>0){
		if(left_size>BUF_SIZE){
			n_received=BUF_SIZE;
		}
		else{
			n_received=left_size%BUF_SIZE;
		}
		#ifdef DEBUG
			printf("File SIZE ATUAL: %d\t", left_size);
		#endif
		fread(stream,n_received,1,read_ptr);

		n_sent=write(client_fd,stream,n_received);
		memset(stream, 0, sizeof(stream));

		#ifdef DEBUG
			printf("N_SENT: %d\n", n_sent);
		#endif

		left_size-=n_sent;
		usleep(100);
	}
	fclose(read_ptr);
}

void handleUDP(char *file, char*type, int port) {

  struct sockaddr_in serv_addr, prox_addr;
	//creates udp socket
	int sockfd;
	if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
		perror("udp socket creation has failed");
	}

	memset(&serv_addr, 0, sizeof(serv_addr));
  memset(&prox_addr, 0, sizeof(prox_addr));

  // Filling server information
  serv_addr.sin_family = AF_INET; // IPv4
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(port);

  // Bind the socket with the server address
  if ( bind(sockfd, (const struct sockaddr *)&serv_addr,  sizeof(serv_addr)) < 0 ) {
      perror("bind on udp socket has failed");
  }

	FILE *read_ptr;
	read_ptr = fopen(file,"rb");

	fseek(read_ptr, 0, SEEK_END);
	unsigned int filesize = ftell(read_ptr);
	#ifdef DEBUG
		printf("Tamanho do ficheiro %d\n", filesize);
	#endif
	fseek(read_ptr, 0, SEEK_SET);
	char stream[BUF_SIZE];
	memset(stream, 0, sizeof(stream));
	//send filesize
	send_int_UDP(filesize, sockfd, prox_addr);
	usleep(1000);
	printf ("sent");
	sleep(3);
	int n_sent,  n_received, left_size=filesize;
	//Encripted
	if ((strcmp(type,"ENC"))==0){
		//TODO: Function
	}
	//Not Encripted
	else{
		while(left_size>0){
			if(left_size>BUF_SIZE){
				n_received=BUF_SIZE;
			}
			else{
				n_received=left_size%BUF_SIZE;
			}
			#ifdef DEBUG
				printf("File SIZE ATUAL: %d\t", left_size);
			#endif
			fread(stream,n_received,1,read_ptr);

			n_sent=sendto(sockfd, (const char *)stream, strlen(stream),0, (const struct sockaddr *) &prox_addr, sizeof(prox_addr));
			memset(stream, 0, sizeof(stream));

			#ifdef DEBUG
				printf("N_SENT: %d\n", n_sent);
			#endif

			left_size-=n_sent;
			usleep(100);
		}
		fclose(read_ptr);
	}
}

void send_int_UDP(int num, int fd, struct sockaddr_in addr){
	sendto(fd, &num, sizeof(num),0, (const struct sockaddr *) &addr, sizeof(addr));
}

int receive_int_UDP(int fd, struct sockaddr_in addr){
	int aux;
	socklen_t len = sizeof(addr);
	recvfrom(fd, &aux, sizeof(aux),  0, ( struct sockaddr *) &addr, &len);
  return ntohl(aux);
}

// len = sizeof(cliaddr);  //len is value/resuslt
//
// n = recvfrom(sockfd, (char *)buffer, MAXLINE,  MSG_WAITALL, ( struct sockaddr *) &cliaddr, &len);
// buffer[n] = '\0';


//sendto(sockfd, (const char *)hello, strlen(hello),MSG_CONFIRM, (const struct sockaddr *) &cliaddr, len);
