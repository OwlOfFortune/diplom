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

void mka_cpl_freeze(connection_params_list *connectionParamsList, int type) {
    int rand1 = rand() % 1000 + 1;
    time_t time2 = time(NULL) % 1000;
    if(type == 0){
        while(connectionParamsList->cp_is_busy != 0) usleep(time2);
        connectionParamsList->cp_is_busy = rand1;
        while(connectionParamsList->cp_is_busy != rand1 && connectionParamsList->cp_is_busy != 0)
            usleep(time2);
        connectionParamsList->cp_is_busy = rand1;
    } else {
        int p = 0;
        while(connectionParamsList->cp_is_busy > 0) usleep(time2);
        --connectionParamsList->cp_is_busy;
        if(++connectionParamsList->cp_is_busy > 0)
            while(connectionParamsList->cp_is_busy > 0) usleep(time2);
        --connectionParamsList->cp_is_busy;
    }
}

void mka_cpl_unfreeze(connection_params_list *connectionParamsList, int type) {
    if(type == 0){
        connectionParamsList->cp_is_busy = 0;
    } else {
        ++connectionParamsList->cp_is_busy;
    }
}

inline int mka_cp_create_socket(connection_params *connectionParams){
    if((connectionParams->sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL))) == -1) {
        perror("socket");
        return 1;
    }
    return 0;
}

void mka_cp_send_data(connection_params *connectionParams, char *data, int data_len){

    int sendbuf_len = sizeof(struct ether_header) + data_len;
    unsigned char *sendbuf = (unsigned char*) calloc(sendbuf_len,sizeof(char));
    struct ether_header *eth_h =  (struct ether_header *) sendbuf;

    if(data_len > MTU_SIZE) printf("Data is too long");


    /* Construct the Ethernet header */
    memcpy(eth_h->ether_shost, connectionParams->src_mac, ETH_ALEN);
    memcpy(eth_h->ether_dhost, connectionParams->dest_mac, ETH_ALEN);
    eth_h->ether_type = htons(MKA_PROTO);

    /* Packet data */
    memcpy(sendbuf+sizeof(struct ether_header) , data, data_len);

    /* Send packet */
    if (sendto(connectionParams->sockfd, sendbuf, sendbuf_len, 0, (struct sockaddr*)&connectionParams->socket_address, sizeof(struct sockaddr_ll)) < 0)
        printf("Send failed\n");
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

int mka_cpl_upd_peer_id_by_interface_id(connection_params_list *connectionParamsList, int id, int peer_id){
    mka_cpl_freeze(connectionParamsList,0);
    if(id < connectionParamsList->num_of_c_p && id >= 0){
        connectionParamsList->c_p_list[id].peer_id = peer_id;
        mka_cpl_unfreeze(connectionParamsList,0);
        return 0;
    }
    mka_cpl_unfreeze(connectionParamsList,0);
    return 1;
}

void mka_cpl_add_connection_param_in_list(connection_params_list *connectionParamsList, connection_params *connectionParams){
    mka_cpl_freeze(connectionParamsList,0);
    connectionParams->interface_num = connectionParamsList->num_of_c_p;
    connectionParamsList->c_p_list = (connection_params *) realloc (connectionParamsList->c_p_list, (++connectionParamsList->num_of_c_p) * sizeof(connection_params));
    memmove(&connectionParamsList->c_p_list[connectionParamsList->num_of_c_p - 1], connectionParams, sizeof(connection_params));
    mka_cpl_unfreeze(connectionParamsList,0);
}

int mka_cp_init(connection_params *c_p, char *interface, int interface_len, unsigned char *dest_mac){
    struct ifreq if_idx;
    struct ifreq if_mac;
    c_p->peer_id = -1;
    c_p->stop = 0;
    c_p->tid_recv = -1;
    c_p->pause_HT = 0;
    c_p->interface = (char *) calloc(interface_len, sizeof (char));
    memcpy(c_p->interface, interface, interface_len);
    c_p->interface_len = interface_len;
    memcpy(c_p->dest_mac, dest_mac, 6);
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
//    if(setsockopt(c_p->sockfd, SOL_SOCKET, SO_BINDTODEVICE, (void*)&if_idx, sizeof(struct ifreq)) == -1) {
//        close(c_p->sockfd);
//        return 1;
//    }
//    if (ioctl (c_p->sockfd, SIOCGIFFLAGS, &if_idx) < 0){
//        perror ("SIOCGIFFLAGS");
//        close(c_p->sockfd);
//        return 1;
//    }
//    }
//    if_idx.ifr_flags |= IFF_PROMISC;
//    if (ioctl (c_p->sockfd, SIOCSIFFLAGS, &if_idx) < 0){
//        perror ("SIOCSIFFLAGS");
//        close(c_p->sockfd);
//        return 1;
//    }

    // Определяем размер MTU
//    if(ioctl(fd, SIOCGIFMTU, &ifr) < 0)
//    {
//        perror("ioctl SIOCGIFMTU");
//        return -1;
//    }
//    ifp->mtu = ifr.ifr_mtu;

    c_p->socket_address.sll_family=AF_PACKET;
    c_p->socket_address.sll_protocol=htons(ETH_P_ALL);
    c_p->socket_address.sll_ifindex=if_idx.ifr_ifindex;
//    c_p->socket_address.sll_hatype=ARPHRD_NETROM;
//    c_p->socket_address.sll_pkttype=PACKET_HOST;
    c_p->socket_address.sll_halen = ETH_ALEN;
//    memcpy(c_p->socket_address.sll_addr, c_p->dest_mac, ETH_ALEN);

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
    mka_cpl_freeze(connectionParamsList,0);
    for(int i = 0; i < connectionParamsList->num_of_c_p; ++i){
        connectionParamsList->c_p_list[i].stop = 1;
        close(connectionParamsList->c_p_list[i].sockfd);
        free(connectionParamsList->c_p_list[i].interface);
        if(connectionParamsList->c_p_list[i].tid_recv != -1)
            pthread_join(connectionParamsList->c_p_list[i].tid_recv,NULL);
    }
    free(connectionParamsList->c_p_list);
    mka_cpl_unfreeze(connectionParamsList,0);
    return 0;
}

int mka_cpl_init(connection_params_list *connectionParamsList){
    connectionParamsList->num_of_c_p = 0;
    connectionParamsList->c_p_list = (connection_params *) malloc(0);
    connectionParamsList->cp_is_busy =0;
    return 0;
}
