/*
 * =====================================================================================
 *
 *       Filename:  server.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  12/13/2019 10:06:04 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 * =====================================================================================
 */

#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/select.h>
#include <stdint.h>

typedef struct threadinput{
    int fd;
	struct hashtable* hash_t;
	char** docdic;
    fd_set* masterset_t;
    fd_set* workingset_t;
}t_input;

typedef struct packet {
    uint32_t length;
    int msg_type;
} packet;
