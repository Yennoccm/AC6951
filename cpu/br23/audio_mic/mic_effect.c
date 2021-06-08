#include "effect_tool.h"
#include "effect_debug.h"
/* #include "reverb/reverb_api.h" */
/* #include "application/audio_dig_vol.h" */
#include "audio_splicing.h"
/* #include "effect_config.h" */
#include "application/audio_bfilt.h"
/* #include "audio_effect/audio_eq.h" */
#include "application/audio_eq_drc_apply.h"
#include "application/audio_echo_src.h"
#include "application/audio_energy_detect.h"
#include "clock_cfg.h"
#include "media/audio_stream.h"
#include "media/includes.h"
#include "mic_effect.h"
#include "asm/dac.h"
#include "audio_enc.h"
#include "audio_dec.h"
#include "stream_entry.h"
#include "effect_linein.h"
#include "audio_recorder_mix.h"
#define LOG_TAG     "[APP-REVERB]"
#define LOG_ERROR_ENABLE
#define LOG_INFO_ENABLE
#define LOG_DUMP_ENABLE
#include "debug.h"

#if (TCFG_MIC_EFFECT_ENABLE)

extern struct audio_dac_hdl dac_hdl;
extern int *get_outval_addr(u8 mode);

enum {
    MASK_REVERB = 0x0,
    MASK_PITCH,
    MASK_ECHO,
    MASK_NOISEGATE,
    MASK_SHOUT_WHEAT,
    MASK_LOW_SOUND,
    MASK_HIGH_SOUND,
    MASK_EQ,
    MASK_EQ_SEG,
    MASK_EQ_GLOBAL_GAIN,
    MASK_MIC_GAIN,
    MASK_MAX,
};

typedef struct _BFILT_API_STRUCT_ {
    SHOUT_WHEAT_PARM_SET 	shout_wheat;
    LOW_SOUND_PARM_SET 		low_sound;
    HIGH_SOUND_PARM_SET 	high_sound;
    audio_bfilt_hdl             *hdl;     //ϵ��������
} BFILT_API_STRUCT;

struct __fade {
    int wet;
    u32 delay;
    u32 decayval;
};
extern struct audio_mixer mixer;
struct __mic_effect {
    OS_MUTEX				 		mutex;
    struct __mic_effect_parm     	parm;
    struct __fade	 				fade;
    volatile u32					update_mask;
    mic_stream 						*mic;
    /* PITCH_SHIFT_PARM        		*p_set; */
    /* NOISEGATE_API_STRUCT    		*n_api; */
    BFILT_API_STRUCT 				*filt;
    struct audio_eq_drc             *eq_drc;    //eq drc���

    struct audio_stream *stream;		// ��Ƶ��
    struct audio_stream_entry entry;	// effect ��Ƶ���
    int out_len;
    int process_len;
    u8 input_ch_num;                      //mic�����������������Դ������

    struct audio_mixer_ch mix_ch;//for test

    REVERBN_API_STRUCT 		*p_reverb_hdl;
    ECHO_API_STRUCT 		*p_echo_hdl;
    s_pitch_hdl 			*p_pitch_hdl;
    NOISEGATE_API_STRUCT	*p_noisegate_hdl;
    void            		*d_vol;
    HOWLING_API_STRUCT 		*p_howling_hdl;
    ECHO_SRC_API_STRUCT 	*p_echo_src_hdl;
    struct channel_switch   *channel_zoom;
    struct audio_dac_channel *dac;
#if (RECORDER_MIX_EN)
    struct __stream_entry 	*rec_hdl;
#endif
#if (TCFG_USB_MIC_DATA_FROM_MICEFFECT||TCFG_USB_MIC_DATA_FROM_DAC)
    struct __stream_entry 	*usbmic_hdl;
    u8    usbmic_start;
#endif
    void *energy_hdl;     //��������hdl
    u8 dodge_en;          //����������й��������Ƿ�ʹ��

    struct __effect_linein *linein;

    u8  pause_mark;
};

struct __mic_stream_parm *g_mic_parm = NULL;
static struct __mic_effect *p_effect = NULL;
#define __this  p_effect
#define R_ALIN(var,al)     ((((var)+(al)-1)/(al))*(al))

void *mic_eq_drc_open(u32 sample_rate, u8 ch_num);
void mic_eq_drc_close(struct audio_eq_drc *eq_drc);
void mic_eq_drc_update();
BFILT_API_STRUCT *mic_high_bass_coeff_cal_init(u32 sample_rate);
void mic_high_bass_coeff_cal_uninit(BFILT_API_STRUCT *filt);
void mic_effect_echo_parm_parintf(ECHO_PARM_SET *parm);


void *mic_energy_detect_open(u32 sr, u8 ch_num);
void mic_energy_detect_close(void *hdl);

/*----------------------------------------------------------------------------*/
/**@brief    mic��������Ч�����������
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
static void mic_effect_parm_update(struct __mic_effect *effect)
{
    for (int i = 0; i < MASK_MAX; i++) {
        if (effect->update_mask & BIT(i)) {
            effect->update_mask &= ~BIT(i);
            switch (i) {
            case MASK_REVERB:
                break;
            case MASK_PITCH:
                break;
            case MASK_ECHO:
                break;
            case MASK_NOISEGATE:
                break;
            case MASK_SHOUT_WHEAT:
                break;
            case MASK_LOW_SOUND:
                break;
            case MASK_HIGH_SOUND:
                break;
            case MASK_EQ:
                break;
            case MASK_EQ_SEG:
                break;
            case MASK_EQ_GLOBAL_GAIN:
                break;
            case MASK_MIC_GAIN:
                break;
            }
        }
    }
}
/*----------------------------------------------------------------------------*/
/**@brief    mic����������뵭������
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
static void mic_effect_fade_run(struct __mic_effect *effect)
{
    if (effect == NULL) {
        return ;
    }
    u8 update = 0;
#if 10
    if (effect->p_reverb_hdl) {
        int wet = effect->p_reverb_hdl->parm.wet;
        if (wet != effect->fade.wet) {
            update = 1;
            if (wet > effect->fade.wet) {
                wet --;
            } else {
                wet ++;
            }
        }
        if (update) {
            effect->p_reverb_hdl->parm.wet = wet;
            update_reverb_parm(effect->p_reverb_hdl, &effect->p_reverb_hdl->parm);
        }
    }
    update = 0;
    if (effect->p_echo_hdl) {
        int delay = effect->p_echo_hdl->echo_parm_obj.delay;
        int decayval = effect->p_echo_hdl->echo_parm_obj.decayval;
        if (delay != effect->fade.delay) {
            update = 1;
            delay = effect->fade.delay;
        }
        if (decayval != effect->fade.decayval) {
            update = 1;
            if (decayval > effect->fade.decayval) {
                decayval --;
            } else {
                decayval ++;
            }
        }
        if (update) {
            effect->p_echo_hdl->echo_parm_obj.delay = delay;
            effect->p_echo_hdl->echo_parm_obj.decayval = decayval;
            update_echo_parm(effect->p_echo_hdl, &effect->p_echo_hdl->echo_parm_obj);
        }
    }
#endif
}
/*----------------------------------------------------------------------------*/
/**@brief    mic�������������
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
/* #define TEST_PORT IO_PORTA_00 */
static u32 mic_effect_effect_run(void *priv, void *in, void *out, u32 inlen, u32 outlen)
{
    struct __mic_effect *effect = (struct __mic_effect *)priv;
    if (effect == NULL) {
        return 0;
    }
    struct audio_data_frame frame = {0};
    frame.channel = effect->input_ch_num;
    frame.sample_rate = effect->parm.sample_rate;
    frame.data_len = inlen;
    frame.data = in;
    effect->out_len = 0;
    effect->process_len = inlen;

    if (effect->pause_mark) {
        memset(in, 0, inlen);
        /* return outlen; */
    }
    mic_effect_fade_run(effect);//reverb ����echo �������뵭��
    while (1) {
        audio_stream_run(&effect->entry, &frame);
        if (effect->out_len >= effect->process_len) {
            break;
        }
        frame.data = (s16 *)((u8 *)in + effect->out_len);
        frame.data_len = inlen - effect->out_len;
    }
    return outlen;
}

