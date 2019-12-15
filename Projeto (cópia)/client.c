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
int send_int(int num, int fd);
int recebeStringBytes(char *file, int server_fd);

int main(int argc, char *argv[]) {
	char endServer[100];
	int fd;
	struct sockaddr_in addr;
	struct hostent *hostPtr;

	if (argc != 3) {
		printf("cliente <host> <port> \n");
		exit(-1);
	}

	strcpy(endServer, argv[1]);
	if ((hostPtr = gethostbyname(endServer)) == 0)
		erro("Nao consegui obter endereço");
	
	bzero((void *) &addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = ((struct in_addr *)(hostPtr->h_addr))->s_addr;
	addr.sin_port = htons((short) atoi(argv[2]));

	if((fd = socket(AF_INET,SOCK_STREAM,0)) == -1)
		erro("socket");
	if( connect(fd,(struct sockaddr *)&addr,sizeof (addr)) < 0)
		erro("Connect");
	process_client(fd);
	close(fd);
	exit(0);
}

void erro(char *msg) {
	printf("Erro: %s\n", msg);
	exit(-1);
}
void process_client(int client_fd){
	int nread;
	char buffer[BUF_SIZE];
	char comando[BUF_SIZE];
	while(1){
		printf("Qual o comado?");
		scanf("%[^\n]%*c",comando);
		fflush(stdin);
		if (strcmp(comando,"LIST")==0){
			write(client_fd,comando,1+strlen(comando));
			int n_files=0;
			receive_int(&n_files,client_fd);
			printf("Os ficheiros existentes para download são:\n");
			for(int i=0; i<n_files; i++){
				bzero(buffer, BUF_SIZE);
				nread = read(client_fd, buffer, BUF_SIZE-1);
				buffer[nread] = '\0';
				printf("%s\n",buffer);
				}
		}else if(strcmp(comando,"QUIT")==0){//fecha a ligaçao com o server
			write(client_fd,comando,1+strlen(comando));
			break;
		}else{
		char *token,*saved;
		char dup[BUF_SIZE];
		strcpy(dup, comando);
		token = strtok_r(comando," ",&saved);
		if(strcmp(token,"DOWNLOAD")==0){
			char file[256], tipo[4], enc[4];
			printf("Comando: %s", dup);//DEBUG
			if(sscanf(dup,"%*s %s %s %s",tipo,enc,file)!=3){
				int aux;
				write(client_fd,comando,1+strlen(comando));
				receive_int(&aux, client_fd);
				printf("Filesize %d\n", aux);//DEBUG
				if(aux==0){
					printf("Ocorreu um erro a transferir o ficheiro\n");
				}else{
					recebeStringBytes(saved, client_fd);
					}
			}else{
				printf("DOWNLOAD <TCP/UDP> <ENC/NOR> FILE\n");
				}
		}else{
			//COMANDO DESCONHECIDO
			printf("Comando desconhecido");
			}
		}
	}
}
int recebeStringBytes(char *file, int server_fd){
	FILE *write_ptr;
	int nread;
	char buffer[BUF_SIZE];
	bzero(buffer, BUF_SIZE);
	nread=0;
	int filesize;
	receive_int(&filesize, server_fd);
	char cwd[256];
	if(getcwd(cwd, sizeof(cwd)) == NULL){
		printf("No such file or directory");
		return 1;
	}
	strcat(cwd,"/Downloads/");
	strcat(cwd,file);
	write_ptr = fopen(cwd, "wb+");
	for(int i=filesize; i>0; i=i-BUF_SIZE){
		nread=0;
		nread = read(server_fd, buffer, BUF_SIZE);
		//Enviar confirmacao
		send_int(nread, server_fd);
		if(nread!=(BUF_SIZE%i)){
			i=i+BUF_SIZE;
			}else{
				fwrite(buffer, BUF_SIZE%i, 1, write_ptr);
				}
	}
	fclose(write_ptr);
	return 0;
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

	int send_int(int num, int fd){
    int32_t conv = htonl(num);
    char *data = (char*)&conv;
    int left = sizeof(conv);
    int rc;
    do {
        rc = write(fd, data, left);
        
		data += rc;
		left -= rc;
    }
    while (left > 0);
    return 0;
}
