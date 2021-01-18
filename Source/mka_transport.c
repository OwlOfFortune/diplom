#include "../Headers/mka_transport.h"

/*  nonce:
    11 22 33 44 55 66 77 00 FF EE DD CC BB AA 99 88 */
static ak_uint8 iv128[16] = {
        0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x00, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11 };

void mka_tr_send_data(connection_params *connectionParams, int id_recv, mka_info *mka_i, char *data, int data_len,
                      int msg_type) {
    int num_of_chars = (MTU_SIZE - sizeof(mka_tr_encr_msg) - sizeof(mka_tr_open_msg) - sizeof(struct ether_header) - 1);
    int num_of_parts = (int)ceil(data_len / num_of_chars);
    if(num_of_parts == 0) num_of_parts = 1;
    int smallest_part = data_len  - num_of_chars * (num_of_parts - 1);
    char *pointer = data;
    int sendbuf_len = MTU_SIZE;
    char *sendbuf = (char*) calloc(sendbuf_len,sizeof(char));
    mka_tr_open_msg *mka_t_o = (struct mka_tr_open_msg *)sendbuf;
    mka_tr_encr_msg *mka_t = (struct mka_tr_encr_msg *)(sendbuf + sizeof(struct mka_tr_open_msg));

    mka_t_o->id_src = mka_i->id;
//    printf("Id sent %d\n", mka_i->id);
    mka_t->id_rcv = id_recv;
    mka_t_o->encr = 0;
    mka_t->KS_priority = 0;
    mka_t->num_of_fragments = num_of_parts;
    mka_t_o->type = msg_type;

//    printf("--- Send func ---\n");
    // data может быть любого размера, нужно отправлять фрагменты, нужно учесть фрагмент меньшей длины
    for (int i = 0; i < num_of_parts - 1; ++i) {
        mka_t->fragment_id = i;
        memcpy(sendbuf + sizeof(mka_tr_encr_msg) + sizeof(mka_tr_open_msg), pointer, num_of_chars);
        pointer += num_of_chars;
//        print_mka_frame_without_ether(sendbuf, sendbuf_len);
        sendbuf[sendbuf_len - 1] = '\0';
        mka_cp_send_data(connectionParams, sendbuf, sendbuf_len, id_recv);
        memset(sendbuf + sizeof(mka_tr_encr_msg) + sizeof(mka_tr_open_msg), 0, num_of_chars);
    }
    mka_t->fragment_id = num_of_parts - 1;
    memcpy(sendbuf + sizeof(mka_tr_encr_msg) + sizeof(mka_tr_open_msg), pointer, smallest_part);
    sendbuf[sizeof(mka_tr_encr_msg) + sizeof(mka_tr_open_msg) + smallest_part] = '\0';
//    print_mka_frame_without_ether(sendbuf, sizeof(mka_transport_encr_msg) + sizeof(mka_transport_open_msg) + smallest_part);
    mka_cp_send_data(connectionParams, sendbuf,
                     sizeof(mka_tr_encr_msg) + sizeof(mka_tr_open_msg) + smallest_part, id_recv);
    free(sendbuf);
}

