#include "../Headers/key_server.h"
void *mka_ksmsg_send_thread_func(void *args){
    mka_common_args *commonArgs = (mka_common_args*) args;
    while (!commonArgs->stop) {
        if(commonArgs->need_to_refresh_SAK == 1 && commonArgs->mka_i.isKS == 1) {
            mka_key_str_gen_SAK(&commonArgs->mka_i.mkaKeyStr, &commonArgs->mka_i.peer_l);
            // sending to all interfaces;
            printf("SAK: ");
            commonArgs->mka_i.mkaKeyStr.SAK.key.unmask( &commonArgs->mka_i.mkaKeyStr.SAK.key ); /* снимаем маску */
            print_string( commonArgs->mka_i.mkaKeyStr.SAK.key.key,commonArgs->mka_i.mkaKeyStr.SAK.key.key_size);
            commonArgs->mka_i.mkaKeyStr.SAK.key.set_mask( &commonArgs->mka_i.mkaKeyStr.SAK.key );

            mka_ksmsg_send_sak(commonArgs, -1);
            printf("Key sended 1\n");
            fflush(stdout);
            commonArgs->need_to_refresh_SAK = 0;
        }
        if(commonArgs->need_to_send_SAK_to_others__received_t != 0){
            printf("Key sended 2\n");
            fflush(stdout);
            mka_ksmsg_send_sak(commonArgs, commonArgs->need_to_send_SAK_to_others__received_t);
            commonArgs->need_to_send_SAK_to_others__received_t = 0;
        }
        usleep(10000);
    }
}

void mka_ksmsg_send_sak(mka_common_args *commonArgs, int ignored_interface_num) {
    ak_uint8 *sendbuf = (ak_uint8*) calloc(sizeof(mka_ksmsg) + commonArgs->mka_i.mkaKeyStr.SAK.key.key_size, sizeof(ak_uint8));
    mka_ksmsg *mkaKsMsg = (mka_ksmsg *)sendbuf;
    /// here can be initialised vars in mka_ks_msg (eg encr mtd)
    commonArgs->mka_i.mkaKeyStr.SAK.key.unmask( &commonArgs->mka_i.mkaKeyStr.SAK.key ); /* снимаем маску */
    memcpy(sendbuf + sizeof(mka_ksmsg), commonArgs->mka_i.mkaKeyStr.SAK.key.key, commonArgs->mka_i.mkaKeyStr.SAK.key.key_size);
    commonArgs->mka_i.mkaKeyStr.SAK.key.set_mask( &commonArgs->mka_i.mkaKeyStr.SAK.key );
    mka_cpl_freeze(&commonArgs->connectionParamsList, 1);
    for(int i = 0; i < commonArgs->connectionParamsList.num_of_c_p; ++i){
        if(i == ignored_interface_num || commonArgs->connectionParamsList.c_p_list[i].peer_id == -1) continue;
        mka_tr_send_encr_data(&commonArgs->connectionParamsList.c_p_list[i], &commonArgs->mka_i, sendbuf,
                              sizeof(mka_ksmsg) + commonArgs->mka_i.mkaKeyStr.SAK.key.key_size, 1);
    }
    mka_cpl_unfreeze(&commonArgs->connectionParamsList, 1);
    ++commonArgs->mka_i.msg_num;
    free(sendbuf);
}