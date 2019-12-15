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

void erro(char *msg);
void process_client(int client_fd);
int receive_int(int *num, int fd);

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
		erro("couldnt get Proxy address");

	bzero((void *) &addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = ((struct in_addr *)(proxyHostPtr->h_addr))->s_addr;
	addr.sin_port = htons((short) atoi(argv[2]));			//host to network

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
		printf("Command: ");
		fgets(comando,BUF_SIZE,stdin);
		comando[strlen(comando)-1]='\0';			//removes \n from input

		if (strcmp(comando,"LIST")==0){
			write(client_fd,comando,1+strlen(comando));
			int n_files=0;
			receive_int(&n_files,client_fd);
			for(int i=0; i<n_files; i++){
				bzero(buffer, BUF_SIZE);
				nread = read(client_fd, buffer, BUF_SIZE-1);
				buffer[nread] = '\0';
				printf("%s",buffer);
				}
		}
		else if(strcmp(comando,"QUIT")==0){//fecha a ligaçao com o server
			write(client_fd,comando,1+strlen(comando));
			break;
		}
		else if(strcmp(strtok(comando, ' '),"DOWNLOAD")==0){
			//deviamos estar a verificar isto no servidor e não no cliente certo?
			char down_comm [4][BUF_SIZE/4];
			strcpy (down_comm [0],"DOWNLOAD");
			char* token;
			int count =1;
			while( (token = strtok(NULL, ' ')) ) {
				if (count>3) break;
				strcpy (down_comm [count],token);
				count++;
			}
			if (count==3){
				write(client_fd,down_comm,1+sizeof(down_comm));
				//receber o download
			}
			//else //wrong command
		}
	}
}

void recebeStringBytes(int server_fd,int CLIENT){
	FILE *write_ptr;
	int nread;
	char buffer[BUF_SIZE];
	bzero(buffer, BUF_SIZE);
	nread=0;
	int filesize;
	read(server_fd, &filesize, sizeof(filesize));
	for(int i=0; i<filesize; i=i+BUF_SIZE){
		nread = read(server_fd, buffer, BUF_SIZE-1);
		buffer[nread] = '\0';
		char cwd[256];
		struct dirent *dptr;
		DIR *dp = NULL;
		if(getcwd(cwd, sizeof(cwd)) == NULL){
			printf("No such file or directory");
			continue;
		}
		strcat(cwd,"/Downloads/");
		strcat(cwd, "temp.bin");
		write_ptr = fopen(cwd, "wb");
		//Enviar confirmacao
	}
}

int receive_int(int *num, int fd){
    int32_t ret;
    char *data = (char*)&ret;
    int left = sizeof(ret);
    int rc;
    do {
      rc = read(fd, data, left);
			data += rc;
			left -= rc;
    }while (left > 0);
    *num = ntohl(ret);
    return 0;
}