/*----------------------------------------------------------------------------*/
/**@brief   �ͷ�mic��������Դ
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
static void mic_effect_destroy(struct __mic_effect **hdl)
{
    if (hdl == NULL || *hdl == NULL) {
        return ;
    }
    struct __mic_effect *effect = *hdl;
    if (effect->mic) {
        log_i("mic_stream_destroy\n\n\n");
        mic_stream_destroy(&effect->mic);
    }

#if TCFG_MIC_DODGE_EN
    if (effect->energy_hdl) {
        mic_energy_detect_close(effect->energy_hdl);
    }
#endif

    if (effect->p_noisegate_hdl) {
        log_i("close_noisegate\n\n\n");
        close_noisegate(effect->p_noisegate_hdl);
    }
    if (effect->p_howling_hdl) {
        log_i("close_howling\n\n\n");
        close_howling(effect->p_howling_hdl);
    }

    if (effect->eq_drc) {
        log_i("mic_eq_drc_close\n\n\n");
        mic_eq_drc_close(effect->eq_drc);
    }
    if (effect->p_pitch_hdl) {
        log_i("close_pitch\n\n\n");
        close_pitch(effect->p_pitch_hdl);
    }

    if (effect->p_reverb_hdl) {
        log_i("close_reverb\n\n\n");
        close_reverb(effect->p_reverb_hdl);
    }
    if (effect->p_echo_hdl) {
        log_i("close_echo\n\n\n");
        close_echo(effect->p_echo_hdl);
    }


    if (effect->filt) {
        log_i("mic_high_bass_coeff_cal_uninit\n\n\n");
        mic_high_bass_coeff_cal_uninit(effect->filt);
    }
    if (effect->d_vol) {
        audio_stream_del_entry(audio_dig_vol_entry_get(effect->d_vol));
#if SYS_DIGVOL_GROUP_EN
        sys_digvol_group_ch_close("mic_mic");
#else
        audio_dig_vol_close(effect->d_vol);
#endif/*SYS_DIGVOL_GROUP_EN*/
    }
    if (effect->linein) {
        effect_linein_close(&effect->linein);
    }
    if (effect->p_echo_src_hdl) {
        log_i("close_echo src\n\n\n");
        close_echo_src(effect->p_echo_src_hdl);
    }
    if (effect->channel_zoom) {
        channel_switch_close(&effect->channel_zoom);
        /*effect->channel_zoom = NULL;*/
    }
    if (effect->dac) {
        audio_stream_del_entry(&effect->dac->entry);
        audio_dac_free_channel(effect->dac);
        free(effect->dac);
        effect->dac = NULL;
    }

#if (RECORDER_MIX_EN)
    if (effect->rec_hdl) {
        stream_entry_close(&effect->rec_hdl);
    }
#endif
#if (TCFG_USB_MIC_DATA_FROM_MICEFFECT||TCFG_USB_MIC_DATA_FROM_DAC)
    if (effect->usbmic_hdl) {
        stream_entry_close(&effect->usbmic_hdl);
    }
#endif

    if (effect->stream) {
        audio_stream_close(effect->stream);
    }
    local_irq_disable();
    free(effect);
    *hdl = NULL;
    local_irq_enable();

    mem_stats();
    clock_remove_set(REVERB_CLK);
}
/*----------------------------------------------------------------------------*/
/**@brief    ��������
   @param
   @return
   @note ��δʹ��
*/
/*----------------------------------------------------------------------------*/
static void mic_stream_resume(void *p)
{
    struct __mic_effect *effect = (struct __mic_effect *)p;
    /* audio_decoder_resume_all(&decode_task); */
}

/*----------------------------------------------------------------------------*/
/**@brief    �������ݴ����Ȼص�
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
static void mic_effect_data_process_len(struct audio_stream_entry *entry, int len)
{

    struct __mic_effect *effect = container_of(entry, struct __mic_effect, entry);
    effect->out_len += len;
    /* printf("out len[%d]",effect->out_len); */
}

static int mic_effect_record_stream_callback(void *priv, struct audio_data_frame *in)
{
    struct __mic_effect *effect = (struct __mic_effect *)priv;

    s16 *data = in->data;
    u32 len = in->data_len;

    return recorder_mix_pcm_write((u8 *)data, len);
}
/* extern int usb_audio_mic_write(void *data, u16 len); */
extern int usb_audio_mic_write_do(void *data, u16 len);
static int mic_effect_otherout_stream_callback(void *priv, struct audio_data_frame *in)
{
    struct __mic_effect *effect = (struct __mic_effect *)priv;
    s16 *data = in->data;
    u32 len = in->data_len;

#if ((TCFG_USB_MIC_DATA_FROM_MICEFFECT||TCFG_USB_MIC_DATA_FROM_DAC) && (TCFG_MIC_EFFECT_DEBUG == 0))
    if (effect->usbmic_start) {
        /* putchar('A');		 */
        if (len) {
            usb_audio_mic_write_do(data, len);
        }
    } else {
        /* putchar('B');		 */
    }
#endif
    return len;
}

void mic_effect_to_usbmic_onoff(u8 mark)
{
#if (TCFG_USB_MIC_DATA_FROM_MICEFFECT||TCFG_USB_MIC_DATA_FROM_DAC)
    if (__this) {
        __this->usbmic_start = mark ? 1 : 0;
    }
#endif
}

static void  inline rl_rr_mix_to_rl_rr(short *data, int len)
{
    s32 tmp32_1;
    s32 tmp32_2;
    s16 *inbuf = data;
    inbuf = inbuf + 2;  //��λ������ͨ��
    len >>= 3;
    __asm__ volatile(
        "1:                      \n\t"
        "rep %0 {                \n\t"
        "  %2 = h[%1 ++= 2](s)     \n\t"  //ȡ����ͨ��ֵ������ַƫ�������ֽ�ָ�����ͨ������
        " %3 = h[%1 ++= -2](s)   \n\t"   //ȡ����ͨ��ֵ������ַƫ�������ֽ�ָ�����ͨ������
        " %2 = %2 + %3           \n\t"
        " %2 = sat16(%2)(s)      \n\t"  //���ʹ���
        " h[%1 ++= 2] = %2      \n\t"  //��ȡ����ͨ�����ݣ�����ַƫ�������ֽ�ָ�����ͨ������
        " h[%1 ++= 6] = %2      \n\t"  //��ȡ����ͨ�����ݣ�����ַƫ�������ֽ�ָ�����ͨ�����ڵ�����
        "}                      \n\t"
        "if(%0 != 0) goto 1b    \n\t"
        :
        "=&r"(len),
        "=&r"(inbuf),
        "=&r"(tmp32_1),
        "=&r"(tmp32_2)
        :
        "0"(len),
        "1"(inbuf),
        "2"(tmp32_1),
        "3"(tmp32_2)
        :
    );
}
static int effect_to_dac_data_pro_handle(struct audio_stream_entry *entry,  struct audio_data_frame *in)
{
#if (SOUNDCARD_ENABLE)
    if (in->data_len == 0) {
        return 0;
    }
#if 1
    rl_rr_mix_to_rl_rr(in->data, in->data_len);//������
#else
    s16 *outbuf = in->data;
    s16 *inbuf = in->data;
    s32 tmp32;
    u16 len = in->data_len;
    len >>= 3;
    while (len--) {
        *outbuf++ = inbuf[0];
        *outbuf++ = inbuf[1];
        tmp32 = (inbuf[2] + inbuf[3]);
        if (tmp32 < -32768) {
            tmp32 = -32768;
        } else if (tmp32 > 32767) {
            tmp32 = 32767;
        }
        *outbuf++ = tmp32;
        *outbuf++ = tmp32;
        inbuf += 4;
    }
#endif
#endif
    return 0;
}

