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
#define DEBUG

void erro(char *msg);
void process_client(int client_fd);
int receive_int(int *num, int fd);
int send_int(int num, int fd);
int recebeStringBytes(char *file, int server_fd);
int readfile(int sock,char *filename);

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
			#ifdef DEBUG
			printf("N de ficheiros: %d\n",n_files);
			#endif
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
		char *token;
		char dup[BUF_SIZE];
		strcpy(dup, comando);
		token = strtok(comando," ");
		if(strcmp(token,"DOWNLOAD")==0){
			char file[256], tipo[4], enc[4];
			token = strtok(NULL, " ");
			if(token!=NULL){
				strcpy(tipo, token);
				}
			token = strtok(NULL, " ");
			if(token!=NULL){
			strcpy(enc, token);
			}
			token = strtok(NULL, " ");
			if(token!=NULL){
			strcpy(file, token);
			#ifdef DEBUG
			printf("TIPO: %s ENC: %s FILE: %s\n", tipo, enc, file);
			#endif
			int aux=0;
			write(client_fd,dup,1+strlen(dup));
			read(client_fd, &aux, sizeof(aux));
			aux = ntohl(aux);
			if(aux==0){
				printf("Ocorreu um erro a transferir o ficheiro\n");
			}else{
				recebeStringBytes(file, client_fd);
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
	unsigned char buffer[BUF_SIZE];
	int filesize=0;
	read(server_fd, &filesize, sizeof(filesize));
	filesize = ntohl(filesize);
	printf("Filesize %d\n", filesize);//DEBUG
	char cwd[256];
	if(getcwd(cwd, sizeof(cwd)) == NULL){
		printf("No such file or directory");
		return 1;
	}
	strcat(cwd,"/Downloads/");
	strcat(cwd,file);
	write_ptr = fopen(cwd, "wb");
	int n_received;
	for(int i=filesize; i>=0; i=i-BUF_SIZE){
		nread=0;
		n_received=0;
		if(filesize/BUF_SIZE>0){
				n_received=BUF_SIZE;
				}else{
					n_received=filesize%BUF_SIZE;
					}
		nread = read(server_fd, buffer, n_received);
		//Enviar confirmacao
		#ifdef DEBUG
		printf("N_READ: %d", nread);
		#endif
		int converted_number = htonl(nread);
		write(server_fd, &converted_number, sizeof(converted_number));
		if(nread!=n_received){
			i=i+BUF_SIZE;
			}else{
				#ifdef DEBUG
				printf("Escrevi\n");
				#endif
				fwrite(buffer, sizeof(unsigned char), nread, write_ptr);
				}
	}
	fclose(write_ptr);
	return 0;
}
int readdata(int sock, void *buf, int buflen)
{
    unsigned char *pbuf = (unsigned char *) buf;

    while (buflen > 0)
    {
        int num = read(sock, pbuf, buflen);

        pbuf += num;
        buflen -= num;
    }

    return 0;
}

int readlong(int sock, long *value)
{
    if (!readdata(sock, value, sizeof(value)))
        return 1;
    *value = ntohl(*value);
    return 0;
}

int readfile(int sock,char *filename)
{
	char cwd[256];
	if(getcwd(cwd, sizeof(cwd)) == NULL){
		printf("No such file or directory");
		return 1;
	}
	strcat(cwd,"/Downloads/");
	strcat(cwd,filename);
	FILE *f = fopen(cwd, "wb");
    long filesize;
    if (!readlong(sock, &filesize))
        return 1;
    printf("Filesize: %li\n", filesize);
    if (filesize > 0)
    {
        char buffer[BUF_SIZE];
        do
        {
            int num = BUF_SIZE%filesize;
            if (!readdata(sock, buffer, num))
                return 1;
            int offset = 0;
            do
            {
                size_t written = fwrite(&buffer[offset], 1, num-offset, f);
                if (written < 1)
                    return 1;
                offset += written;
            }
            while (offset < num);
            filesize -= num;
        }
        while (filesize > 0);
    }
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
