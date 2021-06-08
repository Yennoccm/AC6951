/***********************************Jieli tech************************************************
  File : localtws_dec.c
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
#include "application/audio_eq_drc_apply.h"

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
//

struct localtws_dec_hdl {
    struct audio_stream *stream;	// ��Ƶ��
    struct localtws_decoder local_dec;	// local������
    struct audio_res_wait wait;		// ��Դ�ȴ����
    struct audio_eq_drc *eq_drc;    //eq drc�����music eq��ʱ��twsת����sbc���ݣ�����eq
    struct audio_mixer_ch mix_ch;	// ���Ӿ��
    u32 id;					// Ψһ��ʶ�������ֵ
    u32 media_value;		// localtwsý����Ϣ
    struct audio_wireless_sync *sync;
};
static struct localtws_dec_hdl *localtws_dec = NULL;

void *file_eq_drc_open(u16 sample_rate, u8 ch_num);
void file_eq_drc_close(struct audio_eq_drc *eq_drc);

//////////////////////////////////////////////////////////////////////////////
//
extern struct audio_decoder_task decode_task;

#if TCFG_DEC2TWS_TASK_ENABLE
extern struct audio_decoder_task localtws_decode_task;
#define LOCALTWS_TASK		localtws_decode_task
#else /*TCFG_DEC2TWS_TASK_ENABLE*/
#define LOCALTWS_TASK		decode_task
#endif /*TCFG_DEC2TWS_TASK_ENABLE*/

extern void tws_api_local_media_trans_clear_no_ready(void);
extern int tws_api_local_media_trans_get_total_buffer_size(void);
extern void bt_drop_a2dp_frame_start(void);
extern int a2dp_media_clear_packet();
extern struct audio_wireless_sync *audio_localtws_sync_open(int sample_rate, int output_sample_rate, u8 channels);
extern void audio_localtws_sync_close(struct audio_wireless_sync *localtws_sync);
extern u8 is_tws_active_device(void);

extern void sys_auto_shut_down_disable(void);

///////////////////////////////////////////////////////////////////////////////////
struct local_dec_type {
    u32 type;
    u32 clk;
};

const struct local_dec_type local_dec_clk_tb[] = {
    {AUDIO_CODING_SBC,  DEC_TWS_SBC_CLK},
    {AUDIO_CODING_MP3,  DEC_MP3_CLK},
    {AUDIO_CODING_WMA,  DEC_WMA_CLK},
    {AUDIO_CODING_M4A,  DEC_M4A_CLK},
};

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
/**@brief    localtws����ʱ�����
   @param    type: ��������
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
static void local_dec_clock_add(u32 type)
{
    LOCALTWS_LOG("local_dec_clock_add : 0x%x \n", type);
    int i = 0;
    for (i = 0; i < ARRAY_SIZE(local_dec_clk_tb); i++) {
        if (type == local_dec_clk_tb[i].type) {
            clock_add(local_dec_clk_tb[i].clk);
            return;
        }
    }
}

/*----------------------------------------------------------------------------*/
/**@brief    localtws����ʱ���Ƴ�
   @param    type: ��������
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
static void local_dec_clock_remove(u32 type)
{
    LOCALTWS_LOG("local_dec_clock_remove : 0x%x \n", type);
    int i = 0;
    for (i = 0; i < ARRAY_SIZE(local_dec_clk_tb); i++) {
        if (type == local_dec_clk_tb[i].type) {
            clock_remove(local_dec_clk_tb[i].clk);
            return;
        }
    }
}

/*----------------------------------------------------------------------------*/
/**@brief    localtws����ǰ������/����
   @param
   @return
   @note     �������ض���
*/
/*----------------------------------------------------------------------------*/
#if 0
void localtws_decoder_resume_pre(void)
{
    audio_stream_resume(&g_localtws.push.entry);
#ifdef TCFG_PCM_ENC2TWS_ENABLE
    localtws_enc_resume();
#endif
}
#endif

