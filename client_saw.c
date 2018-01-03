#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>

#define WIN  5
#define TIMEOUT 1
#define DELAY 210000
struct MyPacket		//Design your packet format (fields and possible values)
{
    char type;
    unsigned short seqno;
    unsigned int offset;
    int length;
    int eof;
    char payload[256];
};


void error(const char *msg)
{
    perror(msg);
    exit(0);
}


int main(int argc, char *argv[])
{
    int sockfd, portno, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    socklen_t addrlen = sizeof(serv_addr);
    char buffer[256];


    char message[256];

    struct MyPacket pkt_buf[5] = {};
    struct MyPacket pkt = {};
    struct timeval timeout;
    struct MyPacket *pkt_rcvd = malloc(sizeof(struct MyPacket));

    clock_t start, end;

    if (argc != 4) {
       fprintf(stderr,"usage %s hostname port filename\n", argv[0]);
       exit(0);
    }


    /* Create a socket */
    portno = atoi(argv[2]);
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");
    server = gethostbyname(argv[1]);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }

    /* fill in the server's address and port*/
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
         (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);
    serv_addr.sin_port = htons(portno);

/*
    if (connect(sockfd,(struct sockaddr*)&serv_addr,sizeof(serv_addr)) < 0)
 	error("ERROR connecting");*/
//    bzero(&timeout, sizeof(timeout));
    timeout.tv_usec = DELAY; 
    int errorid = setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    if (errorid < 0){
        printf("errorid: %d\n", errorid);
        error("ERROR timeout set\n");
    }



 	/* create file for reading*/

    FILE *fp = fopen(argv[3], "rb");
    if (fp == NULL)
    {
        error("ERROR open file error");
    }

    int seqno = 0;
    int can_send = 1;
    int resend = 0;
    char payload[256];
    unsigned int offset = 0;
    int done = 0;
	// send data in chunks of 256 bytes
    clock_t start_run;
    clock_t end_run;
    double run_time;
    start_run = clock();
    while(!done)
    {
		//Do Stuff

      if(can_send) {

        bzero(buffer,256);

        int read = fread(buffer, 1, 256, fp);
        if(read < 0)
          error("Read In Error\n");

        pkt.offset = offset;
        pkt.length = read;
        offset += read;
        strncpy(payload, buffer, 256);

        pkt.type = 'P';
        pkt.seqno = seqno;
        pkt.eof = (feof(fp));
        
        strncpy(pkt.payload, payload, 256);

        can_send = 0;
  	n = sendto(sockfd, &pkt, sizeof(struct MyPacket), 0, (struct sockaddr*)&serv_addr,sizeof(serv_addr));
        start = clock();
        if(n==-1)
          error("Error writing to socket\n");

        printf("[send data] %d(%d)\n", pkt.offset, pkt.length);
        resend = 0;
      }

      if(((double) clock() - start) / CLOCKS_PER_SEC > TIMEOUT) {
//        n = send(sockfd, &pkt, sizeof(struct MyPacket),0); //flags = 0?
 n = sendto(sockfd, &pkt, sizeof(struct MyPacket), 0, (struct sockaddr*)&serv_addr,sizeof(serv_addr));  
      start = clock();
        if(n==-1)
          error("Error writing to socket\n");
        printf("[resend data] %d(%d)\n", pkt.offset, pkt.length);
      }

      bzero(buffer, 256);

      int serv_len = sizeof(serv_addr);
      int errorid = setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
      if (errorid < 0){
        printf("errorid: %d\n", errorid);
        error("ERROR timeout set\n");
      }

      n = recvfrom(sockfd, pkt_rcvd, sizeof(struct MyPacket), 0, (struct 
sockaddr*)&serv_addr,&serv_len);
      if(n>0) {
        if(pkt_rcvd->seqno==seqno && pkt_rcvd->type=='C') {
          printf("[recv ack] %d\n", pkt_rcvd->seqno);
          seqno = (seqno+1)%2;
          can_send = 1;
          if(feof(fp)) done = 1;
        }
      }
    }
    end_run = clock();
    run_time = (double)(end_run - start_run) /CLOCKS_PER_SEC;
    printf("Sent %d bytes in %f seconds\n", offset, run_time);
    if(run_time == 0){
      printf("A bigger file is needed to calculate throughput\n");	
    } else{
      printf("Throughput = %f bytes per second", (offset/run_time)); 
    }
    printf("[completed]\n");
    close(sockfd);
    return 0;
}
