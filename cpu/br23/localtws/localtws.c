/***********************************Jieli tech************************************************
  File : localtws.c
  By   : Huxi
  brief:
  Email: huxi@zh-jieli.com
  date : 2020-07
********************************************************************************************/

#include "asm/includes.h"
#include "media/includes.h"
#include "system/includes.h"
#include "classic/tws_api.h"
#include "classic/tws_local_media_sync.h"
#include "localtws/localtws.h"
#include "clock_cfg.h"
#include "app_config.h"
#include "audio_config.h"
#include "audio_dec.h"
#include "audio_enc.h"
#include "app_task.h"
#include "bt/bt_tws.h"
#include "bt/bt.h"

#if TCFG_DEC2TWS_ENABLE

#define LOCALTWS_LOG_ENABLE
#ifdef LOCALTWS_LOG_ENABLE
#define LOCALTWS_LOG		log_i //y_printf
#define LOCALTWS_LOG_CHAR	putchar
#else
#define LOCALTWS_LOG(...)
#define LOCALTWS_LOG_CHAR(...)
#endif


//////////////////////////////////////////////////////////////////////////////

#define LOCALTWS_MEDIA_ALLOC_EN		1	// ʹ�ÿռ����뷽ʽ
#define LOCALTWS_CHANGE_USE_BT_CMD	1	// ʹ�����������localtws�仯��Ϣ

const u16 LOCALTWS_MEDIA_BUF_LEN = (12 * 1024);	// localtws����ռ�
const u16 LOCALTWS_MEDIA_BUF_LIMIT_LEN = (LOCALTWS_MEDIA_BUF_LEN * 7 / 10); // ��ʼ�����ۼƴﵽ��ֵ�ŷ���
const u16 LOCALTWS_MEDIA_TO_MS = 6000;	// tws�೤ʱ��û�յ�������Ϊ�ǳ�ʱ

#ifdef CONFIG_MIXER_CYCLIC
#if MIXER_AUDIO_DIRECT_OUT
const u16 LOCALTWS_MIXER_BUF_LEN = (8);	// mixer�ռ��С����ֱͨ
#else /*MIXER_AUDIO_DIRECT_OUT*/
const u16 LOCALTWS_MIXER_BUF_LEN = (256 * 2 * 2);	// mixer�ռ��С��û��ֱͨ
#endif /*MIXER_AUDIO_DIRECT_OUT*/
#else /*CONFIG_MIXER_CYCLIC*/
const u16 LOCALTWS_MIXER_BUF_LEN = 8;	// mixer�ռ��С��ֻ��һ��ͨ��ʱֱ���������������buf
#endif /*CONFIG_MIXER_CYCLIC*/

//////////////////////////////////////////////////////////////////////////////
//
#if (LOCALTWS_MEDIA_ALLOC_EN == 0)
static u8 localtws_media_buf[LOCALTWS_MEDIA_BUF_LEN] sec(.dec2tws_mem);
#endif

struct localtws_globle_hdl g_localtws = {0};
static u8 tws_background_connected_flag = 0;

//////////////////////////////////////////////////////////////////////////////
//
extern struct audio_encoder_task *encode_task;
extern struct audio_decoder_task decode_task;

extern void local_tws_sync_no_check_data_buf(u8 no_check);
extern void tws_api_local_media_trans_clear_no_ready(void);
extern u8 is_tws_active_device(void);

extern int tone_get_status();

extern void localtws_decoder_resume_pre(void);

///////////////////////////////////////////////////////////////////////////////////

void set_tws_background_connected_flag(u8 flag)
{
    tws_background_connected_flag = flag;
}

u8 get_tws_background_connected_flag()
{
    return tws_background_connected_flag;
}