/*----------------------------------------------------------------------------*/
/**@brief    localtws�����������
   @param    ms: ��������ʱ��
   @return
   @note     �������ض���
*/
/*----------------------------------------------------------------------------*/
void localtws_decoder_output_reset(u32 ms)
{
    app_audio_output_reset(ms);
}

/*----------------------------------------------------------------------------*/
/**@brief    localtws���뼤��
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void localtws_dec_resume(void)
{
    if (localtws_dec && localtws_dec->local_dec.status) {
        audio_decoder_resume(&localtws_dec->local_dec.decoder);
    }
}

/*----------------------------------------------------------------------------*/
/**@brief    localtws�Ѿ���
   @param
   @return   true: ��
   @return   false: û�д�
   @note
*/
/*----------------------------------------------------------------------------*/
u8 localtws_dec_is_open(void)
{
    if (localtws_dec) {
        return true;
    }
    return false;
}

/*----------------------------------------------------------------------------*/
/**@brief    localtws��������
   @param
   @return   true: ��������
   @return   false: ������
   @note
*/
/*----------------------------------------------------------------------------*/
int localtws_media_dat_abandon(void)
{
    if ((localtws_dec) && (localtws_dec->local_dec.status) && (localtws_dec->local_dec.tmp_pause)) {
        // �ӻ�����ͣ�������м����ݰ�
        return true;
    }
    return false;
}


/*----------------------------------------------------------------------------*/
/**@brief    localtws�����ͷ�
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
static void localtws_dec_release()
{
    audio_decoder_task_del_wait(&LOCALTWS_TASK, &localtws_dec->wait);

    clock_remove(DEC_TWS_SBC_CLK);

    local_irq_disable();
    free(localtws_dec);
    localtws_dec = NULL;
    g_localtws.tws_send_pause = 0;
    local_irq_enable();
}

/*----------------------------------------------------------------------------*/
/**@brief    localtws�����¼��ص�
   @param    *decoder: ���������
   @param    argc: ��������
   @param    *argv: ����
   @note
*/
/*----------------------------------------------------------------------------*/
static void localtws_dec_event_handler(struct audio_decoder *decoder, int argc, int *argv)
{
    switch (argv[0]) {
    case AUDIO_DEC_EVENT_END:
        LOCALTWS_LOG("AUDIO_DEC_EVENT_END\n");
        localtws_dec_close(1);
        //audio_decoder_resume_all(&LOCALTWS_TASK);
        break;
    }
}

