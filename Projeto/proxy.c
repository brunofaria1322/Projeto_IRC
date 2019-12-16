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
#include <time.h>

#define BUF_SIZE	1024
#define DEBUG

int server_port;

void process_client(int fd, struct sockaddr_in client_info);
void erro(char *msg);
void enviaStringBytes(char *file, int client_fd);
void process_clientUDP(int server_fd, int client_fd, struct sockaddr_in client_info);
int receive_intTCP(int fd);
void send_intTCP(int num, int fd);
void recebeStringBytesTCP(char *file, int server_fd, int client_fd);
void process_clientTCP(int server_fd, int client_fd, struct sockaddr_in client_info);
void initialize_connection(int client_fd, struct sockaddr_in client_info);
void passBytes(int server_fd, int client_fd);
void process_clientUDP(int server_fd, int client_fd, struct sockaddr_in client_info);

int main(int argc, char **argv) {
	if (argc != 2) {
		printf("proxy <port>\n");
		exit(-1);
	}
	server_port=atoi(argv[1]);

	int fd, client;
	struct sockaddr_in addr, client_addr;
	int client_addr_size;

	bzero((void *) &addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;			//ENDEREÇO DA PROXY
	addr.sin_port = htons(server_port);			//comunicacoes de rede utilizam big endian - conversão de inteiro short no host para tipo de inteiro a guardar com formato rede

	if ( (fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)//cria um socket - devolve -1 se falhar. SOCK_STREAM para TCP
		erro("na funcao socket");
	if ( bind(fd,(struct sockaddr*)&addr,sizeof(addr)) < 0)//associa um socket a um endereço, neste caso aquele que foi especificado na estrutura addr - devolve -1 se falhar
		erro("na funcao bind");
	if( listen(fd, 5) < 0)						//aguarda ligacoes no socket, 5 sao o numero de clientes que podem estar numa queue de espera - devolve -1 se falhar
		erro("na funcao listen");
	client_addr_size = sizeof(client_addr);
	while (1) {
		//clean finished child processes, avoiding zombies
		//must use WNOHANG or would block whenever a child process was working
		//while(waitpid(-1,NULL,WNOHANG)>0);
		//wait for new connection
		client = accept(fd,(struct sockaddr *)&client_addr,(socklen_t *)&client_addr_size);
		if(client<0){
			close(client);
		}
		//quando finalmente chega um cliente temos de guardar os seus dados numa estrutura(ip, ...). A funcao accept escreve nos seus parametros os dados dos clientes. Devolve um descritor de socket(mesmo tipo de fd) ou -1 se falhar
		if (fork() == 0) {	//depois do accept faço um fork para continuar à escuta no socket principal
			close(fd);				//processo filho fecha o socket fd
			initialize_connection(client, (struct sockaddr_in) client_addr);//processa os dados do cliente
			exit(0);
		}
		close(client);			//fecha a ligação com o cliente se falhar
	}
	return 0;
}

void initialize_connection(int client_fd, struct sockaddr_in client_info){

	#ifdef DEBUG
		char client_ip_address[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &client_info.sin_addr,client_ip_address, INET_ADDRSTRLEN);
		printf("From (IP:port): %s:%d\n", client_ip_address, client_info.sin_port);
	#endif

	char buffer[BUF_SIZE];
	memset(buffer, 0, BUF_SIZE);

	#ifdef DEBUG
		printf("EM ESPERA\n");
	#endif

	read(client_fd, buffer, BUF_SIZE);
	//buffer[nread] = '\0';				last line: nread=read(..., BUF_SIZE-1);
	printf("PRIMEIRA LIGACAO %s\n", buffer);//DEBUG
	fflush(stdout);


	struct hostent *hostPtr;

	char * token;
	token = strtok(buffer, " ");

	if ((hostPtr = gethostbyname(token)) == 0)
		erro("Nao consegui obter endereço");


	struct sockaddr_in addr;
	bzero((void *) &addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = ((struct in_addr *)(hostPtr->h_addr))->s_addr;
	addr.sin_port = htons(9001);

	int fd;
	if((fd = socket(AF_INET,SOCK_STREAM,0)) == -1){
		erro("socket");
	}

	token = strtok(NULL, " ");
	if((strcmp(token, "TCP"))==0){

		if( connect(fd,(struct sockaddr *)&addr,sizeof (addr)) < 0){
			erro("Connect");
		}
		process_clientTCP(fd,client_fd, client_info);
	}
	else if ((strcmp(token, "UDP"))==0){
		process_clientUDP(fd, client_fd, client_info);

	}
}

void erro(char *msg){
	perror(msg);
	exit(-1);
}

void process_clientTCP(int server_fd, int client_fd, struct sockaddr_in client_info){
	char command[BUF_SIZE];
	//save
	int save=0;
	char buffer[BUF_SIZE];
	while(1){
		memset(buffer, 0, BUF_SIZE);

		#ifdef DEBUG
			printf("EM ESPERA\n");
		#endif
		read(client_fd, command, BUF_SIZE);
		//buffer[nread] = '\0';				last line: nread=read(..., BUF_SIZE-1);

		printf("RECEBI %s\n", command);//DEBUG
		fflush(stdout);


		if (strcmp(command,"LIST")==0){
			write(server_fd,command,1+strlen(command));
			int n_files=receive_intTCP(server_fd);
			#ifdef DEBUG
				printf("Num de ficheiros: %d\n",n_files);
			#endif
			send_intTCP(n_files, client_fd);
			printf("Os ficheiros existentes para download são:\n");
			for(int i=0; i<n_files; i++){
				memset(buffer, 0, BUF_SIZE);
				read(server_fd, buffer, BUF_SIZE);
				//buffer[nread] = '\0';				last line: nread=read(..., BUF_SIZE-1);
				printf("%s\n",buffer);//DEBUG
				write(client_fd, buffer, strlen(buffer));
			}
		}
		else if(strcmp(command,"QUIT")==0){//fecha a ligaçao com o server
			write(server_fd,command,1+strlen(command));
			break;
		}
		else{
			char aux_com[BUF_SIZE];
			strcpy(aux_com,command);
			if(strcmp(strtok(aux_com, " "),"DOWNLOAD")==0){
				//deviamos estar a verificar isto no servidor e não no cliente certo?
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

				write(server_fd,command,sizeof(command));

				//receber o download
				if(receive_intTCP(server_fd)==0){
					send_intTCP(0,client_fd);
					//wrong download command
					printf("Command not accepted\n");//DEBUG
				}
				else{
					printf("accepted\n");//DEBUG
					send_intTCP(1,client_fd);
					//TCP
					if ((strcmp("TCP", down_comm[0]))==0){
						//not encripted
						if (strcmp("NOR", down_comm[1])==0){
							recebeStringBytesTCP(down_comm[2], server_fd, client_fd);
						}
						//encripted
						else if (strcmp("ENC", down_comm[1])==0){

						}
					}
				}
			}
		}
	}
}

void process_clientUDP(int server_fd, int client_fd, struct sockaddr_in client_info){

}
	save=0;
	void recebeStringBytesTCP(char *file, int server_fd, int client_fd){
	int nread, n_received, timeout=3;
	unsigned char buffer[BUF_SIZE];
	memset(buffer, 0, BUF_SIZE);

	int filesize =receive_intTCP(server_fd);
	#ifdef DEBUG
		printf("Filesize %d\n", filesize);
	#endif
	send_intTCP(filesize, client_fd);

	FILE *write_ptr;
	if (save==1){
		char dir[256];
		if(getcwd(dir, sizeof(dir)) == NULL){					//get current path
			erro("Couldn't get current directory");
		}
		strcat(dir,"/Sniffed/");
		strcat(dir,file);
		write_ptr = fopen(dir, "wb+");
	}
	int total_read=0, left_size=filesize;
	while(left_size>0){
		if(left_size-total_read>BUF_SIZE){
			n_received=BUF_SIZE;
		}
		else{
			n_received=left_size%BUF_SIZE;
		}
		nread = read(server_fd, buffer, n_received);
		//Enviar confirmacao
		#ifdef DEBUG
			printf("PACOTE DE: %d\t", n_received);
			printf("N_READ: %d\n", nread);
		#endif
		if(nread!=0){
			printf("Vou escrver no file\n" );
			if (save==1){
				fwrite(buffer, sizeof(unsigned char), nread, write_ptr);
			}
			write(client_fd, buffer, n_received);
		}
		else{
			int n, start= time(NULL);
			n=start;
			while(n-start<timeout){
				nread = read(server_fd, buffer, n_received);
				if(nread!=0){
					fwrite(buffer, sizeof(unsigned char), nread, write_ptr);
					write(client_fd, buffer, n_received);
					break;
				}
				n=time(NULL);
			}
			break;
		}
		left_size-=nread;
		usleep(100);
	}
	fclose(write_ptr);
}
int receive_intTCP(int fd){
	int aux=0;
  read(fd, &aux, sizeof(aux));
  return ntohl(aux);
}

void send_intTCP(int num, int fd){
	num = htonl(num);
	write(fd, &num, sizeof(num));
}
