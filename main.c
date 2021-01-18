#include "signal.h"
#include "Headers/mka_interface.h"

int global = 0;
void sig_g1(){
    ++(global);
}

int main() {
    char line[512], c[1];
    size_t len;
    int number_of_interfaces = 0, peer_id;
    mka_common_args commonArgs;

    // signal
    struct sigaction sig;
    sig.sa_handler = sig_g1;
    sigemptyset(&sig.sa_mask);
    sigprocmask(0,0,&sig.sa_mask);
    sig.sa_flags = 0;
    sigaction(SIGINT,&sig,0);

    printf("Enter peer id\n");
    scanf("%d", &peer_id);
    FILE *fp = fopen("../bkey.txt", "r"); // ключ Блома
    if(mka_common_args_init(&commonArgs, peer_id, 1, 10, fp)) {
        mka_common_args_destruct(&commonArgs);
        fclose(fp);
        return 1;
    }
    fclose(fp);


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
        while(global == 0) sleep(1);
    stop_mka_connection(&commonArgs);
    printf("exit\n");

    return 0;
}