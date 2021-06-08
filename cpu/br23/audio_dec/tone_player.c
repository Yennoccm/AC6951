#include "system/includes.h"
#include "media/includes.h"
#include "application/audio_dec_app.h"
#include "tone_player.h"
#include "audio_config.h"
#include "app_main.h"
#include "audio_dec.h"
#include "sine_make.h"
#if TCFG_USER_TWS_ENABLE
#include "bt_tws.h"
#endif/*TCFG_USER_TWS_ENABLE*/

//////////////////////////////////////////////////////////////////////////////


#define TWS_FUNC_ID_TONE2TWS		TWS_FUNC_ID('2', 'T', 'W', 'S')

#define TONE_TWS_CONFIRM_TIME		500

//////////////////////////////////////////////////////////////////////////////
static const char *const tone_index[] = {
    [IDEX_TONE_NUM_0] 			= SDFILE_RES_ROOT_PATH"tone/0.*",
    [IDEX_TONE_NUM_1] 			= SDFILE_RES_ROOT_PATH"tone/1.*",
    [IDEX_TONE_NUM_2] 			= SDFILE_RES_ROOT_PATH"tone/2.*",
    [IDEX_TONE_NUM_3] 			= SDFILE_RES_ROOT_PATH"tone/3.*",
    [IDEX_TONE_NUM_4] 			= SDFILE_RES_ROOT_PATH"tone/4.*",
    [IDEX_TONE_NUM_5] 			= SDFILE_RES_ROOT_PATH"tone/5.*",
    [IDEX_TONE_NUM_6] 			= SDFILE_RES_ROOT_PATH"tone/6.*",
    [IDEX_TONE_NUM_7] 			= SDFILE_RES_ROOT_PATH"tone/7.*",
    [IDEX_TONE_NUM_8] 			= SDFILE_RES_ROOT_PATH"tone/8.*",
    [IDEX_TONE_NUM_9] 			= SDFILE_RES_ROOT_PATH"tone/9.*",
    [IDEX_TONE_BT_MODE] 		= SDFILE_RES_ROOT_PATH"tone/bt.*",
    [IDEX_TONE_BT_CONN] 		= SDFILE_RES_ROOT_PATH"tone/bt_conn.*",
    [IDEX_TONE_BT_DISCONN] 		= SDFILE_RES_ROOT_PATH"tone/bt_dconn.*",
    [IDEX_TONE_TWS_CONN] 		= SDFILE_RES_ROOT_PATH"tone/tws_conn.*",
    [IDEX_TONE_TWS_DISCONN]		= SDFILE_RES_ROOT_PATH"tone/tws_dconn.*",
    [IDEX_TONE_LOW_POWER] 		= SDFILE_RES_ROOT_PATH"tone/low_power.*",
    [IDEX_TONE_POWER_OFF] 		= SDFILE_RES_ROOT_PATH"tone/power_off.*",
    [IDEX_TONE_POWER_ON] 		= SDFILE_RES_ROOT_PATH"tone/power_on.*",
    [IDEX_TONE_RING] 			= SDFILE_RES_ROOT_PATH"tone/ring.*",
    [IDEX_TONE_MAX_VOL] 		= SDFILE_RES_ROOT_PATH"tone/vol_max.*",
    [IDEX_TONE_NORMAL] 			= TONE_NORMAL,
#if (TCFG_APP_MUSIC_EN)
    [IDEX_TONE_MUSIC] 			= SDFILE_RES_ROOT_PATH"tone/music.*",
#endif
#if (TCFG_APP_LINEIN_EN)
    [IDEX_TONE_LINEIN] 			= SDFILE_RES_ROOT_PATH"tone/linein.*",
#endif
#if (TCFG_APP_FM_EN)
    [IDEX_TONE_FM] 				= SDFILE_RES_ROOT_PATH"tone/fm.*",
#endif
#if (TCFG_APP_PC_EN)
    [IDEX_TONE_PC] 				= SDFILE_RES_ROOT_PATH"tone/pc.*",
#endif
#if (TCFG_APP_RTC_EN)
    [IDEX_TONE_RTC] 			= SDFILE_RES_ROOT_PATH"tone/rtc.*",
#endif
#if (TCFG_APP_RECORD_EN)
    [IDEX_TONE_RECORD] 			= SDFILE_RES_ROOT_PATH"tone/record.*",
#endif
} ;

