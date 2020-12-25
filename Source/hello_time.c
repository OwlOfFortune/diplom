#include "../Headers/hello_time.h"

int mka_htmsg_send(mka_common_args *commonArgs, int interface_num, int encr, int potential_peer,
                   int random_live_peer_str, char *live_peer_str, int live_peer_str_len,
                   char *potential_peer_list, int ppl_len, char *other, int other_len) {
    char *sendbuf = (char*) calloc(sizeof (mka_htmsg) + live_peer_str_len + other_len + ppl_len, sizeof(char));
    mka_htmsg *helloTimeMsg = (struct mka_htmsg *)sendbuf;
    helloTimeMsg->potential_peer = potential_peer; // + номер сообщения
    helloTimeMsg->msg_num = commonArgs->mka_i.msg_num;
    helloTimeMsg->random_live_peer_str = random_live_peer_str;
    helloTimeMsg->other_len = other_len;
    helloTimeMsg->live_peer_str_len = live_peer_str_len;
    helloTimeMsg->potential_peer_str_len = ppl_len;
    if(live_peer_str != NULL && live_peer_str_len > 0) memcpy(sendbuf + sizeof(mka_htmsg), live_peer_str, live_peer_str_len);
    if(potential_peer_list != NULL && ppl_len > 0) memcpy(sendbuf + sizeof(mka_htmsg) + live_peer_str_len, potential_peer_list, ppl_len);
    if(other != NULL && other_len > 0) memcpy(sendbuf + sizeof(mka_htmsg) + live_peer_str_len + ppl_len, other, other_len);

//    printf("Sendbuf with HT hdr: ");
//    print_string(sendbuf, live_peer_str_len + other_len + ppl_len + sizeof(hello_time_msg));
    if(encr == 1)
        mka_tr_send_encr_data(&commonArgs->connectionParamsList.c_p_list[interface_num], &commonArgs->mka_i, sendbuf,
                              live_peer_str_len + other_len + ppl_len + sizeof(mka_htmsg), 0);
    else
        mka_tr_send_data(&commonArgs->connectionParamsList.c_p_list[interface_num], &commonArgs->mka_i, sendbuf,
                         live_peer_str_len + ppl_len + other_len + sizeof(mka_htmsg), 0);
//    printf("One HT sended\n");
    ++commonArgs->mka_i.msg_num;
    free(sendbuf);
    return 0;
}

void mka_htmsg_send_data_to_all(mka_common_args *commonArgs, int encr, int potential_peer, int random_live_peer_str,
                                char *live_peer_str, int live_peer_str_len, char *potential_peer_list, int ppl_len,
                                char *other, int other_len) {
    char *sendbuf = (char*) calloc(sizeof (mka_htmsg) + live_peer_str_len + other_len + ppl_len, sizeof(char));
    mka_htmsg *helloTimeMsg = (struct mka_htmsg *)sendbuf;
    helloTimeMsg->potential_peer = potential_peer; // + номер сообщения
    helloTimeMsg->msg_num = commonArgs->mka_i.msg_num;
    helloTimeMsg->other_len = other_len;
    helloTimeMsg->random_live_peer_str = random_live_peer_str;
    helloTimeMsg->live_peer_str_len = live_peer_str_len;
    helloTimeMsg->potential_peer_str_len = ppl_len;
    if(live_peer_str != NULL && live_peer_str_len > 0) memcpy(sendbuf + sizeof(mka_htmsg), live_peer_str, live_peer_str_len);
    if(potential_peer_list != NULL && ppl_len > 0) memcpy(sendbuf + sizeof(mka_htmsg), live_peer_str, live_peer_str_len);
    if(other != NULL && other_len > 0) memcpy(sendbuf + sizeof(mka_htmsg) + live_peer_str_len, other, other_len);
    mka_cpl_freeze(&commonArgs->connectionParamsList, 1);
    for(int i = 0; i < commonArgs->connectionParamsList.num_of_c_p; ++i){
        if(mka_pls_peer_get_id_by_peer_id(&commonArgs->mka_i.peer_l,
                                          commonArgs->connectionParamsList.c_p_list[i].peer_id) == -1) commonArgs->connectionParamsList.c_p_list[i].peer_id = -1;
//        if(commonArgs->connectionParamsList.c_p_list[i].pause_HT == 1) {
//            printf("HT paused on %d interface\n", i);
//            continue;
//        }
        if(commonArgs->connectionParamsList.c_p_list[i].peer_id == -1) {
            helloTimeMsg->live_peer_str_len = 0;
            helloTimeMsg->other_len = 0;
            helloTimeMsg->potential_peer_str_len = 0;
            helloTimeMsg->potential_peer = -1;
            mka_tr_send_data(&commonArgs->connectionParamsList.c_p_list[i], &commonArgs->mka_i, sendbuf,
                             sizeof(mka_htmsg), 0);
        } else if(encr == 1)
            mka_tr_send_encr_data(&commonArgs->connectionParamsList.c_p_list[i], &commonArgs->mka_i, sendbuf,
                                  live_peer_str_len + other_len + ppl_len + sizeof(mka_htmsg), 0);
        else
            mka_tr_send_data(&commonArgs->connectionParamsList.c_p_list[i], &commonArgs->mka_i, sendbuf,
                             live_peer_str_len + ppl_len + other_len + sizeof(mka_htmsg), 0);
//        printf("HT sended\n");
    }
    mka_cpl_unfreeze(&commonArgs->connectionParamsList, 1);
    ++commonArgs->mka_i.msg_num;
    free(sendbuf);
}

void *mka_htmsg_send_thread_func(void *args){
    mka_common_args *commonArgs = (mka_common_args*) args;
    char *buffer;
    int len;
    time_t time_start;
    mka_htmsg helloTimeMsg;

    while (!commonArgs->stop) {
        buffer = mka_pls_get_live_peers_with_peer_id_pr_t(&commonArgs->mka_i.peer_l, &len);
        mka_htmsg_send_data_to_all(commonArgs, 1, 0, 0, buffer, len, NULL, 0, NULL, 0);
        free(buffer);
        time_start = time(NULL);
        while(time(NULL) - time_start < commonArgs->hello_time_timeout) {
            if (commonArgs->need_to_send_HTM != 0) {
                buffer = mka_pls_get_live_peers_with_peer_id_pr_t(&commonArgs->mka_i.peer_l, &len);
                mka_htmsg_send_data_to_all(commonArgs, 1, 0, 0, buffer, len, NULL, 0, NULL, 0);
                free(buffer);
                commonArgs->need_to_send_HTM = 0;
                time_start = time(NULL);
            }
            usleep(50000);
        }
        while(commonArgs->need_to_refresh_SAK == 1 && !commonArgs->stop)
            usleep(50000);
    }
}