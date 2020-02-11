/*
 * =====================================================================================
 *
 *       Filename:  client.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  12/13/2019 17:07:22
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Junha Hyung (), sharpeeee@kaist.ac.kr
 *   Organization:  
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include <sys/socket.h>
#include <stdint.h>

typedef struct connect_input {
    int num_req;
    char* search;
    struct sockaddr_in* server_addr;
} c_input;

typedef struct packet {
    uint32_t length;
    int msg_type;
} packet;