#if TCFG_TONE2TWS_ENABLE
// ����ת������ʾ��
static const char *const tone2tws_index[] = {
    TONE_BT_MODE,
    TONE_MUSIC,
    TONE_LINEIN,
    TONE_FM,
    TONE_PC,
    TONE_RTC,
    TONE_RECORD,
    TONE_RES_ROOT_PATH"tone/sd.*",
    TONE_RES_ROOT_PATH"tone/udisk.*",
} ;
#endif

/*
 * ��������:
 * freq : ʵ��Ƶ�� * 512
 * points : ���Ҳ�����
 * win : ���Ҵ�
 * decay : ˥��ϵ��(�ٷֱ�), ���Ҵ�ģʽ��ΪƵ�����ã�Ƶ��*512
 *
 */
static const struct sin_param sine_16k_normal[] = {
    /*{0, 1000, 0, 100},*/
    {200 << 9, 4000, 0, 100},
};

#if CONFIG_USE_DEFAULT_SINE
static const struct sin_param sine_tws_disconnect_16k[] = {
    /*
    {390 << 9, 4026, SINE_TOTAL_VOLUME / 4026},
    {262 << 9, 8000, SINE_TOTAL_VOLUME / 8000},
    */
    {262 << 9, 4026, 0, 100},
    {390 << 9, 8000, 0, 100},
};

static const struct sin_param sine_tws_connect_16k[] = {
    /*
    {262 << 9, 4026, SINE_TOTAL_VOLUME / 4026},
    {390 << 9, 8000, SINE_TOTAL_VOLUME / 8000},
    */
    {262.298 * 512, 8358, 0, 100},
};

static const struct sin_param sine_low_power[] = {
    {848 << 9, 3613, 0, 50},
    {639 << 9, 3623, 0, 50},
};

static const struct sin_param sine_ring[] = {
    {450 << 9, 24960, 1, 16.667 * 512},
    {0, 16000, 0, 100},
};

static const struct sin_param sine_tws_max_volume[] = {
    {210 << 9, 2539, 0, 100},
    {260 << 9, 7619, 0, 100},
    {400 << 9, 2539, 0, 100},
};
#endif

//////////////////////////////////////////////////////////////////////////////

struct tone_dec_handle *tone_dec = NULL;


//////////////////////////////////////////////////////////////////////////////
/*----------------------------------------------------------------------------*/
/**@brief    �����кŻ�ȡsine����
   @param    id: ���
   @param    *num: �������
   @return   sine����
   @note
*/
/*----------------------------------------------------------------------------*/
static const struct sin_param *get_sine_param_data(u8 id, u8 *num)
{
    const struct sin_param *param_data;

    switch (id) {
    case SINE_WTONE_NORAML:
        param_data = sine_16k_normal;
        *num = ARRAY_SIZE(sine_16k_normal);
        break;
#if CONFIG_USE_DEFAULT_SINE
    case SINE_WTONE_TWS_CONNECT:
        param_data = sine_tws_connect_16k;
        *num = ARRAY_SIZE(sine_tws_connect_16k);
        break;
    case SINE_WTONE_TWS_DISCONNECT:
        param_data = sine_tws_disconnect_16k;
        *num = ARRAY_SIZE(sine_tws_disconnect_16k);
        break;
    case SINE_WTONE_LOW_POWER:
        param_data = sine_low_power;
        *num = ARRAY_SIZE(sine_low_power);
        break;
    case SINE_WTONE_RING:
        param_data = sine_ring;
        *num = ARRAY_SIZE(sine_ring);
        break;
    case SINE_WTONE_MAX_VOLUME:
        param_data = sine_tws_max_volume;
        *num = ARRAY_SIZE(sine_tws_max_volume);
        break;
#endif
    default:
        return NULL;
    }

    return param_data;
}

