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
#define DEBUG

int SERVER_PORT;
int MAX_CLIENTS;
int CURR_CLIENTS=0;
int sendfile(int sock, char *filename);

void process_client(int fd, int CLIENT, struct sockaddr_in client_info);
void erro(char *msg);
int send_int(int num, int fd);
void enviaStringBytes(char *file, int client_fd);

int main(int argc, char **argv) {
	if (argc != 3) {
		printf("server <port> <max_clients>\n");
		exit(-1);
	}
	SERVER_PORT=atoi(argv[1]);
	MAX_CLIENTS=atoi(argv[2]);
	//Server Port Max_Clients
	
	int fd, client;
	struct sockaddr_in addr, client_addr;
	int client_addr_size;

	bzero((void *) &addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(SERVER_PORT);//comunicacoes de rede utilizam big endian - conversão de inteiro short no host para tipo de inteiro a guardar com formato rede

	if ( (fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)//cria um socket - devolve -1 se falhar. SOCK_STREAM para TCP
		erro("na funcao socket");
	if ( bind(fd,(struct sockaddr*)&addr,sizeof(addr)) < 0)//associa um socket a um endereço, neste caso aquele que foi especificado na estrutura addr - devolve -1 se falhar
		erro("na funcao bind");
	if( listen(fd, 5) < 0)//aguarda ligacoes no socket, 5 sao o numero de clientes que podem estar numa queue de espera - devolve -1 se falhar
		erro("na funcao listen");
	client_addr_size = sizeof(client_addr);
	while (1) {
	//clean finished child processes, avoiding zombies
	//must use WNOHANG or would block whenever a child process was working
	while(waitpid(-1,NULL,WNOHANG)>0);
	//wait for new connection
	if(CURR_CLIENTS>=MAX_CLIENTS){
		client = accept(fd,(struct sockaddr *)&client_addr,(socklen_t *)&client_addr_size);
		if(client>0){
		close(client);
		}
	}else{
	client = accept(fd,(struct sockaddr *)&client_addr,(socklen_t *)&client_addr_size);//quando finalmente chega um cliente temos de guardar os seus dados numa estrutura(ip, ...). A funcao accept escreve nos seus parametros os dados dos clientes. Devolve um descritor de socket(mesmo tipo de fd) ou -1 se falhar
	if (client > 0) {
		CURR_CLIENTS++;
	if (fork() == 0) {//depois do accept faço um fork para continuar à escuta no socket principal
		close(fd);//processo filho fecha o socket fd
		process_client(client, CURR_CLIENTS, (struct sockaddr_in) client_addr);//processa os dados do cliente
		CURR_CLIENTS--;
		exit(0);
	}
	close(client);//fecha a ligação com o cliente se falhar
	}
	}
	}
	return 0;
	}

	void process_client(int client_fd, int CLIENT, struct sockaddr_in client_info){

	char client_ip_address[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &client_info.sin_addr,client_ip_address, INET_ADDRSTRLEN);
	printf("From (IP:port): %s:%d\n", client_ip_address, client_info.sin_port);
	
	int nread = 0;
	char buffer[BUF_SIZE];
	while(1){
		bzero(buffer, BUF_SIZE);
		nread=0;
		#ifdef DEBUG
		printf("EM ESPERA\n");
		#endif
		nread = read(client_fd, buffer, BUF_SIZE-1);
		buffer[nread] = '\0';
		printf("RECEBI %s\n", buffer);//DEBUG
		fflush(stdout);
		
		
		if (strcmp(buffer,"LIST")==0){
			char cwd[256];
			struct dirent *dptr;
			DIR *dp = NULL;
			if(getcwd(cwd, sizeof(cwd)) == NULL){
				printf("No such file or directory");
				continue;
			}
			strcat(cwd,"/server_files/");
			if (NULL == (dp = opendir(cwd))){
				printf("Cannot open the given directory %s", cwd);
				exit(1);
			}
			int n_files=0;
			while((dptr = readdir(dp))!=NULL){
				if(dptr->d_name[0]=='.')  //Files never begin with '.'
					continue;
				if(dptr->d_name[0]=='.' && dptr->d_name[1]=='.')  //Files never begin with '..'
					continue;
				//Ver como e que vamos ver os tamanhos a enviar
				//Se os dados a enviar excederem o tamanho do pacote vamos ter problemas
				//Os dados podem não chegar por ordem
				n_files++;
			}
			send_int(n_files, client_fd);
			if (NULL == (dp = opendir(cwd))){
				printf("Cannot open the given directory %s", cwd);
				exit(1);
			}
			while((dptr = readdir(dp))!=NULL){
				if(dptr->d_name[0]=='.')  //Files never begin with '.'
					continue;
				bzero(buffer, sizeof(buffer));
				strcat(buffer,dptr->d_name);
				#ifdef DEBUG
				printf("%s\n", dptr->d_name);
				#endif
				write(client_fd, buffer, 1+strlen(buffer));
				sleep(1);
			}
		}else if(strcmp(buffer,"QUIT")==0){
		close(client_fd);//fecha a ligaçao com o cliente
		}else{
			char *token;
			token = strtok(buffer," ");
			if(strcmp(token,"DOWNLOAD")==0){
				char file[256], tipo[4], enc[4];
				token = strtok(NULL, " ");
				strcpy(tipo, token);
				token = strtok(NULL, " ");
				strcpy(enc, token);
				token = strtok(NULL, " ");
				strcpy(file, token);
				#ifdef DEBUG
				printf("TIPO: %s ENC: %s FILE: %s\n", tipo, enc, file);
				#endif
				char cwd[256];
				struct dirent *dptr;
				DIR *dp = NULL;
				if(getcwd(cwd, sizeof(cwd)) == NULL){
					printf("No such file or directory");
					continue;
				}
				strcat(cwd,"/server_files/");
				if (NULL == (dp = opendir(cwd))){
					printf("Cannot open the given directory %s", cwd);
					exit(1);
				}
				while((dptr = readdir(dp))!=NULL){
					if(dptr->d_name[0]=='.')  //Files never begin with '.'
						continue;
					if(strcmp(dptr->d_name, file)==0){//Se encontrar o ficheiro
						send_int(1,client_fd);
						strcat(cwd,dptr->d_name);
						enviaStringBytes(cwd, client_fd);
						break;
						}
				}
				if(dptr==NULL){//Não encontrou o ficheiro
					send_int(0,client_fd);
					}
			}else{
				//COMANDO DESCONHECIDO
				printf("Unknown command received\n");
				}
			}
	}
	}

	void erro(char *msg)
	{
	printf("Erro: %s\n", msg);
	exit(-1);
	}
	
	void enviaStringBytes(char *file, int client_fd){
		FILE *read_ptr;
		read_ptr = fopen(file,"rb");
		if(read_ptr == NULL)
	   {
		  printf("Error!");   
		  exit(1);             
		}
		
		fseek(read_ptr, 0, SEEK_END);
		int filesize = ftell(read_ptr);
		#ifdef DEBUG
		printf("Tamanho do ficheiro %d\n", filesize);
		#endif
		int fsize=filesize;
		fseek(read_ptr, 0, SEEK_SET);
		unsigned char *stream;
		stream=(unsigned char*)malloc(BUF_SIZE*sizeof(unsigned char));
		//Send filesize
		filesize = htonl(filesize);
		// Write the number to the opened socket
		write(client_fd, &filesize, sizeof(filesize));
		int n_sent;
		int n_received;
		while(fsize>0){
			n_sent=0;
			n_received=0;
			if(fsize/BUF_SIZE>0){
				n_received=BUF_SIZE;
				}else{
					n_received=fsize%BUF_SIZE;
					}
			#ifdef DEBUG
			printf("FSIZE ATUAL: %d\t", fsize);
			#endif
			n_sent=fread(stream,sizeof(unsigned char),n_received,read_ptr);
			#ifdef DEBUG
			printf("N_SENT: %d\n", n_sent);
			#endif
			write(client_fd,stream,BUF_SIZE);
			usleep(100);

			fsize=fsize-BUF_SIZE;//Continua
		}
		fclose(read_ptr);
		free(stream);
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

int senddata(int sock, void *buf, int buflen){
    unsigned char *pbuf = (unsigned char *) buf;
    while (buflen > 0)
    {
        int num = write(sock, pbuf, buflen);
        sleep(1);

        pbuf += num;
        buflen -= num;
    }

    return 1;
}

int sendlong(int sock, long value)
{
    value = htonl(value);
    return senddata(sock, &value, sizeof(value));
}

int sendfile(int sock, char *filename)
{
	FILE *f = fopen(filename,"rb");
    fseek(f, 0, SEEK_END);
    long filesize = ftell(f);
    rewind(f);
    if (filesize == EOF)
        return 0;
    if (!sendlong(sock, filesize))
        return 0;
    if (filesize > 0)
    {
        char buffer[BUF_SIZE];
        do
        {
            int num = BUF_SIZE%filesize;
            num = fread(buffer, 1, num, f);
            if (num < 1)
                return 0;
            if (!senddata(sock, buffer, num))
                return 0;
            filesize -= num;
        }
        while (filesize > 0);
    }
    return 1;
}
