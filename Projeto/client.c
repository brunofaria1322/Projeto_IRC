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
void process_client(int client_fd);
void recebeStringBytes(char *file, int server_fd);
int readfile(int sock,char *filename);
int receive_int(int fd);
void send_int(int num, int fd);

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

	memset((void *) &addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = ((struct in_addr *)(proxyHostPtr->h_addr))->s_addr;
	addr.sin_port = htons((short) atoi(argv[3]));			//host to network

	if((fd = socket(AF_INET,SOCK_STREAM,0)) == -1)
		erro("socket");
	if( connect(fd,(struct sockaddr *)&addr,sizeof (addr)) !=0)
		erro("Connect");
	process_client(fd);
	close(fd);
	exit(0);
}

void erro(char *msg) {
	perror(msg);
	exit(-1);
}
void process_client(int client_fd){
	char buffer[BUF_SIZE];
	char command[BUF_SIZE];
	while(1){
		printf("Enter command!\n");
		fgets(command,BUF_SIZE,stdin);
		fflush(stdin);
		command[strlen(command)-1]='\0';			//removes \n from input

		if (strcmp(command,"LIST")==0){
			write(client_fd,command,1+strlen(command));
			int n_files=receive_int(client_fd);
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
				char file [BUF_SIZE/4];
				char* token;
				int count =0;

				while( (token = strtok(NULL, " "))) {
					if (count>2) break;
					else if (count==2) strcpy(file,token);
					count++;
				}
				write(client_fd,command,sizeof(command));

				//receber o download
				if(receive_int(client_fd)==0){
					//wrong download command
					printf("Command not accepted\n");
				}
				else{
					printf("accepted\n");
					recebeStringBytes(file, client_fd);
				}
				//else //wrong download command /num of elements
			}
			//else //wrong command
		}
	}
}

void recebeStringBytes(char *file, int server_fd){
	FILE *write_ptr;
	int nread, n_received, timeout=3;
	unsigned char buffer[BUF_SIZE];
	memset(buffer, 0, BUF_SIZE);

	int filesize =receive_int(server_fd);
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

int receive_int(int fd){
	int aux=0;
  read(fd, &aux, sizeof(aux));
  return ntohl(aux);
}

void send_int(int num, int fd){
	num = htonl(num);
	write(fd, &num, sizeof(num));
}
