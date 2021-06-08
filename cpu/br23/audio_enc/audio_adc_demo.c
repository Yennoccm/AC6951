/*
 *****************************************************************
 *
 * Audio ADC ��ͨ��ʹ��demo
 *
 *****************************************************************
 */

#include "asm/includes.h"
#include "media/includes.h"
#include "system/includes.h"
#include "asm/audio_src.h"
#include "audio_enc.h"
#include "app_main.h"
//#include "app_task.h"
#include "audio_config.h"

extern struct audio_adc_hdl adc_hdl;

/*�ܹ�ʹ�ܶ��ٸ�ͨ��*/
#define LADC_CH_NUM         1

#define LADC_BUF_NUM        2
#define LADC_IRQ_POINTS     256	/*�жϵ���*/
#define LADC_BUFS_SIZE      (LADC_CH_NUM * LADC_BUF_NUM * LADC_IRQ_POINTS)

/*����ʹ�ã���mic����/linein����/mic&line������ݵ�dac*/
#define  LADC_2_DAC_ENABLE	1
#define LADC_MIC_2_DAC		BIT(0)
#define LADC_LIN_2_DAC		BIT(1)
#define LADC_2_DAC			(LADC_MIC_2_DAC | LADC_LIN_2_DAC)

typedef struct {
    struct audio_adc_output_hdl output;
    struct audio_adc_ch linein_ch;
    struct adc_mic_ch mic_ch;
    s16 adc_buf[LADC_BUFS_SIZE];    //align 4Bytes
    s16 temp_buf[LADC_IRQ_POINTS * 3];
} audio_adc_t;
static audio_adc_t *ladc_var = NULL;

/*
 * ʹ��1��ͨ��,1��linein����1��mic��
 * ���ݽṹ��DAT0 DAT1 DAT2
 */
static void audio_adc1_output_demo(void *priv, s16 *data, int len)
{
    struct audio_adc_hdl *hdl = priv;
    int wlen = 0;

    putchar('1');
    if (ladc_var == NULL) {
        return;
    }
    /* printf("linein:%x,len:%d,ch:%d",data,len,hdl->channel); */

#if LADC_2_DAC_ENABLE
#if (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_LR)//˫�������ݽṹ
    for (int i = 0; i < (len / 2); i++) {
        ladc_var->temp_buf[i * 2] = data[i];
        ladc_var->temp_buf[i * 2 + 1] = data[i];
    }
    wlen = app_audio_output_write(ladc_var->temp_buf, len * 2);
#else //���������ݽṹ
    //TODO
    wlen = app_audio_output_write(data, len * hdl->channel);
#endif/*TCFG_AUDIO_DAC_CONNECT_MODE*/
#endif/*LADC_2_DAC_ENABLE*/
}

/*
 * ʹ��2��ͨ��,1��linein��1��mic��
 * ���ݽṹ��LIN0 MIC0 LIN1 MIC1 LIN2 MIC2
 */
static void audio_adc2_output_demo(void *priv, s16 *data, int len)
{
    struct audio_adc_hdl *hdl = priv;
    int wlen = 0;

    putchar('2');
    if (ladc_var == NULL) {
        return;
    }
    /* printf("linein:%x,len:%d,ch:%d",data,len,hdl->channel); */

#if LADC_2_DAC_ENABLE
#if (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_LR)//˫�������ݽṹ
    wlen = app_audio_output_write(data, len * 2);
#else //���������ݽṹ
    //TODO
#endif/*TCFG_AUDIO_DAC_CONNECT_MODE*/
#endif/*LADC_2_DAC_ENABLE*/
}
/*
 * ʹ��3��ͨ��,2��linein �� 1��mic��
 * ���ݽṹ��LINL0 LINR0 MIC0 LINL1 LINR1 MIC1 LINL2 LINR2 MIC2...
 */
static void audio_adc3_output_demo(void *priv, s16 *data, int len)
{
    struct audio_adc_hdl *hdl = priv;
    int wlen = 0;

    putchar('3');
    if (ladc_var == NULL) {
        return;
    }
    /* printf("linein:%x,len:%d,ch:%d",data,len,hdl->channel); */

#if LADC_2_DAC_ENABLE
#if (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_LR)//˫�������ݽṹ
#if (LADC_2_DAC == LADC_LIN_2_DAC)
    for (u16 i = 0; i < len / 2; i++) {
        data[2 * i] = data[3 * i];
        data[2 * i + 1] = data[3 * i + 1];
    }
#elif (LADC_2_DAC == LADC_MIC_2_DAC)
    for (u16 i = 0; i < len / 2; i++) {
        data[2 * i] = data[3 * i + 2];
        data[2 * i + 1] = data[3 * i + 2];
    }
#else //linein data and mic data mix
    int tmp_data;
    for (u16 i = 0; i < len / 2; i++) {
        tmp_data = data[3 * i];
        tmp_data += data[3 * i + 2];
        if (tmp_data > 32767) {
            tmp_data = 32767;
        } else if (tmp_data < -32768) {
            tmp_data = -32768;
        }
        data[2 * i] = tmp_data;

        tmp_data = data[3 * i + 1];
        tmp_data += data[3 * i + 2];
        if (tmp_data > 32767) {
            tmp_data = 32767;
        } else if (tmp_data < -32768) {
            tmp_data = -32768;
        }
        data[2 * i + 1] = tmp_data;
    }
#endif/*LADC_2_DAC*/
    wlen = app_audio_output_write(data, len * 2);
#else //���������ݽṹ

#endif/*TCFG_AUDIO_DAC_CONNECT_MODE*/
#endif/*LADC_2_DAC_ENABLE*/
}


