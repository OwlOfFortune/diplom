#include <unistd.h>
#include "../Headers/peer_list_str.h"
#include "../Headers/common_args.h"

peer_list_str *mka_pls_init(peer_list_str *peer_l, int timeout){
    peer_l->peer_l = (peer_list_unit *)calloc(0,sizeof(peer_list_unit));
    peer_l->timeout = timeout;
    peer_l->capacity = 0;
    peer_l->num_live_peers = 0;
    peer_l->pl_is_busy = 0;
    peer_l->pl_is_changed = 0;
    return peer_l;
}

void mka_pls_freeze(peer_list_str *peer_l, int type) {
    int rand1 = rand() % 1000 + 1;
    time_t time2 = time(NULL) % 1000;
    if(type == 0){
        while(peer_l->pl_is_busy != 0) usleep(time2);
        peer_l->pl_is_busy = rand1;
        while(peer_l->pl_is_busy != rand1 && peer_l->pl_is_busy != 0)
            usleep(time2);
        peer_l->pl_is_busy = rand1;
    } else {
        while(peer_l->pl_is_busy > 0) usleep(time2);
        --peer_l->pl_is_busy;
        if(++peer_l->pl_is_busy > 0)
            while(peer_l->pl_is_busy > 0) usleep(time2);
        --peer_l->pl_is_busy;
    }
}

void mka_pls_unfreeze(peer_list_str *peer_l, int type) {
    if(type == 0){
        peer_l->pl_is_busy = 0;
    } else {
        ++peer_l->pl_is_busy;
    }
}

peer *mka_pls_peer_init(peer *peer, int peer_id, int KS_priority){
    peer->peer_id = peer_id;
    peer->KS_priority = KS_priority;
    return peer;
}

// нельзя использовать вне других ф-ий с контролем доступа к peer_l
void mka_pls_peer_add(peer_list_str *peer_l, peer *p){
    peer_l->peer_l = (peer_list_unit *) realloc (peer_l->peer_l, (peer_l->capacity + 1) * sizeof(peer_list_unit));
    memcpy(&peer_l->peer_l[peer_l->capacity].p, p, sizeof(peer));
    peer_l->peer_l[peer_l->capacity].id = peer_l->capacity;
    peer_l->peer_l[peer_l->capacity].status = 0;
    peer_l->peer_l[peer_l->capacity].time = time(NULL) % 3600;
    ++peer_l->capacity;
    peer_l->pl_is_changed = 1;
//    printf("Peer %d was added\n", p->peer_id);
}
// нельзя использовать вне других ф-ий с контролем доступа к peer_l
void mka_pls_peer_add_with_time_and_status(peer_list_str *peer_l, peer *p, unsigned short time, int status) {
    peer_l->peer_l = (peer_list_unit *) realloc (peer_l->peer_l, (peer_l->capacity + 1) * sizeof(peer_list_unit));
    memcpy(&peer_l->peer_l[peer_l->capacity].p, p, sizeof(peer));
    peer_l->peer_l[peer_l->capacity].id = peer_l->capacity;
    peer_l->peer_l[peer_l->capacity].status = status;
    if(status == 1) ++peer_l->num_live_peers;
    peer_l->peer_l[peer_l->capacity].time = time;
    ++peer_l->capacity;
    peer_l->pl_is_changed = 1;
//    printf("Peer %d was added\n", p->peer_id);
}

void mka_pls_peer_upd_status_by_id(peer_list_str *peer_l, int id, char status){
    mka_pls_freeze(peer_l, 0);
    if(status == 1 && peer_l->peer_l[id].status == 0) ++peer_l->num_live_peers;
    else if(status == 0 && peer_l->peer_l[id].status == 1) --peer_l->num_live_peers;
    peer_l->peer_l[id].status = status;
    peer_l->peer_l[id].time = time(NULL) % 3600;
    peer_l->pl_is_changed = 1;
    mka_pls_unfreeze(peer_l, 0);
}

