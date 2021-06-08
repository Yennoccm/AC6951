/*
 ****************************************************************
 *File : audio_dec_file.c
 *Note :
 *
 ****************************************************************
 */
//////////////////////////////////////////////////////////////////////////////
#include "asm/includes.h"
#include "media/includes.h"
#include "system/includes.h"
#include "effectrs_sync.h"
#include "app_config.h"
#include "audio_config.h"
#include "audio_dec.h"
#include "app_config.h"
#include "app_main.h"
#include "classic/tws_api.h"

#include "music/music_decrypt.h"
#include "music/music_id3.h"
#include "pitchshifter/pitchshifter_api.h"
#include "mono2stereo/reverb_mono2stero_api.h"
#include "audio_enc.h"
#include "clock_cfg.h"
#include "application/audio_pitch.h"
#include "application/audio_eq_drc_apply.h"
#include "audio_base.h"
#include "channel_switch.h"


#if TCFG_APP_MUSIC_EN


#define FILE_DEC_PICK_EN			1 // ���ؽ�����ת��

#if (!TCFG_DEC2TWS_ENABLE)
#undef FILE_DEC_PICK_EN
#define FILE_DEC_PICK_EN			0
#endif

#ifndef BREAKPOINT_DATA_LEN
#define BREAKPOINT_DATA_LEN			32
#endif

const int FILE_DEC_ONCE_OUT_NUM	= ((512 * 4) * 2);	// һ�����������ȡ������������ʱ��ס��������̫��ʱ��

// �ļ��������ʹ�õ�������
#if TCFG_AUDIO_DEC_OUT_TASK
#define FILE_DEC_USE_OUT_TASK		1
#else
#define FILE_DEC_USE_OUT_TASK		0
#endif

#define  CHECK_SR_WHEN_DECODING   0 //ÿ��run����������
//////////////////////////////////////////////////////////////////////////////

struct dec_type {
    u32 type;	// ��������
    u32 clk;	// ����ʱ��
};

const struct dec_type  dec_clk_tb[] = {
    {AUDIO_CODING_MP3,  DEC_MP3_CLK},
    {AUDIO_CODING_WAV,  DEC_WAV_CLK},
    {AUDIO_CODING_G729, DEC_G729_CLK},
    {AUDIO_CODING_G726, DEC_G726_CLK},
    {AUDIO_CODING_PCM,  DEC_PCM_CLK},
    {AUDIO_CODING_MTY,  DEC_MTY_CLK},
    {AUDIO_CODING_WMA,  DEC_WMA_CLK},

    {AUDIO_CODING_APE,  DEC_APE_CLK},
    {AUDIO_CODING_FLAC, DEC_FLAC_CLK},
    {AUDIO_CODING_DTS,  DEC_DTS_CLK},
    {AUDIO_CODING_M4A,  DEC_M4A_CLK},
    {AUDIO_CODING_ALAC, DEC_ALAC_CLK},
    {AUDIO_CODING_MIDI, DEC_MIDI_CLK},

    {AUDIO_CODING_MP3 | AUDIO_CODING_STU_PICK,  DEC_MP3PICK_CLK},
    {AUDIO_CODING_WMA | AUDIO_CODING_STU_PICK,  DEC_WMAPICK_CLK},
    {AUDIO_CODING_M4A | AUDIO_CODING_STU_PICK,  DEC_M4APICK_CLK},
};

//////////////////////////////////////////////////////////////////////////////

struct file_dec_hdl *file_dec = NULL;	// �ļ�������
u8 file_dec_start_pause = 0;	// ��������󵫲����Ͽ�ʼ����


//////////////////////////////////////////////////////////////////////////////

void *file_eq_drc_open(u16 sample_rate, u8 ch_num);
void file_eq_drc_close(struct audio_eq_drc *eq_drc);
void *file_rl_rr_eq_drc_open(u16 sample_rate, u8 ch_num);
void file_rl_rr_eq_drc_close(struct audio_eq_drc *eq_drc);

extern void put_u16hex(u16 dat);

extern int tws_api_get_tws_state();
extern void local_tws_sync_no_check_data_buf(u8 no_check);

int file_dec_repeat_set(u8 repeat_num);

surround_hdl *surround_open_demo(u8 ch_num);
void surround_close(surround_hdl *surround);
void surround_effect_switch(surround_hdl *surround, u32 effect_type);
vbass_hdl *vbass_open_demo(u16 sample_rate, u8 ch_num);
void vbass_close_demo(vbass_hdl *vbass);
void vbass_switch(vbass_hdl *vbass, u32 vbass_switch);

//////////////////////////////////////////////////////////////////////////////
/*----------------------------------------------------------------------------*/
/**@brief    ��ȡ�ļ�����hdl
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void *get_file_dec_hdl()
{
    return file_dec;
}
/*----------------------------------------------------------------------------*/
/**@brief    ����ʱ�����
   @param    type: ��������
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
static void dec_clock_add(u32 type)
{
    int i = 0;
    for (i = 0; i < ARRAY_SIZE(dec_clk_tb); i++) {
        if (type == dec_clk_tb[i].type) {
            clock_add(dec_clk_tb[i].clk);
            return;
        }
    }
}

/*----------------------------------------------------------------------------*/
/**@brief    ����ʱ���Ƴ�
   @param    type: ��������
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
static void dec_clock_remove(u32 type)
{
    int i = 0;
    for (i = 0; i < ARRAY_SIZE(dec_clk_tb); i++) {
        if (type == dec_clk_tb[i].type) {
            clock_remove(dec_clk_tb[i].clk);
            return;
        }
    }
}

/*----------------------------------------------------------------------------*/
/**@brief    ��ȡ�ļ�����
   @param    *decoder: ���������
   @param    *buf: ����
   @param    len: ���ݳ���
   @return   >=0�����������ݳ���
   @return   <0������
   @note
*/
/*----------------------------------------------------------------------------*/
static int file_fread(struct audio_decoder *decoder, void *buf, u32 len)
{
    struct file_decoder *file_dec = container_of(decoder, struct file_decoder, decoder);
    struct file_dec_hdl *dec = container_of(file_dec, struct file_dec_hdl, file_dec);
    int rlen;
#if TCFG_DEC_DECRYPT_ENABLE
    // ���ܶ�ȡ
    u32 addr;
    addr = fpos(dec->file);
    rlen = fread(dec->file, buf, len);
    if (rlen && (rlen <= len)) {
        // read����н���
        cryptanalysis_buff(&dec->mply_cipher, buf, addr, rlen);
    }
#else
    rlen = fread(dec->file, buf, len);
#endif
    if (rlen > len) {
        // putchar('r');
        if (rlen == (-1)) {
            //file err
            dec->read_err = 1;
        } else {
            //dis err
            dec->read_err = 2;
        }
        rlen = 0;
    } else {
        // putchar('R');
        dec->read_err = 0;
    }
    return rlen;
}

