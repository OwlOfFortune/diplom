#include "../Headers/other_functions.h"

void print_string(char *str, int str_len){
    for (int i = 0; i < str_len; i++)
        printf("%02x", str[i] & 0xff);
    fputc('\n', stdout);
    fflush(stdout);
}

char *generate_rnd_string(int rand_len){
    if( !ak_libakrypt_create( NULL )) return NULL;
    struct random generator;
    ak_uint8 seed[4] = { 0x13, 0xAE, 0x4F, 0x0E }; /* константа */
    ak_uint8 *buffer = (ak_uint8 *) calloc(rand_len, sizeof (ak_uint8));
    /* создаем генератор */
    ak_random_create_lcg( &generator );

    /* инициализируем константным значением */
//    if( generator.randomize_ptr != NULL )
//        ak_random_randomize( &generator, &seed, sizeof( seed ));

    ak_random_ptr( &generator, buffer, rand_len );
    return buffer;
}

