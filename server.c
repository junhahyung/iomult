/*
 * =====================================================================================
 *
 *       Filename:  server.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  12/10/2019 15:12:42
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Junha Hyung (), sharpeeee@kaist.ac.kr
 *   Organization:  
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <errno.h>
#include <stdbool.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#include "pool.h"
#include "server.h"
#include "util.h"
#include "tcmalloc_api.h"

void* thwork(void* ti);

pthread_spinlock_t workdone_lock;

int main(int argc, char *argv[]){
	void* pageheap = tc_central_init();
	if(pageheap==NULL){
		printf("central initialization failed\n");
		return 0;
	}
	else printf("central init success\n");
    void*threadcache = tc_thread_init();
    if(threadcache==NULL){
        printf("thread init failed\n");
    }
    else printf("init thread cache on main thread success\n");

	if (argc != 4){
		printf("invalid number of inputs\n");
		exit(-1);
	}
    struct sockaddr_in addr;
    int on=1;
    int port = atoi(argv[3]);
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    int num_ready=0, i=0;

    char* servername = argv[1];
    char* targetdir = argv[2];
    char* curdir = ".";
    char* prevdir= "..";
    DIR*d;
	struct hashtable *hash_t = NULL;
    struct dirent *dir = NULL;
    char *docDic[1024] = {0,};
	int docID=0;
	int docfd = -1;


    if((d=opendir(targetdir))==NULL){
    	perror("cannot access directory");
	exit(EXIT_FAILURE);
    }
    if(d){
    	while((dir=readdir(d)) !=NULL){
			char* dname = dir->d_name;
            docDic[docID] = (char*)tc_malloc(strlen(dname)+1);
			strncpy(docDic[docID], dname, strlen(dname)+1);
			if(stringncmp(dname, curdir, 2) && stringncmp(dname, prevdir,3)){
				int strlen1 = strlen(targetdir);
				int strlen2 = strlen(dname);
				char* fullpath = joinpath(targetdir, dname);	
			
				if((docfd = open(fullpath, O_RDONLY)) == -1){
					perror("cannot open file\n");
					exit(EXIT_FAILURE);	
				}
				hash_t = readntokenize(docfd, docID, hash_t);
				docID ++;
				close(docfd);	
				docfd = -1;
			}
		}
		closedir(d);
    }
	printf("finished bootstrapping\n");


    fd_set master_set, working_set;
    pthread_spin_init(&workdone_lock,1);

    if (listen_fd < 0){
        perror("socket() failed");
        exit(-1);
    }

    int rc = setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof(on));
    if(rc<0){
        perror("setsockopt() failed");
        close(listen_fd);
        exit(-1);
    }

    rc = ioctl(listen_fd, FIONBIO, (char*)&on);
    if(rc <0){
        perror("ioctl() failed");
        close(listen_fd);
        exit(-1);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY); 
    addr.sin_port = htons(port);
    //memcpy(&addr.sin_addr, &htonl(INADDR_ANY), sizeof(INADDR_ANY));
    rc = bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr));
    if(rc<0){
        perror("bind() failed");
        close(listen_fd);
        exit(-1);
    }

    rc = listen(listen_fd, 128);
    if(rc<0){
        perror("listen() failed");
        close(listen_fd);
        exit(-1);
    }

    FD_ZERO(&master_set);
    int max_fd = listen_fd;
    FD_SET(listen_fd, &master_set);
    int cnt = 0;

    threadpool thpool = thpool_init(10); 

    do{
        FD_ZERO(&working_set);
        memcpy(&working_set, &master_set, sizeof(master_set));
        rc = select(max_fd+1, &working_set, NULL, NULL, NULL);

        if(rc<0){
            perror("select() failed");
            break;
        }
        if(rc==0){
            printf("select() timed out, End program\n");
            break;
        }
        num_ready = rc;
        int new_fd;
        for(i=0;i<=max_fd && num_ready >0;++i){
            if(FD_ISSET(i, &working_set)){
                num_ready -= 1;
                if(i==listen_fd){
                    while((new_fd = accept(listen_fd, NULL, NULL))){
                        if(new_fd <0){
                            if(errno != EWOULDBLOCK){
                                perror("accept() failed");
                                //end_server = true;
                            }
                            break;
                        }
                        FD_SET(new_fd, &master_set);
                        if(new_fd > max_fd) max_fd = new_fd;
                    }
                }
                // not listen fd - work fd
                else{
                    t_input *ti = tc_malloc(sizeof(t_input)); 
                    memset(ti, 0, sizeof(t_input));
                    ti->fd = i;
					ti->hash_t = hash_t; 
					ti->docdic = (char**)docDic;
                    ti->masterset_t = &master_set; 
                    ti->workingset_t = &working_set;
                    thpool_add_work(thpool, (void*)thwork, (void*)ti);
                    cnt++;
                    FD_CLR(i, &master_set);
                    }
                }
            }
        //FD_ZERO(&working_set);
    } while(1);

    return 0;
}
int workdone=0;
void* thwork(void* ti){
    int connfd = ((t_input*)ti)->fd;
	struct hashtable *hash_t = ((t_input*)ti)->hash_t;
	char** docDic = ((t_input*)ti)->docdic;
    int nb=0, readb=0;
    char buf[1024] = "";
    char results[4096] = "";

    // read first to get header
    do{
        nb = read(connfd, buf+readb, sizeof(buf));
        if(nb<0) printf("error reading\n");
        if(nb==0) break;
        readb += nb;
    } while(readb<8);
    
    // get rest if any
    uint32_t r_total = ((packet*)buf)->length;
    while(readb<r_total){
        nb = read(connfd, buf+readb, sizeof(buf));
        if(nb<0) printf("error reading\n");
        if(nb==0) break;
        readb += nb;
    } 

    /*  
    printf("received packet size: %u\n", ((packet*)buf)->length);
    printf("received packet type: %x\n",((packet*)buf)->msg_type);
    printf("received packet data: %s\n",buf+8);
    */

	search_v2(hash_t, buf+8, docDic, results);
	//search(hash_t, buf+8, docDic);
	int sendpacketlength = strlen(results)+9;
    char* sendpacket = (char*)tc_malloc(sendpacketlength);
    memset(sendpacket, 0, sendpacketlength);
    ((packet*)sendpacket)->length = sendpacketlength;
	if(sendpacketlength==9){
			((packet*)sendpacket)->msg_type = 32;
	}
	else ((packet*)sendpacket)->msg_type = 17;
    memcpy(sendpacket+8, results, strlen(results)+1);
    int wb = write(connfd, sendpacket, sendpacketlength); 

    /*  
    printf("sent packet size: %u\n", ((packet*)sendpacket)->length);
    printf("sent packet type: %x\n",((packet*)sendpacket)->msg_type);
    printf("sent packet data: %s\n",sendpacket+8);
	printf("really sent: %d\n",wb);
    */


    close(connfd);
    tc_free(sendpacket);
    tc_free(ti);
    pthread_spin_lock(&workdone_lock);
    workdone++;
    pthread_spin_unlock(&workdone_lock);
    printf("workdone %d\n",workdone);
    return NULL;
}