/*----------------------------------------------------------------------------*/
/**@brief    �ļ�ָ�붨λ
   @param    *decoder: ���������
   @param    offset: ��λƫ��
   @param    seek_mode: ��λ����
   @return   0���ɹ�
   @return   ��0������
   @note
*/
/*----------------------------------------------------------------------------*/
static int file_fseek(struct audio_decoder *decoder, u32 offset, int seek_mode)
{
    struct file_decoder *file_dec = container_of(decoder, struct file_decoder, decoder);
    struct file_dec_hdl *dec = container_of(file_dec, struct file_dec_hdl, file_dec);
    return fseek(dec->file, offset, seek_mode);
}

/*----------------------------------------------------------------------------*/
/**@brief    ��ȡ�ļ�����
   @param    *decoder: ���������
   @return   �ļ�����
   @note
*/
/*----------------------------------------------------------------------------*/
static int file_flen(struct audio_decoder *decoder)
{
    struct file_decoder *file_dec = container_of(decoder, struct file_decoder, decoder);
    struct file_dec_hdl *dec = container_of(file_dec, struct file_dec_hdl, file_dec);
    int len = 0;
    len = flen(dec->file);
    return len;
}

// �ȼ����file_input�ж�������ͣ������file_input_coding_more�������������
static const u32 file_input_coding_more[] = {
#if TCFG_DEC_MP3_ENABLE
    AUDIO_CODING_MP3,
#endif
    0,
};

static const struct audio_dec_input file_input = {
    .coding_type = 0
#if TCFG_DEC_WMA_ENABLE
    | AUDIO_CODING_WMA
#endif
#if TCFG_DEC_WAV_ENABLE
    | AUDIO_CODING_WAV
#endif
#if TCFG_DEC_FLAC_ENABLE
    | AUDIO_CODING_FLAC
#endif
#if TCFG_DEC_APE_ENABLE
    | AUDIO_CODING_APE
#endif
#if TCFG_DEC_M4A_ENABLE
    | AUDIO_CODING_M4A
#endif
#if TCFG_DEC_ALAC_ENABLE
    | AUDIO_CODING_ALAC
#endif
#if TCFG_DEC_AMR_ENABLE
    | AUDIO_CODING_AMR
#endif
#if TCFG_DEC_DTS_ENABLE
    | AUDIO_CODING_DTS
#endif
#if TCFG_DEC_G726_ENABLE
    | AUDIO_CODING_G726
#endif
#if TCFG_DEC_MIDI_ENABLE
    | AUDIO_CODING_MIDI
#endif
    ,
    .p_more_coding_type = (u32 *)file_input_coding_more,
    .data_type   = AUDIO_INPUT_FILE,
    .ops = {
        .file = {
            .fread = file_fread,
            .fseek = file_fseek,
            .flen  = file_flen,
        }
    }
};

/*----------------------------------------------------------------------------*/
/**@brief    �ļ������ͷ�
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
static void file_dec_release(void)
{
    struct file_dec_hdl *dec = file_dec;

#if TCFG_DEC_ID3_V1_ENABLE
    if (dec->p_mp3_id3_v1) {
        id3_obj_post(&dec->p_mp3_id3_v1);
    }
#endif
#if TCFG_DEC_ID3_V2_ENABLE
    if (dec->p_mp3_id3_v2) {
        id3_obj_post(&dec->p_mp3_id3_v2);
    }
#endif

    audio_decoder_task_del_wait(&decode_task, &dec->wait);

    if (dec->file_dec.decoder.fmt.coding_type) {
        dec_clock_remove(dec->file_dec.decoder.fmt.coding_type);
    }

    local_irq_disable();
    if (file_dec->dec_bp) {
        free(file_dec->dec_bp);
        file_dec->dec_bp = NULL;
    }
    free(file_dec);
    file_dec = NULL;
    local_irq_enable();
}

/*----------------------------------------------------------------------------*/
/**@brief    �ļ������¼�����
   @param    *decoder: ���������
   @param    argc: ��������
   @param    *argv: ����
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
static void file_dec_event_handler(struct audio_decoder *decoder, int argc, int *argv)
{
    switch (argv[0]) {
    case AUDIO_DEC_EVENT_END:
        log_i("AUDIO_DEC_EVENT_END\n");
        if (!file_dec) {
            log_i("file_dec handle err ");
            break;
        }

        if (file_dec->id != argv[1]) {
            log_w("file_dec id err : 0x%x, 0x%x \n", file_dec->id, argv[1]);
            break;
        }

        // �лص������ϲ�close������close���ϲ�������ϵ��
        if (file_dec->evt_cb) {
            /* file_dec->evt_cb(file_dec->evt_priv, argc, argv); */
            int msg[2];
            msg[0] = argv[0];
            msg[1] = file_dec->read_err;
            /* log_i("read err0:%d ", file_dec->read_err); */
            file_dec->evt_cb(file_dec->evt_priv, 2, msg);
        } else {
            file_dec_close();
        }
        //audio_decoder_resume_all(&decode_task);
        break;
    }
}

/*----------------------------------------------------------------------------*/
/**@brief    �ļ���������������
   @param    *p: ˽�о��
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
static void file_dec_out_stream_resume(void *p)
{
    struct file_dec_hdl *dec = p;
#if FILE_DEC_USE_OUT_TASK
    if (dec->file_dec.dec_no_out_sound == 0) {
        audio_decoder_resume_out_task(&dec->file_dec.decoder);
        return ;
    }
#endif
    audio_decoder_resume(&dec->file_dec.decoder);
}


#if CHECK_SR_WHEN_DECODING
/*----------------------------------------------------------------------------*/
/**@brief    �ļ�����Ԥ����
   @param    *decoder: ���������
   @return   0���ɹ�
   @note
*/
/*----------------------------------------------------------------------------*/
static int file_decoder_probe_handler_app(struct audio_decoder *decoder)
{
    struct file_decoder *dec = container_of(decoder, struct file_decoder, decoder);
    dec->once_out_cnt = 0;
    audio_decoder_get_fmt_info(&dec->decoder, &dec->decoder.fmt);
    return 0;
}

/*----------------------------------------------------------------------------*/
/**@brief    �ļ��������
   @param    *decoder: ���������
   @return   0���ɹ�
   @note
*/
/*----------------------------------------------------------------------------*/
static int file_decoder_post_handler_app(struct audio_decoder *decoder)
{
    struct file_decoder *dec = container_of(decoder, struct file_decoder, decoder);
    if (dec->status) {
        dec->dec_cur_time = audio_decoder_get_play_time(&dec->decoder);
    }
    if (FILE_DEC_ONCE_OUT_NUM && (dec->once_out_cnt >= FILE_DEC_ONCE_OUT_NUM)) {
        audio_decoder_resume(&dec->decoder);
        return 0;
    }
    return 0;
}

