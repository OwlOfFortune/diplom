#ifndef DIPLOMA_HELLO_TIME_H
#define DIPLOMA_HELLO_TIME_H

#include "common_args.h"


/* ----------------------------------------------------------------------------------------------- */
/*!  Структура заголовка приветственных сообщений, необходимы для поддержания соединения */
/* ----------------------------------------------------------------------------------------------- */
typedef struct mka_htmsg {
    int msg_num;                    /// Номер приветственного сообщения
    int potential_peer;             /// Номер потенциального узла (для установки соединения):
                                    ///       -1 - первые сообщения
                                    ///        0 - отправка уже живому узлу
                                    ///        i - отправку потенциальному i узлу
    int random_live_peer_str;       /// Флаг, свидетельствующий о том, что строка с "живыми" узлами - псевдо случайная
    int live_peer_str_len;          /// Длина строки с "живыми" узлами
    int potential_peer_str_len;     /// Длина строки с потенциальными узлами
    int other_len;                  /// Длина дополнительной строки
} mka_htmsg;

/* ----------------------------------------------------------------------------------------------- */
/*! Ф-ия, передаваемая в поток отправки приветственных сообщений

    @param args - mka_common_args - структура с общими параметрами

*/
/* ----------------------------------------------------------------------------------------------- */
void *mka_htmsg_send_thread_func(void *args);

/* ----------------------------------------------------------------------------------------------- */
/*! Ф-ия отправки приветственных сообщений всем узлам. Если это первые сообщения, то отправляются
 *  пустые, незашифрованные сообщения с potential_peer = -1

    @param commonArgs   - структура с общими параметрами
    @param encr         - 0 - не шифруются, 1 - зашифровываются
    @param остальные параметры передаются для заполнения заголовка

*/
/* ----------------------------------------------------------------------------------------------- */
void mka_htmsg_send_data_to_all(mka_common_args *commonArgs, int encr, int potential_peer, int random_live_peer_str,
                                char *live_peer_str, int live_peer_str_len, char *potential_peer_list, int ppl_len,
                                char *other, int other_len);
/* ----------------------------------------------------------------------------------------------- */
/*! Ф-ия отправки приветственного сообщения одному узлу

    @param commonArgs       - структура с общими параметрами
    @param interface_num    - номер интерфейса для отправки
    @param encr             - 0 - не шифруются, 1 - зашифровываются
    @param остальные параметры передаются для заполнения заголовка

    @return 0 - успех, 1 - ошибка
*/
/* ----------------------------------------------------------------------------------------------- */
int mka_htmsg_send(mka_common_args *commonArgs, int interface_num, int encr, int potential_peer,
                   int random_live_peer_str, char *live_peer_str, int live_peer_str_len,
                   char *potential_peer_list, int ppl_len, char *other, int other_len);

#endif //DIPLOMA_HELLO_TIME_H