/*----------------------------------------------------------------------------*/
/**@brief    tws���ݴ���
   @param    *data: ����
   @param    len: ���ݳ���
   @param    rx: ����
   @return   0: �ɹ�
   @note     �������ض���
*/
/*----------------------------------------------------------------------------*/
int tws_api_local_media_sync_rx_handler_notify(u8 *data, int len, u8 rx)
{
    /* LOCALTWS_LOG_CHAR('r'); */
    if (g_localtws.drop_frame_start) {
        /* LOCALTWS_LOG("localtws_wait_drop_over\n"); */
        LOCALTWS_LOG_CHAR('O');
        LOCALTWS_LOG_CHAR('V');
        LOCALTWS_LOG_CHAR(' ');
    }
    return localtws_tws_rx_handler_notify(data, len, rx);
}

/*----------------------------------------------------------------------------*/
/**@brief    localtws�¼�����
   @param    event_type: �¼�
   @param    value: �¼�����
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
static void localtws_dec_event(u8 event_type, u32 value)
{
    struct sys_event event;
    event.type = SYS_BT_EVENT;
    event.arg = (void *)SYS_BT_EVENT_FROM_TWS;
    event.u.bt.event = event_type;
    event.u.bt.value = value;
    sys_event_notify(&event);
}

/*----------------------------------------------------------------------------*/
/**@brief    localtws�¼��ص�
   @param    event: �¼�
   @param    *pram: �¼�����
   @return   0: �ɹ�
   @note
*/
/*----------------------------------------------------------------------------*/
static int localtws_globle_event_cb(int event, int *pram)
{
    switch (event) {
#if (!LOCALTWS_CHANGE_USE_BT_CMD)
    case LOCALTWS_GLOBLE_EVENT_MEDIA_START:
        localtws_dec_event(TWS_EVENT_LOCAL_MEDIA_START, pram[0]);
        break;
    case LOCALTWS_GLOBLE_EVENT_MEDIA_STOP:
        localtws_dec_event(TWS_EVENT_LOCAL_MEDIA_STOP, 0);
        break;
#endif
    case LOCALTWS_GLOBLE_EVENT_MEDIA_TIMEOUT:
        localtws_dec_event(TWS_EVENT_LOCAL_MEDIA_STOP, 0);
        break;
    }
    return 0;
}

/*----------------------------------------------------------------------------*/
/**@brief    localtws����豸
   @param
   @return   true: ��豸
   @return   false: �ǻ�豸
   @note
*/
/*----------------------------------------------------------------------------*/
static int localtws_globle_check_active(void)
{
    if (app_check_curr_task(APP_BT_TASK)) {
        // ����ģʽһ����unactive�豸
        return false;
    }
    if (is_tws_active_device()) {
        return true;
    }
    return false;
}

/*----------------------------------------------------------------------------*/
/**@brief    localtws�жϴ����ж�
   @param
   @return   true: �жϴ���
   @return   false: ��������
   @note
*/
/*----------------------------------------------------------------------------*/
static int localtws_globle_check_frame_drop(void)
{
    if (tone_get_status() || bt_media_is_running()) {
        return true;
    }
    return false;
}

//////////////////////////////////////////////////////////////////////////////
//
/*----------------------------------------------------------------------------*/
/**@brief    localtwsȫ�ֳ�ʼ��
   @param
   @return   0: �ɹ�
   @note
*/
/*----------------------------------------------------------------------------*/
static int localtws_globle_init(void)
{
    os_mutex_create(&g_localtws.mutex);
#if LOCALTWS_MEDIA_ALLOC_EN
    g_localtws.media_buf_malloc = 1;
#else
    g_localtws.media_buf = localtws_media_buf;
#endif
    localtws_globle_set_event_cb(localtws_globle_event_cb);
    localtws_globle_set_check_active(localtws_globle_check_active);
    localtws_globle_set_resume(localtws_dec_resume);
    localtws_globle_set_data_abandon(localtws_media_dat_abandon);
    localtws_globle_set_frame_drop(localtws_globle_check_frame_drop);
    localtws_media_enable();
    return 0;
}
__initcall(localtws_globle_init);