#if TCFG_APP_RECORD_EN
static int prob_handler_to_record(struct audio_stream_entry *entry,  struct audio_data_frame *in)
{
    if (in->data_len == 0) {
        return 0;
    }
    if (recorder_is_encoding() == 0) {
        return 0;
    }
    int  wlen = recorder_userdata_to_enc(in->data, in->data_len);
    if (wlen != in->data_len) {
        putchar('N');
    }
    return 0;
}
#endif

#if (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_LR || TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_DUAL_LR_DIFF)
#define DAC_OUTPUT_CHANNELS     2
#elif (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_L || TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_R || TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_LR_DIFF)
#define DAC_OUTPUT_CHANNELS     1
#elif (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_FRONT_LR_REAR_LR)
#define DAC_OUTPUT_CHANNELS     4
#else
#define DAC_OUTPUT_CHANNELS     3
#endif
/*----------------------------------------------------------------------------*/
/**@brief    (mic������)����򿪽ӿ�
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
bool mic_effect_start(void)
{
    bool ret = false;
    printf("\n--func=%s\n", __FUNCTION__);
    if (__this) {
        log_e("reverb is already start \n");
        return ret;
    }
    struct __mic_effect *effect = (struct __mic_effect *)zalloc(sizeof(struct __mic_effect));
    if (effect == NULL) {
        return false;
    }
    clock_add_set(REVERB_CLK);
    os_mutex_create(&effect->mutex);
    memcpy(&effect->parm, &effect_parm_default, sizeof(struct __mic_effect_parm));
    struct __mic_stream_parm *mic_parm = (struct __mic_stream_parm *)&effect_mic_stream_parm_default;
    if (g_mic_parm) {
        mic_parm = g_mic_parm;
    }

    if ((effect->parm.effect_config & BIT(MIC_EFFECT_CONFIG_REVERB))
        && (effect->parm.effect_config & BIT(MIC_EFFECT_CONFIG_ECHO))) {
        log_e("effect config err ?? !!!, cann't support echo && reverb at the same time\n");
        mic_effect_destroy(&effect);
        return false;
    }
    u8 ch_num = 1; //??
    effect->input_ch_num = ch_num;
#if TCFG_MIC_DODGE_EN
    if (effect->parm.effect_config & BIT(MIC_EFFECT_CONFIG_ENERGY_DETECT))	{
        effect->energy_hdl = mic_energy_detect_open(effect->parm.sample_rate, ch_num);
        effect->dodge_en = 0;//Ĭ�Ϲرգ� ��Ҫͨ������������
    }
#endif

    ///�������޳�ʼ��
    if (effect->parm.effect_config & BIT(MIC_EFFECT_CONFIG_NOISEGATE)) {
        effect->p_noisegate_hdl = open_noisegate((NOISEGATE_PARM *)&effect_noisegate_parm_default, 0, 0);
    }
    ///Х�����Ƴ�ʼ��
    if (effect->parm.effect_config & BIT(MIC_EFFECT_CONFIG_HOWLING)) {
        log_i("open_howling\n\n\n");
        effect->p_howling_hdl = open_howling(NULL, effect->parm.sample_rate, 0, 1);
    }
    ///�˲�����ʼ��
    ///eq��ʼ��
    if (effect->parm.effect_config & BIT(MIC_EFFECT_CONFIG_EQ)) {
        log_i("effect->parm.sample_rate %d\n", effect->parm.sample_rate);
        effect->filt = mic_high_bass_coeff_cal_init(effect->parm.sample_rate);
        if (!effect->filt) {
            log_e("mic filt malloc err\n");
        }
        effect->eq_drc = mic_eq_drc_open(effect->parm.sample_rate, ch_num);
    }
    ///pitch ��ʼ��
    if (effect->parm.effect_config & BIT(MIC_EFFECT_CONFIG_PITCH)) {
        log_i("open_pitch\n\n\n");
        effect->p_pitch_hdl = open_pitch((PITCH_SHIFT_PARM *)&effect_pitch_parm_default);
        pause_pitch(effect->p_pitch_hdl, 1);
    }
    ///reverb ��ʼ��
    if (effect->parm.effect_config & BIT(MIC_EFFECT_CONFIG_REVERB)) {
        ch_num = 2;
        effect->fade.wet = effect_reverb_parm_default.wet;
        effect->p_reverb_hdl = open_reverb((REVERBN_PARM_SET *)&effect_reverb_parm_default, effect->parm.sample_rate);
        pause_reverb(effect->p_reverb_hdl, 1);
    }
    ///echo ��ʼ��
    if (effect->parm.effect_config & BIT(MIC_EFFECT_CONFIG_ECHO)) {
        effect->fade.decayval = effect_echo_parm_default.decayval;
        effect->fade.delay = effect_echo_parm_default.delay;
        log_i("open_echo\n\n\n");
        effect->p_echo_hdl = open_echo((ECHO_PARM_SET *)&effect_echo_parm_default, (EF_REVERB_FIX_PARM *)&effect_echo_fix_parm_default);
    }

    ///��ʼ����������
    if (effect->parm.effect_config & BIT(MIC_EFFECT_CONFIG_DVOL)) {
        effect_dvol_default_parm.ch_total = ch_num;
        struct audio_stream_entry *dvol_entry;
#if SYS_DIGVOL_GROUP_EN
        dvol_entry = sys_digvol_group_ch_open("mic_mic", -1, &effect_dvol_default_parm);
        effect->d_vol = audio_dig_vol_group_hdl_get(sys_digvol_group, "mic_mic");
#else
        effect->d_vol = audio_dig_vol_open((audio_dig_vol_param *)&effect_dvol_default_parm);
        dvol_entry = audio_dig_vol_entry_get(effect->d_vol);
#endif /*SYS_DIGVOL_GROUP_EN*/
    }

    //�򿪻�������
    if (effect->parm.effect_config & BIT(MIC_EFFECT_CONFIG_SOFT_SRC)) {
        u32 out_sr = audio_output_nor_rate();
        effect->p_echo_src_hdl = open_echo_src(effect->parm.sample_rate, out_sr, ch_num);
#if TCFG_APP_RECORD_EN
        effect->p_echo_src_hdl->entry.prob_handler = prob_handler_to_record;
#endif//TCFG_APP_RECORD_EN
    }

    //����ͨ·���linein
    if (effect->parm.effect_config & BIT(MIC_EFFECT_CONFIG_LINEIN)) {
        effect->linein = effect_linein_open();
    }

    u8 output_channels = DAC_OUTPUT_CHANNELS;
    if (output_channels != ch_num) {
        u32 points_num = REVERB_LADC_IRQ_POINTS * 4;
        effect->channel_zoom = channel_switch_open(output_channels == 2 ? AUDIO_CH_LR : (output_channels == 4 ? AUDIO_CH_QUAD : AUDIO_CH_DIFF), output_channels == 4 ? (points_num * 2 + 128) : 1024);
    }
    // dac mix open

    effect->dac = (struct audio_dac_channel *)zalloc(sizeof(struct audio_dac_channel));
    if (effect->dac) {
        audio_dac_new_channel(&dac_hdl, effect->dac);
        struct audio_dac_channel_attr attr;
        audio_dac_channel_get_attr(effect->dac, &attr);
        attr.delay_time = mic_parm->dac_delay;
        attr.write_mode = WRITE_MODE_FORCE;
        audio_dac_channel_set_attr(effect->dac, &attr);
        effect->dac->entry.prob_handler = effect_to_dac_data_pro_handle;
        /* audio_dac_channel_set_pause(effect->dac,1); */
    }

    effect->entry.data_process_len = mic_effect_data_process_len;