/*----------------------------------------------------------------------------*/
/**@brief    localtws��������������
   @param    *p: ˽�о��
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
static void localtws_dec_out_stream_resume(void *p)
{
    struct localtws_dec_hdl *dec = p;
    audio_decoder_resume(&dec->local_dec.decoder);
}

/*----------------------------------------------------------------------------*/
/**@brief    localtws���뿪ʼ
   @param
   @return   0���ɹ�
   @note
*/
/*----------------------------------------------------------------------------*/
static int localtws_dec_start()
{
    int err;
    struct localtws_dec_hdl *dec = localtws_dec;

    if (!dec) {
        return -EINVAL;
    }

    struct audio_fmt fmt = {0};
    localtws_media_get_info((u8 *)&dec->media_value, &fmt);
    dec->local_dec.dec_type = fmt.coding_type;
    dec->local_dec.sample_rate = fmt.sample_rate;
    dec->local_dec.ch_num = fmt.channel;

    LOCALTWS_LOG("localtws_dec_start: in, type:0x%x \n", dec->local_dec.dec_type);
    LOCALTWS_LOG("sr:%d, ch:%d, outch:%d \n", dec->local_dec.sample_rate, dec->local_dec.ch_num, dec->local_dec.output_ch_num);

    // ��file������
    err = localtws_decoder_open(&dec->local_dec, &LOCALTWS_TASK);
    if (err) {
        goto __err1;
    }

    audio_decoder_set_event_handler(&dec->local_dec.decoder, localtws_dec_event_handler, dec->id);

    audio_mode_main_dec_open(AUDIO_MODE_MAIN_STATE_DEC_LOCALTWS);

    // ���õ��ӹ���
    audio_mixer_ch_open_head(&dec->mix_ch, &mixer); // ���ص�mixer��ǰ��
    audio_mixer_ch_set_src(&dec->mix_ch, 1, 0);
    audio_mixer_ch_set_no_wait(&dec->mix_ch, 1, 10); // ��ʱ�Զ�����
    audio_mixer_ch_sample_sync_enable(&dec->mix_ch, 1);
    audio_mixer_ch_set_sample_rate(&dec->mix_ch, dec->local_dec.sample_rate);

    dec->sync = audio_localtws_sync_open(dec->local_dec.sample_rate, audio_mixer_get_sample_rate(&mixer), dec->local_dec.output_ch_num);
    localtws_decoder_stream_sync_enable(&dec->local_dec, dec->sync->context, 200, is_tws_active_device);

    if (dec->local_dec.dec_type != AUDIO_CODING_SBC) {
        dec->eq_drc = file_eq_drc_open(dec->local_dec.sample_rate, dec->local_dec.output_ch_num);
    }

    // ����������
    struct audio_stream_entry *entries[8] = {NULL};
    u8 entry_cnt = 0;
    entries[entry_cnt++] = &dec->local_dec.decoder.entry;
    if (dec->sync) {
        entries[entry_cnt++] = dec->sync->entry;
    }
    if (dec->eq_drc) {
        entries[entry_cnt++] = &dec->eq_drc->entry;
    }
    if (dec->sync) {
        entries[entry_cnt++] = dec->sync->resample_entry;
    }
    entries[entry_cnt++] = &dec->mix_ch.entry;
    dec->stream = audio_stream_open(dec, localtws_dec_out_stream_resume);
    audio_stream_add_list(dec->stream, entries, entry_cnt);

    audio_output_set_start_volume(APP_AUDIO_STATE_MUSIC);

    dec->local_dec.status = 1;
    if (is_tws_active_device()) {
        /*audio_decoder_set_run_max(&dec->local_dec.decoder, 10);*/
    }
    err = audio_decoder_start(&dec->local_dec.decoder);
    if (err) {
        goto __err3;
    }

    localtws_drop_frame_stop();
    bt_drop_a2dp_frame_start();
    local_dec_clock_add(dec->local_dec.decoder.dec_ops->coding_type);
    clock_set_cur();

    return 0;

__err3:
    dec->local_dec.status = 0;
    file_eq_drc_close(dec->eq_drc);
    audio_mixer_ch_close(&dec->mix_ch);
    if (dec->stream) {
        audio_stream_close(dec->stream);
        dec->stream = NULL;
    }
    localtws_decoder_close(&dec->local_dec);
__err1:
    localtws_dec_release();

    return err;
}

static void localtws_dec_res_put(struct localtws_dec_hdl *dec)
{
    if (!dec->local_dec.status) {
        return;
    }
    os_mutex_pend(&g_localtws.mutex, 0);
    g_localtws.drop_frame_start = 1;
    dec->local_dec.status = 0;
    localtws_decoder_close(&dec->local_dec);
    file_eq_drc_close(dec->eq_drc);
    if (dec->sync) {
        audio_localtws_sync_close(dec->sync);
        dec->sync = NULL;
    }
    audio_mixer_ch_close(&dec->mix_ch);
    if (dec->stream) {
        audio_stream_close(dec->stream);
        dec->stream = NULL;
    }
    local_dec_clock_remove(dec->local_dec.decoder.dec_ops->coding_type);
    /* g_localtws.media_value = 0; */
    if (g_localtws.tmrout) {
        sys_hi_timeout_del(g_localtws.tmrout);
        g_localtws.tmrout = 0;
    }
    tws_api_local_media_trans_clear();
    localtws_drop_frame_start();
    os_mutex_post(&g_localtws.mutex);
}

