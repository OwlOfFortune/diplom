#include <memory.h>
#include "../Headers/mka_key_str.h"
#include "../Headers/other_functions.h"
#include <unistd.h>

int log_func( const char *message )
{
//    fprintf( stderr, "%s\n", message );
    return ak_error_ok;
}

int mka_key_str_blomkey_get_abonent_key_from_file(ak_blomkey bkey, FILE *fp)
{
    struct hash ctx;
    size_t memsize = 0;
    char temp;

    memset( bkey, 0, sizeof( struct blomkey ));
    fscanf(fp,"%u",&bkey->size);
    fscanf(fp,"%u",&bkey->count);
    fread(&temp, 1, 1, fp);
    ak_hash_create_streebog256( &ctx );
    bkey->type = blom_abonent_key;

    /* создаем контекст хеш-функции */
    if(( ak_hash_create_oid( &bkey->ctx, ctx.oid )) != ak_error_ok ) {
        fclose(fp);
        return -1;
    }

    /* формируем ключевые данные */
    if(( bkey->data = malloc(( memsize = bkey->size * bkey->count ))) == NULL ) {
        fclose(fp);
        return -1;
    }
    memset( bkey->data, 0, memsize );
    fread(bkey->data, bkey->size*bkey->count, 1, fp);

    if(( ak_hash_ptr( &bkey->ctx, bkey->data, memsize,
                              bkey->icode, 32 )) != ak_error_ok ) {
        ak_blomkey_destroy( bkey );
        return -1;
    }
    ak_hash_destroy(&ctx);
    return 0;
}


int mka_key_str_init(mka_key_str *mka_k_s, FILE *fp){
    if( !ak_libakrypt_create( log_func )) return ak_libakrypt_destroy();
    ak_bckey_create_kuznechik( &mka_k_s->SAK );
    mka_key_str_blomkey_get_abonent_key_from_file(&mka_k_s->PSK, fp);
    mka_k_s->peer_keys_list_capacity = 0;
    mka_k_s->peer_keys_list = (peer_keys*) calloc(mka_k_s->peer_keys_list_capacity, sizeof (peer_keys));
    return 0;
}

int mka_key_str_set_KEK(mka_key_str *mka_str, unsigned char *KEK, int kek_len, int peer_id) {
    int num = mka_key_str_get_peer_key_num_by_peer_id(mka_str, peer_id);
    mka_key_str_freeze(mka_str, 0);
    ak_bckey_set_key( &mka_str->peer_keys_list[num].KEK, KEK, kek_len);
    mka_key_str_unfreeze(mka_str, 0);
    return 0;
}

int mka_key_str_get_peer_key_num_by_peer_id(mka_key_str *mka_ks_str, int peer_id){
    mka_key_str_freeze(mka_ks_str, 1);
    for(int i = 0; i < mka_ks_str->peer_keys_list_capacity; ++i)
        if(mka_ks_str->peer_keys_list[i].peer_id == peer_id) {
            mka_key_str_unfreeze(mka_ks_str, 1);
            return i;
        }
    mka_key_str_unfreeze(mka_ks_str, 1);
    return -1;
}

void mka_key_str_set_peer_id(mka_key_str *mka_ks_str, int peer_id, int pos) {
    mka_key_str_freeze(mka_ks_str, 0);
    mka_ks_str->peer_keys_list[pos].peer_id = peer_id;
    mka_key_str_unfreeze(mka_ks_str, 0);
}

int mka_key_str_gen_KEK(mka_key_str *mka_ks_str, int peer_id) {
    char kek_str[] = "KEK";
    int num = mka_key_str_get_peer_key_num_by_peer_id(mka_ks_str, peer_id);
    if(num != -1) {
        mka_key_str_freeze(mka_ks_str, 1);
        mka_ks_str->peer_keys_list[num].CAK.key.unmask(&mka_ks_str->peer_keys_list[num].CAK.key); /* снимаем маску */
        ak_bckey_set_key_from_password(&mka_ks_str->peer_keys_list[num].KEK, mka_ks_str->peer_keys_list[num].CAK.key.key, mka_ks_str->peer_keys_list[num].CAK.key.key_size , kek_str, sizeof (kek_str) );
        mka_ks_str->peer_keys_list[num].CAK.key.set_mask(&mka_ks_str->peer_keys_list[num].CAK.key);
        mka_key_str_unfreeze(mka_ks_str, 1);
        return 0;
    }
    return -1;
}

int mka_key_str_gen_SAK(mka_key_str *mka_ks_str, peer_list_str *list) {
//    int len;
    if(mka_ks_str->peer_keys_list_capacity != 0) {
        int num = rand() % mka_ks_str->peer_keys_list_capacity;
        int len_salt = 256;
        char *salt = generate_rnd_string(len_salt);
        mka_key_str_freeze(mka_ks_str, 1);
        mka_ks_str->peer_keys_list[num].CAK.key.unmask(&mka_ks_str->peer_keys_list[num].CAK.key);
        if(mka_ks_str->SAK.decrypt != NULL) ak_bckey_set_key_from_password(&mka_ks_str->SAK, &mka_ks_str->peer_keys_list[num].CAK.key.key, mka_ks_str->peer_keys_list[num].CAK.key.key_size , salt, len_salt);
        mka_ks_str->peer_keys_list[num].CAK.key.set_mask(&mka_ks_str->peer_keys_list[num].CAK.key);
        mka_key_str_unfreeze(mka_ks_str, 1);
        return 0;
    } else {
        ak_bckey_destroy(&mka_ks_str->SAK);
        ak_bckey_create_kuznechik( &mka_ks_str->SAK );
    }
    return -1;
}