//mp3���⴦��
static const struct audio_dec_handler file_decoder_handler_app = {
    .dec_probe  = file_decoder_probe_handler_app,
    .dec_post   = file_decoder_post_handler_app,
};
#endif
/*----------------------------------------------------------------------------*/
/**@brief    �ļ����뿪ʼ
   @param
   @return   0���ɹ�
   @return   ��0��ʧ��
   @note
*/
/*----------------------------------------------------------------------------*/
static int file_dec_start(void)
{
    int err;
    struct file_dec_hdl *dec = file_dec;
    struct audio_mixer *p_mixer = &mixer;
    u8 pcm_enc_flag = 0;

    if (!dec) {
        return -EINVAL;
    }

    log_i("file_dec_start: in\n");

#if FILE_DEC_PICK_EN
    if (localtws_check_enable() == true) {
        // ʹ�ò����ʽ����
        err = file_decoder_open(&dec->file_dec, &file_input, &decode_task,
                                dec->bp, 1);
        if (err == 0) {
            dec->pick_flag = 1;
            dec->file_dec.dec_no_out_sound = 1;
            // ͬ������Ҫ���buf�仯
            local_tws_sync_no_check_data_buf(1);
            // ��localtws����
            localtws_push_open();
            // ����localtws����
            localtws_start(&dec->file_dec.decoder.fmt);
            // �ر���Դ�ȴ������ջ���localtws���봦�ȴ�
            audio_decoder_task_del_wait(&decode_task, &dec->wait);
            goto __open_ok;
        }
    }
#endif

    // ��file������
    err = file_decoder_open(&dec->file_dec, &file_input, &decode_task,
                            dec->bp, 0);
    if (err) {
        goto __err1;
    }

__open_ok:
#if CHECK_SR_WHEN_DECODING
    if (dec->file_dec.decoder.dec_ops->coding_type == AUDIO_CODING_MP3) {
        audio_decoder_set_handler(&dec->file_dec.decoder, &file_decoder_handler_app); //ע�� ����ʱ��������
    }
#endif

    file_decoder_set_event_handler(&dec->file_dec, file_dec_event_handler, dec->id);

    // ��ȡid3
    if (dec->file_dec.decoder.dec_ops->coding_type == AUDIO_CODING_MP3) {
#if TCFG_DEC_ID3_V1_ENABLE
        if (dec->p_mp3_id3_v1) {
            id3_obj_post(&dec->p_mp3_id3_v1);
        }
        dec->p_mp3_id3_v1 = id3_v1_obj_get(dec->file);
#endif
#if TCFG_DEC_ID3_V2_ENABLE
        if (dec->p_mp3_id3_v2) {
            id3_obj_post(&dec->p_mp3_id3_v2);
        }
        dec->p_mp3_id3_v2 = id3_v2_obj_get(dec->file);
#endif
    }


#if TCFG_PCM_ENC2TWS_ENABLE
    if (dec->file_dec.dec_no_out_sound == 0) {
        // û�в��������£�����ʹ��sbc�ȱ���ת��
        struct audio_fmt enc_f;
        memcpy(&enc_f, &dec->file_dec.decoder.fmt, sizeof(struct audio_fmt));
        enc_f.coding_type = AUDIO_CODING_SBC;
        enc_f.channel = 2;//dec->file_dec.output_ch_num; // ʹ��˫���������localtws�ڽ���ʱ�ű�ɶ�Ӧ����
        int ret = localtws_enc_api_open(&enc_f, 0);
        if (ret == true) {
            dec->file_dec.dec_no_out_sound = 1;
            // �ض���mixer
            p_mixer = &g_localtws.mixer;
            // �ر���Դ�ȴ������ջ���localtws���봦�ȴ�
            audio_decoder_task_del_wait(&decode_task, &dec->wait);
            if (dec->file_dec.output_ch_num != enc_f.channel) {
                dec->file_dec.output_ch_num = dec->file_dec.decoder.fmt.channel = enc_f.channel;
                if (enc_f.channel == 2) {
                    dec->file_dec.output_ch_type = AUDIO_CH_LR;
                } else {
                    dec->file_dec.output_ch_type = AUDIO_CH_DIFF;
                }
            }
            // �������ý����������
            file_decoder_set_output_channel(&dec->file_dec);
        }
    }
#endif

#if TCFG_SPEED_PITCH_ENABLE
    static	PS69_CONTEXT_CONF pitch_param;
    pitch_param.pitchV = 32768;//32767 ��ԭʼ����  >32768��������ߣ���32768 ������ͣ����鷶Χ20000 - 50000
    pitch_param.speedV = 40;//>80���,<80 ���������鷶Χ30-130
    pitch_param.sr = dec->file_dec.sample_rate ;
    pitch_param.chn = dec->file_dec.output_ch_num ;
    dec->p_pitchspeed_hdl = open_pitchspeed(&pitch_param, NULL);
#endif
    if (!dec->file_dec.dec_no_out_sound) {
        audio_mode_main_dec_open(AUDIO_MODE_MAIN_STATE_DEC_FILE);
    }
    // ���õ��ӹ���
    audio_mixer_ch_open_head(&dec->mix_ch, p_mixer);
    audio_mixer_ch_set_src(&dec->mix_ch, 1, 0);

#if FILE_DEC_USE_OUT_TASK
    if (dec->file_dec.dec_no_out_sound == 0) {
        audio_decoder_out_task_ch_enable(&dec->file_dec.decoder, 1024);
    }
#endif

    dec_clock_add(dec->file_dec.decoder.dec_ops->coding_type);


    if (dec->stream_handler && (dec->pick_flag == 0)) {
        dec->stream_handler(dec->stream_priv, FILE_DEC_STREAM_OPEN, dec);
        goto __stream_set_end;
    }
    // eq\drc��Ч
    dec->eq_drc = file_eq_drc_open(dec->file_dec.sample_rate, dec->file_dec.output_ch_num);

#if TCFG_EQ_DIVIDE_ENABLE
    dec->eq_drc_rl_rr = file_rl_rr_eq_drc_open(dec->file_dec.sample_rate, dec->file_dec.output_ch_num);
    if (dec->eq_drc_rl_rr) {
        audio_vocal_tract_open(&dec->vocal_tract, AUDIO_SYNTHESIS_LEN);
        {
            u8 entry_cnt = 0;
            struct audio_stream_entry *entries[8] = {NULL};
            entries[entry_cnt++] = &dec->vocal_tract.entry;
            entries[entry_cnt++] = &dec->mix_ch.entry;
            dec->vocal_tract.stream = audio_stream_open(&dec->vocal_tract, audio_vocal_tract_stream_resume);
            audio_stream_add_list(dec->vocal_tract.stream, entries, entry_cnt);
        }
        audio_vocal_tract_synthesis_open(&dec->synthesis_ch_fl_fr, &dec->vocal_tract, FL_FR);
        audio_vocal_tract_synthesis_open(&dec->synthesis_ch_rl_rr, &dec->vocal_tract, RL_RR);
    } else {
        dec->ch_switch = channel_switch_open(AUDIO_CH_QUAD, AUDIO_SYNTHESIS_LEN / 2);
    }
#ifdef CONFIG_MIXER_CYCLIC
    audio_mixer_ch_set_aud_ch_out(&dec->mix_ch, 0, BIT(0));
    audio_mixer_ch_set_aud_ch_out(&dec->mix_ch, 1, BIT(1));
    audio_mixer_ch_set_aud_ch_out(&dec->mix_ch, 2, BIT(2));
    audio_mixer_ch_set_aud_ch_out(&dec->mix_ch, 3, BIT(3));
#endif

#endif

#if AUDIO_SURROUND_CONFIG
    dec->surround = surround_open_demo(dec->file_dec.output_ch_num);
#endif
#if AUDIO_VBASS_CONFIG
    dec->vbass = vbass_open_demo(dec->file_dec.sample_rate, dec->file_dec.output_ch_num);
#endif



    // ����������
    struct audio_stream_entry *entries[8] = {NULL};
    u8 entry_cnt = 0;
    entries[entry_cnt++] = &dec->file_dec.decoder.entry;

#if FILE_DEC_PICK_EN
    if (dec->pick_flag) {
        // ���ֱ�������localtws���ͣ��м䲻�����κδ���
        entries[entry_cnt++] = &g_localtws.push.entry;
    } else
#endif
    {
#if TCFG_SPEED_PITCH_ENABLE
        if (dec->p_pitchspeed_hdl) {
            entries[entry_cnt++] = &dec->p_pitchspeed_hdl->entry;
        }
#endif

#if TCFG_EQ_ENABLE && TCFG_MUSIC_MODE_EQ_ENABLE
        if (dec->eq_drc) {
            entries[entry_cnt++] = &dec->eq_drc->entry;
        }
#endif

#if AUDIO_SURROUND_CONFIG
        if (dec->surround) {
            entries[entry_cnt++] = &dec->surround->entry;
        }
#endif

#if AUDIO_VBASS_CONFIG
        if (dec->vbass) {
            entries[entry_cnt++] = &dec->vbass->entry;
        }
#endif

#if SYS_DIGVOL_GROUP_EN
        void *dvol_entry = sys_digvol_group_ch_open("music_file", -1, NULL);
        entries[entry_cnt++] = dvol_entry;
#endif // SYS_DIGVOL_GROUP_EN

#if TCFG_EQ_DIVIDE_ENABLE
        if (dec->eq_drc_rl_rr) {
            entries[entry_cnt++] = &dec->synthesis_ch_fl_fr.entry;//������eq����ʱ���ýڵ�󲻽ӽڵ�
        } else {
            if (dec->ch_switch) {
                entries[entry_cnt++] = &dec->ch_switch->entry;
            }
            entries[entry_cnt++] = &dec->mix_ch.entry;
        }
#else
        entries[entry_cnt++] = &dec->mix_ch.entry;
#endif
    }
    // �����������������нڵ���������
    dec->stream = audio_stream_open(dec, file_dec_out_stream_resume);
    audio_stream_add_list(dec->stream, entries, entry_cnt);

#if TCFG_EQ_DIVIDE_ENABLE
    if (dec->eq_drc_rl_rr) { //����eq_drc����һ���ڵ�
        audio_stream_add_entry(entries[0], &dec->eq_drc_rl_rr->entry);
        audio_stream_add_entry(&dec->eq_drc_rl_rr->entry, &dec->synthesis_ch_rl_rr.entry);
    }
#endif

__stream_set_end:
    log_i("total_time : %d \n", dec->file_dec.dec_total_time);

#if FILE_DEC_REPEAT_EN
    // �޷�ѭ������
    file_dec_repeat_set(3);
#endif

#if FILE_DEC_DEST_PLAY
    // ����ָ��λ�ò���
    file_dec_set_start_play(3 * 1000);
    /* u32 file_dec_dest_test_cb(void *priv); */
    /* file_dec_set_start_dest_play(2000, 4000, file_dec_dest_test_cb, dec); */
#endif

    // ������Ƶ�������
    audio_output_set_start_volume(APP_AUDIO_STATE_MUSIC);

    // �ļ��򿪾���ͣ
    if (file_dec_start_pause) {
        log_i("file_dec_start_pause\n");
        file_dec_start_pause = 0;
        dec->file_dec.status = FILE_DEC_STATUS_PAUSE;
        return 0;
    }

    // ����ʱ��
    clock_set_cur();

    if (dec->evt_cb) {
        int msg[2];
        msg[0] = AUDIO_DEC_EVENT_START;
        dec->evt_cb(dec->evt_priv, 2, msg);
    }

    // ��ʼ����
    dec->file_dec.status = FILE_DEC_STATUS_PLAY;
    err = audio_decoder_start(&dec->file_dec.decoder);
    if (err) {
        goto __err3;
    }
    return 0;

__err3:
    dec->file_dec.status = 0;
#if TCFG_SPEED_PITCH_ENABLE
    if (dec->p_pitchspeed_hdl) {
        close_pitchspeed(dec->p_pitchspeed_hdl);
    }
#endif
    file_eq_drc_close(dec->eq_drc);
#if TCFG_EQ_DIVIDE_ENABLE
    file_rl_rr_eq_drc_close(dec->eq_drc_rl_rr);
    audio_vocal_tract_synthesis_close(&dec->synthesis_ch_fl_fr);
    audio_vocal_tract_synthesis_close(&dec->synthesis_ch_rl_rr);
    audio_vocal_tract_close(&dec->vocal_tract);
    channel_switch_close(&dec->ch_switch);
#endif

#if AUDIO_SURROUND_CONFIG
    surround_close(dec->surround);
#endif
#if AUDIO_VBASS_CONFIG
    vbass_close_demo(dec->vbass);
#endif

    audio_mixer_ch_close(&dec->mix_ch);
#if TCFG_PCM_ENC2TWS_ENABLE
    if (file_dec->file_dec.dec_no_out_sound) {
        file_dec->file_dec.dec_no_out_sound = 0;
        localtws_enc_api_close();
    }
#endif
    if (dec->stream_handler) {
        dec->stream_handler(dec->stream_priv, FILE_DEC_STREAM_CLOSE, dec);
    }

#if SYS_DIGVOL_GROUP_EN
    sys_digvol_group_ch_close("music_file");
#endif // SYS_DIGVOL_GROUP_EN


    if (dec->stream) {
        audio_stream_close(dec->stream);
        dec->stream = NULL;
    }

    file_decoder_close(&dec->file_dec);
__err1:
    if (dec->evt_cb) {
        int msg[2];
        msg[0] = AUDIO_DEC_EVENT_ERR;
        dec->evt_cb(dec->evt_priv, 2, msg);
    }
    file_dec_release();
    // ����ʱ��
    clock_set_cur();
    return err;
}