/*----------------------------------------------------------------------------*/
/**@brief    localtws������Դ�ȴ�
   @param    *wait: ���
   @param    event: �¼�
   @return   0���ɹ�
   @note     ���ڶ�����ϴ���
*/
/*----------------------------------------------------------------------------*/
static int localtws_dec_wait_res_handler(struct audio_res_wait *wait, int event)
{
    int err = 0;

    struct localtws_dec_hdl *dec = localtws_dec;

#if 0
    if (event == AUDIO_RES_GET) {
        if (dec->local_dec.status == 0) {
            tws_api_local_media_set_limit_size(LOCALTWS_MEDIA_BUF_LIMIT_LEN);
            err = localtws_dec_start();
        } else if (dec->local_dec.tmp_pause) {
            tws_api_local_media_set_limit_size(LOCALTWS_MEDIA_BUF_LIMIT_LEN);
            localtws_drop_frame_stop();
            dec->local_dec.tmp_pause = 0;

            /* audio_mixer_ch_open(&dec->mix_ch, &mixer); */

            audio_output_set_start_volume(APP_AUDIO_STATE_MUSIC);
            /*audio_output_start(dec->src_out_sr, 1);*/
            if (dec->local_dec.status) {
                err = audio_decoder_start(&dec->local_dec.decoder);
            }
        }
    } else if (event == AUDIO_RES_PUT) {
        /* LOCALTWS_LOG("localtws AUDIO_RES_PUT\n"); */
        if (dec->local_dec.status) {
            dec->local_dec.tmp_pause = 1;
            err = audio_decoder_pause(&dec->local_dec.decoder);
            os_time_dly(2);
            // ����ͣ�������ݣ������������ݺ�dec����output
            tws_api_local_media_trans_clear();
            localtws_drop_frame_start();
            /* audio_mixer_ch_close(&dec->mix_ch); */
        }
    }
#else
    y_printf("%s,%d, evt:%d \n", __func__, __LINE__, event);
    if (event == AUDIO_RES_GET) {
        tws_api_local_media_set_limit_size(LOCALTWS_MEDIA_BUF_LIMIT_LEN);
        err = localtws_dec_start();
        localtws_decoder_resume_pre();
    } else if (event == AUDIO_RES_PUT) {
        if (dec->local_dec.status) {
            localtws_dec_res_put(dec);
        }
    }
#endif

    return err;
}