void mka_key_str_set_SAK(mka_key_str *mka_ks_str, ak_uint8 *SAK, int size) {
    if(mka_ks_str->SAK.decrypt != NULL) ak_bckey_set_key( &mka_ks_str->SAK, SAK, size);
}

void mka_key_str_gen_CAK(mka_key_str *mka_ks_str, int peer_id) {
    char *id = (char *) calloc(512, sizeof(char));
    size_t id_len = sprintf (id, "%d", peer_id);
    int num = mka_key_str_get_peer_key_num_by_peer_id(mka_ks_str, peer_id);
    mka_key_str_freeze(mka_ks_str, 0);
    ak_blomkey_create_pairwise_key(&mka_ks_str->PSK, id, id_len,
                                   &mka_ks_str->peer_keys_list[num].CAK, ak_oid_find_by_name("kuznechik" ));
    mka_key_str_unfreeze(mka_ks_str, 0);
    free(id);
}

void mka_key_str_add_peer_keys(mka_key_str *mka_key_str){
    mka_key_str_freeze(mka_key_str, 0);
    mka_key_str->peer_keys_list = (peer_keys *) realloc (mka_key_str->peer_keys_list, (mka_key_str->peer_keys_list_capacity + 1) * sizeof(peer_keys));
    ak_bckey_create_kuznechik( &mka_key_str->peer_keys_list[mka_key_str->peer_keys_list_capacity].CAK);
    ak_bckey_create_kuznechik( &mka_key_str->peer_keys_list[mka_key_str->peer_keys_list_capacity].KEK);
    ++mka_key_str->peer_keys_list_capacity;
    mka_key_str_unfreeze(mka_key_str, 0);
}

void mka_key_str_rm_peer_keys_by_peer_id(mka_key_str *mka_key_str, int peer_id){
    int num = mka_key_str_get_peer_key_num_by_peer_id(mka_key_str, peer_id);
    if (mka_key_str->peer_keys_list_capacity > num && num >= 0) {
        mka_key_str_freeze(mka_key_str, 0);
        --mka_key_str->peer_keys_list_capacity;
        if(mka_key_str->peer_keys_list_capacity == 0) {
            ak_bckey_destroy( &mka_key_str->peer_keys_list[0].CAK);
            ak_bckey_destroy( &mka_key_str->peer_keys_list[num].KEK);
            free(mka_key_str->peer_keys_list);
            mka_key_str->peer_keys_list = (peer_keys *)calloc(0, sizeof(peer_keys));
        } else if (mka_key_str->peer_keys_list_capacity >= num) {
            ak_bckey_destroy( &mka_key_str->peer_keys_list[num].CAK);
            ak_bckey_destroy( &mka_key_str->peer_keys_list[num].KEK);
            if(mka_key_str->peer_keys_list_capacity > num) memcpy(&mka_key_str->peer_keys_list[num], &mka_key_str->peer_keys_list[num + 1], sizeof(peer_keys) * (mka_key_str->peer_keys_list_capacity - num));
            mka_key_str->peer_keys_list = (peer_keys *) realloc(mka_key_str->peer_keys_list, sizeof(peer_keys) * (mka_key_str->peer_keys_list_capacity));
        }
        mka_key_str_unfreeze(mka_key_str, 0);
    }
}

int mka_key_str_destruct(mka_key_str *mka_ks_str){
    ak_blomkey_destroy( &mka_ks_str->PSK );
    ak_bckey_destroy( &mka_ks_str->SAK );
    mka_key_str_freeze(mka_ks_str, 0);
    for(int i = 0; i < mka_ks_str->peer_keys_list_capacity; ++i){
        ak_bckey_destroy( &mka_ks_str->peer_keys_list[i].KEK );
        ak_bckey_destroy( &mka_ks_str->peer_keys_list[i].CAK );
    }
    mka_key_str_unfreeze(mka_ks_str, 0);
    free(mka_ks_str->peer_keys_list);
    ak_libakrypt_destroy();
    return 0;
}

void mka_key_str_unfreeze(mka_key_str *mka_ks, int type){
    if(type == 0){
        mka_ks->peer_keys_list_is_busy = 0;
    } else {
        ++mka_ks->peer_keys_list_is_busy;
    }
}


void mka_key_str_freeze(mka_key_str *mka_ks, int type){
    int rand1 = rand() % 1000 + 1;
    time_t time2 = time(NULL) % 1000;
    if(type == 0){
        while(mka_ks->peer_keys_list_is_busy != 0) usleep(time2);
        mka_ks->peer_keys_list_is_busy = rand1;
        while(mka_ks->peer_keys_list_is_busy != rand1 && mka_ks->peer_keys_list_is_busy != 0)
            usleep(time2);
        mka_ks->peer_keys_list_is_busy = rand1;
    } else {
        while(mka_ks->peer_keys_list_is_busy > 0) usleep(time2);
        --mka_ks->peer_keys_list_is_busy;
        if(++mka_ks->peer_keys_list_is_busy > 0)
            while(mka_ks->peer_keys_list_is_busy > 0) usleep(time2);
        --mka_ks->peer_keys_list_is_busy;
    }
}