/*----------------------------------------------------------------------------*/
/**@brief    �ļ�������Դ�ȴ�
   @param    *wait: ���
   @param    event: �¼�
   @return   0���ɹ�
   @note     ���ڶ�����ϴ��������ʱ����ͣ����
*/
/*----------------------------------------------------------------------------*/
static int file_wait_res_handler(struct audio_res_wait *wait, int event)
{
    int err = 0;

    log_i("file_wait_res_handler, event:%d, status:%d ", event, file_dec->file_dec.status);
    if (event == AUDIO_RES_GET) {
        // ��������
        if (file_dec->file_dec.status == 0) {
            err = file_dec_start();
        } else if (file_dec->file_dec.tmp_pause) {
            file_dec->file_dec.tmp_pause = 0;

            audio_output_set_start_volume(APP_AUDIO_STATE_MUSIC);
            if (file_dec->file_dec.status == FILE_DEC_STATUS_PLAY) {
                err = audio_decoder_start(&file_dec->file_dec.decoder);
                if (!file_dec->pick_flag) {
                    audio_mixer_ch_pause(&file_dec->mix_ch, 0);
                }
            }
        }
    } else if (event == AUDIO_RES_PUT) {
        // �����
        if (file_dec->file_dec.status) {
            if (file_dec->file_dec.status == FILE_DEC_STATUS_PLAY || \
                file_dec->file_dec.status == FILE_DEC_STATUS_PAUSE) {
                if (!file_dec->pick_flag) {
                    audio_mixer_ch_pause(&file_dec->mix_ch, 1);
                }
                err = audio_decoder_pause(&file_dec->file_dec.decoder);
                /* os_time_dly(2); */
                /* audio_output_stop(); */

            }
            file_dec->file_dec.tmp_pause = 1;
        }
    }

    return err;
}