/*----------------------------------------------------------------------------*/
/**@brief    ��ʾ�����Ż��������������¼��ص���
   @param    **list: �ļ���
   @param    follow: ������ʾ��������Ϻ�����Ų���
   @param    preemption: ��ϱ��
   @param    *evt_handler: �¼��ص��ӿ�
   @param    *evt_priv: �¼��ص�˽�о��
   @return   0: �ɹ�
   @note
*/
/*----------------------------------------------------------------------------*/
static int tone_play_open_with_callback_base(const char **list, u8 follow, u8 preemption, void (*evt_handler)(void *priv, int flag), void *evt_priv, int sync_confirm_time)
{
    if (follow && tone_dec) {
        struct tone_dec_list_handle *dec_list = tone_dec_list_create(tone_dec, list, preemption, evt_handler, evt_priv, NULL, NULL);
        int ret = tone_dec_list_add_play(tone_dec, dec_list);
        if (ret == false) {
            tone_play_stop();
            return -1;
        }
        return 0;
    }

    tone_dec_stop(&tone_dec, 1, TONE_DEC_STOP_BY_OTHER_PLAY);

    tone_dec = tone_dec_create();
    if (tone_dec == NULL) {
        return -1;
    }
    tone_dec_set_sin_get_hdl(tone_dec, get_sine_param_data);

    struct tone_dec_list_handle *dec_list = tone_dec_list_create(tone_dec, list, preemption, evt_handler, evt_priv, NULL, NULL);
    if (dec_list) {
        dec_list->sync_confirm_time = sync_confirm_time;
    }
    int ret = tone_dec_list_add_play(tone_dec, dec_list);
    if (ret == false) {
        tone_play_stop();
        return -1;
    }
    return 0;
}

#if TCFG_TONE2TWS_ENABLE
#include "app_task.h"

static u32 tone2tws_dat = 0;

/*----------------------------------------------------------------------------*/
/**@brief    tws��ʾ�����ݴ���
   @param    dat: ����
   @return
   @note     �������м��д���tws��Ϣ
*/
/*----------------------------------------------------------------------------*/
static void tone2tws_rx_play(u32 dat)
{
    int idex = dat - 1;
    if (idex >= ARRAY_SIZE(tone2tws_index)) {
        return ;
    }
    /* y_printf("tone2tws_rx_play name:%d \n", tone2tws_index[idex]); */
    char *single_file[2] = {NULL};
    // is file name
    single_file[0] = (char *)tone2tws_index[idex];
    single_file[1] = NULL;
    tone_play_open_with_callback_base(single_file, 0, 1, NULL, NULL, TONE_TWS_CONFIRM_TIME);
}

void tone2tws_bt_task_start(u8 tone_play)
{
    if (tone2tws_dat == 0) {
        return ;
    }
    if (tone_play) {
        // ����������ʾ������������ʾ��
        tone2tws_dat = 0;
    } else {
        // ���Ÿ���ʾ��
        tone2tws_rx_play(tone2tws_dat);
        tone2tws_dat = 0;
    }
}

static void tone2tws_rx_callback_func(u32 dat)
{
    /* printf("tone2tws_rx_callback_func\n"); */
    if (!dat) {
        return ;
    }
#if TCFG_DEC2TWS_ENABLE
    if (tone2tws_dat) { // �ͷžɵ�
        tone2tws_dat = 0;
    }
    if (APP_BT_TASK != app_get_curr_task()) {
        r_printf("tone2tws task:%d ", app_get_curr_task());
        // ����ʾ��һ����������ģʽ���ţ�������������⡣
        // �ȷ�������ģʽ�ٲ���
        tone2tws_dat = dat;
        return ;
    }
#endif

    tone2tws_rx_play(dat);
}

