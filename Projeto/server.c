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

int SERVER_PORT;
int MAX_CLIENTS;
int CURR_CLIENTS=0;

void process_client(int fd, int CLIENT, struct sockaddr_in client_info);
void erro(char *msg);
int send_int(int num, int fd);

int main(int argc, char **argv) {
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
	while(CURR_CLIENTS>MAX_CLIENTS);
		
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
				if(dptr->d_name[0]=='.' && dptr->d_name[1]=='.')  //Files never begin with '..'
					continue;
				bzero(buffer, sizeof(buffer));
				strcat(buffer,dptr->d_name);
				write(client_fd, buffer, 1+strlen(buffer));
			}
		}else if(strcmp(buffer,"QUIT")==0){
		close(client_fd);//fecha a ligaçao com o cliente
		break;
		}
	}
	}

	void erro(char *msg)
	{
	printf("Erro: %s\n", msg);
	exit(-1);
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
	void enviaStringBytes(int client_fd, int CLIENT){
		FILE *read_ptr;
		char temp[256];
		bzero(temp, sizeof(temp));
		sprintf(temp,"temp%d.bin", CLIENT);
		read_ptr = fopen(temp,"rb");
		
		fseek(read_ptr, 0, SEEK_END);
		unsigned long filesize = ftell(read_ptr);
		fseek(read_ptr, 0, SEEK_SET);
		unsigned char stream[BUF_SIZE];
		bzero(stream, sizeof(stream));
		//Send filesize
		int n_sent;
		int n_received;
		do{
			n_sent=0;
			n_received=0;
			n_sent=fread(stream,BUF_SIZE,1,read_ptr);
			stream[BUF_SIZE]='\0';
			write(client_fd,stream,BUF_SIZE+1);
			bzero(stream, sizeof(stream));
			
			read(client_fd, &n_received, sizeof(n_received));
			if(n_sent!=n_received){
				fseek(read_ptr, -BUF_SIZE, SEEK_CUR);
			}else{
				filesize=filesize-BUF_SIZE;
				}
		}while(filesize-BUF_SIZE>0);
		}