#if (RECORDER_MIX_EN)
    ///��¼��������
    effect->rec_hdl = stream_entry_open(effect, mic_effect_record_stream_callback, 1);
#endif
#if (TCFG_USB_MIC_DATA_FROM_MICEFFECT||TCFG_USB_MIC_DATA_FROM_DAC)
    effect->usbmic_hdl = stream_entry_open(effect, mic_effect_otherout_stream_callback, 1);
#endif

// ����������
    struct audio_stream_entry *entries[15] = {NULL};
    u8 entry_cnt = 0;
    entries[entry_cnt++] = &effect->entry;
    if (effect->energy_hdl) {
        entries[entry_cnt++] = audio_energy_detect_entry_get(effect->energy_hdl);
    }
    if (effect->p_noisegate_hdl) {
        entries[entry_cnt++] = &effect->p_noisegate_hdl->entry;
    }
    if (effect->p_howling_hdl) {
        entries[entry_cnt++] = &effect->p_howling_hdl->entry;
    }
    if (effect->eq_drc) {
        entries[entry_cnt++] = &effect->eq_drc->entry;
    }
    if (effect->p_pitch_hdl) {
        entries[entry_cnt++] = &effect->p_pitch_hdl->entry;
    }

    if (effect->p_reverb_hdl) {
        entries[entry_cnt++] = &effect->p_reverb_hdl->entry;
    }
    if (effect->p_echo_hdl) {
        entries[entry_cnt++] = &effect->p_echo_hdl->entry;
    }

    if (effect->d_vol) {
        entries[entry_cnt++] = audio_dig_vol_entry_get(effect->d_vol);
    }

    if (effect->linein) {
        entries[entry_cnt++] = effect_linein_get_stream_entry(effect->linein);
    }

#if (RECORDER_MIX_EN)
    u8 record_entry_cnt = entry_cnt - 1;
#endif

    if (effect->p_echo_src_hdl) {
        entries[entry_cnt++] = &effect->p_echo_src_hdl->entry;
    }

    if (effect->channel_zoom) {
        entries[entry_cnt++] = &effect->channel_zoom->entry;
    }
    if (effect->dac) {
        entries[entry_cnt++] = &effect->dac->entry;
#if (TCFG_USB_MIC_DATA_FROM_DAC)
        if (effect->usbmic_hdl) {
            entries[entry_cnt++] = &effect->usbmic_hdl->entry;
        }
#endif
    }

    effect->stream = audio_stream_open(effect, mic_stream_resume);
    audio_stream_add_list(effect->stream, entries, entry_cnt);

#if (RECORDER_MIX_EN)
    ///��������֧��¼����
    if (effect->rec_hdl) {
        //�ӱ����ǰһ�ڵ����
        audio_stream_add_entry(entries[record_entry_cnt], &effect->rec_hdl->entry);
    }
#endif
#if (TCFG_USB_MIC_DATA_FROM_MICEFFECT)
    ///��������֧��usbmic��
    if (effect->usbmic_hdl) {
        //�ӱ����ǰһ�ڵ����
        audio_stream_add_entry(entries[entry_cnt - 4], &effect->usbmic_hdl->entry);
    }
#endif
    ///mic ��������ʼ��
    effect->mic = mic_stream_creat(mic_parm);
    if (effect->mic == NULL) {
        mic_effect_destroy(&effect);
        return false;
    }
    mic_stream_set_output(effect->mic, (void *)effect, mic_effect_effect_run);
    mic_stream_start(effect->mic);
#if (RECORDER_MIX_EN)
    recorder_mix_pcm_stream_open(effect->parm.sample_rate, ch_num);
#endif

    clock_set_cur();
    __this = effect;


    log_info("--------------------------effect start ok\n");
    mem_stats();
    mic_effect_change_mode(0);



    return true;
}

/*----------------------------------------------------------------------------*/
/**@brief    (mic������)����رսӿ�
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void mic_effect_stop(void)
{
    mic_effect_destroy(&__this);
#if (RECORDER_MIX_EN)
    recorder_mix_pcm_stream_close();
#endif
}
/*----------------------------------------------------------------------------*/
/**@brief    (mic������)������ͣ�ӿ�(����������������)
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void mic_effect_pause(u8 mark)
{
    if (__this) {
        __this->pause_mark = mark ? 1 : 0;
    }
}
/*----------------------------------------------------------------------------*/
/**@brief    (mic������)������ͣ�����DAC(�������󼶲�д��DAC)
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void mic_effect_dac_pause(u8 mark)
{
    if (__this && __this->dac) {
        audio_dac_channel_set_pause(__this->dac, mark);
    }
}

/*----------------------------------------------------------------------------*/
/**@brief    (mic������)����״̬��ȡ�ӿ�
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
u8 mic_effect_get_status(void)
{
    return ((__this) ? 1 : 0);
}

/*----------------------------------------------------------------------------*/
/**@brief    �����������ڽӿ�
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void mic_effect_set_dvol(u8 vol)
{
    if (__this == NULL) {
        return ;
    }
    audio_dig_vol_set(__this->d_vol, 3, vol);
}
u8 mic_effect_get_dvol(void)
{
    if (__this) {
        return audio_dig_vol_get(__this->d_vol, 1);
    }
    return 0;
}
/*----------------------------------------------------------------------------*/
/**@brief    ��ȡmic����ӿ�
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
u8 mic_effect_get_micgain(void)
{
    /* if (__this) { */
    /* } */
    //��̬������ʵʱ��¼
    return effect_mic_stream_parm_default.mic_gain;
}