/*----------------------------------------------------------------------------*/
/**@brief    ��ʾ��tws˽����Ϣ����
   @param    *data: ����
   @param    len: ���ݳ���
   @param    rx: 1-���ܶ�
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
static void tone2tws_tws_rx_data(void *data, u16 len, bool rx)
{
    /* printf("tone2tws_tws_rx_data, l:%d, rx:%d \n", len, rx); */
    if (!rx) {
        return;
    }
    /* printf("data rx \n"); */
    /* printf_buf(data, len); */

    u32 dat;
    memcpy(&dat, data, 4);
    /* y_printf("rx dat:%d \n", dat); */

    // �ú��������ж�������ã�ʵ�ʴ������task����
    int argv[3];
    argv[0] = (int)tone2tws_rx_callback_func;
    argv[1] = 1;
    argv[2] = (int)dat;
    int ret = os_taskq_post_type("app_core", Q_CALLBACK, 3, argv);
    /* printf("put taskq, ret:%d, len:%d \n", ret, len); */
    if (ret) {
        log_e("taskq post err \n");
    }
}

REGISTER_TWS_FUNC_STUB(tws_tone2tws_rx) = {
    .func_id = TWS_FUNC_ID_TONE2TWS,
    .func = tone2tws_tws_rx_data,
};

#endif

/*----------------------------------------------------------------------------*/
/**@brief    ��ʾ�����ţ������¼��ص���
   @param    **list: �ļ���
   @param    follow: ������ʾ��������Ϻ�����Ų���
   @param    preemption: ��ϱ��
   @param    *evt_handler: �¼��ص��ӿ�
   @param    *evt_priv: �¼��ص�˽�о��
   @return   0: �ɹ�
   @note
*/
/*----------------------------------------------------------------------------*/
int tone_play_open_with_callback(const char **list, u8 follow, u8 preemption, void (*evt_handler)(void *priv, int flag), void *evt_priv)
{
    int sync_confirm_time = 0;
#if TCFG_TONE2TWS_ENABLE
    int state = tws_api_get_tws_state();
    if (state & TWS_STA_SIBLING_CONNECTED) {
        // ��tws���ӣ�����Ƿ���Ҫ����ʾ�����͸��Է�
        char *name = list[0];
        for (int i = 0; i < ARRAY_SIZE(tone2tws_index); i++) {
            if (IS_REPEAT_BEGIN(name) || IS_REPEAT_END(name)) {
                break;
            }
            if (IS_DEFAULT_SINE(name) || IS_DEFAULT_SINE(tone2tws_index[i])) {
                if ((u32)name == (u32)tone2tws_index[i]) {
                    // is sine idx
                    u32 dat = i + 1;
                    tws_api_send_data_to_sibling(&dat, 4, TWS_FUNC_ID_TONE2TWS);
                    sync_confirm_time = TONE_TWS_CONFIRM_TIME;
                    break;
                }
            } else if (!strcmp(tone2tws_index[i], name)) {
                // is file name
                u32 dat = i + 1;
                tws_api_send_data_to_sibling(&dat, 4, TWS_FUNC_ID_TONE2TWS);
                sync_confirm_time = TONE_TWS_CONFIRM_TIME;
                break;
            }
        }
    }
#endif
    return tone_play_open_with_callback_base(list, follow, preemption, evt_handler, evt_priv, sync_confirm_time);
}

/*----------------------------------------------------------------------------*/
/**@brief    ���ļ�������ʾ��
   @param    *name: �ļ���
   @param    preemption: ��ϱ��
   @return   0: �ɹ�
   @note
*/
/*----------------------------------------------------------------------------*/
int tone_play(const char *name, u8 preemption)
{
    char *single_file[2] = {NULL};
    single_file[0] = (char *)name;
    single_file[1] = NULL;
    return tone_play_open_with_callback(single_file, 0, preemption, NULL, NULL);
}

