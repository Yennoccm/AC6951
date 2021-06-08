#include "system/includes.h"
#include "media/includes.h"
#include "tone_player.h"
#include "audio_config.h"
#include "app_main.h"
#include "clock_cfg.h"
#include "audio_dec.h"

//////////////////////////////////////////////////////////////////////////////

static FILE *record_file = NULL;

//////////////////////////////////////////////////////////////////////////////

extern int last_enc_file_path_get(char path[64]);

//////////////////////////////////////////////////////////////////////////////
/*----------------------------------------------------------------------------*/
/**@brief    �ر�¼���ļ�����
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void record_file_close(void)
{
    file_dec_close();
    if (record_file) {
        fclose(record_file);
        record_file = NULL;
    }
}

/*----------------------------------------------------------------------------*/
/**@brief    ¼�������¼��ص�
   @param    *priv:  ˽�о��
   @param    argc: ��������
   @param    *argv: ����
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
static void record_file_play_evt_handler(void *priv, int argc, int *argv)
{
    u8 event = (u8)argv[0];
    /* printf("fun = %s\n", __FUNCTION__); */
    if (event == AUDIO_DEC_EVENT_END) {
        record_file_close();
    }
}

/*----------------------------------------------------------------------------*/
/**@brief    ¼���ļ�����
   @param
   @return   0:�ɹ�
   @note
*/
/*----------------------------------------------------------------------------*/
int record_file_play(void)
{
    int ret;
    record_file_close();

    char path[64] = {0};
    ret = last_enc_file_path_get(path);
    if (ret) {
        return -1;
    }
    record_file = fopen(path, "r");
    if (!record_file) {
        return -1;
    }
    ret = file_dec_create(NULL, record_file_play_evt_handler);
    if (ret) {
        return -1;
    }
    ret = file_dec_open(record_file, NULL);
    return ret;
}

/*----------------------------------------------------------------------------*/
/**@brief    ����·������¼���ļ�
   @param
   @return   0:�ɹ�
   @note	 ����������� �û�����ֱ��ָ��¼��·������,�̷���·����ֱ��ָ��
*/
/*----------------------------------------------------------------------------*/
int record_file_play_by_path(char *path)
{
    int ret;
    record_file_close();

    record_file = fopen(path, "r");
    if (!record_file) {
        return -1;
    }
    ret = file_dec_create(NULL, record_file_play_evt_handler);
    if (ret) {
        return -1;
    }
    ret = file_dec_open(record_file, NULL);
    return ret;
}


/*----------------------------------------------------------------------------*/
/**@brief    ��ȡ¼���������¼�
   @param
   @return   ��ʱ��
   @note
*/
/*----------------------------------------------------------------------------*/
int record_file_get_total_time(void)
{
    if (!record_file) {
        return 0;
    }
    return file_dec_get_total_time();
}

/*----------------------------------------------------------------------------*/
/**@brief    ��ȡ¼�����ŵ�ǰʱ��
   @param
   @return   ��ǰʱ��
   @note
*/
/*----------------------------------------------------------------------------*/
int record_file_dec_get_cur_time(void)
{
    if (!record_file) {
        return 0;
    }
    return file_dec_get_cur_time();
}



