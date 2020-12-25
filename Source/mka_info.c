#include "../Headers/mka_info.h"

int mka_info_init(mka_info *mka_i, int id, int KS_priority, int list_timeout){
    mka_i->my_KS_priority = KS_priority;
    mka_i->KS_priority = KS_priority;
    mka_i->id = id;
    mka_pls_init(&mka_i->peer_l, list_timeout);
    mka_i->isKS = 1;
    mka_i->msg_num = 1;
    if(mka_key_str_init(&mka_i->mkaKeyStr)) return 1;
    return 0;
}

int mka_info_destruct(mka_info *mka_i){
    mka_pls_destruct(&mka_i->peer_l);
    if(mka_key_str_destruct(&mka_i->mkaKeyStr)) return 1;
    return 0;
}