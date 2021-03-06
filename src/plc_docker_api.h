/*------------------------------------------------------------------------------
 *
 *
 * Copyright (c) 2016, Pivotal.
 *
 *------------------------------------------------------------------------------
 */


#ifndef PLC_DOCKER_API_H
#define PLC_DOCKER_API_H

#include "plc_configuration.h"

#ifndef CURL_DOCKER_API
    int plc_docker_connect(void);
    int plc_docker_create_container(int sockfd, plcContainer *cont, char **name);
    int plc_docker_start_container(int sockfd, char *name);
    int plc_docker_kill_container(int sockfd, char *name);
    int plc_docker_inspect_container(int sockfd, char *name, int *port);
    int plc_docker_wait_container(int sockfd, char *name);
    int plc_docker_delete_container(int sockfd, char *name);
    int plc_docker_disconnect(int sockfd);
#endif

#endif /* PLC_DOCKER_API_H */