/*----------------------------------------------------------------------------*/
/**@brief    localtws����Ƿ�ʹ��
   @param
   @return   true: ʹ��
   @return   false: ��ʹ��
   @note
*/
/*----------------------------------------------------------------------------*/
int localtws_check_enable(void)
{
    int state = tws_api_get_tws_state();
    if ((state & TWS_STA_SIBLING_CONNECTED)) {
        return true;
    }
    return false;
}

/*----------------------------------------------------------------------------*/
/**@brief    localtws�����¼�����
   @param    *evt: �����¼�
   @return   0: �ɹ�
   @note
*/
/*----------------------------------------------------------------------------*/
#define __this 	(&app_bt_hdl)
int localtws_bt_event_deal(struct bt_event *evt)
{
    /* int state = tws_api_get_tws_state(); */
    /* LOCALTWS_LOG(">>>>>>>>>>>>>>>>> %s, state:0x%x ", __func__, state); */

    switch (evt->event) {
    case TWS_EVENT_CONNECTED:
        __this->cmd_flag = 2;
        tws_background_connected_flag = 1;
        app_task_switch_to(APP_BT_TASK);
        break;
    case TWS_EVENT_CONNECTION_DETACH:
        if (g_localtws.dec_restart) {
            g_localtws.dec_restart();
        }
        break;
    }
    return 0;
}

/*----------------------------------------------------------------------------*/
/**@brief    localtws mixer�¼��ص�
   @param    *mixer: ���
   @param    event: �¼�
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
static void localtws_mixer_event_handler(struct audio_mixer *mixer, int event)
{
    switch (event) {
    case MIXER_EVENT_OPEN:
        break;
    case MIXER_EVENT_CLOSE:
        break;
    case MIXER_EVENT_SR_CHANGE:
        break;
    }
}

/*----------------------------------------------------------------------------*/
/**@brief    localtws mixer ��������
   @param    *mixer: ���
   @param    sr: ������
   @return   ֧�ֵĲ�����
   @note
*/
/*----------------------------------------------------------------------------*/
static u32 localtws_mixer_check_sbc_sr(struct audio_mixer *mixer, u32 sr)
{
    return localtws_enc_sbc_sample_rate_select(sr, 1);
}

/*----------------------------------------------------------------------------*/
/**@brief    localtws mixer �ر�
   @param    *pfmt: ��Ƶ��Ϣ
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
static void localtws_mixer_close(struct audio_fmt *pfmt)
{
    if (g_localtws.mixer_buf) {
        audio_mixer_close(&g_localtws.mixer);
        free(g_localtws.mixer_buf);
        g_localtws.mixer_buf = NULL;
    }
}

/*----------------------------------------------------------------------------*/
/**@brief    ��localtws mixer
   @param    *enc: ������
   @param    *pfmt: ��Ƶ��Ϣ
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
static void localtws_mixer_open(struct localtws_enc_hdl *enc, struct audio_fmt *pfmt)
{
    g_localtws.mixer_buf = malloc(LOCALTWS_MIXER_BUF_LEN);
    ASSERT(g_localtws.mixer_buf);

    // ��mixer
    audio_mixer_open(&g_localtws.mixer);
    audio_mixer_set_event_handler(&g_localtws.mixer, localtws_mixer_event_handler);
    /* if (pfmt->coding_type & AUDIO_CODING_SBC) { */
    /* audio_mixer_set_check_sr_handler(&g_localtws.mixer, localtws_mixer_check_sbc_sr); */
    /* } */
    // �̶�mixer������
    audio_mixer_set_sample_rate(&g_localtws.mixer, MIXER_SR_SPEC, pfmt->sample_rate);

    /*��ʼ��mix_buf�ĳ���*/
    audio_mixer_set_output_buf(&g_localtws.mixer, g_localtws.mixer_buf, LOCALTWS_MIXER_BUF_LEN);
    /* audio_mixer_set_channel_num(&g_localtws.mixer, audio_output_channel_num()); */
    audio_mixer_set_channel_num(&g_localtws.mixer, pfmt->channel);