/*----------------------------------------------------------------------------*/
/**@brief    reverb Ч����������ڽӿ�
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void mic_effect_set_reverb_wet(int wet)
{
    if (__this == NULL || __this->p_reverb_hdl == NULL) {
        return ;
    }
    os_mutex_pend(&__this->mutex, 0);
    __this->fade.wet = wet;
    os_mutex_post(&__this->mutex);
}

int mic_effect_get_reverb_wet(void)
{
    if (__this && __this->p_reverb_hdl) {
        return __this->fade.wet;
    }
    return 0;
}
/*----------------------------------------------------------------------------*/
/**@brief    echo ������ʱ���ڽӿ�
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void mic_effect_set_echo_delay(u32 delay)
{
    if (__this == NULL || __this->p_echo_hdl == NULL) {
        return ;
    }
    os_mutex_pend(&__this->mutex, 0);
    __this->fade.delay = delay;
    os_mutex_post(&__this->mutex);
}
u32 mic_effect_get_echo_delay(void)
{
    if (__this && __this->p_echo_hdl) {
        return __this->fade.delay;
    }
    return 0;
}
/*----------------------------------------------------------------------------*/
/**@brief    echo ����˥��ϵ�����ڽӿ�
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void mic_effect_set_echo_decay(u32 decay)
{
    if (__this == NULL || __this->p_echo_hdl == NULL) {
        return ;
    }
    os_mutex_pend(&__this->mutex, 0);
    __this->fade.decayval = decay;
    os_mutex_post(&__this->mutex);
}

u32 mic_effect_get_echo_decay(void)
{
    if (__this && __this->p_echo_hdl) {
        return __this->fade.decayval;
    }
    return 0;
}

void mic_effect_set_mic_parm(struct __mic_stream_parm *parm)
{
    g_mic_parm = parm;
}

/*----------------------------------------------------------------------------*/
/**@brief    ���ø�����Ч���б��
   @param
   @return
   @note �ݲ�ʹ��
*/
/*----------------------------------------------------------------------------*/
void mic_effect_set_function_mask(u32 mask)
{
    if (__this == NULL) {
        return ;
    }
    os_mutex_pend(&__this->mutex, 0);
    __this->parm.effect_run = mask;
    os_mutex_post(&__this->mutex);
}
/*----------------------------------------------------------------------------*/
/**@brief    ��ȡ������Ч���б��
   @param
   @return
   @note �ݲ�ʹ��
*/
/*----------------------------------------------------------------------------*/
u32 mic_effect_get_function_mask(void)
{
    if (__this) {
        return __this->parm.effect_run;
    }
    return 0;
}
/*----------------------------------------------------------------------------*/
/**@brief    ��micЧ��ϵ������
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
static void mic_effect_shout_wheat_cal_coef(int sw)
{
    /* log_info("%s %d\n", __FUNCTION__, __LINE__); */
    if (__this == NULL || __this->filt == NULL) {
        return ;
    }
    os_mutex_pend(&__this->mutex, 0);
    BFILT_API_STRUCT *filt = __this->filt;
    if (__this->parm.sample_rate == 0) {
        __this->parm.sample_rate = MIC_EFFECT_SAMPLERATE;
    }

    if (filt->shout_wheat.center_frequency == 0) {
        memcpy(&filt->shout_wheat, &effect_shout_wheat_default, sizeof(SHOUT_WHEAT_PARM_SET));
    }
    audio_bfilt_update_parm u_parm = {0};
    u_parm.freq = filt->shout_wheat.center_frequency;
    u_parm.bandwidth = filt->shout_wheat.bandwidth;
    u_parm.iir_type = TYPE_BANDPASS;
    u_parm.filt_num = 0;
    u_parm.filt_tar_tab  = get_outval_addr(u_parm.filt_num);
    if (sw) {
        u_parm.gain = filt->shout_wheat.occupy;
        log_i("shout_wheat_cal_coef on\n");
    } else {
        u_parm.gain = 0;
        log_i("shout_wheat_cal_coef off\n");
    }
    audio_bfilt_cal_coeff(filt->hdl, &u_parm);//�����˲���
    os_mutex_post(&__this->mutex);
}
/*----------------------------------------------------------------------------*/
/**@brief    ����ϵ������
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
static void mic_effect_low_sound_cal_coef(int gainN)
{
    /* log_info("%s %d\n", __FUNCTION__, __LINE__); */
    if (__this == NULL || __this->filt == NULL) {
        return ;
    }
    os_mutex_pend(&__this->mutex, 0);
    if (__this->parm.sample_rate == 0) {
        __this->parm.sample_rate = MIC_EFFECT_SAMPLERATE;
    }

    BFILT_API_STRUCT *filt = __this->filt;
    audio_bfilt_update_parm u_parm = {0};

    u_parm.freq = filt->low_sound.cutoff_frequency;
    u_parm.bandwidth = 1024;
    u_parm.iir_type = TYPE_LOWPASS;
    u_parm.filt_num = 1;
    u_parm.filt_tar_tab  = get_outval_addr(u_parm.filt_num);

    gainN = filt->low_sound.lowest_gain + gainN * (filt->low_sound.highest_gain - filt->low_sound.lowest_gain) / 10;

    log_i("low sound gainN %d\n", gainN);
    log_i("lowest_gain %d\n", filt->low_sound.lowest_gain);
    log_i("highest_gain %d\n", filt->low_sound.highest_gain);

    if ((gainN >= filt->low_sound.lowest_gain) && (gainN <= filt->low_sound.highest_gain)) {
        u_parm.gain = gainN;
        audio_bfilt_cal_coeff(filt->hdl, &u_parm);//�����˲���
    }
    os_mutex_post(&__this->mutex);
}
/*----------------------------------------------------------------------------*/
/**@brief    ����ϵ������
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
static void mic_effect_high_sound_cal_coef(int gainN)
{
    if (__this == NULL || __this->filt == NULL) {
        return ;
    }
    os_mutex_pend(&__this->mutex, 0);

    if (__this->parm.sample_rate == 0) {
        __this->parm.sample_rate = MIC_EFFECT_SAMPLERATE;
    }

    BFILT_API_STRUCT *filt = __this->filt;
    audio_bfilt_update_parm u_parm = {0};
    u_parm.freq = filt->high_sound.cutoff_frequency;
    u_parm.bandwidth = 1024;
    u_parm.iir_type = TYPE_HIGHPASS;
    u_parm.filt_num = 2;
    u_parm.filt_tar_tab  = get_outval_addr(u_parm.filt_num);
    gainN = filt->high_sound.lowest_gain + gainN * (filt->high_sound.highest_gain - filt->high_sound.lowest_gain) / 10;
    log_i("high gainN %d\n", gainN);
    log_i("lowest_gain %d\n", filt->high_sound.lowest_gain);
    log_i("highest_gain %d\n", filt->high_sound.highest_gain);
    if ((gainN >= filt->high_sound.lowest_gain) && (gainN <= filt->high_sound.highest_gain)) {
        u_parm.gain = gainN;
        audio_bfilt_cal_coeff(filt->hdl, &u_parm);//�����˲���
    }
    os_mutex_post(&__this->mutex);
}


/*----------------------------------------------------------------------------*/
/**@brief    mic_effect_cal_coef ���캰�󡢸ߵ��� ���ڽӿ�
   @param    filtN:MIC_EQ_MODE_SHOUT_WHEAT ����ģʽ��gainN�����󿪹أ�0:�غ��� 1��������
   @param    filtN:MIC_EQ_MODE_LOW_SOUND   ��������  gainN�����ڵ����棬��Χ0~10
   @param    filtN:MIC_EQ_MODE_HIGH_SOUND  ��������  gainN�����ڵ����棬��Χ0~10
   @return
   @note     ���캰�󡢸ߵ�������
*/
/*----------------------------------------------------------------------------*/
void mic_effect_cal_coef(u8 type, u32 gainN)
{

    log_i("filN %d, gainN %d\n", type, gainN);
    if (type == MIC_EQ_MODE_SHOUT_WHEAT) {
        mic_effect_shout_wheat_cal_coef(gainN);
    } else if (type == MIC_EQ_MODE_LOW_SOUND) {
        mic_effect_low_sound_cal_coef(gainN);
    } else if (type == MIC_EQ_MODE_HIGH_SOUND) {
        mic_effect_high_sound_cal_coef(gainN);
    }
    mic_eq_drc_update();
}