void mka_pls_peer_upd_time_by_id(peer_list_str *peer_l, int id, unsigned short time){
    mka_pls_freeze(peer_l, 0);
    peer_l->peer_l[id].time = time;
    mka_pls_unfreeze(peer_l, 0);
}

int mka_pls_peer_get_id_by_peer_id(peer_list_str *peer_l, int peer_id){
    mka_pls_freeze(peer_l, 1);
    for(int i = 0; i < peer_l->capacity; ++i)
        if(peer_l->peer_l[i].p.peer_id == peer_id) {
            mka_pls_unfreeze(peer_l, 1);
            return i;
        }
    mka_pls_unfreeze(peer_l, 1);
    return -1;
}

// нельзя использовать вне других ф-ий с контролем доступа к peer_l
int mka_pls_peer_get_id_by_peer_id_wthout_access_control(peer_list_str *peer_l, int peer_id){
    for(int i = 0; i < peer_l->capacity; ++i)
        if(peer_l->peer_l[i].p.peer_id == peer_id)
            return i;
    return -1;
}

// нельзя использовать вне других ф-ий с контролем доступа к peer_l
void mka_pls_peer_dlt_by_id(peer_list_str *peer_l, int id) {
    if (peer_l->capacity > id && id >= 0) {
        if (peer_l->peer_l[id].status == 1) --peer_l->num_live_peers;
        --peer_l->capacity;
        if(peer_l->capacity == 0) {
            free(peer_l->peer_l);
            peer_l->peer_l = (peer_list_unit *)calloc(0,sizeof(peer_list_unit));
        } else if (peer_l->capacity >= id) {
            if(peer_l->capacity > id){
                memcpy(&peer_l->peer_l[id], &peer_l->peer_l[id + 1], sizeof(peer_list_unit) * (peer_l->capacity - id));
                for (int i = id; i < peer_l->capacity; ++i) peer_l->peer_l[i].id = i;
            }
            peer_l->peer_l = (peer_list_unit *) realloc(peer_l->peer_l, sizeof(peer_list_unit) * (peer_l->capacity));
        }
        peer_l->pl_is_changed = 1;
    }
}

void mka_pls_dlt_died_peers(peer_list_str *peer_l){
    mka_pls_freeze(peer_l, 0);
    for (int i = 0; i < peer_l->capacity; ++i) {
        if(time(NULL) % 3600 - peer_l->peer_l[i].time > peer_l->timeout) {
            if(peer_l->peer_l[i].status == 1) printf("Peer %d was deleted\n", peer_l->peer_l[i].p.peer_id);
            mka_pls_peer_dlt_by_id(peer_l, i);
        }
    }
    mka_pls_unfreeze(peer_l, 0);
}

int mka_pls_peer_get_status_by_peer_id(peer_list_str *peer_l, int peer_id){
    mka_pls_freeze(peer_l, 1);
    for(int i = 0; i < peer_l->capacity; ++i)
        if(peer_l->peer_l[i].id == peer_id) {
            mka_pls_unfreeze(peer_l, 1);
            return peer_l->peer_l[i].status;
        }
    mka_pls_unfreeze(peer_l, 1);
    return -1;
}

char *mka_pls_get_live_peers_with_peer_id_pr_t(peer_list_str *peer_l, int *len){
    mka_pls_freeze(peer_l, 1);
//    printf("----- num of live peers: %d\n", peer_l->num_live_peers);
    *len = (int)(sizeof(peer_l->peer_l->p.peer_id) + sizeof(peer_l->peer_l->p.KS_priority) + sizeof(peer_l->peer_l->time)) * peer_l->num_live_peers;
    if(*len == 0) {
        mka_pls_unfreeze(peer_l, 1);
        return NULL;
    }
    int ln = sizeof(peer_l->peer_l->p.peer_id) + sizeof(peer_l->peer_l->p.KS_priority) + sizeof(peer_l->peer_l->time);
    char *buffer = (char *) calloc(peer_l->num_live_peers * ln, sizeof(char));
    char *pointer = buffer;

    for (int i = 0; i < peer_l->capacity; ++i){
        if(peer_l->peer_l[i].status == 1) {
            memcpy(pointer, &peer_l->peer_l[i].p.peer_id, sizeof(peer_l->peer_l[i].p.peer_id));
            pointer += sizeof(peer_l->peer_l[i].p.peer_id);
            memcpy(pointer, &peer_l->peer_l[i].p.KS_priority, sizeof(peer_l->peer_l[i].p.KS_priority));
            pointer += sizeof(peer_l->peer_l->p.KS_priority);
            memcpy(pointer, &peer_l->peer_l[i].time, sizeof(peer_l->peer_l[i].time));
            pointer += sizeof(peer_l->peer_l[i].time);
        }
    }
    pointer = NULL;
    mka_pls_unfreeze(peer_l, 1);
    return buffer;
}