#ifdef CONFIG_MIXER_CYCLIC
    audio_mixer_set_min_len(&g_localtws.mixer, LOCALTWS_MIXER_BUF_LEN / 2);
#endif

    struct audio_stream_entry *entries[8] = {NULL};
    // ����������
    u8 entry_cnt = 0;
    entries[entry_cnt++] = &g_localtws.mixer.entry;
    entries[entry_cnt++] = &enc->entry;
    g_localtws.mixer.stream = audio_stream_open(&g_localtws.mixer, audio_mixer_stream_resume);
    audio_stream_add_list(g_localtws.mixer.stream, entries, entry_cnt);
}

//////////////////////////////////////////////////////////////////////////////
// lib weak func

/*----------------------------------------------------------------------------*/
/**@brief    localtws��������
   @param    *enc: ������
   @param    *pfmt: ��Ƶ��Ϣ
   @return   0-�ɹ�
   @note     �������ض���
*/
/*----------------------------------------------------------------------------*/
int localtws_enc_open_use(struct localtws_enc_hdl *enc, struct audio_fmt *pfmt)
{
    /* if (!encode_task) { */
    /* encode_task = zalloc(sizeof(*encode_task)); */
    /* ASSERT(encode_task); */
    /* audio_encoder_task_create(encode_task, "audio_enc"); */
    /* } */
    audio_encoder_task_open();
    enc->encode_task = encode_task;
    if (pfmt->coding_type & AUDIO_CODING_SBC) {
        clock_add(ENC_TWS_SBC_CLK);
    } else {
        clock_add(ENC_MP3_CLK);
    }

    clock_set_cur();

    localtws_mixer_open(enc, pfmt);

    return 0;
}

/*----------------------------------------------------------------------------*/
/**@brief    localtws����ر����
   @param    *enc: ������
   @return
   @note     �������ض���
*/
/*----------------------------------------------------------------------------*/
void localtws_enc_close_use(struct localtws_enc_hdl *enc)
{
    struct audio_fmt *pfmt = &enc->encoder.fmt;

    audio_encoder_task_close();
    /* if (encode_task) { */
    /* audio_encoder_task_del(encode_task); */
    /* free(encode_task); */
    /* encode_task = NULL; */
    /* } */
    if (enc->encoder.enc_ops->coding_type & AUDIO_CODING_SBC) {
        clock_remove(ENC_TWS_SBC_CLK);
    } else {
        clock_remove(ENC_MP3_CLK);
    }
    clock_set_cur();

    localtws_mixer_close(pfmt);
}

/*----------------------------------------------------------------------------*/
/**@brief    localtwsת�������
   @param    *dec: ת�����
   @param    *pfmt: ��Ƶ��Ϣ
   @return   0-�ɹ�
   @note     �������ض���
*/
/*----------------------------------------------------------------------------*/
int localtws_code_convert_open_use(struct localtws_code_convert_hdl *dec, struct audio_fmt *pfmt)
{
    dec->decode_task = &decode_task;
    audio_stream_add_tail(g_localtws.mixer.stream, &dec->entry);
    return 0;
}

/*----------------------------------------------------------------------------*/
/**@brief    localtwsת���ر����
   @param    *dec: ת�����
   @return
   @note     �������ض���
*/
/*----------------------------------------------------------------------------*/
void localtws_code_convert_close_use(struct localtws_code_convert_hdl *dec)
{
}

//////////////////////////////////////////////////////////////////////////////
// enc api