#if !MIC_EFFECT_EQ_EN
static int outval[3][5]; //��3��2���˲����Ŀռ䣬��Ӳ��eq��ϵ����
__attribute__((weak))int *get_outval_addr(u8 mode)
{
    //�ߵ���ϵ�����ַ
    return outval[mode];
}
#endif

u8 mic_effect_eq_section_num(void)
{
#if TCFG_EQ_ENABLE
    return (EFFECT_EQ_SECTION_MAX + 3);
#else
    return 0;
#endif
}
/*----------------------------------------------------------------------------*/
/**@brief    reverb �����������
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void mic_effect_reverb_parm_fill(REVERBN_PARM_SET *parm, u8 fade, u8 online)
{

    if (__this == NULL || __this->p_reverb_hdl == NULL) {
        return ;
    }
    if (parm == NULL) {
        __this->parm.effect_run &= ~BIT(MIC_EFFECT_CONFIG_REVERB);
        pause_reverb(__this->p_reverb_hdl, 1);
        return ;
    }
    mic_effect_reverb_parm_printf(parm);
    REVERBN_PARM_SET tmp;
    os_mutex_pend(&__this->mutex, 0);
    memcpy(&tmp, parm, sizeof(REVERBN_PARM_SET));
    if (fade) {
        //�����Ҫfade�Ĳ�������ȡ��ֵ��ͨ��fade�����¶�Ӧ�Ĳ���
        tmp.wet = __this->p_reverb_hdl->parm.wet;///��ȡ��ֵ,��ʱ������
        if (online) {
            __this->fade.wet = parm->wet;///����wet fade Ŀ��ֵ, ͨ��fade����
        } else {
            __this->fade.wet = __this->p_reverb_hdl->parm.wet;//ֵ������, ͨ���ⲿ�������£� ����ť
        }
    }
    __this->update_mask |= BIT(MASK_REVERB);
    __this->parm.effect_run |= BIT(MIC_EFFECT_CONFIG_REVERB);
    pause_reverb(__this->p_reverb_hdl, 0);
    update_reverb_parm(__this->p_reverb_hdl, &tmp);

    os_mutex_post(&__this->mutex);
}
/*----------------------------------------------------------------------------*/
/**@brief    echo �����������
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void mic_effect_echo_parm_fill(ECHO_PARM_SET *parm, u8 fade, u8 online)
{

    if (__this == NULL || __this->p_echo_hdl == NULL) {
        return ;
    }
    if (parm == NULL) {
        __this->parm.effect_run &= ~BIT(MIC_EFFECT_CONFIG_ECHO);
        pause_echo(__this->p_echo_hdl, 1);
        return ;
    }
    ECHO_PARM_SET tmp;
    os_mutex_pend(&__this->mutex, 0);
    memcpy(&tmp, parm, sizeof(ECHO_PARM_SET));
    if (fade) {
        //�����Ҫfade�Ĳ�������ȡ��ֵ��ͨ��fade�����¶�Ӧ�Ĳ���
        tmp.delay = __this->p_echo_hdl->echo_parm_obj.delay;
        tmp.decayval = __this->p_echo_hdl->echo_parm_obj.decayval;
        if (online) {
            __this->fade.delay = parm->delay;///����wet fade Ŀ��ֵ, ͨ��fade����
            __this->fade.decayval = parm->decayval;///����wet fade Ŀ��ֵ, ͨ��fade����
        } else {
            __this->fade.delay = __this->p_echo_hdl->echo_parm_obj.delay;///ֵ������, ͨ���ⲿ�������£� ����ť
            __this->fade.decayval = __this->p_echo_hdl->echo_parm_obj.decayval;///ֵ������, ͨ���ⲿ�������£� ����ť
        }
    }
    __this->update_mask |= BIT(MASK_ECHO);
    __this->parm.effect_run |= BIT(MIC_EFFECT_CONFIG_ECHO);
    pause_echo(__this->p_echo_hdl, 0);
    update_echo_parm(__this->p_echo_hdl, &tmp);
    os_mutex_post(&__this->mutex);
}
/*----------------------------------------------------------------------------*/
/**@brief    ��������ֱ�Ӹ���
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void set_pitch_para(u32 shiftv, u32 sr, u8 effect, u32 formant_shift)
{
    if (__this == NULL || __this->p_pitch_hdl == NULL) {
        return ;
    }
    PITCH_SHIFT_PARM p_pitch_parm = {0};//get_pitch_parm();

    p_pitch_parm.sr = sr;
    p_pitch_parm.shiftv = shiftv;
    p_pitch_parm.effect_v = effect;
    p_pitch_parm.formant_shift = formant_shift;

    printf("\n\n\nshiftv[%d],sr[%d],effect[%d],formant_shift[%d] \n\n", p_pitch_parm.shiftv, p_pitch_parm.sr, p_pitch_parm.effect_v, p_pitch_parm.formant_shift);
    update_picth_parm(__this->p_pitch_hdl, &p_pitch_parm);

}

PITCH_SHIFT_PARM picth_mode_table[] = {
    {16000, 56, EFFECT_PITCH_SHIFT, 0}, //������
    {16000, 136, EFFECT_PITCH_SHIFT, 0}, //Ů����
    {16000, 56, EFFECT_VOICECHANGE_KIN0, 150}, //�б�Ů1
    // {16000,56,EFFECT_VOICECHANGE_KIN1,150},	//�б�Ů2
    // {16000,56,EFFECT_VOICECHANGE_KIN2,150},	//�б�Ů3
    {16000, 196, EFFECT_PITCH_SHIFT, 100}, //ħ����������
    {16000, 100, EFFECT_AUTOTUNE, D_MAJOR} //����
};

/*----------------------------------------------------------------------------*/
/**@brief    ������������
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void mic_effect_pitch_parm_fill(PITCH_PARM_SET2 *parm, u8 fade, u8 online)
{
    if (__this == NULL || __this->p_pitch_hdl == NULL) {
        return ;
    }
    if (parm == NULL) {
        __this->parm.effect_run &= ~BIT(MIC_EFFECT_CONFIG_PITCH);
        pause_pitch(__this->p_pitch_hdl, 1);
        return ;
    }
    mic_effect_pitch2_parm_printf(parm);

    os_mutex_pend(&__this->mutex, 0);
    PITCH_SHIFT_PARM p_parm;
    p_parm.sr = MIC_EFFECT_SAMPLERATE;
    p_parm.effect_v = parm->effect_v;
    p_parm.formant_shift = parm->formant_shift;
    p_parm.shiftv = parm->pitch;

    __this->update_mask |= BIT(MASK_PITCH);
    __this->parm.effect_run |= BIT(MIC_EFFECT_CONFIG_PITCH);
    pause_pitch(__this->p_pitch_hdl, 0);
    update_picth_parm(__this->p_pitch_hdl, &p_parm);

    os_mutex_post(&__this->mutex);
}

/*----------------------------------------------------------------------------*/
/**@brief    �������Ʋ�������
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void mic_effect_noisegate_parm_fill(NOISE_PARM_SET *parm, u8 fade, u8 online)
{
    if (__this == NULL || __this->p_noisegate_hdl == NULL) {
        return ;
    }
    if (parm == NULL) {
        __this->parm.effect_run &= ~BIT(MIC_EFFECT_CONFIG_NOISEGATE);
        pause_noisegate(__this->p_noisegate_hdl, 1);
        return ;
    }
    mic_effect_noisegate_parm_printf(parm);

    os_mutex_pend(&__this->mutex, 0);


    NOISEGATE_PARM noisegate;
    memcpy(&noisegate, &__this->p_noisegate_hdl->parm, sizeof(NOISEGATE_PARM));
    noisegate.attackTime = parm->attacktime;
    noisegate.releaseTime = parm->releasetime;
    noisegate.threshold = parm->threadhold;
    noisegate.low_th_gain = parm->gain;


    __this->update_mask |= BIT(MASK_NOISEGATE);
    __this->parm.effect_run |= BIT(MIC_EFFECT_CONFIG_NOISEGATE);
    pause_noisegate(__this->p_noisegate_hdl, 0);
    update_noisegate(__this->p_noisegate_hdl, &noisegate);
    os_mutex_post(&__this->mutex);
}
/*----------------------------------------------------------------------------*/
/**@brief    ��mic��������
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void mic_effect_shout_wheat_parm_fill(SHOUT_WHEAT_PARM_SET *parm, u8 fade, u8 online)
{
    /* log_info("%s %d\n", __FUNCTION__, __LINE__); */
    if (__this == NULL || __this->filt == NULL || parm == NULL) {
        return ;
    }
    mic_effect_shout_wheat_parm_printf(parm);

    SHOUT_WHEAT_PARM_SET tmp;
    os_mutex_pend(&__this->mutex, 0);
    memcpy(&tmp, parm, sizeof(SHOUT_WHEAT_PARM_SET));
    if (fade) {
        //�����Ҫfade�Ĳ�������ȡ��ֵ��ͨ��fade�����¶�Ӧ�Ĳ���
    }
    memcpy(&__this->filt->shout_wheat, &tmp, sizeof(SHOUT_WHEAT_PARM_SET));
    __this->update_mask |= BIT(MASK_SHOUT_WHEAT);
    os_mutex_post(&__this->mutex);
}
void mic_effect_low_sound_parm_fill(LOW_SOUND_PARM_SET *parm, u8 fade, u8 online)
{
    /* log_info("%s %d\n", __FUNCTION__, __LINE__); */
    if (__this == NULL || __this->filt == NULL || parm == NULL) {
        return ;
    }
    mic_effect_low_sound_parm_printf(parm);

    LOW_SOUND_PARM_SET tmp;
    os_mutex_pend(&__this->mutex, 0);
    memcpy(&tmp, parm, sizeof(LOW_SOUND_PARM_SET));
    if (fade) {
        //�����Ҫfade�Ĳ�������ȡ��ֵ��ͨ��fade�����¶�Ӧ�Ĳ���
    }
    memcpy(&__this->filt->low_sound, &tmp, sizeof(LOW_SOUND_PARM_SET));
    __this->update_mask |= BIT(MASK_LOW_SOUND);
    os_mutex_post(&__this->mutex);
}
void mic_effect_high_sound_parm_fill(HIGH_SOUND_PARM_SET *parm, u8 fade, u8 online)
{
    if (__this == NULL || __this->filt == NULL || parm == NULL) {
        return ;
    }
    mic_effect_high_sound_parm_printf(parm);

    HIGH_SOUND_PARM_SET tmp;
    os_mutex_pend(&__this->mutex, 0);
    memcpy(&tmp, parm, sizeof(HIGH_SOUND_PARM_SET));
    if (fade) {
        //�����Ҫfade�Ĳ�������ȡ��ֵ��ͨ��fade�����¶�Ӧ�Ĳ���
    }
    memcpy(&__this->filt->high_sound, &tmp, sizeof(HIGH_SOUND_PARM_SET));
    __this->update_mask |= BIT(MASK_HIGH_SOUND);
    os_mutex_post(&__this->mutex);
}