void mka_pls_peer_print(peer *p){
    printf("peer id: %d\n",p->peer_id);
    printf("KS priority: %d\n",p->KS_priority);
}

int mka_pls_check_for_died_peers(peer_list_str *peer_l){
    mka_pls_freeze(peer_l, 1);
    for(int i = 0; i < peer_l->capacity; ++i)
        if(time(NULL) % 3600 - peer_l->peer_l[i].time >= peer_l->timeout) {
            mka_pls_unfreeze(peer_l, 1);
            return 1;
        }
    mka_pls_unfreeze(peer_l, 1);
    return 0;
}

int mka_pls_get_highest_ks_priority(peer_list_str *peer_l, int *imKS, int my_ks_pr, int my_id) {
    mka_pls_freeze(peer_l, 1);

    int pr = my_ks_pr;
    *imKS = 1;
    for(int i = 0; i < peer_l->capacity; ++i)
        if(peer_l->peer_l[i].status == 1 ) {
            if (peer_l->peer_l[i].p.KS_priority > pr || (peer_l->peer_l[i].p.KS_priority == pr && my_id < peer_l->peer_l[i].p.peer_id)) {
                pr = peer_l->peer_l[i].p.KS_priority;
                *imKS = 0;
            }
        }
    mka_pls_unfreeze(peer_l, 1);
    return pr;
}

void *mka_pls_cleaner_thread_func(void *args){
    mka_common_args *c = (mka_common_args *) args;
    int num_alive_peers;
    while (!c->stop) {
        if (mka_pls_check_for_died_peers(&c->mka_i.peer_l) == 1) {
            num_alive_peers = c->mka_i.peer_l.num_live_peers;
            mka_pls_dlt_died_peers(&c->mka_i.peer_l);
            if(num_alive_peers > c->mka_i.peer_l.num_live_peers){
                c->mka_i.KS_priority = mka_pls_get_highest_ks_priority(&c->mka_i.peer_l, &c->mka_i.isKS,
                                                                       c->mka_i.my_KS_priority,
                                                                       c->mka_i.id);
                c->need_to_send_HTM = 1;
                c->need_to_refresh_SAK = 1;
            }
            upd_peer_id_of_died_peers(c);
        }
        usleep(1000);
    }
}

