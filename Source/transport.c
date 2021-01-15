#include "../Headers/transport.h"

int mka_cp_get_interface_MAC(struct ifreq *if_mac, connection_params *connectionParams)
{
    /* Get the MAC address of the interface to send on */
    memset(if_mac, 0, sizeof(struct ifreq));
    memcpy(if_mac->ifr_name, connectionParams->interface, connectionParams->interface_len);
    if (ioctl(connectionParams->sockfd, SIOCGIFHWADDR, if_mac) < 0) {
        perror("SIOCGIFHWADDR");
        return 1;
    }
    return 0;
}

int mka_cp_get_interface_index(struct ifreq *if_idx, connection_params *connectionParams)
{
    /* Get the index of the interface to send on */
    memset(if_idx, 0, sizeof(struct ifreq));
    strncpy(if_idx->ifr_name, connectionParams->interface, connectionParams->interface_len);
    if (ioctl(connectionParams->sockfd, SIOCGIFINDEX, if_idx) < 0) {
        perror("SIOCGIFINDEX");
        return 1;
    }
    return 0;
}

void mka_cp_freeze(connection_params *connectionParams, int type, int list_type) {
    int rand1 = rand() % 1000 + 1;
    time_t time2 = time(NULL) % 1000;
    int *is_busy;
    if(list_type == 0) is_busy = &connectionParams->pcl_is_busy;
    else is_busy = &connectionParams->acl_is_busy;
    if(type == 0){
        while(*is_busy != 0) usleep(time2);
        *is_busy = rand1;
        while(*is_busy != rand1 && *is_busy != 0)
            usleep(time2);
        *is_busy = rand1;
    } else {
        int p = 0;
        while(*is_busy > 0) usleep(time2);
        --*is_busy;
        if(++*is_busy > 0)
            while(*is_busy > 0) usleep(time2);
        --*is_busy;
    }
}

void mka_cp_unfreeze(connection_params *connectionParams, int type, int list_type) {
    int *is_busy;
    if(list_type == 0) is_busy = &connectionParams->pcl_is_busy;
    else is_busy = &connectionParams->acl_is_busy;
    if(type == 0){
        *is_busy = 0;
    } else {
        ++*is_busy;
    }
}

inline int mka_cp_create_socket(connection_params *connectionParams){
    if((connectionParams->sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL))) == -1) {
        perror("socket");
        return 1;
    }
    return 0;
}

void mka_cp_send_data(connection_params *connectionParams, char *data, int data_len, int peer_id) {

    int sendbuf_len = sizeof(struct ether_header) + data_len;
    unsigned char *sendbuf = (unsigned char*) calloc(sendbuf_len,sizeof(char));
    struct ether_header *eth_h =  (struct ether_header *) sendbuf;
    unsigned char dest_mac[] = {0xff,0xff,0xff,0xff,0xff,0xff}, *dest_found;

    if(data_len > MTU_SIZE) printf("Data is too long");


    /* Construct the Ethernet header */
    memcpy(eth_h->ether_shost, connectionParams->src_mac, ETH_ALEN);
    if((dest_found = mka_cp_get_dest_mac_by_peer_id(connectionParams, peer_id)) == NULL)
        memcpy(eth_h->ether_dhost, &dest_mac, ETH_ALEN);
    else memcpy(eth_h->ether_dhost, dest_found, ETH_ALEN);
    eth_h->ether_type = htons(MKA_PROTO);

    /* Packet data */
    memcpy(sendbuf+sizeof(struct ether_header) , data, data_len);

    /* Send packet */
    if (sendto(connectionParams->sockfd, sendbuf, sendbuf_len, 0, (struct sockaddr*)&connectionParams->socket_address, sizeof(struct sockaddr_ll)) < 0)
        printf("Send failed\n");
    free(sendbuf);
}

char *mka_cp_recieve_data(connection_params *connectionParams, int *recieved_buf_len){
    fd_set rfds;
    struct timeval tv;
    int retval, recv_buf_len;
    int buffer_len = sizeof(struct ether_header) + 1024;
    char *buffer = (char*) calloc(buffer_len,sizeof(char));
    struct ether_header *eh = (struct ether_header *) buffer;

    FD_ZERO(&rfds);
    FD_SET(connectionParams->sockfd, &rfds);
    tv.tv_sec = 0;
    tv.tv_usec = 500000;
    retval = select(connectionParams->sockfd + 1, &rfds, NULL, NULL, &tv);
    if (!retval){
        free(buffer);
        return NULL;
    }
    memset(buffer, 0, buffer_len);
    if ((*recieved_buf_len = recv(connectionParams->sockfd, buffer, buffer_len, 0)) <= 0) {
        if(buffer != NULL) free(buffer);
        return NULL;
    }
    if (htons(eh->ether_type) == MKA_PROTO) return (char *) realloc(buffer, sizeof(char) * (*recieved_buf_len));
    free(buffer);
    return NULL;
}

