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
    mka_key_str_add_CAK(&commonArgs->mka_i.mkaKeyStr);
    return 0;
}

int get_interface_num_to_listen(connection_params_list *connectionParamsList, pthread_t t){
    mka_cpl_freeze(connectionParamsList, 1);
    int i = 0;
    while(i < connectionParamsList->num_of_c_p && connectionParamsList->c_p_list[i].tid_recv != t) ++i;
    mka_cpl_unfreeze(connectionParamsList, 1);
    return i;
}

void *recv_mka_msg(void *args){
    struct sended_live_peers_char{
        int id;
        char *sended_buffer;
        int sended_buffer_len;
    } sendedLivePeersCharArrays;
    mka_common_args *commonArgs = (mka_common_args*) args;
    pthread_t self = pthread_self();
    struct ether_header eh;
    char *recv_buffer = NULL, *buffer = NULL;
    int buffer_len, recv_buffer_len = 0;
    mka_tr_encr_msg mka_t;
    mka_tr_open_msg mka_t_o;
    peer *p, p_recv;
    mka_htmsg helloTimeMsg;
    mka_ksmsg mkaKsMsg;
    int id, num_incorrect_answers;

    int interface_num = get_interface_num_to_listen(&commonArgs->connectionParamsList, self);

    while (!commonArgs->stop) {
        while((buffer == NULL || mka_t_o.id_src == 0) && commonArgs->stop == 0)
//            buffer = recv_mka_hello_time_data__live_peer_other(commonArgs, &eh, &mka_t, &mka_t_o, &helloTimeMsg, &len_recv);
            buffer = mka_tr_recv_defragment_data(&commonArgs->connectionParamsList.c_p_list[interface_num],
                                                 &commonArgs->mka_i, &eh, &mka_t, &mka_t_o, &buffer_len);
        if(commonArgs->stop == 1) break;

        if(mka_t_o.type == 1) {
            // todo: а если не получает KS, то что?
            printf("Received KS\n");
            fflush(stdout);
            memcpy(&mkaKsMsg, buffer, sizeof(mka_ksmsg));
            recv_buffer_len = buffer_len - sizeof(mka_ksmsg);
            {
                fprintf(stdout,
                        "HT msg:\nmsg_num: %d\npotential_peer: %d\nlive_peer_str_len: %d\npotential_peer_str_len: %d\nother_len: %d\ndata: ",
                        helloTimeMsg.msg_num,
                        helloTimeMsg.potential_peer,
                        helloTimeMsg.live_peer_str_len,
                        helloTimeMsg.potential_peer_str_len,
                        helloTimeMsg.other_len);
                char *p = buffer + sizeof(mka_ksmsg);

                for (int i = 0; i < recv_buffer_len; i++) {
                    printf("%x ", p[i] & 0xff);
                }
                fputc('\n', stdout);
                printf("------------------------------------------------------\n");
            }
            if(commonArgs->need_to_refresh_SAK == 1 && commonArgs->mka_i.isKS == 0) {
                memcpy(&mkaKsMsg, buffer, sizeof(mka_ksmsg));
                // todo: может добавить еще длину ключа, иначе будет выход за границу
                mka_key_str_set_SAK(&commonArgs->mka_i.mkaKeyStr, buffer + sizeof(mka_ksmsg),
                                    buffer_len - sizeof(mka_ksmsg));
                printf("SAK: ");
                commonArgs->mka_i.mkaKeyStr.SAK.key.unmask( &commonArgs->mka_i.mkaKeyStr.SAK.key ); /* снимаем маску */
                print_string( commonArgs->mka_i.mkaKeyStr.SAK.key.key,commonArgs->mka_i.mkaKeyStr.SAK.key.key_size);
                commonArgs->mka_i.mkaKeyStr.SAK.key.set_mask( &commonArgs->mka_i.mkaKeyStr.SAK.key );
                commonArgs->need_to_refresh_SAK = 0;
                commonArgs->need_to_send_SAK_to_others__received_t = interface_num;
                commonArgs->connectionParamsList.c_p_list[interface_num].pause_HT = 0;
                printf("Set KS\n");
                fflush(stdout);
            }
            memset(&mka_t, 0, sizeof(mka_tr_encr_msg));
            memset(&mka_t_o, 0, sizeof(mka_tr_open_msg));
            free(buffer);
            buffer = NULL;
            buffer_len = 0;
            continue;
        } else if (mka_t_o.type == 0) {
//            printf("Received HT from %d\n", mka_t_o.id_src);
            fflush(stdout);
            memcpy(&helloTimeMsg, buffer, sizeof(mka_htmsg));
            recv_buffer_len = helloTimeMsg.other_len + helloTimeMsg.live_peer_str_len + helloTimeMsg.potential_peer_str_len;
            {
                fprintf(stdout,
                        "HT msg:\nmsg_num: %d\npotential_peer: %d\nlive_peer_str_len: %d\npotential_peer_str_len: %d\nother_len: %d\ndata: ",
                        helloTimeMsg.msg_num,
                        helloTimeMsg.potential_peer,
                        helloTimeMsg.live_peer_str_len,
                        helloTimeMsg.potential_peer_str_len,
                        helloTimeMsg.other_len);
                char *p = buffer + sizeof(mka_htmsg);

                for (int i = 0; i < recv_buffer_len; i++) {
                    printf("%x ", p[i] & 0xff);
                }
                fputc('\n', stdout);
                printf("------------------------------------------------------\n");
            }
            recv_buffer = (char *) calloc(recv_buffer_len, sizeof(char));
            memcpy(recv_buffer, buffer + sizeof(mka_htmsg), recv_buffer_len); // recv_buff incorrect
        }
        free(buffer);
        buffer = NULL;
        buffer_len = 0;
        // -1 если такого id нет -> процедура аутентификации (неизвестный -> потенциальный)
        id = mka_pls_peer_get_id_by_peer_id(&commonArgs->mka_i.peer_l, mka_t_o.id_src);
        if(id == -1 || num_incorrect_answers == 9) {
            // процедура неизвестный -> потенциальный
            if(id == -1){
                printf("Peer id to add: %d\n", mka_t_o.id_src);
                memcpy(commonArgs->connectionParamsList.c_p_list[interface_num].dest_mac, eh.ether_shost, 6);
                commonArgs->connectionParamsList.c_p_list[interface_num].pause_HT = 1;
                mka_pls_peer_init(&p_recv, mka_t_o.id_src, mka_t.KS_priority);
                mka_pls_peer_add(&commonArgs->mka_i.peer_l, &p_recv);
                mka_cpl_upd_peer_id_by_interface_id(&commonArgs->connectionParamsList, interface_num,
                                                    mka_t_o.id_src);
                mka_key_str_gen_CAK(&commonArgs->mka_i.mkaKeyStr, commonArgs->mka_i.id, mka_t_o.id_rcv, interface_num);
            }
            if(helloTimeMsg.potential_peer == -1) {
                // отправляем для аутентификации абонента
                printf("Sended rnd str, potential_p = -1\n");
                buffer = generate_rnd_string(10);
                buffer_len = 10;
                mka_htmsg_send(commonArgs, interface_num, 0, mka_t_o.id_src, 1, buffer, buffer_len, NULL, 0,
                               NULL, 0);
            } else {
                printf("Sended rnd str or alive_peers and recv_buffer, potential_p = i\n");
                // отправляем ответочку на его аутентификацию
                buffer = mka_pls_get_live_peers_with_peer_id_pr_t(&commonArgs->mka_i.peer_l, &buffer_len);
                int rand = 0;
                if (buffer_len == 0) {
                    buffer_len = 10;
                    buffer = generate_rnd_string(10);
                    rand = 1;
                }
                mka_htmsg_send(commonArgs, interface_num, 1, mka_t_o.id_src, rand, buffer, buffer_len,
                               recv_buffer,
                               recv_buffer_len, NULL, 0);
            }
//            printf("Set auth buffer\n");
            sendedLivePeersCharArrays.sended_buffer = (char *) calloc(buffer_len, sizeof(char));
            memcpy(sendedLivePeersCharArrays.sended_buffer, buffer, buffer_len);
            sendedLivePeersCharArrays.sended_buffer_len = buffer_len;
            sendedLivePeersCharArrays.id = mka_t_o.id_src;
            num_incorrect_answers = 0;
            free(buffer);
            buffer = NULL;
            buffer_len = 0;
            // todo: send with msg num of recv
        } else {
            mka_pls_peer_upd_time_by_id(&commonArgs->mka_i.peer_l, id, time(NULL) % 3600);
            if (commonArgs->mka_i.peer_l.peer_l[id].status == 1 && helloTimeMsg.potential_peer == 0){
                if(mka_pls_upd_peer_list_by_peers_id_pr_t(recv_buffer, helloTimeMsg.live_peer_str_len,
                                                          &commonArgs->mka_i.peer_l, commonArgs->mka_i.id) == 1){
                    commonArgs->need_to_send_HTM = interface_num;
                    commonArgs->need_to_refresh_SAK = 1;
                    printf("Recv alive peer HT\n");
                }
                commonArgs->mka_i.KS_priority = mka_pls_get_highest_ks_priority(&commonArgs->mka_i.peer_l,
                                                                                &commonArgs->mka_i.isKS,
                                                                                commonArgs->mka_i.my_KS_priority,
                                                                                commonArgs->mka_i.id);
            }
            else if (commonArgs->mka_i.peer_l.peer_l[id].status == 0) {
                // процедура потенциальный -> живой
                // он должен послать весь список живых (моих) в потенциальных, как подтверждение
                if(mka_t_o.id_src == sendedLivePeersCharArrays.id && (helloTimeMsg.potential_peer == commonArgs->mka_i.id || helloTimeMsg.potential_peer == 0)) {
                    if (helloTimeMsg.potential_peer_str_len != 0 && memcmp(recv_buffer + helloTimeMsg.live_peer_str_len, sendedLivePeersCharArrays.sended_buffer, helloTimeMsg.potential_peer_str_len) == 0){
                        if(helloTimeMsg.potential_peer == commonArgs->mka_i.id) {
                            buffer = mka_pls_get_live_peers_with_peer_id_pr_t(&commonArgs->mka_i.peer_l, &buffer_len);
                            mka_htmsg_send(commonArgs, interface_num, 1, 0, 0, buffer, buffer_len,
                                           recv_buffer,
                                           helloTimeMsg.live_peer_str_len, NULL, 0);
                        }
                        mka_pls_peer_upd_status_by_id(&commonArgs->mka_i.peer_l, id, 1);
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
                        free(sendedLivePeersCharArrays.sended_buffer);
                        sendedLivePeersCharArrays.id = 0;
                        sendedLivePeersCharArrays.sended_buffer_len = 0;
                        commonArgs->connectionParamsList.c_p_list[interface_num].pause_HT = 0;
                        commonArgs->need_to_refresh_SAK = 1;
                        num_incorrect_answers = 0;
                    } else {
                        // todo: if he will not??
                        printf("Incorrect answer\n");
                        print_string(recv_buffer + helloTimeMsg.live_peer_str_len, helloTimeMsg.potential_peer_str_len);
                        print_string(sendedLivePeersCharArrays.sended_buffer, helloTimeMsg.potential_peer_str_len);
                        fflush(stdout);
                    }
                } else {
                    ++num_incorrect_answers;
                    if(num_incorrect_answers == 3 || num_incorrect_answers == 6) {
                        buffer = mka_pls_get_live_peers_with_peer_id_pr_t(&commonArgs->mka_i.peer_l, &buffer_len);
                        if (buffer_len == 0) {
                            buffer_len = 10;
                            buffer = generate_rnd_string(10);
                        }
                        mka_htmsg_send(commonArgs, interface_num, 1, mka_t_o.id_src, 0, buffer, buffer_len,
                                       recv_buffer,
                                       recv_buffer_len, NULL, 0);
                    } else if(num_incorrect_answers == 9){
                        free(sendedLivePeersCharArrays.sended_buffer);
                        sendedLivePeersCharArrays.id = 0;
                        sendedLivePeersCharArrays.sended_buffer_len = 0;
                    }
                }
            }
        }
        free(recv_buffer);
        recv_buffer = NULL;
        recv_buffer_len = 0;
        memset(&mka_t, 0, sizeof(mka_tr_encr_msg));
        memset(&mka_t_o, 0, sizeof(mka_tr_open_msg));
    }
    if(recv_buffer != NULL && recv_buffer_len != 0) free(recv_buffer);
    if(buffer != NULL && buffer_len != 0) free(buffer);
}

