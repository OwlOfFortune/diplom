#include <memory.h>
#include "../Headers/mka_key_str.h"

int log_func( const char *message )
{
//    fprintf( stderr, "%s\n", message );
    return ak_error_ok;
}

int mka_key_str_init(mka_key_str *mka_k_s){
    if( !ak_libakrypt_create( log_func )) return ak_libakrypt_destroy();
    ak_bckey_create_kuznechik( &mka_k_s->KEK );
    ak_bckey_create_kuznechik( &mka_k_s->SAK );
    ak_bckey_create_kuznechik( &mka_k_s->PSK );
    mka_k_s->cak_capacity = 0;
    mka_k_s->CAK = (struct bckey*) calloc(mka_k_s->cak_capacity, sizeof (struct bckey));
    return 0;
}

int mka_key_str_set_PSK(mka_key_str *mka_str, unsigned char *PSK, int psk_len){
    if(mka_str->PSK.decrypt != NULL) {
        ak_bckey_set_key( &mka_str->PSK, PSK, psk_len);
        return 0;
    } else return 1;
}

int mka_key_str_set_KEK(mka_key_str *mka_str, unsigned char *KEK, int kek_len){
    if(mka_str->PSK.decrypt != NULL) {
        ak_bckey_set_key( &mka_str->KEK, KEK, kek_len);
        return 0;
    } else return 1;
}
static ak_uint8 keyAnnexA[32] = {
        0xef, 0xcd, 0xab, 0x89, 0x67, 0x45, 0x23, 0x01, 0x10, 0x32, 0x54, 0x76, 0x98, 0xba, 0xdc, 0xfe,
        0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x00, 0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x99, 0x88 };


void mka_key_str_gen_KEK(mka_key_str *mka_ks_str){
    if(mka_ks_str->KEK.decrypt != NULL) ak_bckey_set_key( &mka_ks_str->KEK, keyAnnexA, sizeof(keyAnnexA));
    //ak_bckey_set_key_from_password( &mka_ks_str->KEK, mka_ks_str->CAK, mka_ks_str->cak_len , mka_ks_str->CKN, mka_ks_str->ckn_len );
}

void mka_key_str_gen_SAK(mka_key_str *mka_ks_str, peer_list_str *list){
    int len;
    // todo: можно взять рандомное значение CAK
    int cak_interface_num = 0, ckn_len = sizeof (mka_ks_str->CAK[cak_interface_num].key.number)/2;
    int len_salt = ckn_len;
    char *string = mka_pls_get_live_peers_with_peer_id(list, &len);
    char *salt = (char *) calloc(len_salt, sizeof(char));
    memcpy(salt, mka_ks_str->CAK[cak_interface_num].key.number, ckn_len);
    if(string != NULL && len != 0) {
        len_salt = len + ckn_len;
        memcpy(salt + ckn_len, string, len);
        free(string);
    }
//    ak_bckey_destroy( &mka_ks_str->SAK );
//    ak_bckey_create_kuznechik( &mka_ks_str->SAK );
    if(mka_ks_str->SAK.decrypt != NULL) ak_bckey_set_key_from_password( &mka_ks_str->SAK, &mka_ks_str->CAK[0].key.key, mka_ks_str->CAK[0].key.key_size , salt, len_salt);
}

void mka_key_str_set_SAK(mka_key_str *mka_ks_str, ak_uint8 *SAK, int size) {
    if(mka_ks_str->SAK.decrypt != NULL) ak_bckey_set_key( &mka_ks_str->SAK, SAK, size);
}

static ak_uint8 keyAnnex[32] = {
        0xef, 0xcd, 0xab, 0x89, 0x67, 0x45, 0x23, 0x01, 0x10, 0x32, 0x54, 0x76, 0x98, 0xba, 0xdc, 0xfe,
        0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x00, 0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x99, 0x88 };


void mka_key_str_gen_CAK(mka_key_str *mka_ks_str, int id_c, int id_s, int interface_num) {
    if(mka_ks_str->CAK[interface_num].decrypt != NULL)
        ak_bckey_set_key( &mka_ks_str->CAK[interface_num], keyAnnex, sizeof (keyAnnex));
//    f_func_blom(mka_ks_str->PSK.key.key, mka_ks_str->PSK.key.key_size, id_c, id_s, &mka_ks_str->cak_len);
}

void mka_key_str_add_CAK(mka_key_str *mka_key_str){
    mka_key_str->CAK = (struct bckey*) realloc (mka_key_str->CAK, (mka_key_str->cak_capacity + 1) * sizeof(struct bckey));
    ak_bckey_create_kuznechik( &mka_key_str->CAK[mka_key_str->cak_capacity]);
    ++mka_key_str->cak_capacity;
}

// используется при удалении интерфейса
void mka_key_str_rm_CAK(mka_key_str *mka_key_str, int pos){
    if (mka_key_str->cak_capacity > pos && pos >= 0) {
        --mka_key_str->cak_capacity;
        if (mka_key_str->cak_capacity >= pos) {
            ak_bckey_destroy( &mka_key_str->CAK[pos]);
            if(mka_key_str->cak_capacity > pos) memcpy(&mka_key_str->CAK[pos], &mka_key_str->CAK[pos + 1], sizeof(peer_list_unit) * (mka_key_str->cak_capacity - pos));
            mka_key_str->CAK = (struct bckey*) realloc(mka_key_str->CAK, sizeof(struct bckey) * (mka_key_str->cak_capacity));
        } else if(mka_key_str->cak_capacity == 0) {
            ak_bckey_destroy( &mka_key_str->CAK[0]);
            free(mka_key_str->CAK);
            mka_key_str->CAK = (struct bckey *)calloc(0,sizeof(struct bckey));
        }
    }
}

char *vec_pow(char *pow, int *len){
    return 0;
}


char *mka_key_str_func_blom(char *b, int b_len, int id_c, int id_s, int *len){
//    char key = (char*)calloc(256, sizeof(char));
    return 0;
}

int mka_key_str_destruct(mka_key_str *mka_ks_str){
    ak_bckey_destroy( &mka_ks_str->KEK );
    ak_bckey_destroy( &mka_ks_str->PSK );
    ak_bckey_destroy( &mka_ks_str->SAK );
    for(int i = 0; i < mka_ks_str->cak_capacity; ++i)
        ak_bckey_destroy( &mka_ks_str->CAK[i] );
    free(mka_ks_str->CAK);
    ak_libakrypt_destroy();
    return 0;
}


