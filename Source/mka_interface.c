#include "../Headers/mka_interface.h"

int start_mka_connection(mka_common_args *commonArgs){
    for(int i = 0; i < commonArgs->connectionParamsList.num_of_c_p; ++i) {
        if (pthread_create(&commonArgs->connectionParamsList.c_p_list[i].tid_recv, NULL, recv_mka_msg,
                           (void *) commonArgs) != 0) {
            printf("main error: can't create recv thread, i = %d\n", i);
            return 1;
        }
    }
    if (pthread_create(&commonArgs->tid_send, NULL, mka_htmsg_send_thread_func,
                       (void *) commonArgs) != 0) {
        printf("main error: can't create send thread\n");
        return 1;
    }
    if (pthread_create(&commonArgs->tid_cleaner, NULL, mka_pls_cleaner_thread_func, (void *) commonArgs) != 0) {
        printf("main error: can't create cleaner thread\n");
        return 1;
    }
    if (pthread_create(&commonArgs->tid_ks_server, NULL, mka_ksmsg_send_thread_func, (void *) commonArgs) != 0) {
        printf("main error: can't create ks thread\n");
        return 1;
    }
    return 0;
}

int stop_mka_connection(mka_common_args *commonArgs){
    if(mka_common_args_destruct(commonArgs) == 1) return 1;
    return 0;
}


int mka_add_interface(mka_common_args *commonArgs, char *interface, int interface_len){
    unsigned char temp_dest_adr[] = {0xff,0xff,0xff,0xff,0xff,0xff};
    connection_params c_p;
    if(mka_cp_init(&c_p, interface, interface_len, temp_dest_adr) != 0) return 1;
    mka_cpl_add_connection_param_in_list(&commonArgs->connectionParamsList, &c_p);
    return 0;
}

int get_interface_num_to_listen(connection_params_list *connectionParamsList, pthread_t t){
    int i = 0;
    while(i < connectionParamsList->num_of_c_p && connectionParamsList->c_p_list[i].tid_recv != t) ++i;
    return i;
}

struct sended_live_peers_char{
    int id;
    char *sended_buffer;
    int sended_buffer_len;
};

struct sended_live_peers_char *sended_live_peers_char_rm_by_id(struct sended_live_peers_char *sendedLivePeersCharArrays, int *slpca_capacity, int i){
    // удаление проверочной строки
    if (*slpca_capacity > i) {
        free(sendedLivePeersCharArrays[i].sended_buffer); // error
        --*slpca_capacity;
        if (*slpca_capacity >= i) {
            if(*slpca_capacity > i){
                memcpy(&sendedLivePeersCharArrays[i], &sendedLivePeersCharArrays[i + 1], sizeof(connection) * (*slpca_capacity - i));
            }
            return (struct sended_live_peers_char *) realloc(sendedLivePeersCharArrays, sizeof(struct sended_live_peers_char) * (*slpca_capacity));
        } else if(*slpca_capacity == 0) {
            free(sendedLivePeersCharArrays);
            return (struct sended_live_peers_char*)calloc(0,sizeof(struct sended_live_peers_char));
        }
    }
}