/*----------------------------------------------------------------------------*/
/**@brief    ��localtws����
   @param    :
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
int localtws_dec_open(u32 value)
{

    if (localtws_dec && (localtws_dec->media_value != value)) {
        localtws_dec_close(0);
    }

    os_mutex_pend(&g_localtws.mutex, 0);
    if (!localtws_dec) {
        localtws_dec_close(0);
        localtws_dec = zalloc(sizeof(struct localtws_dec_hdl));
        ASSERT(localtws_dec);
    } else {
        os_mutex_post(&g_localtws.mutex);
        return 0;
    }

    int err;
    struct localtws_dec_hdl *dec = localtws_dec;

    LOCALTWS_LOG(" ******  localtws_dec_open: in, \n");
    sys_auto_shut_down_disable();

    if (!dec) {
        os_mutex_post(&g_localtws.mutex);
        return -EPERM;
    }

#if 1
    a2dp_dec_close();
    esco_dec_close();
    g_localtws.drop_frame_start = 0;
#endif

    dec->id = rand32();

    dec->media_value = value;
    dec->local_dec.ch_type = AUDIO_CH_MAX;
    dec->local_dec.output_ch_num = audio_output_channel_num();
    dec->local_dec.output_ch_type = audio_output_channel_type();

    dec->wait.priority = 1;
#if DEC_MIX_ENABLE
    dec->wait.preemption = 0;
    dec->wait.protect = 1;
#else
    dec->wait.preemption = 1;
#endif//DEC_MIX_ENABLE

    dec->wait.handler = localtws_dec_wait_res_handler;
    err = audio_decoder_task_add_wait(&LOCALTWS_TASK, &dec->wait);

    os_mutex_post(&g_localtws.mutex);

    return err;
}

/*----------------------------------------------------------------------------*/
/**@brief    �ر�localtws����
   @param    drop_frame_start: �Ƿ���ֹ����
   @return   0���ɹ�
   @note
*/
/*----------------------------------------------------------------------------*/
int localtws_dec_close(u8 drop_frame_start)
{
    os_mutex_pend(&g_localtws.mutex, 0);
    /* LOCALTWS_LOG("localtws_dec_close start\n"); */
    if (!localtws_dec) {
        os_mutex_post(&g_localtws.mutex);
        return 0;
    }

    if (drop_frame_start) {
        g_localtws.drop_frame_start = 1;	// �رչ����п��ܸպû������ݹ���
    }

    if (localtws_dec->local_dec.status) {
        localtws_dec->local_dec.status = 0;
        localtws_decoder_close(&localtws_dec->local_dec);
        file_eq_drc_close(localtws_dec->eq_drc);
        audio_mixer_ch_close(&localtws_dec->mix_ch);
        if (localtws_dec->sync) {
            audio_localtws_sync_close(localtws_dec->sync);
            localtws_dec->sync = NULL;
        }
        if (localtws_dec->stream) {
            audio_stream_close(localtws_dec->stream);
            localtws_dec->stream = NULL;
        }
        local_dec_clock_remove(localtws_dec->local_dec.decoder.dec_ops->coding_type);
    }

    /* g_localtws.media_value = 0; */
    if (g_localtws.tmrout) {
        sys_hi_timeout_del(g_localtws.tmrout);
        g_localtws.tmrout = 0;
    }

    localtws_dec_release();

    tws_api_local_media_trans_clear();
    if (drop_frame_start) {
        g_localtws.drop_frame_start = 1;
        localtws_drop_frame_start();
    } else {
        localtws_drop_frame_stop();
        g_localtws.drop_frame_start = 0;
    }
    clock_set_cur();
    LOCALTWS_LOG("*****  localtws_dec_close: exit\n");
    os_mutex_post(&g_localtws.mutex);
    return 1;
}

/*----------------------------------------------------------------------------*/
/**@brief    localtws��ͣ
   @param    :
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void localtws_dec_pause(void)
{
    g_localtws.tws_send_pause = 1;
#ifdef TCFG_PCM_ENC2TWS_ENABLE
    localtws_enc_clear();
#endif
    localtws_media_send_pause(100);
    g_localtws.tws_send_pause = 2;
}
/*----------------------------------------------------------------------------*/
/**@brief    localtws�Ѿ���ʼ����
   @param
   @return   true: �Ѿ���ʼ����
   @return   false: û�п�ʼ����
   @note
*/
/*----------------------------------------------------------------------------*/
int localtws_dec_out_is_start(void)
{
    if ((!localtws_dec) || (!localtws_dec->local_dec.status)) {
        return false;
    }
    if (localtws_dec->local_dec.read_en) {
        return true;
    }
    return false;
}


/*----------------------------------------------------------------------------*/
/**@brief    localtws����idle�ж�
   @param
   @return   1: idle
   @return   0: busy
   @note
*/
/*----------------------------------------------------------------------------*/
static u8 localtws_dec_idle_query()
{
    if (localtws_dec) {
        return 0;
    }

    return 1;
}
REGISTER_LP_TARGET(localtws_dec_lp_target) = {
    .name = "local_dec",
    .is_idle = localtws_dec_idle_query,
};



#endif /*(defined(TCFG_DEC2TWS_ENABLE) && (TCFG_DEC2TWS_ENABLE))*/