/*----------------------------------------------------------------------------*/
/**@brief    file����pp����
   @param    play: 1-���š�0-��ͣ
   @return
   @note     �������ض���
*/
/*----------------------------------------------------------------------------*/
static void file_dec_pp_ctrl(u8 play)
{
    if (!file_dec) {
        return ;
    }
    if (play) {
        // ����ǰ����
#if (!TCFG_MIC_EFFECT_ENABLE)
        clock_pause_play(0);
#endif/*TCFG_MIC_EFFECT_ENABLE*/
        if (!file_dec->pick_flag) {
            audio_mixer_ch_pause(&file_dec->mix_ch, 0);
        }
#if TCFG_DEC2TWS_ENABLE
        if (file_dec->file_dec.dec_no_out_sound) {
            localtws_decoder_pause(0);
        }
#endif
    } else {
        // ��ͣ����
#if TCFG_DEC2TWS_ENABLE
        if (file_dec->file_dec.dec_no_out_sound) {
            localtws_decoder_pause(1);
        }
#endif
        if (!file_dec->pick_flag) {
            audio_mixer_ch_pause(&file_dec->mix_ch, 1);
            //audio_decoder_resume_all(&decode_task);
        }
        if (audio_mixer_get_active_ch_num(&mixer) == 0) {
#if (!TCFG_MIC_EFFECT_ENABLE)
            clock_pause_play(1);
#endif/*TCFG_MIC_EFFECT_ENABLE*/
        }
    }
}

/*----------------------------------------------------------------------------*/
/**@brief    ����һ���ļ�����
   @param    *priv: �¼��ص�˽�в���
   @param    *handler: �¼��ص����
   @return   0���ɹ�
   @return   ��0��ʧ��
   @note
*/
/*----------------------------------------------------------------------------*/
int file_dec_create(void *priv, void (*handler)(void *, int argc, int *argv))
{
    struct file_dec_hdl *dec;
    if (file_dec) {
        file_dec_close();
    }

    dec = zalloc(sizeof(*dec));
    if (!dec) {
        return -ENOMEM;
    }

    file_dec = dec;
    file_dec->evt_cb = handler;
    file_dec->evt_priv = priv;

    return 0;
}

/*----------------------------------------------------------------------------*/
/**@brief    ���ý������������ûص��ӿ�
   @param    *dec: ������
   @param    *stream_handler: ���������ûص�
   @param    *stream_priv: ���������ûص�˽�о��
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void file_dec_set_stream_set_hdl(struct file_dec_hdl *dec,
                                 void (*stream_handler)(void *priv, int event, struct file_dec_hdl *),
                                 void *stream_priv)
{
    if (dec) {
        dec->stream_handler = stream_handler;
        dec->stream_priv = stream_priv;
    }
}

/*----------------------------------------------------------------------------*/
/**@brief    ���ļ�����
   @param    *file: �ļ����
   @param    *bp: �ϵ���Ϣ
   @return   0���ɹ�
   @return   ��0��ʧ��
   @note
*/
/*----------------------------------------------------------------------------*/
int file_dec_open(void *file, struct audio_dec_breakpoint *bp)
{
    int err;
    struct file_dec_hdl *dec = file_dec;

    log_i("file_dec_open: in, 0x%x, bp:0x%x \n", file, bp);

    if ((!dec) || (!file)) {
        return -EPERM;
    }
    dec->file = file;
    dec->bp = bp;
    dec->id = rand32();

    dec->file_dec.ch_type = AUDIO_CH_MAX;
    dec->file_dec.output_ch_num = audio_output_channel_num();
    dec->file_dec.output_ch_type = audio_output_channel_type();

#if TCFG_DEC_DECRYPT_ENABLE
    cipher_init(&dec->mply_cipher, TCFG_DEC_DECRYPT_KEY);
    cipher_check_decode_file(&dec->mply_cipher, file);
#endif

#if TCFG_DEC2TWS_ENABLE
    // ����localtws�ز��ӿ�
    localtws_globle_set_dec_restart(file_dec_push_restart);
#endif

    dec->wait.priority = 1;
    dec->wait.preemption = 0;
    dec->wait.snatch_same_prio = 1;
    dec->wait.handler = file_wait_res_handler;
    err = audio_decoder_task_add_wait(&decode_task, &dec->wait);

    return err;
}

/*----------------------------------------------------------------------------*/
/**@brief    �ر��ļ�����
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void file_dec_close(void)
{
    if (!file_dec) {
        return;
    }

    if (file_dec->file_dec.status) {
        if (file_dec->file_dec.dec_no_out_sound == 0) {
            audio_mixer_ch_try_fadeout(&file_dec->mix_ch, 50);
        }
        file_dec->file_dec.status = 0;
        audio_mixer_ch_pause(&file_dec->mix_ch, 1);
        file_decoder_close(&file_dec->file_dec);
#if TCFG_SPEED_PITCH_ENABLE
        if (file_dec->p_pitchspeed_hdl) {
            close_pitchspeed(file_dec->p_pitchspeed_hdl);
        }
#endif
        file_eq_drc_close(file_dec->eq_drc);
#if TCFG_EQ_DIVIDE_ENABLE
        file_rl_rr_eq_drc_close(file_dec->eq_drc_rl_rr);
        audio_vocal_tract_synthesis_close(&file_dec->synthesis_ch_fl_fr);
        audio_vocal_tract_synthesis_close(&file_dec->synthesis_ch_rl_rr);
        audio_vocal_tract_close(&file_dec->vocal_tract);
        channel_switch_close(&file_dec->ch_switch);
#endif
#if AUDIO_SURROUND_CONFIG
        surround_close(file_dec->surround);
#endif
#if AUDIO_VBASS_CONFIG
        vbass_close_demo(file_dec->vbass);
#endif

        audio_mixer_ch_close(&file_dec->mix_ch);
#if TCFG_PCM_ENC2TWS_ENABLE
        if (file_dec->file_dec.dec_no_out_sound) {
            file_dec->file_dec.dec_no_out_sound = 0;
            localtws_enc_api_close();
        }
#endif
        if (file_dec->stream_handler) {
            file_dec->stream_handler(file_dec->stream_priv, FILE_DEC_STREAM_CLOSE, file_dec);
        }
#if SYS_DIGVOL_GROUP_EN
        sys_digvol_group_ch_close("music_file");
#endif // SYS_DIGVOL_GROUP_EN


        // �ȹرո����ڵ㣬����close������
        if (file_dec->stream) {
            audio_stream_close(file_dec->stream);
            file_dec->stream = NULL;
        }

    }

    file_dec_release();

    clock_set_cur();
    log_i("file_dec_close: exit\n");
}

/*----------------------------------------------------------------------------*/
/**@brief    ��ȡfile_dec���
   @param
   @return   file_dec���
   @note
*/
/*----------------------------------------------------------------------------*/
struct file_decoder *file_dec_get_file_decoder_hdl(void)
{
    if (file_dec) {
        return &file_dec->file_dec;
    }
    return NULL;
}

