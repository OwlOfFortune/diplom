#include "signal.h"
#include "Headers/mka_interface.h"

int *global;
void sig_g(){
    ++(*global);
}

static ak_uint8 keyAnnexA[32] = {
        0xef, 0xcd, 0xab, 0x89, 0x67, 0x45, 0x23, 0x01, 0x10, 0x32, 0x54, 0x76, 0x98, 0xba, 0xdc, 0xfe,
        0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x00, 0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x99, 0x88 };

int main() {
    char line[512], c[1];
    size_t len;
    int number_of_interfaces = 0, peer_id;
    mka_common_args commonArgs;
    printf("Enter peer id\n");
    scanf("%d", &peer_id);
    printf("Enter PSK\n");
    scanf("%s", line);
    if(mka_common_args_init(&commonArgs, peer_id, 1, 10, keyAnnexA, 32)) {
        mka_common_args_destruct(&commonArgs);
        return 1;
    }

    printf("Enter number of interfaces\n");
    scanf("%d", &number_of_interfaces);
    for(int i = 0; i < number_of_interfaces; ++i){
        printf("Enter interface name\n");
        scanf("%s", line);
        //enx00e04c3601ae enx00e04c3602d1 enp2s0
        if(mka_add_interface(&commonArgs, line, strlen(line))) {
            mka_common_args_destruct(&commonArgs);
            return 1;
        }
    }
    
    if(start_mka_connection(&commonArgs) == 0)
        sleep(1000);
    stop_mka_connection(&commonArgs);

    return 0;
}