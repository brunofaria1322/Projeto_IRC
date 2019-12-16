#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <dirent.h> //Para ler pastas

#define BUF_SIZE	1024
#define DEBUG 			//comment tjis line to remove debug comments

void erro(char *msg);
void process_client(int client_fd);
int receive_int(int fd);
int send_int(int num, int fd);
int recebeStringBytes(char *file, int server_fd, int filesize);

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

	bzero((void *) &addr, sizeof(addr));
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
	int nread;
	char buffer[BUF_SIZE];
	char comando[BUF_SIZE];
	while(1){
		fgets(comando,BUF_SIZE,stdin);
		comando[strlen(comando)-1]='\0';			//removes \n from input

		if (strcmp(comando,"LIST")==0){
			write(client_fd,comando,1+strlen(comando));
			int n_files=receive_int(client_fd);

			printf("Os ficheiros existentes para download são:\n");
			for(int i=0; i<n_files; i++){
				bzero(buffer, BUF_SIZE);
				nread = read(client_fd, buffer, BUF_SIZE);
				printf("%s",buffer);
				}
		}
		else if(strcmp(comando,"QUIT")==0){//fecha a ligaçao com o server
			write(client_fd,comando,1+strlen(comando));
			break;
		}
		else{
			char aux_com[BUF_SIZE];
			strcpy(aux_com,command);
			if(strcmp(strtok(aux_com, ' '),"DOWNLOAD")==0){
				//deviamos estar a verificar isto no servidor e não no cliente certo?
				char* token;
				int count =1;
				while( (token = strtok(NULL, ' ')) ) {
					if (count>3) break;
					count++;
				}
				if (count==3){
					write(client_fd,command,sizeof(command));
					//receber o download
					if(receive_int(client_fd)==0){
						printf("Ocorreu um erro a encontrar o ficheiro\n");
					}
					else{
						recebeStringBytes(saved, client_fd);
					}
				}
				//else //wrong download command
			}
			//else //wrong command
		}
	}
}

void recebeStringBytes(char *file, int server_fd, int filesize){
	FILE *write_ptr;
	int nread;
	char buffer[BUF_SIZE];
	bzero(buffer, BUF_SIZE);
	nread=0;
	receive_int(server_fd);			//porque??? não pedes já o tamanho na linha 92
	#ifdef DEBUG
		printf("Filesize %d\n", aux);//DEBUG
	#endif
	char dir[256];
	if(getcwd(dir, sizeof(dir)) == NULL){					//get current path
		erro("Couldn't get current directory");
	}
	strcat(dir,"/Downloads/");
	strcat(cwd,file);
	write_ptr = fopen(cwd, "wb+");

	int total_read=0
	while((nread = read(server_fd, buffer, BUF_SIZE))!=0){
		total_read +=nread;
		//Enviar confirmacao
		send_int(nread, server_fd);

		fwrite(buffer, nread,1, write_ptr);
	}
	fclose(write_ptr);
	return 0;
}

int receive_int(int fd){
    int32_t ret;
    char *data = (char*)&ret;
    int left = sizeof(ret);
    int rc;
    do {
      rc = read(fd, data, left);
			data += rc;
			left -= rc;
    }while (left > 0);
    return ntohl(ret);
}

void send_int(int num, int fd){
	int32_t conv = htonl(num);
	char *data = (char*)&conv;
	int left = sizeof(conv);
	int rc;
	do {
		rc = write(fd, data, left);
		data += rc;
		left -= rc;
	}while (left > 0);
}