/*----------------------------------------------------------------------------*/
/**@brief    ��ȡfile_dec״̬
   @param
   @return   ����״̬
   @note
*/
/*----------------------------------------------------------------------------*/
int file_dec_get_status(void)
{
    struct file_decoder *dec = file_dec_get_file_decoder_hdl();
    if (dec) {
        return dec->status;
    }
    return FILE_DEC_STATUS_STOP;
}

/*----------------------------------------------------------------------------*/
/**@brief    �ļ��������¿�ʼ
   @param    id: �ļ�����id
   @return   0���ɹ�
   @return   ��0��ʧ��
   @note
*/
/*----------------------------------------------------------------------------*/
int file_dec_restart(int id)
{
    if ((!file_dec) || (id != file_dec->id)) {
        return -1;
    }
    if (file_dec->bp == NULL) {
        if (file_dec->dec_bp == NULL) {
            file_dec->dec_bp = zalloc(sizeof(struct audio_dec_breakpoint) + BREAKPOINT_DATA_LEN);
            ASSERT(file_dec->dec_bp);
            file_dec->dec_bp->data_len = BREAKPOINT_DATA_LEN;
        }
        file_dec->bp = file_dec->dec_bp;
    }
    if (file_dec->file_dec.status && file_dec->bp) {
        audio_decoder_get_breakpoint(&file_dec->file_dec.decoder, file_dec->bp);
    }

    void *file = file_dec->file;
    void *bp = file_dec->bp;
    void *evt_cb = file_dec->evt_cb;
    void *evt_priv = file_dec->evt_priv;
    int err;
    void *dec_bp = file_dec->dec_bp; // �ȱ���һ�£�����close���ͷ�
    file_dec->dec_bp = NULL;

    file_dec_close();
    err = file_dec_create(evt_priv, evt_cb);
    if (!err) {
        file_dec->dec_bp = dec_bp; // ��ԭ��ȥ
        err = file_dec_open(file, bp);
    } else {
        if (dec_bp) {
            free(dec_bp); // ʧ�ܣ��ͷ�
        }
    }
    return err;
}

/*----------------------------------------------------------------------------*/
/**@brief    �����ļ��������¿�ʼ����
   @param
   @return   true���ɹ�
   @return   false��ʧ��
   @note
*/
/*----------------------------------------------------------------------------*/
int file_dec_push_restart(void)
{
    if (!file_dec) {
        return false;
    }
    int argv[3];
    argv[0] = (int)file_dec_restart;
    argv[1] = 1;
    argv[2] = (int)file_dec->id;
    os_taskq_post_type(os_current_task(), Q_CALLBACK, ARRAY_SIZE(argv), argv);
    return true;
}

#if FILE_DEC_DEST_PLAY
static u32 file_dec_dest_test_cb(void *priv)
{
    struct file_dec_hdl *dec = priv;
    static u8 cnt = 0;
    if (cnt < 3) {
        cnt ++;
        printf("file_dec_dest_test_cb");
        struct audio_dest_time_play_param param = {0};
        param.start_time = 20 * 1000;
        param.dest_time = 30 * 1000;
        param.callback_func = file_dec_dest_test_cb;
        param.callback_priv = dec;
        audio_decoder_ioctrl(&dec->file_dec.decoder, AUDIO_IOCTRL_CMD_SET_DEST_PLAYPOS, &param);
    }
    return 0;
}

/*----------------------------------------------------------------------------*/
/**@brief    ����ָ��λ�ÿ�ʼ���ţ����ŵ�Ŀ��ʱ���ص�
   @param    start_time: Ҫ��ת��ȥ���ŵ���ʼʱ��
   @param    dest_time: Ҫ��ת��ȥ���ŵ�Ŀ��ʱ��
   @param    *cb: ����Ŀ��ʱ���ص�
   @param    *cb_priv: �ص�����
   @return   true���ɹ�
   @return   false��ʧ��
   @note
*/
/*----------------------------------------------------------------------------*/
int file_dec_set_start_dest_play(u32 start_time, u32 dest_time, u32(*cb)(void *), void *cb_priv)
{
    struct file_dec_hdl *dec = file_dec;
    if (!dec || !dec->file_dec.decoder.dec_ops) {
        return false;
    }
    switch (dec->file_dec.decoder.dec_ops->coding_type) {
    case AUDIO_CODING_MP3: {
        struct audio_dest_time_play_param param = {0};
        param.start_time = start_time;
        param.dest_time = dest_time;
        param.callback_func = cb;
        param.callback_priv = cb_priv;
        audio_decoder_ioctrl(&dec->file_dec.decoder, AUDIO_IOCTRL_CMD_SET_DEST_PLAYPOS, &param);
    }
    return true;
    }
    return false;
}

/*----------------------------------------------------------------------------*/
/**@brief    ����ָ��λ�ÿ�ʼ����
   @param    start_time: Ҫ��ת��ȥ���ŵ���ʼʱ��
   @return   true���ɹ�
   @return   false��ʧ��
   @note
*/
/*----------------------------------------------------------------------------*/
int file_dec_set_start_play(u32 start_time)
{
    return file_dec_set_start_dest_play(start_time, 0x7fffffff, NULL, NULL);
}
#endif

#if FILE_DEC_REPEAT_EN
/*----------------------------------------------------------------------------*/
/**@brief    ѭ�����Żص��ӿ�
   @param    *priv: ˽�в���
   @return   0��ѭ������
   @return   ��0������ѭ��
   @note
*/
/*----------------------------------------------------------------------------*/
static int file_dec_repeat_cb(void *priv)
{
    struct file_dec_hdl *dec = priv;
    y_printf("file_dec_repeat_cb\n");
    if (dec->repeat_num) {
        dec->repeat_num--;
    } else {
        y_printf("file_dec_repeat_cb end\n");
        return -1;
    }
    return 0;
}

