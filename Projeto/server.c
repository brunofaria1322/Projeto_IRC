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

void erro(char *msg);
void process_client(int fd, int client, struct sockaddr_in client_info);
void send_int(int num, int fd);
int receive_int (int fd);
void enviaStringBytes(char *file, int client_fd);

int main(int argc, char **argv) {

	if (argc != 3) {
		printf("server {port} {max number of clients} \n");
		exit(-1);
	}

	//Server Port Max_Clients
	SERVER_PORT=atoi(argv[1]);
	MAX_CLIENTS=atoi(argv[2]);

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

		while(CURR_CLIENTS<MAX_CLIENTS){
			//quando finalmente chega um cliente temos de guardar os seus dados numa estrutura(ip, ...). A funcao accept escreve nos seus parametros os dados dos clientes. Devolve um descritor de socket(mesmo tipo de fd) ou -1 se falhar
			if ((client = accept(fd,(struct sockaddr *)&client_addr,(socklen_t *)&client_addr_size))>0) {
				CURR_CLIENTS++;
				if (fork() == 0) {//depois do accept faço um fork para continuar à escuta no socket principal
					close(fd);//processo filho fecha o socket fd
					process_client(client, CURR_CLIENTS, (struct sockaddr_in) client_addr);//processa os dados do cliente
					CURR_CLIENTS--;
					exit(0);
				}
				//??? close(client);//fecha a ligação com o cliente se falhar
			}
		}
	}
	return 0;
}

void process_client(int client_fd, int client, struct sockaddr_in client_info){

	char client_ip_address[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &client_info.sin_addr,client_ip_address, INET_ADDRSTRLEN);
	#ifdef DEBUG
		printf("From (IP:port): %s:%d\n", client_ip_address, client_info.sin_port);
	#endif

	int nread = 0;
	char buffer[BUF_SIZE];
	while(1){
		bzero(buffer, BUF_SIZE);
		nread=0;
		nread = read(client_fd, buffer, BUF_SIZE);
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
				if(dptr->d_name[0]=='.')  //Files never begin with '.'
					continue;
				//??????
				if(dptr->d_name[0]=='.' && dptr->d_name[1]=='.')  //Files never begin with '..'
					continue;
				//Ver como e que vamos ver os tamanhos a enviar
				//Se os dados a enviar excederem o tamanho do pacote vamos ter problemas
				//Os dados podem não chegar por ordem
				n_files++;
			}
			send_int(n_files, client_fd);
			if ((dp = opendir(dir))==NULL){
				perror("Cannot open the given directory");
			}
			while((dptr = readdir(dp))!=NULL){
				if(dptr->d_name[0]=='.')  //Files never begin with '.'
					continue;
				if(dptr->d_name[0]=='.' && dptr->d_name[1]=='.')  //Files never begin with '..'
					continue;
				bzero(buffer, sizeof(buffer));
				strcat(buffer,dptr->d_name);
				write(client_fd, buffer, strlen(buffer));
			}
		}
		else if(strcmp(buffer,"QUIT")==0){
			close(client_fd);//fecha a ligaçao com o cliente
			break;
		}
		else{
			char aux_com[BUF_SIZE];
			strcpy(aux_com,command);
			if(strcmp(strtok(aux_com, ' '),"DOWNLOAD")==0){
				char down_comm [3][BUF_SIZE/4];
				char* token;
				int count =0;
				while( (token = strtok(NULL, ' ')) ) {
					if (count>2) break;
					strcpy (down_comm [count],token);
					count++;
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
						if(dptr->d_name[0]=='.')  //Files never begin with '.'
							continue;
						//??????
						if(dptr->d_name[0]=='.' && dptr->d_name[1]=='.')  //Files never begin with '..'
							continue;

						//tcp
						else if(strcmp(dptr->d_name, down_comm[2])==0){//Se encontrar o ficheiro
							send_int(1,client_fd);				//???
							strcat(cwd,dptr->d_name);
							enviaStringBytes(dir, client_fd);
							break;
						}
					}
					if(dptr==NULL){//Não encontrou o ficheiro
						send_int(0,client_fd);
					}
				}
				//else //wrong download command
			}
			//else //wrong command
		}
	}
}

void erro(char *msg){
	perror(msg);
	exit(-1);
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

int receive_int (int fd){
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
void enviaStringBytes(char *file, int client_fd){
	FILE *read_ptr;
	read_ptr = fopen(file,"rb");

	fseek(read_ptr, 0, SEEK_END);							//que faz isto??
	unsigned int filesize = ftell(read_ptr);	//e isto
	fseek(read_ptr, 0, SEEK_SET);							//mais isto de novo
	unsigned char stream[BUF_SIZE];						//ok... talvez tenha pecebido +/- a ideia... mas resulta??
	bzero(stream, sizeof(stream));
	//Send filesize
	send_int(filesize, client_fd);
	int n_sent;
	int n_received;
	while((n_sent=fread(stream,BUF_SIZE,1,read_ptr)!=0){
		n_received=0;
		write(client_fd,stream,BUF_SIZE);
		bzero(stream, sizeof(stream));

		if(n_sent!=n_received){
			do{
				//problemas;
				//enviar parte que falta
				//fatiar stream??

				// for ( size_t i = start; i <= end; ++i ) {
        // 	buffer[j++] = str[i];
    		// }
    		// buffer[j] = 0;

				//não podes enviar de novo pois vais reescrever no client
				fseek(read_ptr, -(n_sent-n_received), SEEK_CUR);//Anda para tras e reenvia o pacote
			}while (n_sent!=n_received);
		}
		
		filesize=filesize-n_sent;//Continua
	}
	fclose(read_ptr);
}
