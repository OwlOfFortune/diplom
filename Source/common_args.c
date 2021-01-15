#include "../Headers/common_args.h"


int mka_common_args_init(mka_common_args *commonArgs, int id, char KS_priority, int list_timeout, FILE *fp) {
    if(mka_cpl_init(&commonArgs->connectionParamsList)) return 1;
    if(mka_info_init(&commonArgs->mka_i, id, KS_priority, list_timeout, fp)) return 1;
    commonArgs->hello_time_timeout = 2;
    commonArgs->need_to_send_HTM = 0;
    commonArgs->need_to_refresh_SAK = 0;
    commonArgs->stop = 0;
    commonArgs->tid_cleaner = -1;
    commonArgs->tid_send = -1;
    commonArgs->need_to_send_SAK_to_others__received_t = -1;
    commonArgs->reply_SAK__received_t = -1;
    return 0;
}

int mka_common_args_destruct(mka_common_args *commonArgs){
    commonArgs->stop = 1;
    if(mka_info_destruct(&commonArgs->mka_i)) return 1;
    if(mka_cpl_destruct(&commonArgs->connectionParamsList)) return 1;
    if(commonArgs->tid_cleaner != -1)
        pthread_join(commonArgs->tid_cleaner,NULL);
    if(commonArgs->tid_send != -1)
        pthread_join(commonArgs->tid_send,NULL);
    return 0;
}