int mka_pls_upd_peer_list_by_peers_id_pr_t(char *peers_id_pr_t, int len, peer_list_str *peer_l, int my_id) {
    if(peers_id_pr_t == NULL || len == 0){
        printf("zero upd srt\n");
        return 0;
    }
//    print_string(peers_id_pr_t, len);
    int number_of_peers = len / (int)(sizeof(peer_l->peer_l->id) + sizeof(peer_l->peer_l->p.KS_priority) + sizeof(peer_l->peer_l->time));
    char *pointer = peers_id_pr_t;
    int id;
    int priority;
    unsigned short time_p;
    int id_in_pl;
    peer p;

    mka_pls_freeze(peer_l, 0);
    for (int i = 0; i < number_of_peers; ++i){
        memcpy(&id, pointer, sizeof(id));
        pointer += sizeof(id);
        memcpy(&priority, pointer, sizeof(priority));
        pointer += sizeof(priority);
        memcpy(&time_p, pointer, sizeof(time_p));
        pointer += sizeof(time_p);
//        printf("id = %d\n", id);
        id_in_pl = mka_pls_peer_get_id_by_peer_id_wthout_access_control(peer_l, id);
//        printf("id_in_pl = %d\n", id_in_pl);
        peer_l->pl_is_changed = 0;
        if(id_in_pl == -1){
            if(time(NULL) % 3600 - time_p < peer_l->timeout && id != my_id){
                // добавляем peer + там флаг, что надо всем отправить, кроме того же интерфейса (надо будет добавить)
                mka_pls_peer_add_with_time_and_status(peer_l, mka_pls_peer_init(&p, id, priority), time_p, 1);
            }
        } else {
//            printf("time_p = %d\n", time_p);
//            printf("peer_l[id_in_pl].time = %d\n", peer_l->peer_l[id_in_pl].time);
            // todo: посмотреть насчет пересечения границы в 0 сек
            if(time(NULL) % 3600 - time_p < peer_l->timeout && peer_l->peer_l[id_in_pl].time < time_p){
                // меняем время на большее
                peer_l->peer_l[id_in_pl].time = time_p;
            }
        }
    }
    mka_pls_unfreeze(peer_l, 0);
    if(peer_l->pl_is_changed == 0) return 0;
    else return 1;
}

char *mka_pls_get_live_peers_with_peer_id(peer_list_str *peer_l, int *len){
    mka_pls_freeze(peer_l, 1);
    *len = (int)(sizeof(peer_l->peer_l->p.peer_id)) * peer_l->num_live_peers;
    if(*len == 0){
        mka_pls_unfreeze(peer_l, 1);
        return NULL;
    }
    int ln = sizeof(peer_l->peer_l->p.peer_id);
    char *buffer = (char *) calloc(peer_l->num_live_peers * ln, sizeof(char));
    char *pointer = buffer;

    for (int i = 0; i < peer_l->capacity; ++i){
        if(peer_l->peer_l[i].status == 1) {
            sprintf(pointer, "%d", peer_l->peer_l[i].p.peer_id);
            pointer += sizeof(peer_l->peer_l->p.peer_id);
        }
    }
    *pointer = '\0';
    pointer = NULL;
    mka_pls_unfreeze(peer_l, 1);
    return buffer;
}

void mka_pls_destruct(peer_list_str *peer_l){
    free(peer_l->peer_l);
}

/* ----------------------------------------------------------------------------------------------- */
/*! Обновление номера узла интерфейса, у которого умер этот узел

    @param с - общая структура потоков

    @return                                                                      */
/* ----------------------------------------------------------------------------------------------- */
int upd_peer_id_of_died_peers(mka_common_args *c){
    connection_params *c_p;
    int num;
    for(int i = 0; i < c->connectionParamsList.num_of_c_p; ++i){
        c_p = &c->connectionParamsList.c_p_list[i];
        for (int j = 0; j < c_p->pcl_capacity; ++j){
            int num = mka_pls_peer_get_id_by_peer_id(&c->mka_i.peer_l,c_p->potential_connection_list[j].peer_id);
            if(num == -1){
                mka_cp_freeze(c_p, 0, 0);
                mka_key_str_rm_peer_keys_by_peer_id(&c->mka_i.mkaKeyStr, c_p->potential_connection_list[j].peer_id);
                mka_cp_rm_connection_by_peer_id(c_p, c_p->potential_connection_list[j].peer_id, 0);
                mka_cp_unfreeze(c_p, 0, 0);
            }
        }
        for (int j = 0; j < c_p->acl_capacity; ++j) {
            int num = mka_pls_peer_get_id_by_peer_id(&c->mka_i.peer_l,c_p->alive_connection_list[j].peer_id);
            if(num == -1){
                mka_cp_freeze(c_p, 0, 1);
                mka_key_str_rm_peer_keys_by_peer_id(&c->mka_i.mkaKeyStr, c_p->alive_connection_list[j].peer_id);
                mka_cp_rm_connection_by_peer_id(c_p, c_p->alive_connection_list[j].peer_id, 1);
                mka_cp_unfreeze(c_p, 0, 1);
            }
        }
    }
}