char *mka_cp_recieve_data_sec(connection_params *connectionParams, int *recieved_buf_len, time_t sec_timeout){
    char *buffer;
    time_t t = time(NULL);
    while (1) {
        if((buffer = mka_cp_recieve_data(connectionParams, recieved_buf_len)) != NULL) return buffer;
        if(time(NULL) - t >= sec_timeout) return NULL;
    }
}

int mka_cp_add_potential_connection(connection_params *connectionParams, int peer_id,
                                    unsigned char *dest_mac) {
    mka_cp_freeze(connectionParams, 0, 0);
    connectionParams->potential_connection_list = (connection *) realloc (connectionParams->potential_connection_list, (++connectionParams->pcl_capacity) * sizeof(connection));
    connectionParams->potential_connection_list[connectionParams->pcl_capacity - 1].peer_id = peer_id;
    memcpy(connectionParams->potential_connection_list[connectionParams->pcl_capacity - 1].dest_mac, dest_mac, 6);
    connectionParams->potential_connection_list[connectionParams->pcl_capacity - 1].HT_pause = 0;
    mka_cp_unfreeze(connectionParams, 0, 0);
    return 0;
}

int mka_cp_add_alive_connection(connection_params *connectionParams, int peer_id,
                                    unsigned char *dest_mac) {
    mka_cp_freeze(connectionParams, 0, 1);
    connectionParams->alive_connection_list = (connection *) realloc (connectionParams->alive_connection_list, (++connectionParams->acl_capacity) * sizeof(connection));
    connectionParams->alive_connection_list[connectionParams->acl_capacity - 1].peer_id = peer_id;
    memcpy(connectionParams->alive_connection_list[connectionParams->acl_capacity - 1].dest_mac, dest_mac, 6);
    connectionParams->alive_connection_list[connectionParams->acl_capacity - 1].HT_pause = 0;
    mka_cp_unfreeze(connectionParams, 0, 1);
    return 0;
}

int mka_cp_get_connection_num_by_peer_id(connection_params *connectionParams, int peer_id, int type) {
    if(type == 1 || type == -1){
        mka_cp_freeze(connectionParams, 1, 0);
        for(int i = 0; i < connectionParams->acl_capacity; ++i)
            if(peer_id == connectionParams->alive_connection_list[i].peer_id) {
                mka_cp_unfreeze(connectionParams, 1, 0);
                return i;
            }
        mka_cp_unfreeze(connectionParams, 1, 0);
    }
    if(type == 0 || type == -1) {
        mka_cp_freeze(connectionParams, 1, 1);
        for (int i = 0; i < connectionParams->pcl_capacity; ++i)
            if (peer_id == connectionParams->potential_connection_list[i].peer_id) {
                mka_cp_unfreeze(connectionParams, 1, 1);
                return i;
            }
        mka_cp_unfreeze(connectionParams, 1, 1);
    }
    return -1;
}

int mka_cp_make_potential_connection_alive_by_peer_id(connection_params *connectionParams, int peer_id) {
    int num = mka_cp_get_connection_num_by_peer_id(connectionParams, peer_id, 0);
    if(num == -1) return -1;
    mka_cp_add_alive_connection(connectionParams, peer_id, connectionParams->potential_connection_list[num].dest_mac);
    mka_cp_rm_connection_by_peer_id(connectionParams,peer_id, 0);
    return 0;
}

int mka_cp_get_potential_connection_num_by_peer_id(connection_params *connectionParams, int peer_id) {
    mka_cp_freeze(connectionParams, 0, 1);
    for(int i = 0; i < connectionParams->pcl_capacity; ++i)
        if(connectionParams->potential_connection_list[i].peer_id == peer_id) {
            mka_cp_unfreeze(connectionParams, 0, 1);
            return i;
        }
    mka_cp_unfreeze(connectionParams, 0, 1);
    return -1;
}

unsigned char *mka_cp_get_dest_mac_by_peer_id(connection_params *connectionParams, int peer_id){
    for(int i = 0; i < connectionParams->acl_capacity; ++i)
        if(peer_id == connectionParams->alive_connection_list[i].peer_id) return connectionParams->alive_connection_list[i].dest_mac;
    for(int i = 0; i < connectionParams->pcl_capacity; ++i)
        if(peer_id == connectionParams->potential_connection_list[i].peer_id) return connectionParams->potential_connection_list[i].dest_mac;
    return NULL;
}