int mka_tr_send_encr_data(connection_params *connectionParams, int id_recv, mka_info *mka_i, ak_uint8 *data, int data_len,
                          int msg_type) {
    int num_of_chars = (MTU_SIZE - sizeof(mka_tr_encr_msg) - sizeof(mka_tr_open_msg) - sizeof(struct ether_header) - 16 - 1); // для нулевого символа
    int num_of_parts = (int)ceil(data_len / num_of_chars);
    if(data_len < num_of_chars) num_of_parts = 1;
    int smallest_part = data_len  - num_of_chars * (num_of_parts - 1);
    ak_uint8 *pointer = data;
    int sendbuf_len = MTU_SIZE - sizeof (struct ether_header); // добавляется в send(), требуется для подсчета контр. суммы
    char *sendbuf = (char*) calloc(MTU_SIZE,sizeof(char));
    struct ether_header *eh = (struct ether_header *)sendbuf;
    mka_tr_open_msg *mka_t_o = (struct mka_tr_open_msg *)(sendbuf + sizeof (struct ether_header));
    mka_tr_encr_msg *mka_t = (struct mka_tr_encr_msg *)(sendbuf + sizeof (struct ether_header) + sizeof(struct mka_tr_open_msg));
    unsigned char *dest_found;
    int num = mka_key_str_get_peer_key_num_by_peer_id(&mka_i->mkaKeyStr, id_recv);
//    printf("NUM Keys: %d\n", num);

    memcpy(eh->ether_shost, connectionParams->src_mac, ETH_ALEN);
    if((dest_found = mka_cp_get_dest_mac_by_peer_id(connectionParams, id_recv)) == NULL) {
        printf("Send error, no such peer\n");
        if(sendbuf != NULL) free(sendbuf);
        return 1;
    }
    memcpy(eh->ether_dhost, dest_found, ETH_ALEN);
    eh->ether_type = htons(MKA_PROTO);
    mka_t_o->id_src = mka_i->id;
    mka_t->id_rcv = id_recv;
    mka_t_o->encr = 1;
    mka_t->KS_priority = mka_i->my_KS_priority;
    mka_t->num_of_fragments = num_of_parts;
    mka_t_o->type = msg_type;

    // data может быть любого размера, нужно отправлять фрагменты, нужно учесть фрагмент меньшей длины
    for (int i = 0; i < num_of_parts - 1; ++i) {
        mka_t->fragment_id = i;
        memcpy(sendbuf + sizeof(mka_tr_encr_msg) + sizeof(mka_tr_open_msg) + sizeof (struct ether_header), pointer, num_of_chars);
//        print_mka_frame(sendbuf, MTU_SIZE - 16);
        pointer += num_of_chars;
        if(msg_type == 0)
            ak_bckey_encrypt_mgm(
                &mka_i->mkaKeyStr.peer_keys_list[num].CAK,              /* ключ, используемый для шифрования данных */
                &mka_i->mkaKeyStr.peer_keys_list[num].CAK,             /* ключ, используемый для имитозащиты данных */
                sendbuf,                  /* указатель на ассоциированные данные */
                sizeof(mka_tr_open_msg) + sizeof (struct ether_header),          /* длина ассоциированных данных */
                sendbuf + sizeof( mka_tr_open_msg ) + sizeof (struct ether_header),                  /* указатель на зашифровываемые данные */
                sendbuf + sizeof( mka_tr_open_msg ) + sizeof (struct ether_header),   /* указатель на область памяти,
                         в которую помещаются зашифрованные данные */
                sendbuf_len - sizeof( mka_tr_open_msg ) - sizeof (struct ether_header) - 16,              /* размер зашифровываемых данных */
                iv128,             /* синхропосылка (инициализационный вектор) */
                sizeof( iv128 ),                       /* размер синхропосылки */
                /* указатель на область памяти,
             в которую помещается имитовставка */
                sendbuf + sizeof (struct ether_header) + sendbuf_len - 16,
                16                                      /* размер имитовставки */
            );
        else if(msg_type == 1) {
            ak_bckey_encrypt_mgm(
                    &mka_i->mkaKeyStr.peer_keys_list[num].KEK,              /* ключ, используемый для шифрования данных */
                    &mka_i->mkaKeyStr.peer_keys_list[num].KEK,             /* ключ, используемый для имитозащиты данных */
                    sendbuf,                  /* указатель на ассоциированные данные */
                    sizeof(mka_tr_open_msg) + sizeof (struct ether_header),          /* длина ассоциированных данных */
                    sendbuf + sizeof( mka_tr_open_msg ) + sizeof (struct ether_header),                  /* указатель на зашифровываемые данные */
                    sendbuf + sizeof( mka_tr_open_msg ) + sizeof (struct ether_header),   /* указатель на область памяти,
                         в которую помещаются зашифрованные данные */
                    sendbuf_len - sizeof( mka_tr_open_msg ) - sizeof (struct ether_header) - 16,              /* размер зашифровываемых данных */
                    iv128,             /* синхропосылка (инициализационный вектор) */
                    sizeof( iv128 ),                       /* размер синхропосылки */
                    /* указатель на область памяти,
                 в которую помещается имитовставка */
                    sendbuf + sizeof (struct ether_header) + sendbuf_len - 16,
                    16                                      /* размер имитовставки */
            );
        }
        sendbuf[sendbuf_len - 1] = '\0';
        mka_cp_send_data(connectionParams, sendbuf + sizeof(struct ether_header), sendbuf_len, id_recv);
        memset(sendbuf + sizeof(mka_tr_encr_msg) + sizeof(mka_tr_open_msg) + sizeof (struct ether_header), 0, num_of_chars + 16);
    }
    mka_t->fragment_id = num_of_parts - 1;
    memcpy(sendbuf + sizeof(mka_tr_encr_msg) + sizeof(mka_tr_open_msg) + sizeof (struct ether_header), pointer, smallest_part);
//    print_mka_frame(sendbuf, sizeof(mka_transport_encr_msg) + sizeof (struct ether_header) + sizeof(mka_transport_open_msg) + smallest_part);
    if(msg_type == 0)
        ak_bckey_encrypt_mgm(
                &mka_i->mkaKeyStr.peer_keys_list[num].CAK,              /* ключ, используемый для шифрования данных */
                &mka_i->mkaKeyStr.peer_keys_list[num].CAK,             /* ключ, используемый для имитозащиты данных */
                sendbuf,                  /* указатель на ассоциированные данные */
                sizeof(mka_tr_open_msg) + sizeof (struct ether_header),          /* длина ассоциированных данных */
                sendbuf + sizeof( mka_tr_open_msg ) + sizeof (struct ether_header),                  /* указатель на зашифровываемые данные */
                sendbuf + sizeof( mka_tr_open_msg ) + sizeof (struct ether_header),   /* указатель на область памяти,
                         в которую помещаются зашифрованные данные */
                sizeof( mka_tr_encr_msg ) + smallest_part,              /* размер зашифровываемых данных */
                iv128,             /* синхропосылка (инициализационный вектор) */
                sizeof( iv128 ),                       /* размер синхропосылки */
                /* указатель на область памяти,
             в которую помещается имитовставка */
                sendbuf + sizeof (struct ether_header) + sizeof( mka_tr_open_msg ) + sizeof( mka_tr_encr_msg ) + smallest_part,
                16                                      /* размер имитовставки */
        );
    else if(msg_type == 1) {
        ak_bckey_encrypt_mgm(
                &mka_i->mkaKeyStr.peer_keys_list[num].KEK,              /* ключ, используемый для шифрования данных */
                &mka_i->mkaKeyStr.peer_keys_list[num].KEK,             /* ключ, используемый для имитозащиты данных */
                sendbuf,                  /* указатель на ассоциированные данные */
                sizeof(mka_tr_open_msg) + sizeof (struct ether_header),          /* длина ассоциированных данных */
                sendbuf + sizeof( mka_tr_open_msg ) + sizeof (struct ether_header),                  /* указатель на зашифровываемые данные */
                sendbuf + sizeof( mka_tr_open_msg ) + sizeof (struct ether_header),   /* указатель на область памяти,
                         в которую помещаются зашифрованные данные */
                sizeof( mka_tr_encr_msg ) + smallest_part,              /* размер зашифровываемых данных */
                iv128,             /* синхропосылка (инициализационный вектор) */
                sizeof( iv128 ),                       /* размер синхропосылки */
                /* указатель на область памяти,
             в которую помещается имитовставка */
                sendbuf + sizeof (struct ether_header) + sizeof( mka_tr_open_msg ) + sizeof( mka_tr_encr_msg ) + smallest_part,
                16                                      /* размер имитовставки */
        );
    }
    sendbuf[sizeof(struct ether_header) + sizeof(mka_tr_encr_msg) + sizeof(mka_tr_open_msg) + smallest_part + 16] = '\0';
    mka_cp_send_data(connectionParams, sendbuf + sizeof(struct ether_header),
                     sizeof(mka_tr_encr_msg) + sizeof(mka_tr_open_msg) + smallest_part + 16, id_recv);
    if(sendbuf != NULL) free(sendbuf);
    return 0;
}

