#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <dirent.h> //Para ler pastas
#include <time.h>

#define BUF_SIZE	1024
#define DEBUG 			//comment this line to remove debug comments

void erro(char *msg);
void process_client(int client_fd, int port, struct hostent *proxyHostPtr);
void recebeStringBytesTCP(char *file, int server_fd);
int readfile(int sock,char *filename);
int receive_int_TCP(int fd);
void send_int_TCP(int num, int fd);
void handleUDP(char *file, char*type, int port, struct hostent *proxyHostPtr);
void send_int_UDP(int num, int fd, struct sockaddr_in addr);
int receive_int_UDP(int fd, struct sockaddr_in addr);

int main(int argc, char *argv[]) {
	char proxyAddress[128];
	int fd;
	struct sockaddr_in addr;
	struct hostent *proxyHostPtr;

	if (argc != 5) {
		printf("client {proxy address} {server address} {port} {protocol}\n");
		exit(-1);
	}

	//get proxy
	strcpy(proxyAddress, argv[1]);
	if ((proxyHostPtr = gethostbyname(proxyAddress)) == 0)
		erro("Couldnt get Proxy address");

	int port= atoi(argv[3]);
	memset((void *) &addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = ((struct in_addr *)(proxyHostPtr->h_addr))->s_addr;
	addr.sin_port = htons((short) port);			//host to network

	if((fd = socket(AF_INET,SOCK_STREAM,0)) == -1)
		erro("socket");
	if( connect(fd,(struct sockaddr *)&addr,sizeof (addr)) !=0)
		erro("Connect");
	process_client(fd, port, proxyHostPtr);
	close(fd);
	exit(0);
}

void erro(char *msg) {
	perror(msg);
	exit(-1);
}
void process_client(int client_fd, int port, struct hostent *proxyHostPtr){
	char buffer[BUF_SIZE];
	char command[BUF_SIZE];
	while(1){
		printf("Enter command!\n");
		fgets(command,BUF_SIZE,stdin);
		fflush(stdin);
		command[strlen(command)-1]='\0';			//removes \n from input

		if (strcmp(command,"LIST")==0){
			write(client_fd,command,1+strlen(command));
			int n_files=receive_int_TCP(client_fd);
			#ifdef DEBUG
			printf("Num de ficheiros: %d\n",n_files);
			#endif

			printf("Os ficheiros existentes para download são:\n");
			for(int i=0; i<n_files; i++){
				memset(buffer, 0, BUF_SIZE);
				read(client_fd, buffer, BUF_SIZE);
				//buffer[nread] = '\0';				last line: nread=read(..., BUF_SIZE-1);
				printf("%s\n",buffer);
				}
		}
		else if(strcmp(command,"QUIT")==0){//fecha a ligaçao com o server
			write(client_fd,command,1+strlen(command));
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

				write(client_fd,command,sizeof(command));

				//receber o download
					//TCP
					if ((strcmp("TCP", down_comm[0]))==0){
						//not encripted
						if (strcmp("NOR", down_comm[1])==0){
							if(receive_int_TCP(client_fd)==1)
								recebeStringBytesTCP(down_comm[2], client_fd);
						}
						//encripted
						else if (strcmp("ENC", down_comm[1])==0){
							//if(receive_int_TCP(client_fd)==1)
						}
					}
					//UDP
					else if (strcmp("UDP", down_comm[0])==0){
						handleUDP(down_comm[2], down_comm[1],port, proxyHostPtr);
					}
				//else //wrong download command /num of elements
			}
			//else //wrong command
		}
	}
}