/*----------------------------------------------------------------------------*/
/**@brief    mic�������
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void mic_effect_mic_gain_parm_fill(EFFECTS_MIC_GAIN_PARM *parm, u8 fade, u8 online)
{
    if (__this == NULL || parm == NULL) {
        return ;
    }
    audio_mic_set_gain(parm->gain);
}
/*----------------------------------------------------------------------------*/
/**@brief    micЧ��ģʽ�л�����������Ч����л���
   @param
   @return
   @note ʹ��Ч�������ļ�ʱ��Ч
*/
/*----------------------------------------------------------------------------*/
void mic_effect_change_mode(u16 mode)
{
    effect_cfg_change_mode(mode);
}
/*----------------------------------------------------------------------------*/
/**@brief    ��ȡmicЧ��ģʽ����������Ч��ϣ�
   @param
   @return
   @note ʹ��Ч�������ļ�ʱ��Ч
*/
/*----------------------------------------------------------------------------*/
u16 mic_effect_get_cur_mode(void)
{
    return effect_cfg_get_cur_mode();
}




/*----------------------------------------------------------------------------*/
/**@brief    eq�ӿ�
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void mic_eq_drc_update()
{
    local_irq_disable();
    if (__this && __this->eq_drc && __this->eq_drc->eq) {
        __this->eq_drc->eq->updata = 1;
    }
    local_irq_enable();
}


BFILT_API_STRUCT *mic_high_bass_coeff_cal_init(u32 sample_rate)
{
    BFILT_API_STRUCT *filt = zalloc(sizeof(BFILT_API_STRUCT));
    if (!filt) {
        return NULL;
    }
    bfilt_open_parm parm = {0};
    parm.sr = sample_rate;
    filt->hdl = audio_bfilt_open(&parm);

    memcpy(&filt->shout_wheat, &effect_shout_wheat_default, sizeof(SHOUT_WHEAT_PARM_SET));
    memcpy(&filt->low_sound, &effect_low_sound_default, sizeof(LOW_SOUND_PARM_SET));
    memcpy(&filt->high_sound, &effect_high_sound_default, sizeof(HIGH_SOUND_PARM_SET));

    audio_bfilt_update_parm u_parm = {0};
    u_parm.freq = filt->shout_wheat.center_frequency;
    u_parm.bandwidth = filt->shout_wheat.bandwidth;
    u_parm.iir_type = TYPE_BANDPASS;
    u_parm.filt_num = 0;
    u_parm.filt_tar_tab  = get_outval_addr(u_parm.filt_num);
    u_parm.gain = 0;
    audio_bfilt_cal_coeff(filt->hdl, &u_parm);//�����˲���

    u_parm.freq = filt->low_sound.cutoff_frequency;
    u_parm.bandwidth = 1024;
    u_parm.iir_type = TYPE_LOWPASS;
    u_parm.filt_num = 1;
    u_parm.filt_tar_tab  = get_outval_addr(u_parm.filt_num);
    u_parm.gain = 0;
    audio_bfilt_cal_coeff(filt->hdl, &u_parm);//�����˲���

    u_parm.freq = filt->high_sound.cutoff_frequency;
    u_parm.bandwidth = 1024;
    u_parm.iir_type = TYPE_HIGHPASS;
    u_parm.filt_num = 2;
    u_parm.filt_tar_tab  = get_outval_addr(u_parm.filt_num);
    u_parm.gain = 0;
    audio_bfilt_cal_coeff(filt->hdl, &u_parm);//�����˲���
    return filt;
}
void mic_high_bass_coeff_cal_uninit(BFILT_API_STRUCT *filt)
{
    local_irq_disable();
    if (filt && filt->hdl) {
        audio_bfilt_close(filt->hdl);
    }
    if (filt) {
        free(filt);
        filt = NULL;
    }
    local_irq_enable();
}

#if !MIC_EFFECT_EQ_EN
__attribute__((weak))int mic_eq_get_filter_info(void *eq, int sr, struct audio_eq_filter_info *info)
{
    log_info("mic_eq_get_filter_info \n");
    info->L_coeff = info->R_coeff = (void *)outval;
    info->L_gain = info->R_gain = 0;
    info->nsection = 3;
    return 0;
}
#endif

void *mic_eq_drc_open(u32 sample_rate, u8 ch_num)
{

#if TCFG_EQ_ENABLE

    log_i("sample_rate %d %d\n", sample_rate, ch_num);
    struct audio_eq_drc *eq_drc = NULL;
    struct audio_eq_drc_parm effect_parm = {0};

    effect_parm.eq_en = 1;

    if (effect_parm.eq_en) {

        effect_parm.async_en = 0;

        effect_parm.out_32bit = 0;
        effect_parm.online_en = 0;
        effect_parm.mode_en = 0;
    }

    effect_parm.eq_name = mic_eq_mode;

    effect_parm.ch_num = ch_num;
    effect_parm.sr = sample_rate;
    effect_parm.eq_cb = mic_eq_get_filter_info;

    eq_drc = audio_eq_drc_open(&effect_parm);

    clock_add(EQ_CLK);
    return eq_drc;
#else
    return NULL;
#endif//TCFG_EQ_ENABLE

}

void mic_eq_drc_close(struct audio_eq_drc *eq_drc)
{
#if TCFG_EQ_ENABLE
    if (eq_drc) {
        audio_eq_drc_close(eq_drc);
        eq_drc = NULL;
        clock_remove(EQ_CLK);
    }
#endif
    return;
}




#if TCFG_MIC_DODGE_EN
void mic_e_det_handler(u8 event, u8 ch)
{
    //printf(">>>> ch:%d %s\n", ch, event ? ("MUTE") : ("UNMUTE"));

    struct __mic_effect *effect = (struct __mic_effect *)__this;
#if SYS_DIGVOL_GROUP_EN
    //printf("effect_dvol_default_parm.ch_total %d effect->dodge_en %d\n", effect_dvol_default_parm.ch_total, effect->dodge_en);
    if (ch == effect_dvol_default_parm.ch_total) {
        if (effect->dodge_en) {
            if (effect && effect->d_vol) {
                if (event) { //�Ƴ�����
                    audio_dig_vol_group_dodge(sys_digvol_group, "mic_mic", 100, 100);
                } else { //��������
                    audio_dig_vol_group_dodge(sys_digvol_group, "mic_mic", 100, 0);
                }
            }
        }
    }
#endif
}
/*----------------------------------------------------------------------------*/
/**@brief    ��mic �������
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void *mic_energy_detect_open(u32 sr, u8 ch_num)
{
    audio_energy_detect_param e_det_param = {0};
    e_det_param.mute_energy = dodge_parm.dodge_out_thread;//��������С��mute_energy �˳�����
    e_det_param.unmute_energy = dodge_parm.dodge_in_thread;//������������ 100��������
    e_det_param.mute_time_ms = dodge_parm.dodge_out_time_ms;
    e_det_param.unmute_time_ms = dodge_parm.dodge_in_time_ms;
    e_det_param.count_cycle_ms = 2;
    e_det_param.sample_rate = sr;
    e_det_param.event_handler = mic_e_det_handler;
    e_det_param.ch_total = ch_num;
    e_det_param.dcc = 1;
    void *audio_e_det_hdl = audio_energy_detect_open(&e_det_param);
    return audio_e_det_hdl;
}
/*----------------------------------------------------------------------------*/
/**@brief    �ر�mic �������
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void mic_energy_detect_close(void *hdl)
{
    if (hdl) {
        audio_stream_del_entry(audio_energy_detect_entry_get(hdl));
#if SYS_DIGVOL_GROUP_EN
        struct __mic_effect *effect = (struct __mic_effect *)__this;
        if (effect->d_vol) {
            audio_dig_vol_group_dodge(sys_digvol_group, "mic_mic", 100, 100);         // undodge
        }
#endif

        audio_energy_detect_close(hdl);
    }
}

/*----------------------------------------------------------------------------*/
/**@brief    ����������й��̣��Ƿ񴥷�����
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void mic_dodge_ctr(void)
{
    struct __mic_effect *effect = (struct __mic_effect *)__this;
    if (effect) {
        effect->dodge_en = !effect->dodge_en;
    }
}
u8 mic_dodge_get_status(void)
{
    struct __mic_effect *effect = (struct __mic_effect *)__this;
    if (effect) {
        return effect->dodge_en;
    }
    return 0;
}
#endif


//*********************��Ч�л�����**********************************//
//�ο� ��Чֻ���� echo����ͱ���
enum {
    KARAOKE_MIC_OST,//ԭ��,¼����
    KARAOKE_MIC_KTV,//KTV
    KARAOKE_MIC_SHUSHU,//����
    KARAOKE_MIC_GODDESS,//Ů��
    KARAOKE_MIC_BABY,//������
    KARAOKE_MIC_MAX,
    /* KARAOKE_ONLY_MIC,//�ڲ���micģʽ�³�ʼ��mic */
};

