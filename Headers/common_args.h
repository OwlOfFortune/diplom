#ifndef DIPLOMA_COMMON_ARGS_H
#define DIPLOMA_COMMON_ARGS_H

#include "mka_transport.h"
#include "other_functions.h"
#include <unistd.h>

/* ----------------------------------------------------------------------------------------------- */
/*!  Структура, передаваемая в потоки, содержит всю необходимую информацию */
/* ----------------------------------------------------------------------------------------------- */
typedef struct mka_common_args {
    connection_params_list connectionParamsList;         /// Структура, содержащая данные о соединениях
    mka_info mka_i;                                      /// Структура, содержащая данные о текущем узле и ключевую информацию
    int hello_time_timeout;                              /// Интервал в сек для отправки приветственных сообщений
    int need_to_send_HTM;                                /// Флаг, свидетельствующий о необходимости послать helloTimeMsg внепланово
    int need_to_refresh_SAK;                             /// Флаг, свидетельствующий о необходимости пересчитать SAK
    int need_to_send_SAK_to_others__received_t;          /// Номер интерфейса потока, с которого узел получил SAK
    int need_to_send_SAK_to_others__peer_id;             /// Номер узла потока, с которого узел получил SAK
    int reply_SAK__received_t;                           /// Номер интерфейса потока, который хочет заново получить SAK
    int reply_SAK__peer_id;                              /// Номер интерфейса потока, который хочет заново получить SAK
    int stop;                                            /// Флаг для остановки потоков
    pthread_t tid_cleaner, tid_send, tid_ks_server;      /// Дескрипторы потоков (очистка массива узлов, отправки helloTimeMsg, КС)
} mka_common_args;

/* ----------------------------------------------------------------------------------------------- */
/*! Инициализация структуры

    @param commonArgs   - структура с общими параметрами
    @param id           - номер текущего узла
    @param KS_priority  - приоритет КС текущего узла
    @param list_timeout - время жизни узлов
    @param fp           - дескриптор файла с ключем Блома

    @return 0 - успех, 1 - ошибка
*/
/* ----------------------------------------------------------------------------------------------- */
int mka_common_args_init(mka_common_args *commonArgs, int id, char KS_priority, int list_timeout, FILE *fp);

/* ----------------------------------------------------------------------------------------------- */
/*! Очистка структуры

    @param commonArgs - структура с общими параметрами

    @return 0 - успех, 1 - ошибка
*/
/* ----------------------------------------------------------------------------------------------- */
int mka_common_args_destruct(mka_common_args *commonArgs);


#endif //DIPLOMA_COMMON_ARGS_H