// нельзя использовать без контроля доступа
int mka_cp_rm_connection_by_peer_id(connection_params *connectionParams, int peer_id, int list_type) {
    int *capacity;
    connection *list;
    if(list_type == 0){
        list = connectionParams->potential_connection_list;
        capacity = &connectionParams->pcl_capacity;
    } else if(list_type == 1){
        list = connectionParams->alive_connection_list;
        capacity = &connectionParams->acl_capacity;
    }
    int num = mka_cp_get_connection_num_by_peer_id(connectionParams, peer_id, list_type);
    if (*capacity > num && num >= 0) {
        --*capacity;
        if(*capacity == 0) {
            free(list);
            list = (connection *)calloc(0,sizeof(connection));
        } else if (*capacity >= num) {
            if(*capacity > num){
                memcpy(&list[num], &list[num + 1], sizeof(connection) * (*capacity - num));
            }
            list = (connection *) realloc(list, sizeof(connection) * (*capacity));
        }
    }
    return 0;
}

// нельзя удалять и добавлять в любое время
void mka_cpl_add_connection_param_in_list(connection_params_list *connectionParamsList, connection_params *connectionParams){
    connectionParams->interface_num = connectionParamsList->num_of_c_p;
    connectionParamsList->c_p_list = (connection_params *) realloc (connectionParamsList->c_p_list, (++connectionParamsList->num_of_c_p) * sizeof(connection_params));
    memmove(&connectionParamsList->c_p_list[connectionParamsList->num_of_c_p - 1], connectionParams, sizeof(connection_params));
}

int mka_cp_init(connection_params *c_p, char *interface, int interface_len, unsigned char *dest_mac){
    struct ifreq if_idx;
    struct ifreq if_mac;
    c_p->stop = 0;
    c_p->tid_recv = -1;
    c_p->interface = (char *) calloc(interface_len, sizeof (char));
    memcpy(c_p->interface, interface, interface_len);
    c_p->interface_len = interface_len;
    c_p->potential_connection_list = (connection *)calloc(0,sizeof(connection));
    c_p->alive_connection_list = (connection *)calloc(0,sizeof(connection));
    c_p->pcl_capacity = 0;
    c_p->pcl_is_busy = 0;
    c_p->acl_capacity = 0;
    c_p->acl_is_busy = 0;
    if(mka_cp_create_socket(c_p)) return 1;

    /* Getting interface params */
    if(mka_cp_get_interface_MAC(&if_mac, c_p)){
        close(c_p->sockfd);
        return 1;
    }
    if(mka_cp_get_interface_index(&if_idx, c_p)){
        close(c_p->sockfd);
        return 1;
    }

    c_p->socket_address.sll_family=AF_PACKET;
    c_p->socket_address.sll_protocol=htons(ETH_P_ALL);
    c_p->socket_address.sll_ifindex=if_idx.ifr_ifindex;
    c_p->socket_address.sll_halen = ETH_ALEN;

    if(bind(c_p->sockfd, (struct sockaddr *)&c_p->socket_address,sizeof(c_p->socket_address)) != 0){
        perror("error bind");
        close(c_p->sockfd);
        return 1;
    }
    /* Set params for send */

    /* Construct the Ethernet header */
    memcpy(c_p->src_mac,if_mac.ifr_hwaddr.sa_data, ETH_ALEN);
    return 0;
}

int mka_cpl_destruct(connection_params_list *connectionParamsList){
    for(int i = 0; i < connectionParamsList->num_of_c_p; ++i){
        connectionParamsList->c_p_list[i].stop = 1;
        close(connectionParamsList->c_p_list[i].sockfd);
        free(connectionParamsList->c_p_list[i].interface);
        if(connectionParamsList->c_p_list[i].tid_recv != -1)
            pthread_join(connectionParamsList->c_p_list[i].tid_recv,NULL);
        free(connectionParamsList->c_p_list[i].potential_connection_list);
        free(connectionParamsList->c_p_list[i].alive_connection_list);
    }
    free(connectionParamsList->c_p_list);
    return 0;
}

int mka_cpl_init(connection_params_list *connectionParamsList){
    connectionParamsList->num_of_c_p = 0;
    connectionParamsList->c_p_list = (connection_params *) malloc(0);
    return 0;
}