/*----------------------------------------------------------------------------*/
/**@brief    ���ļ�������ʾ��
   @param    *name: �ļ���
   @param    preemption: ��ϱ��
   @return   0: �ɹ�
   @note
*/
/*----------------------------------------------------------------------------*/
int tone_play_by_path(const char *name, u8 preemption)
{
    return tone_play(name, preemption);
}

/*----------------------------------------------------------------------------*/
/**@brief    �����кŲ�����ʾ��
   @param    index: ���к�
   @param    preemption: ��ϱ��
   @param    *evt_handler: �¼��ص��ӿ�
   @param    *evt_priv: �¼��ص�˽�о��
   @return   0: �ɹ�
   @note
*/
/*----------------------------------------------------------------------------*/
int tone_play_index_with_callback(u8 index, u8 preemption, void (*evt_handler)(void *priv), void *evt_priv)
{
    if (index >= IDEX_TONE_NONE) {
        return -1;
    }
    char *single_file[2] = {NULL};
    single_file[0] = (char *)tone_index[index];
    single_file[1] = NULL;
    return tone_play_open_with_callback(single_file, 0, preemption, evt_handler, evt_priv);
}

/*----------------------------------------------------------------------------*/
/**@brief    �����кŲ�����ʾ���������¼��ص���
   @param    index: ���к�
   @param    preemption: ��ϱ��
   @return   0: �ɹ�
   @note
*/
/*----------------------------------------------------------------------------*/
int tone_play_index(u8 index, u8 preemption)
{
    return tone_play_index_with_callback(index, preemption, NULL, NULL);
}

/*----------------------------------------------------------------------------*/
/**@brief    ���ļ����б�����ʾ��
   @param    **list: �ļ����б�
   @param    preemption: ��ϱ��
   @return   0: �ɹ�
   @note
*/
/*----------------------------------------------------------------------------*/
int tone_file_list_play(const char **list, u8 preemption)
{
    return tone_play_open_with_callback(list, 0, preemption, NULL, NULL);
}

/*----------------------------------------------------------------------------*/
/**@brief    �����кŲ�����ʾ��
   @param    index: ���к�
   @param    preemption: ��ϱ��
   @param    *evt_handler: �¼��ص��ӿ�
   @param    *evt_priv: �¼��ص�˽�о��
   @return   0: �ɹ�
   @note
*/
/*----------------------------------------------------------------------------*/
int tone_play_with_callback_by_index(u8 index,
                                     u8 preemption,
                                     void (*evt_handler)(void *priv, int flag),
                                     void *evt_priv)
{
    if (index >= IDEX_TONE_NONE) {
        return -1;
    }
    char *single_file[2] = {NULL};
    single_file[0] = (char *)tone_index[index];
    single_file[1] = NULL;
    return tone_play_open_with_callback(single_file, 0, preemption, evt_handler, evt_priv);
}

/*----------------------------------------------------------------------------*/
/**@brief    �����ֲ�����ʾ��
   @param    *name: �ļ���
   @param    preemption: ��ϱ��
   @param    *evt_handler: �¼��ص��ӿ�
   @param    *evt_priv: �¼��ص�˽�о��
   @return   0: �ɹ�
   @note
*/
/*----------------------------------------------------------------------------*/
int tone_play_with_callback_by_name(char *name,
                                    u8 preemption,
                                    void (*evt_handler)(void *priv, int flag),
                                    void *evt_priv)
{
    char *single_file[2] = {NULL};
    single_file[0] = name;
    single_file[1] = NULL;
    return tone_play_open_with_callback(single_file, 0, preemption, evt_handler, evt_priv);
}

/*----------------------------------------------------------------------------*/
/**@brief    ���б�����ʾ��
   @param    **list: �ļ��б�
   @param    preemption: ��ϱ��
   @param    *evt_handler: �¼��ص��ӿ�
   @param    *evt_priv: �¼��ص�˽�о��
   @return   0: �ɹ�
   @note
*/
/*----------------------------------------------------------------------------*/
int tone_play_with_callback_by_list(const char **list,
                                    u8 preemption,
                                    void (*evt_handler)(void *priv, int flag),
                                    void *evt_priv)
{
    return tone_play_open_with_callback(list, 0, preemption, evt_handler, evt_priv);
}