/*----------------------------------------------------------------------------*/
/**@brief    ��localtws����
   @param    *pfmt: ��Ƶ��Ϣ
   @param    flag: Դ��Ϣ
   @return   true: �ɹ�
   @return   false: ʧ��
   @note
*/
/*----------------------------------------------------------------------------*/
int localtws_enc_api_open(struct audio_fmt *pfmt, u32 flag)
{
    if (localtws_check_enable() == true) {
        struct audio_fmt enc_fmt = {0};
        if (pfmt->coding_type != AUDIO_CODING_MP3) {
            pfmt->coding_type = AUDIO_CODING_SBC;
        }
        if (pfmt->coding_type == AUDIO_CODING_SBC) {
            pfmt->sample_rate = localtws_enc_sbc_sample_rate_select(pfmt->sample_rate, 1);
        }

        enc_fmt.coding_type = pfmt->coding_type;
        enc_fmt.bit_rate = 128;
        enc_fmt.channel = pfmt->channel;
        enc_fmt.sample_rate = pfmt->sample_rate;

        LOCALTWS_LOG("localtws sr:%d, chnum:%d, cod:0x%x \n", enc_fmt.sample_rate, enc_fmt.channel, enc_fmt.coding_type);

        int ret = localtws_enc_open(&enc_fmt);
        if (ret == 0) {
            if (enc_fmt.coding_type != AUDIO_CODING_SBC) {
                ret = localtws_code_convert_open(&enc_fmt);
            }
            if (ret == 0) {

                localtws_push_open();
                audio_stream_add_tail(g_localtws.mixer.stream, &g_localtws.push.entry);

                if (flag & LOCALTWS_ENC_FLAG_STREAM) {
                    localtws_enc_set_stream_data_ctrl(localtws_dec_out_is_start, 500);
                } else {
                    local_tws_sync_no_check_data_buf(1);
                }

                localtws_start(&enc_fmt);
                return true;
            } else {
                localtws_enc_close();
            }
        }
    }
    return false;
}

/*----------------------------------------------------------------------------*/
/**@brief    localtws����д��
   @param    *data: ����
   @param    len: ���ݳ���
   @return   ʵ��д�볤��
   @note
*/
/*----------------------------------------------------------------------------*/
int localtws_enc_api_write(s16 *data, int len)
{
    return localtws_enc_write(NULL, data, len);
}

/*----------------------------------------------------------------------------*/
/**@brief    �ر�localtws����
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void localtws_enc_api_close(void)
{
    localtws_enc_close();
    localtws_code_convert_close();
    localtws_push_close();
    localtws_stop();
}

//////////////////////////////////////////////////////////////////////////////
// api

/*----------------------------------------------------------------------------*/
/**@brief    localtws���õȴ�a2dp״̬
   @param    flag: ״̬
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void localtws_set_wait_a2dp_start(u8 flag)
{
    if (localtws_dec_is_open()) {
        g_localtws.drop_frame_start = flag;
    }
}

#if LOCALTWS_CHANGE_USE_BT_CMD

enum {
    LOCALTWS_BT_CMD_MEDIA = 0,
    LOCALTWS_BT_CMD_PAUSE,
};

struct localtws_bt_info {
    int cmd;
    int value;
};

#define TWS_FUNC_ID_LOCALTWS		TWS_FUNC_ID('L', 'C', 'L', 'T')

static void localtws_rx_data(void *data, u16 len, bool rx)
{
    struct localtws_bt_info info;
    memcpy(&info, data, sizeof(struct localtws_bt_info));
    switch (info.cmd) {
    case LOCALTWS_BT_CMD_MEDIA:
        if (!rx) {
            return;
        }
        y_printf("localtws_rx_data:0x%x \n", info.value);
        g_localtws.media_value = info.value;
        if (info.value) {
            localtws_dec_event(TWS_EVENT_LOCAL_MEDIA_START, info.value);
        } else {
            localtws_dec_event(TWS_EVENT_LOCAL_MEDIA_STOP, 0);
        }
        break;
    case LOCALTWS_BT_CMD_PAUSE:
        y_printf("localtws_rx pause:%d \n", info.value);
        if (info.value) {
            g_localtws.tws_send_pause = LOCALTWS_MEDIA_DEC_PAUSE;
            g_localtws.fade_ms = 30;
            g_localtws.fade_out_mute_ms = 10;
        } else {
            g_localtws.tws_send_pause = LOCALTWS_MEDIA_DEC_PAUSE_RECOVER;
            g_localtws.fade_ms = 30;
        }
        break;
    }
}

REGISTER_TWS_FUNC_STUB(localtws_rx) = {
    .func_id = TWS_FUNC_ID_LOCALTWS,
    .func = localtws_rx_data,
};
#endif /*LOCALTWS_CHANGE_USE_BT_CMD*/