char *mka_tr_recv_data(connection_params *connectionParams, struct ether_header *eh, mka_tr_encr_msg *mka_t, mka_tr_open_msg *mka_t_o, int *data_len){
    char *buffer;
    int len;
    if((buffer = mka_cp_recieve_data(connectionParams, &len)) == NULL) return NULL;
    memcpy(eh, buffer, sizeof(struct ether_header));
    memcpy(mka_t_o, buffer + sizeof(struct ether_header), sizeof(struct mka_tr_open_msg));
    memcpy(mka_t, buffer + sizeof(struct ether_header)+sizeof(struct mka_tr_open_msg), sizeof(struct mka_tr_encr_msg));
    *data_len = len - sizeof(struct ether_header) - sizeof(struct mka_tr_encr_msg) - sizeof(struct mka_tr_open_msg);
    memmove(buffer, buffer + sizeof(struct ether_header) + sizeof(struct mka_tr_encr_msg) + sizeof(struct mka_tr_open_msg), *data_len);
    buffer = (char *)realloc(buffer, *data_len * sizeof(char));
    return buffer;
}

char *mka_tr_recv_data_sec(connection_params *connectionParams, struct ether_header *eh, mka_tr_encr_msg *mka_t, mka_tr_open_msg *mka_t_o, int *data_len, int timeout){
    char *buffer;
    int len;
    if((buffer = mka_cp_recieve_data_sec(connectionParams, &len, timeout)) == NULL) return NULL;
    memcpy(eh, buffer, sizeof(struct ether_header));
    memcpy(mka_t_o, buffer + sizeof(struct ether_header), sizeof(struct mka_tr_open_msg));
    memcpy(mka_t, buffer + sizeof(struct ether_header)+sizeof(struct mka_tr_open_msg), sizeof(struct mka_tr_encr_msg));
    *data_len = len - sizeof(struct ether_header) - sizeof(struct mka_tr_encr_msg) - sizeof(struct mka_tr_open_msg);
    memmove(buffer, buffer + sizeof(struct ether_header) + sizeof(struct mka_tr_encr_msg) + sizeof(struct mka_tr_open_msg), *data_len);
    buffer = (char *)realloc(buffer, *data_len * sizeof(char));
    return buffer;
}