/*----------------------------------------------------------------------------*/
/**@brief    ָֹͣ�������ʾ��
   @param    index: ���
   @return   0: �ɹ�
   @note
*/
/*----------------------------------------------------------------------------*/
int tone_play_stop_by_index(u8 index)
{
    if (index >= IDEX_TONE_NONE) {
        return -1;
    }
    tone_dec_stop_spec_file(&tone_dec, tone_index[index], 1, TONE_DEC_STOP_BY_OTHER_PLAY);
    return 0;
}

/*----------------------------------------------------------------------------*/
/**@brief    ָֹͣ�������ʾ��
   @param    path: ���
   @return   0: �ɹ�
   @note
*/
/*----------------------------------------------------------------------------*/
int tone_play_stop_by_path(char *path)
{
    if (path == NULL) {
        return -1;
    }
    tone_dec_stop_spec_file(&tone_dec, path, 1, TONE_DEC_STOP_BY_OTHER_PLAY);
    return 0;
}

/*----------------------------------------------------------------------------*/
/**@brief    ֹͣ��ʾ������
   @param
   @return   0: �ɹ�
   @note
*/
/*----------------------------------------------------------------------------*/
int tone_play_stop(void)
{
    tone_dec_stop(&tone_dec, 1, TONE_DEC_STOP_BY_OTHER_PLAY);
    return 0;
}

/*----------------------------------------------------------------------------*/
/**@brief    ��ȡ��ʾ������״̬
   @param
   @return   TONE_START: ���ڲ���
   @return   TONE_STOP: ���ڲ���
   @note
*/
/*----------------------------------------------------------------------------*/
int tone_get_status(void)
{
    if (tone_dec && tone_dec->cur_list) {
        return TONE_START;
    }
    return TONE_STOP;
}

/*----------------------------------------------------------------------------*/
/**@brief    ��ȡ��ʾ�����Ž�����״̬
   @param
   @return   TONE_START: ���ڽ���
   @return   TONE_STOP: ���ڽ���
   @note
*/
/*----------------------------------------------------------------------------*/
int tone_get_dec_status()
{
    if (!tone_dec) {
        return TONE_STOP;
    }
    if (tone_dec->dec_sin && (tone_dec->dec_sin->dec->decoder.state != DEC_STA_WAIT_STOP)) {
        return TONE_START;
    }
    if (tone_dec->dec_file && (tone_dec->dec_file->dec->decoder.state != DEC_STA_WAIT_STOP)) {
        return TONE_START;
    }
    return TONE_STOP;
}


/*----------------------------------------------------------------------------*/
/**@brief    ��ʱ�ȴ���ʾ�����Ž������
   @param    timeout_ms: ��ʱ�ȴ�
   @return   TONE_START: ���ڽ���
   @return   TONE_STOP: ���ڽ���
   @note
*/
/*----------------------------------------------------------------------------*/
int tone_dec_wait_stop(u32 timeout_ms)
{
    u32 to_cnt = 0;
    while (tone_get_dec_status()) {
        /* putchar('t'); */
        os_time_dly(1);
        if (timeout_ms) {
            to_cnt += 10;
            if (to_cnt >= timeout_ms) {
                break;
            }
        }
    }
    return tone_get_dec_status();
}

/*----------------------------------------------------------------------------*/
/**@brief    ��ʾ��idle�ж�
   @param
   @return   1: idle
   @return   0: busy
   @note
*/
/*----------------------------------------------------------------------------*/
static u8 tone_dec_idle_query()
{
    if (tone_get_status() == TONE_START) {
        return 0;
    }
    return 1;
}
REGISTER_LP_TARGET(tone_dec_lp_target) = {
    .name = "tone_dec",
    .is_idle = tone_dec_idle_query,
};