int audio_adc_open_demo(void)
{
    u16 ladc_sr = 16000;
    u8 mic_gain = 5;
    u8 linein_gain = 3;
    r_printf("audio_adc_open_demo,sr:%d,mic_gain:%d,linein_gain:%d\n", ladc_sr, mic_gain, linein_gain);
    if (ladc_var) {
        r_printf("ladc already open \n");
        return 0;
    }
    ladc_var = zalloc(sizeof(audio_adc_t));
    if (ladc_var) {
#if (LADC_CH_NUM == 1)
        audio_adc_mic_open(&ladc_var->mic_ch, AUDIO_ADC_MIC_CH, &adc_hdl);
        audio_adc_mic_set_sample_rate(&ladc_var->mic_ch, ladc_sr);
        audio_adc_mic_set_gain(&ladc_var->mic_ch, mic_gain);
        audio_adc_mic_set_buffs(&ladc_var->mic_ch, ladc_var->adc_buf, LADC_CH_NUM * LADC_IRQ_POINTS * 2, LADC_BUF_NUM);
        ladc_var->output.handler = audio_adc1_output_demo;
        ladc_var->output.priv = &adc_hdl;
        audio_adc_add_output_handler(&adc_hdl, &ladc_var->output);
        audio_adc_mic_start(&ladc_var->mic_ch);
#else
        audio_adc_mic_open(&ladc_var->mic_ch, AUDIO_ADC_MIC_CH, &adc_hdl);
        audio_adc_mic_set_sample_rate(&ladc_var->mic_ch, ladc_sr);
        audio_adc_mic_set_gain(&ladc_var->mic_ch, mic_gain);

#if (LADC_CH_NUM ==2 )
        audio_adc_linein_open(&ladc_var->linein_ch, AUDIO_ADC_LINE0_L, &adc_hdl);
        //audio_adc_linein_open(&ladc_var->linein_ch, AUDIO_ADC_LINE0_R, &adc_hdl);
#else
        audio_adc_linein_open(&ladc_var->linein_ch, AUDIO_ADC_LINE0_LR, &adc_hdl);
#endif
        audio_adc_linein_set_sample_rate(&ladc_var->linein_ch, ladc_sr);
        audio_adc_linein_set_gain(&ladc_var->linein_ch, linein_gain);

        printf("adc_buf_size:%d", sizeof(ladc_var->adc_buf));
        audio_adc_set_buffs(&ladc_var->linein_ch, ladc_var->adc_buf, LADC_CH_NUM * LADC_IRQ_POINTS * 2, LADC_BUF_NUM);
#if (LADC_CH_NUM == 2)
        ladc_var->output.handler = audio_adc2_output_demo;
#else
        ladc_var->output.handler = audio_adc3_output_demo;
#endif/*LADC_CH_NUM*/
        ladc_var->output.priv = &adc_hdl;
        audio_adc_add_output_handler(&adc_hdl, &ladc_var->output);
        audio_adc_start(&ladc_var->linein_ch, &ladc_var->mic_ch);
#endif

#if LADC_2_DAC_ENABLE
        app_audio_output_samplerate_set(ladc_sr);
        app_audio_output_start();
#endif/*LADC_2_DAC_ENABLE*/
        return 0;
    } else {
        return -1;
    }
}

void audio_adc_close_demo()
{
    if (ladc_var) {
        audio_adc_close(&ladc_var->linein_ch, &ladc_var->mic_ch);
        audio_adc_del_output_handler(&adc_hdl, &ladc_var->output);
        free(ladc_var);
        ladc_var = NULL;
    }
}

#if 1
static u8 audio_adc_demo_idle_query(void)
{
    if (ladc_var) {
        return 0;
    }
    return 1;
}

REGISTER_LP_TARGET(audio_adc_demo) = {
    .name       = "audio_adc_demo",
    .is_idle    = audio_adc_demo_idle_query,
};
#endif