void recebeStringBytesTCP(char *file, int server_fd){
	FILE *write_ptr;
	int nread, n_received, timeout=3;
	unsigned char buffer[BUF_SIZE];
	memset(buffer, 0, BUF_SIZE);

	int filesize =receive_int_TCP(server_fd);
	#ifdef DEBUG
		printf("Filesize %d\n", filesize);
	#endif

	char dir[256];
	if(getcwd(dir, sizeof(dir)) == NULL){					//get current path
		erro("Couldn't get current directory");
	}
	strcat(dir,"/Downloads/");
	strcat(dir,file);
	write_ptr = fopen(dir, "wb+");

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
			fwrite(buffer, sizeof(unsigned char), nread, write_ptr);
		}
		else{
			int n, start= time(NULL);
			n=start;
			while(n-start<timeout){
				nread = read(server_fd, buffer, n_received);
				if(nread!=0){
					fwrite(buffer, sizeof(unsigned char), nread, write_ptr);
					break;
				}
				n=time(NULL);
			}
			break;
		}
		left_size-=nread;
	}
	fclose(write_ptr);
}

int receive_int_TCP(int fd){
	int aux=0;
  read(fd, &aux, sizeof(aux));
  return ntohl(aux);
}

void send_int_TCP(int num, int fd){
	num = htonl(num);
	write(fd, &num, sizeof(num));
}

void handleUDP(char *file, char*type, int port, struct hostent *proxyHostPtr) {

  struct sockaddr_in serv_addr;

	memset(&serv_addr, 0, sizeof(serv_addr));

  // Filling server information
  serv_addr.sin_family = AF_INET; // IPv4
  serv_addr.sin_addr.s_addr = ((struct in_addr *)(proxyHostPtr->h_addr))->s_addr;
  serv_addr.sin_port = htons( port);

	//creates udp socket
	int sockfd;
	if ( (sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0 ) {
		perror("udp socket creation has failed");
	}

	int filesize =receive_int_UDP(sockfd,serv_addr);
	#ifdef DEBUG
		printf("Filesize %d\n", filesize);
	#endif

	char dir[256];
	if(getcwd(dir, sizeof(dir)) == NULL){					//get current path
		erro("Couldn't get current directory");
	}
	strcat(dir,"/Downloads/");
	strcat(dir,file);

	FILE *write_ptr;
	write_ptr = fopen(dir, "wb+");

	int nread, n_received, timeout=3;
	unsigned char buffer[BUF_SIZE];
	memset(buffer, 0, BUF_SIZE);
	int total_read=0, left_size=filesize;
	socklen_t len;
	while(left_size>0){
		if(left_size-total_read>BUF_SIZE){
			n_received=BUF_SIZE;
		}
		else{
			n_received=left_size%BUF_SIZE;
		}
		len = sizeof(serv_addr);
		nread = recvfrom(sockfd, (char *)buffer, BUF_SIZE,  0, ( struct sockaddr *) &serv_addr, (socklen_t*) sizeof(struct sockaddr_in));

		//Enviar confirmacao
		#ifdef DEBUG
			printf("PACOTE DE: %d\t", n_received);
			printf("N_READ: %d\n", nread);
		#endif
		if(nread!=0){
			fwrite(buffer, sizeof(unsigned char), nread, write_ptr);
		}
		else{
			int n, start= time(NULL);
			n=start;
			while(n-start<timeout){
				len = sizeof(serv_addr);
				nread = recvfrom(sockfd, (char *)buffer, BUF_SIZE,  0, ( struct sockaddr *) &serv_addr, (socklen_t*) sizeof(serv_addr));

				if(nread!=0){
					fwrite(buffer, sizeof(unsigned char), nread, write_ptr);
					break;
				}
				n=time(NULL);
			}
			break;
		}
		left_size-=nread;
	}
	fclose(write_ptr);
}

void send_int_UDP(int num, int fd, struct sockaddr_in addr){
	num = htonl(num);
	if((sendto(fd, &num, sizeof(num),0, (struct sockaddr *) &addr, sizeof(addr)))==-1){
		erro("Erro no sendto");
	}
}

int receive_int_UDP(int fd, struct sockaddr_in addr){
	int aux;
	socklen_t len = sizeof(addr);
	printf("Tou à espera\n");
	if (recvfrom(fd, &aux, sizeof(aux),0, ( struct sockaddr *) &addr,(socklen_t*) sizeof(struct sockaddr_in))==-1){
		erro("Erro no recvfrom");
	}
  return ntohl(aux);
}