char *mka_tr_recv_defragment_data(connection_params *connectionParams, mka_info *mka_i, struct ether_header *eh, mka_tr_encr_msg *mka_t, mka_tr_open_msg *mka_t_o, int *data_len){
    char *buffer;
    char *defragment_data = (char *)calloc(1, sizeof(char));
    *data_len = 0;
    int len, result, icv_len = 0, recv_data_len = 0, frag_num = 1, frag_id = 0;
    int num, id;

    while (frag_id < frag_num) {
        buffer = mka_cp_recieve_data_sec(connectionParams, &len, 3);
        if(buffer == NULL) {
//            printf("Error, could not recv\n");
            free(defragment_data);
            return NULL;
        }
        memcpy(eh, buffer, sizeof(struct ether_header));
        memcpy(mka_t_o, buffer + sizeof(struct ether_header), sizeof(struct mka_tr_open_msg));
        num = mka_key_str_get_peer_key_num_by_peer_id(&mka_i->mkaKeyStr, mka_t_o->id_src);
        if(frag_id == 0 && mka_t_o->id_src != 0 && num == -1){
//            printf("Peer id to add: %d\n", mka_t_o->id_src);
            peer p_recv;
            mka_pls_peer_init(&p_recv, mka_t_o->id_src, 0);
            mka_pls_peer_add(&mka_i->peer_l, &p_recv);
            mka_cp_add_potential_connection(connectionParams, mka_t_o->id_src,
                                            eh->ether_shost);
            mka_key_str_add_peer_keys(&mka_i->mkaKeyStr);
            mka_key_str_set_peer_id(&mka_i->mkaKeyStr, mka_t_o->id_src, mka_i->mkaKeyStr.peer_keys_list_capacity - 1);
            mka_key_str_gen_CAK(&mka_i->mkaKeyStr, mka_t_o->id_src);
            mka_key_str_gen_KEK(&mka_i->mkaKeyStr, mka_t_o->id_src);
            num = mka_i->mkaKeyStr.peer_keys_list_capacity - 1;
        }
//        printf("NUM Keys: %d\n", num);
//        printf("Recv %d interface from %d peer\n", connectionParams->interface_num, mka_t_o->id_src);
        if (mka_t_o->encr == 1 && num != -1) {
            if (mka_t_o->type == 0) {
                result = ak_bckey_decrypt_mgm(
                        &mka_i->mkaKeyStr.peer_keys_list[num].CAK,              /* ключ, используемый для шифрования данных */
                        &mka_i->mkaKeyStr.peer_keys_list[num].CAK,             /* ключ, используемый для имитозащиты данных */
                        buffer,                  /* указатель на ассоциированные данные */
                        sizeof(mka_tr_open_msg) +
                        sizeof(struct ether_header),          /* длина ассоциированных данных */
                        buffer + sizeof(mka_tr_open_msg) +
                        sizeof(struct ether_header),                  /* указатель на зашифровываемые данные */
                        buffer + sizeof(mka_tr_open_msg) + sizeof(struct ether_header),   /* указатель на область памяти,
                         в которую помещаются зашифрованные данные */
                        len - sizeof(mka_tr_open_msg) - sizeof(struct ether_header) -
                        16,              /* размер зашифровываемых данных */
                        iv128,             /* синхропосылка (инициализационный вектор) */
                        sizeof(iv128),                       /* размер синхропосылки */
                        /* указатель на область памяти,
                     в которую помещается имитовставка */
                        buffer + len - 16,
                        16                                      /* размер имитовставки */
                );
            } else {
                result = ak_bckey_decrypt_mgm(
                        &mka_i->mkaKeyStr.peer_keys_list[num].KEK,              /* ключ, используемый для шифрования данных */
                        &mka_i->mkaKeyStr.peer_keys_list[num].KEK,             /* ключ, используемый для имитозащиты данных */
                        buffer,                  /* указатель на ассоциированные данные */
                        sizeof(mka_tr_open_msg) +
                        sizeof(struct ether_header),          /* длина ассоциированных данных */
                        buffer + sizeof(mka_tr_open_msg) +
                        sizeof(struct ether_header),                  /* указатель на зашифровываемые данные */
                        buffer + sizeof(mka_tr_open_msg) + sizeof(struct ether_header),   /* указатель на область памяти,
                         в которую помещаются зашифрованные данные */
                        len - sizeof(mka_tr_open_msg) - sizeof(struct ether_header) - 16,              /* размер зашифровываемых данных */
                        iv128,             /* синхропосылка (инициализационный вектор) */
                        sizeof(iv128),                       /* размер синхропосылки */
                        /* указатель на область памяти,
                     в которую помещается имитовставка */
                        buffer + len - 16,
                        16                                      /* размер имитовставки */
                );
            }
            icv_len = 16;
            if(result != ak_error_ok){
                printf("Decrypt error\n");
                free(buffer);
                free(defragment_data);
                return NULL;
            }
        } else if(num == -1 && mka_t_o->encr == 1){
            free(buffer);
            free(defragment_data);
            return NULL;
        }
        recv_data_len = len - sizeof(struct ether_header) - sizeof(struct mka_tr_encr_msg) -
                        sizeof(struct mka_tr_open_msg) - icv_len;
        memcpy(mka_t, buffer + sizeof(struct ether_header) + sizeof(struct mka_tr_open_msg),
               sizeof(struct mka_tr_encr_msg));
        if(frag_id != mka_t->fragment_id) {
            free(buffer);
            free(defragment_data);
            return NULL;
        }
//        mka_tr_print_frame(buffer, len);
        defragment_data = (char *) realloc(defragment_data, sizeof(char) * (*data_len + recv_data_len));
        memcpy(defragment_data + *data_len, buffer + sizeof(struct ether_header) + sizeof(struct mka_tr_encr_msg) +
                                            sizeof(struct mka_tr_open_msg), recv_data_len);
        *data_len += recv_data_len;
        if(frag_id == 0) frag_num = mka_t->num_of_fragments;
        ++frag_id;
    }
    free(buffer);
    buffer = NULL;
    return defragment_data;
}

