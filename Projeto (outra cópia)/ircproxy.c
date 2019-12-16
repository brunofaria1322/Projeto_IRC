
int main(int argc, char *argv[]) {

  if (argc != 2) {
		printf("ircproxy {port}\n");
		exit(-1);
	}

  proxy_port=atoi(argv[1]);
  int fd, client;
	struct sockaddr_in addr, client_addr;
	int client_addr_size;

	bzero((void *) &addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(server_port);//comunicacoes de rede utilizam big endian - conversão de inteiro short no host para tipo de inteiro a guardar com formato rede

}