/*----------------------------------------------------------------------------*/
/**@brief    localtws����
   @param    *pfmt: ��Ƶ��Ϣ
   @return
   @note     ��豸��������
*/
/*----------------------------------------------------------------------------*/
void localtws_start(struct audio_fmt *pfmt)
{
    clock_add_set(LOCALTWS_CLK);

    localtws_media_set_info((u8 *)&g_localtws.media_value, pfmt);

#if LOCALTWS_CHANGE_USE_BT_CMD
    struct localtws_bt_info info = {0};
    info.cmd = LOCALTWS_BT_CMD_MEDIA;
    info.value = g_localtws.media_value;
    tws_api_send_data_to_sibling((u8 *)&info, sizeof(struct localtws_bt_info), TWS_FUNC_ID_LOCALTWS);
#endif /*LOCALTWS_CHANGE_USE_BT_CMD*/

    localtws_dec_open(g_localtws.media_value);
}

/*----------------------------------------------------------------------------*/
/**@brief    localtwsֹͣ
   @param
   @return
   @note     ��豸��������
*/
/*----------------------------------------------------------------------------*/
void localtws_stop(void)
{
    LOCALTWS_LOG("localtws_stop\n");
    g_localtws.tws_stop = 1;
    // �����û��׼�������
    tws_api_local_media_trans_clear_no_ready();

#if LOCALTWS_CHANGE_USE_BT_CMD
    struct localtws_bt_info info = {0};
    info.cmd = LOCALTWS_BT_CMD_MEDIA;
    info.value = 0;
    tws_api_send_data_to_sibling((u8 *)&info, sizeof(struct localtws_bt_info), TWS_FUNC_ID_LOCALTWS);
#else /*LOCALTWS_CHANGE_USE_BT_CMD*/

    // ����һ��������
    localtws_media_send_end(200);
    int to_cnt = 0;
    // ��ʱ�ȴ���������ȡ��
    while (tws_api_local_media_trans_check_total(0)) {
        localtws_decoder_resume_pre();
        os_time_dly(1);
        to_cnt += 10;
        if (to_cnt > 500) {
            LOCALTWS_LOG("local tws send end timer out \n");
            break;
        }
    }
#endif /*LOCALTWS_CHANGE_USE_BT_CMD*/

    localtws_dec_close(0);
    g_localtws.tws_stop = 0;

    clock_remove_set(LOCALTWS_CLK);
}

void localtws_decoder_pause(u8 pause)
{
#if LOCALTWS_CHANGE_USE_BT_CMD
    struct localtws_bt_info info = {0};
    info.cmd = LOCALTWS_BT_CMD_PAUSE;
    info.value = pause;
    tws_api_send_data_to_sibling((u8 *)&info, sizeof(struct localtws_bt_info), TWS_FUNC_ID_LOCALTWS);
#else /*LOCALTWS_CHANGE_USE_BT_CMD*/
    if (pause) {
        // ����һ����ͣ�������ӻ�������ʱ����stop
        localtws_dec_pause();
    }
#endif /*LOCALTWS_CHANGE_USE_BT_CMD*/
}
#else
void set_tws_background_connected_flag(u8 flag)
{

}

u8 get_tws_background_connected_flag()
{
    return 0;
}
#endif /*(defined(TCFG_DEC2TWS_ENABLE) && (TCFG_DEC2TWS_ENABLE))*/