/*----------------------------------------------------------------------------*/
/**@brief    ����ѭ�����Ŵ���
   @param    repeat_num: ѭ������
   @return   true���ɹ�
   @return   false��ʧ��
   @note
*/
/*----------------------------------------------------------------------------*/
int file_dec_repeat_set(u8 repeat_num)
{
    struct file_dec_hdl *dec = file_dec;
    if (!dec || !dec->file_dec.decoder.dec_ops) {
        return false;
    }
    switch (dec->file_dec.decoder.dec_ops->coding_type) {
    case AUDIO_CODING_MP3:
    case AUDIO_CODING_WAV: {
        dec->repeat_num = repeat_num;
        struct audio_repeat_mode_param rep = {0};
        rep.flag = 1; //ʹ��
        rep.headcut_frame = 2; //�������󿳵�ǰ�漸֡����mp3��ʽ��Ч
        rep.tailcut_frame = 2; //�������󿳵����漸֡����mp3��ʽ��Ч
        rep.repeat_callback = file_dec_repeat_cb;
        rep.callback_priv = dec;
        rep.repair_buf = &dec->repair_buf;
        audio_decoder_ioctrl(&dec->file_dec.decoder, AUDIO_IOCTRL_CMD_REPEAT_PLAY, &rep);
    }
    return true;
    }
    return false;
}
#endif


#if FILE_DEC_AB_REPEAT_EN

#define AUDIO_AB_REPEAT_CODING_TYPE		(AUDIO_CODING_MP3 | AUDIO_CODING_WMA | AUDIO_CODING_WAV | AUDIO_CODING_FLAC | AUDIO_CODING_APE | AUDIO_CODING_DTS)

enum {
    AB_REPEAT_STA_NON = 0,
    AB_REPEAT_STA_ASTA,
    AB_REPEAT_STA_BSTA,
};

/*----------------------------------------------------------------------------*/
/**@brief    ����AB�㸴������
   @param    ab_cmd: ����
   @param    ab_mode: ����
   @return   true:�ɹ�
   @note
*/
/*----------------------------------------------------------------------------*/
static int file_dec_ab_repeat_set(int ab_cmd, int ab_mode)
{
    struct file_dec_hdl *dec = file_dec;
    if (!dec || !dec->file_dec.decoder.dec_ops) {
        return false;
    }
    y_printf("ab repat, cmd:0x%x, mode:%d \n", ab_cmd, ab_mode);
    struct audio_ab_repeat_mode_param rpt = {0};
    rpt.value = ab_mode;
    audio_decoder_ioctrl(&dec->file_dec.decoder, ab_cmd, &rpt);
    return true;
}

/*----------------------------------------------------------------------------*/
/**@brief    ����Ƿ����AB�㸴��
   @param
   @return   true:�ɹ�
   @note
*/
/*----------------------------------------------------------------------------*/
static int file_dec_ab_probe_check(void)
{
    struct file_dec_hdl *dec = file_dec;
    if (!dec || !dec->file_dec.decoder.dec_ops) {
        return false;
    }
    if (false == file_decoder_is_play(&dec->file_dec)) {
        return false;
    }
    if (dec->file_dec.decoder.dec_ops->coding_type & AUDIO_CODING_STU_PICK) {
        return false;
    }
    if (!(dec->file_dec.decoder.dec_ops->coding_type & AUDIO_AB_REPEAT_CODING_TYPE)) {
        return false;
    }
    return true;
}

/*----------------------------------------------------------------------------*/
/**@brief    �л�AB�㸴��״̬
   @param
   @return   true:�ɹ�
   @note
*/
/*----------------------------------------------------------------------------*/
int file_dec_ab_repeat_switch(void)
{
    struct file_dec_hdl *dec = file_dec;
    if (false == file_dec_ab_probe_check()) {
        return false;
    }
    switch (dec->ab_repeat_status) {
    case AB_REPEAT_STA_NON:
        if (file_dec_ab_repeat_set(AUDIO_IOCTRL_CMD_SET_BREAKPOINT_A, 0)) {
            dec->ab_repeat_status = AB_REPEAT_STA_ASTA;
        }
        break;
    case AB_REPEAT_STA_ASTA:
        if (file_dec_ab_repeat_set(AUDIO_IOCTRL_CMD_SET_BREAKPOINT_B, 0)) {
            dec->ab_repeat_status = AB_REPEAT_STA_BSTA;
        }
        break;
    case AB_REPEAT_STA_BSTA:
        if (file_dec_ab_repeat_set(AUDIO_IOCTRL_CMD_SET_BREAKPOINT_MODE, AB_REPEAT_MODE_CUR)) {
            dec->ab_repeat_status = AB_REPEAT_STA_NON;
        }
        break;
    }
    printf("file_dec_ab_repeat_switch = %d\n", dec->ab_repeat_status);
    return true;
}

/*----------------------------------------------------------------------------*/
/**@brief    �ر�AB�㸴��
   @param
   @return   true:�ɹ�
   @note
*/
/*----------------------------------------------------------------------------*/
int file_dec_ab_repeat_close(void)
{
    struct file_dec_hdl *dec = file_dec;
    if (false == file_dec_ab_probe_check()) {
        return false;
    }

    if (dec->ab_repeat_status == AB_REPEAT_STA_NON) {
        return true;
    }

    if (dec->ab_repeat_status == AB_REPEAT_STA_ASTA) {
        switch (dec->file_dec.decoder.dec_ops->coding_type) {
        case AUDIO_CODING_FLAC:
        case AUDIO_CODING_DTS:
        case AUDIO_CODING_APE:
            file_dec_ab_repeat_set(AUDIO_IOCTRL_CMD_SET_BREAKPOINT_B, 0);
            break;
        }
    }

    file_dec_ab_repeat_set(AUDIO_IOCTRL_CMD_SET_BREAKPOINT_MODE, AB_REPEAT_MODE_CUR);
    dec->ab_repeat_status = AB_REPEAT_STA_NON;
    return true;
}

#endif /*FILE_DEC_AB_REPEAT_EN*/

#endif /*TCFG_APP_MUSIC_EN*/


/*----------------------------------------------------------------------------*/
/**@brief    file decoder pp����
   @param    *dec: file������
   @param    play: 1-���š�0-��ͣ
   @return
   @note     �������ض���
*/
/*----------------------------------------------------------------------------*/
void file_decoder_pp_ctrl(struct file_decoder *dec, u8 play)
{
#if TCFG_APP_MUSIC_EN
    if (file_dec && (&file_dec->file_dec == dec)) {
        file_dec_pp_ctrl(play);
    }
#endif /*TCFG_APP_MUSIC_EN*/
}



/*----------------------------------------------------------------------------*/
/**@brief    ����ģʽ eq drc ��
   @param    sample_rate:������
   @param    ch_num:ͨ������
   @return   ���
   @note
*/
/*----------------------------------------------------------------------------*/
void *file_eq_drc_open(u16 sample_rate, u8 ch_num)
{

#if TCFG_EQ_ENABLE

    struct audio_eq_drc *eq_drc = NULL;
    struct audio_eq_drc_parm effect_parm = {0};
#if TCFG_MUSIC_MODE_EQ_ENABLE
    effect_parm.eq_en = 1;

#if TCFG_DRC_ENABLE
#if TCFG_MUSIC_MODE_DRC_ENABLE
    effect_parm.drc_en = 1;
    effect_parm.drc_cb = drc_get_filter_info;
#endif
#endif

    if (effect_parm.eq_en) {
        effect_parm.async_en = 1;
        if (effect_parm.drc_en) {
            effect_parm.out_32bit = 1;
        }
        effect_parm.online_en = 1;
        effect_parm.mode_en = 1;
    }

    effect_parm.eq_name = song_eq_mode;
#if TCFG_EQ_DIVIDE_ENABLE
    effect_parm.divide_en = 1;
#endif

    effect_parm.ch_num = ch_num;
    effect_parm.sr = sample_rate;
    effect_parm.eq_cb = eq_get_filter_info;
    printf("ch_num %d\n,sr %d\n", ch_num, sample_rate);
    eq_drc = audio_eq_drc_open(&effect_parm);

#if TCFG_EQ_DIVIDE_ENABLE
    audio_eq_set_check_running(eq_drc->eq, 1);
#endif

    clock_add(EQ_CLK);
    if (effect_parm.drc_en) {
        clock_add(EQ_DRC_CLK);
    }
#endif
    return eq_drc;
#endif
    return NULL;
}

