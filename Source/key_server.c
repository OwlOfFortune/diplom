#include "../Headers/key_server.h"
void *mka_ksmsg_send_thread_func(void *args){
    mka_common_args *commonArgs = (mka_common_args*) args;
    time_t start_waiting = -1;
    while (!commonArgs->stop) {
        if(commonArgs->need_to_refresh_SAK == 1 && commonArgs->mka_i.isKS == 1) {
            if(mka_key_str_gen_SAK(&commonArgs->mka_i.mkaKeyStr, &commonArgs->mka_i.peer_l) == -1){
                printf("Only 1 peer, SAK cannot be generated\n");
                commonArgs->need_to_refresh_SAK = 0;
                start_waiting = -1;
                continue;
            }
            // sending to all interfaces;
            printf("SAK: ");
            commonArgs->mka_i.mkaKeyStr.SAK.key.unmask( &commonArgs->mka_i.mkaKeyStr.SAK.key ); /* снимаем маску */
            print_string( commonArgs->mka_i.mkaKeyStr.SAK.key.key,commonArgs->mka_i.mkaKeyStr.SAK.key.key_size);
            commonArgs->mka_i.mkaKeyStr.SAK.key.set_mask( &commonArgs->mka_i.mkaKeyStr.SAK.key );

            mka_ksmsg_send_sak_2_all(commonArgs, -1, -1);
//            printf("Key sended 1\n");
            fflush(stdout);
            commonArgs->need_to_refresh_SAK = 0;
            start_waiting = -1;
        }
        if(commonArgs->need_to_send_SAK_to_others__received_t != -1){
//            printf("Key sended 2\n");
            fflush(stdout);
            mka_ksmsg_send_sak_2_all(commonArgs, commonArgs->need_to_send_SAK_to_others__received_t, commonArgs->need_to_send_SAK_to_others__peer_id);
            commonArgs->need_to_send_SAK_to_others__received_t = -1;
            commonArgs->need_to_send_SAK_to_others__peer_id = -1;
            start_waiting = -1;
        }
        if(commonArgs->reply_SAK__received_t != -1){
            printf("Key sended 3\n");
            printf("id = %d\n", commonArgs->reply_SAK__peer_id);
            printf("interface = %d\n", commonArgs->reply_SAK__received_t);
            mka_ksmsg_reply_sak(commonArgs, commonArgs->reply_SAK__received_t, commonArgs->reply_SAK__peer_id);
            commonArgs->reply_SAK__received_t = -1;
            commonArgs->reply_SAK__peer_id = -1;
        }
        if(commonArgs->need_to_refresh_SAK == 1 && commonArgs->mka_i.isKS == 0){
            if(start_waiting == -1) start_waiting = time(NULL);
            else if(time(NULL) - start_waiting > 1){
                // не дождался ключа
                mka_ksmsg_send_reply(commonArgs);
                start_waiting = -1;
            }
        }
        usleep(10000);
    }
}

void mka_ksmsg_send_sak_2_all(mka_common_args *commonArgs, int ignored_interface_num, int ignored_alive_p_num) {
    ak_uint8 *sendbuf = (ak_uint8*) calloc(sizeof(mka_ksmsg) + commonArgs->mka_i.mkaKeyStr.SAK.key.key_size, sizeof(ak_uint8));
    mka_ksmsg *mkaKsMsg = (mka_ksmsg *)sendbuf;
    mkaKsMsg->reply_flg = 0;
    commonArgs->mka_i.mkaKeyStr.SAK.key.unmask( &commonArgs->mka_i.mkaKeyStr.SAK.key ); /* снимаем маску */
    memcpy(sendbuf + sizeof(mka_ksmsg), commonArgs->mka_i.mkaKeyStr.SAK.key.key, commonArgs->mka_i.mkaKeyStr.SAK.key.key_size);
    commonArgs->mka_i.mkaKeyStr.SAK.key.set_mask( &commonArgs->mka_i.mkaKeyStr.SAK.key );
    for(int i = 0; i < commonArgs->connectionParamsList.num_of_c_p; ++i){
        connection_params *param = &commonArgs->connectionParamsList.c_p_list[i];
        mka_cp_freeze(param, 1, 1);
        for(int j = 0; j < param->acl_capacity; ++j){
            if(i == ignored_interface_num && ignored_alive_p_num == j) continue;
            mka_tr_send_encr_data(param, param->alive_connection_list[j].peer_id, &commonArgs->mka_i, sendbuf,
                                  sizeof(mka_ksmsg) + commonArgs->mka_i.mkaKeyStr.SAK.key.key_size, 1);
        }
        mka_cp_unfreeze(param, 1, 1);
    }
    ++commonArgs->mka_i.msg_num;
    free(sendbuf);
}

void mka_ksmsg_reply_sak(mka_common_args *commonArgs, int interface_num, int peer_id) {
    ak_uint8 *sendbuf = (ak_uint8*) calloc(sizeof(mka_ksmsg) + commonArgs->mka_i.mkaKeyStr.SAK.key.key_size, sizeof(ak_uint8));
    mka_ksmsg *mkaKsMsg = (mka_ksmsg *)sendbuf;
    mkaKsMsg->reply_flg = 0;
    commonArgs->mka_i.mkaKeyStr.SAK.key.unmask( &commonArgs->mka_i.mkaKeyStr.SAK.key ); /* снимаем маску */
    memcpy(sendbuf + sizeof(mka_ksmsg), commonArgs->mka_i.mkaKeyStr.SAK.key.key, commonArgs->mka_i.mkaKeyStr.SAK.key.key_size);
    commonArgs->mka_i.mkaKeyStr.SAK.key.set_mask( &commonArgs->mka_i.mkaKeyStr.SAK.key );
    connection_params *param = &commonArgs->connectionParamsList.c_p_list[interface_num];
    mka_cp_freeze(param, 1, 1);
    mka_tr_send_encr_data(&commonArgs->connectionParamsList.c_p_list[interface_num], peer_id, &commonArgs->mka_i, sendbuf,
                              sizeof(mka_ksmsg) + commonArgs->mka_i.mkaKeyStr.SAK.key.key_size, 1);
    mka_cp_unfreeze(param, 1, 1);
    ++commonArgs->mka_i.msg_num;
    free(sendbuf);
}

void mka_ksmsg_send_reply(mka_common_args *commonArgs) {
    ak_uint8 *sendbuf = (ak_uint8*) calloc(1, sizeof(mka_ksmsg));
    mka_ksmsg *mkaKsMsg = (mka_ksmsg *)sendbuf;
    mkaKsMsg->reply_flg = 1;
    for(int i = 0; i < commonArgs->connectionParamsList.num_of_c_p; ++i){
        connection_params *param = &commonArgs->connectionParamsList.c_p_list[i];
        mka_cp_freeze(param, 1, 1);
        for(int j = 0; j < param->acl_capacity; ++j){
            mka_tr_send_encr_data(&commonArgs->connectionParamsList.c_p_list[i], param->alive_connection_list[j].peer_id, &commonArgs->mka_i, sendbuf,
                                  sizeof(mka_ksmsg), 1);
        }
        mka_cp_unfreeze(param, 1, 1);
    }
    ++commonArgs->mka_i.msg_num;
    free(sendbuf);
}