//K�����ģʽ�л��ӿ�
void mic_mode_switch(u8 eff_mode)
{
#if TCFG_KARAOKE_EARPHONE
    if (!mic_effect_get_status()) {
        mic_effect_start();
        pause_echo(__this->p_echo_hdl, 1);
        return;
    }
    u32 sample_rate = 0;
    switch (eff_mode) {
    case KARAOKE_MIC_OST://ԭ��,¼����
        /* tone_play_index(IDEX_TONE_MIC_OST, 1); */
        puts("OST\n");
        // ��Чֱͨ
        pause_echo(__this->p_echo_hdl, 1);
        pause_pitch(__this->p_pitch_hdl, 1);
        if (__this->p_howling_hdl) {
            pause_howling(__this->p_howling_hdl, 1);
        }
        break;
    case KARAOKE_MIC_KTV://KTV
        puts("KTV\n");
        //����ֱͨ
        pause_echo(__this->p_echo_hdl, 0);
        pause_pitch(__this->p_pitch_hdl, 1);
        break;
    case KARAOKE_MIC_SHUSHU://����
        /* tone_play_index(IDEX_TONE_UNCLE, 1); */
        puts("UNCLE\n");
        pause_pitch(__this->p_pitch_hdl, 1);
        set_pitch_para(136, sample_rate, EFFECT_PITCH_SHIFT, 0);
        pause_pitch(__this->p_pitch_hdl, 0);
        break;
    case KARAOKE_MIC_GODDESS://Ů��
        /* tone_play_index(IDEX_TONE_GODNESS, 1); */
        puts("GODDESS\n");
        pause_pitch(__this->p_pitch_hdl, 1);
        set_pitch_para(66, sample_rate, EFFECT_VOICECHANGE_KIN0, 150);
        pause_pitch(__this->p_pitch_hdl, 0);
        break;
    case KARAOKE_MIC_BABY://������
        /* tone_play_index(IDEX_TONE_BABY, 1); */
        puts("WAWA\n");
        pause_pitch(__this->p_pitch_hdl, 1);
        set_pitch_para(50, sample_rate, EFFECT_PITCH_SHIFT, 0);
        pause_pitch(__this->p_pitch_hdl, 0);
        break;
    default:
        puts("mic_ERROR\n");
        mic_effect_stop();
        break;
    }
#endif//TCFG_KARAOKE_EARPHONE
}


#endif//TCFG_MIC_EFFECT_ENABLE