/*----------------------------------------------------------------------------*/
/**@brief    ����ģʽ eq drc �ر�
   @param    ���
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void file_eq_drc_close(struct audio_eq_drc *eq_drc)
{
#if TCFG_EQ_ENABLE
#if TCFG_MUSIC_MODE_EQ_ENABLE
    if (eq_drc) {
        audio_eq_drc_close(eq_drc);
        eq_drc = NULL;
        clock_remove(EQ_CLK);
#if TCFG_DRC_ENABLE
#if TCFG_MUSIC_MODE_DRC_ENABLE
        clock_remove(EQ_DRC_CLK);
#endif
#endif
    }
#endif
#endif
    return;
}

/*----------------------------------------------------------------------------*/
/**@brief    ����ģʽ RL RR ͨ��eq drc ��
   @param    sample_rate:������
   @param    ch_num:ͨ������
   @return   ���
   @note
*/
/*----------------------------------------------------------------------------*/
void *file_rl_rr_eq_drc_open(u16 sample_rate, u8 ch_num)
{

#if TCFG_EQ_ENABLE

    struct audio_eq_drc *eq_drc = NULL;
    struct audio_eq_drc_parm effect_parm = {0};
#if TCFG_MUSIC_MODE_EQ_ENABLE
    effect_parm.eq_en = 1;

#if TCFG_DRC_ENABLE
#if TCFG_MUSIC_MODE_DRC_ENABLE
    effect_parm.drc_en = 1;
    effect_parm.drc_cb = drc_get_filter_info;
#endif
#endif


    if (effect_parm.eq_en) {
        effect_parm.async_en = 1;
        effect_parm.out_32bit = 1;
        effect_parm.online_en = 1;
        effect_parm.mode_en = 1;
    }

#if TCFG_EQ_DIVIDE_ENABLE
    effect_parm.divide_en = 1;
    effect_parm.eq_name = rl_eq_mode;
#endif


    effect_parm.ch_num = ch_num;
    effect_parm.sr = sample_rate;
    effect_parm.eq_cb = eq_get_filter_info;
    log_i("ch_num %d\n,sr %d\n", ch_num, sample_rate);
    eq_drc = audio_eq_drc_open(&effect_parm);

#if TCFG_EQ_DIVIDE_ENABLE
    audio_eq_set_check_running(eq_drc->eq, 1);
#endif

    clock_add(EQ_CLK);
    if (effect_parm.drc_en) {
        clock_add(EQ_DRC_CLK);
    }
#endif
    return eq_drc;
#endif
    return NULL;
}

/*----------------------------------------------------------------------------*/
/**@brief    ����ģʽ RL RR ͨ��eq drc �ر�
   @param    ���
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void file_rl_rr_eq_drc_close(struct audio_eq_drc *eq_drc)
{
#if TCFG_EQ_ENABLE
#if TCFG_MUSIC_MODE_EQ_ENABLE
    if (eq_drc) {
        audio_eq_drc_close(eq_drc);
        eq_drc = NULL;
        clock_remove(EQ_CLK);
#if TCFG_DRC_ENABLE
#if TCFG_MUSIC_MODE_DRC_ENABLE
        clock_remove(EQ_DRC_CLK);
#endif
#endif
    }
#endif
#endif
    return;
}

/*----------------------------------------------------------------------------*/
/**@brief    ������Ч����������л�����
   @param    eff_mode:
			KARAOKE_SPK_OST,//ԭ��
			KARAOKE_SPK_DBB,//�ص���
			KARAOKE_SPK_SURROUND,//ȫ������
			KARAOKE_SPK_3D,//3d����
			KARAOKE_SPK_FLOW_VOICE,//��������
			KARAOKE_SPK_KING,//������ҫ
			KARAOKE_SPK_WAR,//�ļ�ս��
			KARAOKE_SPK_MAX,

   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void audio_file_effect_switch(u8 eff_mode)
{
#if TCFG_APP_MUSIC_EN
    if (!file_dec) {
        return;
    }
    log_i("SPK eff***************:%d ", eff_mode);
#if AUDIO_SURROUND_CONFIG
    surround_hdl *surround = file_dec->surround;
#if AUDIO_VBASS_CONFIG
    vbass_hdl *vbass = file_dec->vbass;
    if (eff_mode != KARAOKE_SPK_DBB) {
        vbass_switch(vbass, 0);
    }
#endif
    switch (eff_mode) {
    case KARAOKE_SPK_OST://ԭ��
        surround_effect_switch(surround, EFFECT_OFF);
        log_i("spk_OST\n");
        break;
    case KARAOKE_SPK_DBB://�ص���
        /* tone_play_index(IDEX_TONE_VABSS, 1); */
        surround_effect_switch(surround, EFFECT_OFF);//����Ҫ�û�ԭ������ĵط���һ��

#if AUDIO_VBASS_CONFIG
        vbass_switch(vbass, 1);
#endif
        log_i("spk_DDB\n");
        break;
    case KARAOKE_SPK_SURROUND://ȫ������
        /* tone_play_index(IDEX_TONE_SURROUND, 1); */
        surround_effect_switch(surround, EFFECT_3D_PANORAMA);
        log_i("spk_SURRROUND\n");
        break;
    case KARAOKE_SPK_3D://3d��ת
        /* tone_play_index(IDEX_TONE_3D, 1); */
        surround_effect_switch(surround, EFFECT_3D_ROTATES);
        log_i("spk_3D\n");
        break;
    case KARAOKE_SPK_FLOW_VOICE://��������
        /* tone_play_index(IDEX_TONE_FLOW, 1); */
        surround_effect_switch(surround, EFFECT_FLOATING_VOICE);
        log_i("spk_FLOW\n");
        break;
    case KARAOKE_SPK_KING://������ҫ
        /* tone_play_index(IDEX_TONE_KING, 1); */
        surround_effect_switch(surround, EFFECT_GLORY_OF_KINGS);
        log_i("spk_KING\n");
        break;
    case KARAOKE_SPK_WAR://�ļ�ս��
        /* tone_play_index(IDEX_TONE_WAR, 1); */
        surround_effect_switch(surround, EFFECT_FOUR_SENSION_BATTLEFIELD);
        log_i("spk_WAR\n");
        break;
    default:
        log_i("spk_ERROR\n");
        break;
    }
#endif
#endif
}