void mka_tr_print_frame(char *data, int data_len){
    struct ether_header *eh = (struct ether_header *) data;
    mka_tr_open_msg *mka_t_o = (mka_tr_open_msg *) (data + sizeof(struct ether_header));
    mka_tr_encr_msg *mka_t = (mka_tr_encr_msg *) (data + sizeof(struct ether_header) + sizeof(mka_tr_open_msg));
    int recv_buf_len = data_len - sizeof(struct ether_header) - sizeof(mka_tr_open_msg) - sizeof(mka_tr_encr_msg) - 16;
    char *p;

    printf("------------------------------------------------------\n");

    fprintf(stdout, "mac: %02x:%02x:%02x:%02x:%02x:%02x -> %02x:%02x:%02x:%02x:%02x:%02x\nproto: %04x \n",
            eh->ether_shost[0],
            eh->ether_shost[1],
            eh->ether_shost[2],
            eh->ether_shost[3],
            eh->ether_shost[4],
            eh->ether_shost[5],
            eh->ether_dhost[0],
            eh->ether_dhost[1],
            eh->ether_dhost[2],
            eh->ether_dhost[3],
            eh->ether_dhost[4],
            eh->ether_dhost[5],
            htons(eh->ether_type));

    fprintf(stdout, "type: %d\nid_src: %d\nnum of fragments: %d\nfragment id_src: %d\nKS priority: %d\ndata: ",
            mka_t_o->type, mka_t_o->id_src, mka_t->num_of_fragments, mka_t->fragment_id, mka_t->KS_priority);
    p = data + sizeof(struct ether_header) + sizeof(mka_tr_open_msg) + sizeof(mka_tr_encr_msg);

    for (int i = 0; i < recv_buf_len; i++)
        printf("%x ", p[i] & 0xff);

    fputc('\n', stdout);


    printf("------------------------------------------------------\n");
}

void mka_tr_print_frame_without_ether(char *data, int recieved_buf_len){
    mka_tr_open_msg *mka_t_o = (mka_tr_open_msg *) (data);
    mka_tr_encr_msg *mka_t = (mka_tr_encr_msg *) (data + sizeof(mka_tr_open_msg));
    int recv_buf_len = recieved_buf_len - sizeof(mka_tr_open_msg) - sizeof(mka_tr_encr_msg);
    char *p;

    printf("------------------------------------------------------\n");

    fprintf(stdout, "type: %d\nid_src: %d\nnum of fragments: %d\nfragment id_src: %d\nKS priority: %d\ndata: ",
            mka_t_o->type, mka_t_o->id_src, mka_t->num_of_fragments, mka_t->fragment_id, mka_t->KS_priority);
    p = data + sizeof(mka_tr_open_msg) + sizeof(mka_tr_encr_msg);

    for (int i = 0; i < recv_buf_len; i++)
        printf("%x ", p[i] & 0xff);

    fputc('\n', stdout);

    printf("------------------------------------------------------\n");
}