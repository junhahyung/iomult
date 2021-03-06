/*
 * =====================================================================================
 *
 *       Filename:  client.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  12/11/2019 22:49:24
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Junha Hyung (), sharpeeee@kaist.ac.kr
 *   Organization:  
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <pthread.h>

#include "client.h"
#include "tcmalloc_api.h"

void* connect_client(void* arg);

int main(int argc, char** argv){
	void* pageheap = tc_central_init();
	if(pageheap == NULL){
		printf("central initialization failed\n");
		return 0;
	}
	else printf("central init success\n");

	void* threadcache = tc_thread_init();
	if(threadcache == NULL){
		printf("thread init failed\n");
		return 0;
	}
    //struct addrinfo hints, *listp, *p;
    struct sockaddr_in *server_addr = tc_malloc(sizeof(struct sockaddr_in));
    memset(server_addr, 0, sizeof(struct sockaddr_in));
    c_input* client_arg = (c_input*)tc_malloc(sizeof(c_input));
    int server_port, num_thread, num_req; 
    char *server_ip, *search; 
    server_ip = argv[1];
    server_port = atoi(argv[2]);
    num_thread = atoi(argv[3]);
    num_req = atoi(argv[4]);
    search = argv[5];

    if(argc != 6){
        printf("invalid number of inputs\n");
        exit(-1);
    }


    pthread_t thread[num_thread];
    struct hostent *he = gethostbyname(server_ip);
    if(he==NULL){
        herror("gethostbyname");
        exit(-1);
    }

    server_addr->sin_family = AF_INET;
    server_addr->sin_port = htons(server_port);
    server_addr->sin_addr = *((struct in_addr *)he->h_addr);
    bzero(&(server_addr->sin_zero), 8);
    client_arg->num_req = num_req;
    client_arg->server_addr = server_addr;
    client_arg->search = (char*)tc_malloc(strlen(search)+1);
    memset(client_arg->search,0,strlen(search)+1);
    memcpy(client_arg->search, search, strlen(search)+1);

    for(int i=0;i<num_thread;i++){
        if(pthread_create(&thread[i], NULL, connect_client, (void*)client_arg)){
            printf("failed pthread create\n");
            return 0;
        } 
    }

    for(int i=0;i<num_thread;i++){
        if(pthread_join(thread[i],NULL)){
            printf("pthread join failed\n");
            return 0;
        }
    }
}

void* connect_client(void* arg){
    c_input *carg = (c_input*)arg;
    int num_req = carg->num_req;
    char* search = carg->search;
    struct sockaddr_in* server_addr = carg->server_addr;
    uint32_t packet_size = sizeof(packet)+strlen(search)+1; //for testing

    char* pkt = (char*)tc_malloc(packet_size);
    memset(pkt, 0, packet_size);
    ((packet*)pkt)->length = packet_size;
    ((packet*)pkt)->msg_type = 16; //0x00000010
    strncpy(pkt+8,search, strlen(search)+1);

	void* threadcache = tc_thread_init();
	if(threadcache == NULL){
		printf("thread init failed\n");
		return NULL;
	}

    /* 
    printf("packet length: %u\n",((packet*)pkt)->length);
    printf("packet type: %x\n",((packet*)pkt)->msg_type);
    printf("packet data: %s\n", pkt+8);
    */

    for(int i=0;i<num_req;i++){
        int client_fd;
        int nb=0, sentb=0, readb=0;
        char buf[4096] = "";
        int on=1;

        if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
            perror("socket");
            exit(1);
        }
        int rc = setsockopt(client_fd, SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof(on));
        if(rc<0){
            perror("setsockopt() failed\n");
            close(client_fd);
            exit(-1);
        }

        if(connect(client_fd, (struct sockaddr *)server_addr, sizeof(struct sockaddr)) == -1) {
            perror("connect");
            exit(1);
        }

        if((nb=write(client_fd, pkt, packet_size))<0){
            printf("error writing\n");
        }

        // read first to get header
        do{
            nb = read(client_fd, buf+readb, sizeof(buf));
            if(nb<0) printf("error reading\n");
            if(nb==0) break;
            readb += nb;
        } while(readb<8);
        int r_total = ((packet*)buf)->length;
        // get the rest
        while(readb<r_total){
            nb = read(client_fd, buf+readb, sizeof(buf));
            if(nb<0) printf("error reading\n");
            if(nb==0) break;
            readb += nb;
        }

        /*  
        printf("received packet size: %d\n",readb);
        printf("received packet type: %x\n",((packet*)buf)->msg_type);
        printf("received packet data: %s\n",buf+8);
        */
        //testing for query division
        /* 
        if(strcmp(buf+8, "100west.txt: line #6\ndoc4.txt: line #15\ndoc5.txt: line #11\n")){
            printf("error\n");
        }
        */
        /* 
        if(strcmp(buf+8, "100west.txt: line #26\n100west.txt: line #322\ndoc4.txt: line #12\ndoc5.txt: line #8\ndoc5.txt: line #23\n")){
            printf("error\n");
        }
        */


        close(client_fd);
    }

    tc_free(pkt);
    return 0;
}