void *recv_mka_msg(void *args){
    struct sended_live_peers_char *sendedLivePeersCharArrays = (struct sended_live_peers_char *) calloc(0, sizeof(struct sended_live_peers_char));
    mka_common_args *commonArgs = (mka_common_args*) args;
    int slpca_capacity = 0;
    pthread_t self = pthread_self();
    struct ether_header eh;
    char *recv_buffer = NULL, *buffer = NULL;
    int buffer_len, recv_buffer_len = 0;
    time_t start_period = time(NULL);
    int period_length_sec = 2, pause_listening_sec = 15, max_num_new_connections_period = 30, num_new_connections_period = 0;
    mka_tr_encr_msg mka_t;
    mka_tr_open_msg mka_t_o;
    peer p_recv;
    mka_htmsg helloTimeMsg;
    mka_ksmsg mkaKsMsg;
    int id, num_incorrect_answers = 0;

    int interface_num = get_interface_num_to_listen(&commonArgs->connectionParamsList, self);

    while (!commonArgs->stop) {
        while((buffer == NULL || mka_t_o.id_src == 0) && commonArgs->stop == 0)
            buffer = mka_tr_recv_defragment_data(&commonArgs->connectionParamsList.c_p_list[interface_num],
                                                 &commonArgs->mka_i, &eh, &mka_t, &mka_t_o, &buffer_len);
        if(num_new_connections_period > max_num_new_connections_period){
            time_t start = time(NULL);
            printf("Sorry for blocking interface for %d sec\n", pause_listening_sec);
            while(time(NULL) - start < pause_listening_sec)
                continue;
            start_period = time(NULL);
            num_new_connections_period = 0;
            continue;
        }
        if(commonArgs->stop == 1) break;
        if(time(NULL) - start_period > period_length_sec) {
            start_period = time(NULL);
            num_new_connections_period = 0;
        }

        if(mka_t_o.type == 1) {
            printf("Received KS\n");
            fflush(stdout);
            memcpy(&mkaKsMsg, buffer, sizeof(mka_ksmsg));
            recv_buffer_len = buffer_len - sizeof(mka_ksmsg);
            if(commonArgs->need_to_refresh_SAK == 1 && commonArgs->mka_i.isKS == 0 && mkaKsMsg.reply_flg == 0) {
                mka_key_str_set_SAK(&commonArgs->mka_i.mkaKeyStr, buffer + sizeof(mka_ksmsg), recv_buffer_len);
                printf("SAK: ");
                commonArgs->mka_i.mkaKeyStr.SAK.key.unmask( &commonArgs->mka_i.mkaKeyStr.SAK.key ); /* снимаем маску */
                print_string( commonArgs->mka_i.mkaKeyStr.SAK.key.key,commonArgs->mka_i.mkaKeyStr.SAK.key.key_size);
                commonArgs->mka_i.mkaKeyStr.SAK.key.set_mask( &commonArgs->mka_i.mkaKeyStr.SAK.key );
                commonArgs->need_to_refresh_SAK = 0;
                commonArgs->need_to_send_SAK_to_others__received_t = interface_num;
                commonArgs->need_to_send_SAK_to_others__peer_id = mka_t_o.id_src;
//                printf("Set KS\n");
                fflush(stdout);
            }
            if(commonArgs->need_to_refresh_SAK == 0  && mkaKsMsg.reply_flg == 1 && mka_pls_peer_get_status_by_peer_id(&commonArgs->mka_i.peer_l, mka_t_o.id_src) == 1){
                commonArgs->reply_SAK__received_t = interface_num;
                commonArgs->reply_SAK__peer_id = mka_t_o.id_src;
            }
            memset(&mka_t, 0, sizeof(mka_tr_encr_msg));
            memset(&mka_t_o, 0, sizeof(mka_tr_open_msg));
            memset(&mkaKsMsg, 0, sizeof(mka_ksmsg));
            free(buffer);
            buffer = NULL;
            buffer_len = 0;
            continue;
        } else if (mka_t_o.type == 0) {
//            printf("Received HT from %d\n", mka_t_o.id_src);
            fflush(stdout);
            memcpy(&helloTimeMsg, buffer, sizeof(mka_htmsg));
            recv_buffer_len = helloTimeMsg.other_len + helloTimeMsg.live_peer_str_len + helloTimeMsg.potential_peer_str_len;
            recv_buffer = (char *) calloc(recv_buffer_len, sizeof(char));
            memcpy(recv_buffer, buffer + sizeof(mka_htmsg), recv_buffer_len); // recv_buff incorrect
//            {
//                printf("HT.pp = %d\n", helloTimeMsg.potential_peer);
//            }
        }
        free(buffer);
        buffer = NULL;
        buffer_len = 0;
        // -1 если такого id нет -> процедура аутентификации (неизвестный -> потенциальный)
        id = mka_pls_peer_get_id_by_peer_id(&commonArgs->mka_i.peer_l, mka_t_o.id_src);
//        printf("id = %d\n", id);
//        printf("status = %d\n", commonArgs->mka_i.peer_l.peer_l[id].status);
        // процедура неизвестный -> потенциальный
        int i = slpca_capacity;
        for(int j = 0; j < slpca_capacity; ++j){
            int id2 = mka_pls_peer_get_id_by_peer_id(&commonArgs->mka_i.peer_l, sendedLivePeersCharArrays[j].id);
            if(id2 == -1) {
                sendedLivePeersCharArrays = sended_live_peers_char_rm_by_id(sendedLivePeersCharArrays, &slpca_capacity, j);
                --j;
            }
            if(j!= -1 && mka_t_o.id_src == sendedLivePeersCharArrays[j].id) i = j;
        }
        if(helloTimeMsg.potential_peer == -1 && i == slpca_capacity && num_incorrect_answers != 9 && commonArgs->mka_i.peer_l.peer_l[id].status == 0) {
            // отправляем для аутентификации абонента
            printf("Sended rnd str, potential_p = -1\n");
            ++num_new_connections_period;
            buffer = mka_pls_get_live_peers_with_peer_id_pr_t(&commonArgs->mka_i.peer_l, &buffer_len);
            int rand = 0;
            if (buffer_len == 0) {
                buffer_len = 256;
                buffer = generate_rnd_string(buffer_len);
                rand = 1;
            }
            commonArgs->connectionParamsList.c_p_list[interface_num].potential_connection_list[mka_cp_get_potential_connection_num_by_peer_id(&commonArgs->connectionParamsList.c_p_list[interface_num], mka_t_o.id_src)].HT_pause = 1;
            mka_htmsg_send(commonArgs, interface_num, mka_t_o.id_src, 1, mka_t_o.id_src, rand, buffer, buffer_len, NULL, 0,
                           NULL, 0);
            sendedLivePeersCharArrays = (struct sended_live_peers_char*) realloc (sendedLivePeersCharArrays, (++slpca_capacity) * sizeof(struct sended_live_peers_char));
            sendedLivePeersCharArrays[slpca_capacity - 1].id = mka_t_o.id_src;
            sendedLivePeersCharArrays[slpca_capacity - 1].sended_buffer = (char *) calloc(buffer_len, sizeof(char));
            memcpy(sendedLivePeersCharArrays[slpca_capacity - 1].sended_buffer, buffer, buffer_len);
            sendedLivePeersCharArrays[slpca_capacity - 1].sended_buffer_len = buffer_len;
            num_incorrect_answers = 0;
            free(buffer);
            buffer = NULL;
            buffer_len = 0;
        } else if(     commonArgs->mka_i.peer_l.peer_l[id].status   == 0
                    && helloTimeMsg.potential_peer                  == commonArgs->mka_i.id
                    && helloTimeMsg.live_peer_str_len               != 0
                    && i                                            != slpca_capacity
                    && memcmp(recv_buffer + helloTimeMsg.live_peer_str_len, sendedLivePeersCharArrays[i].sended_buffer, helloTimeMsg.potential_peer_str_len) == 0){
            printf("Sended rnd str or alive_peers and recv_buffer, potential_p = i\n");
            // отправляем ответочку на его аутентификацию
            mka_pls_peer_upd_status_by_id(&commonArgs->mka_i.peer_l, id, 1);
            mka_cp_make_potential_connection_alive_by_peer_id(&commonArgs->connectionParamsList.c_p_list[interface_num], mka_t_o.id_src);
            printf("Peer %d status was updated\n", mka_t_o.id_src);
            if(helloTimeMsg.random_live_peer_str == 0)
                mka_pls_upd_peer_list_by_peers_id_pr_t(recv_buffer, helloTimeMsg.live_peer_str_len,
                                                   &commonArgs->mka_i.peer_l, commonArgs->mka_i.id);
            buffer = mka_pls_get_live_peers_with_peer_id_pr_t(&commonArgs->mka_i.peer_l, &buffer_len);
            int rand = 0;
            if (buffer_len == 0) {
                buffer_len = 256;
                buffer = generate_rnd_string(buffer_len);
                rand = 1;
            }
            mka_htmsg_send(commonArgs, interface_num, mka_t_o.id_src, 1, 0, rand, buffer, buffer_len, recv_buffer,
                           recv_buffer_len, NULL, 0);
            commonArgs->mka_i.peer_l.peer_l[id].p.KS_priority = mka_t.KS_priority;
            commonArgs->mka_i.KS_priority = mka_pls_get_highest_ks_priority(&commonArgs->mka_i.peer_l,
                                                                            &commonArgs->mka_i.isKS,
                                                                            commonArgs->mka_i.my_KS_priority,
                                                                            commonArgs->mka_i.id);
            commonArgs->need_to_send_HTM = 1;
            commonArgs->need_to_refresh_SAK = 1;
            if(buffer_len != 0 && buffer != NULL) free(buffer);
            buffer = NULL;
            buffer_len = 0;
        } else if(helloTimeMsg.potential_peer == 0 && id != -1) {
//            printf("id: %d\n", id);
//            printf("capacity: %d\n", commonArgs->mka_i.peer_l.capacity);
//            printf("Time old: %d\n", commonArgs->mka_i.peer_l.peer_l[id].time);
            mka_pls_peer_upd_time_by_id(&commonArgs->mka_i.peer_l, id, time(NULL) % 3600);
//            printf("Time new: %d\n", commonArgs->mka_i.peer_l.peer_l[id].time);
            commonArgs->mka_i.peer_l.peer_l[id].p.KS_priority = mka_t.KS_priority;
            if (commonArgs->mka_i.peer_l.peer_l[id].status == 1){
                if(mka_pls_upd_peer_list_by_peers_id_pr_t(recv_buffer, helloTimeMsg.live_peer_str_len,
                                                          &commonArgs->mka_i.peer_l, commonArgs->mka_i.id) == 1){
                    commonArgs->mka_i.KS_priority = mka_pls_get_highest_ks_priority(&commonArgs->mka_i.peer_l,
                                                                                    &commonArgs->mka_i.isKS,
                                                                                    commonArgs->mka_i.my_KS_priority,
                                                                                    commonArgs->mka_i.id);
                    commonArgs->need_to_send_HTM = 1;
                    commonArgs->need_to_refresh_SAK = 1;
//                    printf("Recv alive peer HT\n");
                }
            } else if (commonArgs->mka_i.peer_l.peer_l[id].status == 0 && helloTimeMsg.potential_peer_str_len != 0  && i!=slpca_capacity &&  memcmp(recv_buffer + helloTimeMsg.live_peer_str_len, sendedLivePeersCharArrays[i].sended_buffer, helloTimeMsg.potential_peer_str_len) == 0) {
                // процедура потенциальный -> живой
                // он должен послать весь список живых (моих) в потенциальных, как подтверждение
                mka_pls_peer_upd_status_by_id(&commonArgs->mka_i.peer_l, id, 1);
                mka_cp_make_potential_connection_alive_by_peer_id(&commonArgs->connectionParamsList.c_p_list[interface_num], mka_t_o.id_src);
                commonArgs->mka_i.peer_l.peer_l[id].p.KS_priority = mka_t.KS_priority;
                printf("Peer %d status was updated\n", mka_t_o.id_src);
                fflush(stdout);
                if(helloTimeMsg.random_live_peer_str == 0)
                    mka_pls_upd_peer_list_by_peers_id_pr_t(recv_buffer, helloTimeMsg.live_peer_str_len,
                                                           &commonArgs->mka_i.peer_l, commonArgs->mka_i.id);
                commonArgs->need_to_send_HTM = 1;
                commonArgs->mka_i.KS_priority = mka_pls_get_highest_ks_priority(&commonArgs->mka_i.peer_l,
                                                                                &commonArgs->mka_i.isKS,
                                                                                commonArgs->mka_i.my_KS_priority,
                                                                                commonArgs->mka_i.id);
                sendedLivePeersCharArrays = sended_live_peers_char_rm_by_id(sendedLivePeersCharArrays, &slpca_capacity, i);
                commonArgs->need_to_refresh_SAK = 1;
                num_incorrect_answers = 0;
            } else {
                ++num_incorrect_answers;
                if(num_incorrect_answers == 3 || num_incorrect_answers == 6) {
                    mka_htmsg_send(commonArgs, interface_num, mka_t_o.id_src, 1, mka_t_o.id_src, 0,
                                   sendedLivePeersCharArrays[i].sended_buffer,
                                   sendedLivePeersCharArrays[i].sended_buffer_len,
                                   recv_buffer, recv_buffer_len, NULL, 0);
                } else if(num_incorrect_answers == 9){
                    sendedLivePeersCharArrays = sended_live_peers_char_rm_by_id(sendedLivePeersCharArrays, &slpca_capacity, i);
                }
            }
        } else if(commonArgs->mka_i.peer_l.peer_l[id].status   == 0
                  && helloTimeMsg.potential_peer                  == commonArgs->mka_i.id) {
            ++num_incorrect_answers;
            if (num_incorrect_answers == 3 || num_incorrect_answers == 6) {
                mka_htmsg_send(commonArgs, interface_num, mka_t_o.id_src, 1, mka_t_o.id_src, 0,
                               sendedLivePeersCharArrays[i].sended_buffer,
                               sendedLivePeersCharArrays[i].sended_buffer_len,
                               recv_buffer, recv_buffer_len, NULL, 0);
            } else if (num_incorrect_answers == 9) {
                sendedLivePeersCharArrays = sended_live_peers_char_rm_by_id(sendedLivePeersCharArrays, &slpca_capacity,
                                                                            i);
            }
        }
        if(recv_buffer_len != 0 && recv_buffer != NULL) free(recv_buffer);
        recv_buffer = NULL;
        recv_buffer_len = 0;
        memset(&mka_t, 0, sizeof(mka_tr_encr_msg));
        memset(&mka_t_o, 0, sizeof(mka_tr_open_msg));
    }
    for(int i = 0; i < slpca_capacity; ++i) {
        free(sendedLivePeersCharArrays[i].sended_buffer);
    }
    if(slpca_capacity != 0) free(sendedLivePeersCharArrays);
    if(recv_buffer != NULL && recv_buffer_len != 0) free(recv_buffer);
    if(buffer != NULL && buffer_len != 0) free(buffer);
}

