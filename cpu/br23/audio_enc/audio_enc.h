#ifndef _AUDIO_ENC_H_
#define _AUDIO_ENC_H_

#include "generic/typedef.h"
#include "media/includes.h"

#ifndef CONFIG_LITE_AUDIO
#include "mic_effect.h"
#include "loud_speaker.h"
#include "audio_enc_file.h"
#include "audio_enc_recoder.h"
#include "audio_recorder_mix.h"
#endif/*CONFIG_LITE_AUDIO*/


struct record_file_fmt {
    u8  gain;//��������
    u8  channel;//��������1:�������� 2��˫����
    u16 sample_rate;//������
    u32 bit_rate;//����
    u32 coding_type;//�����ʽ
    char *dev;//¼���豸�̷�
    char *folder;//¼���ļ���
    char *filename;//¼���ļ���
    u32 cut_head_time;//¼���ļ�ȥͷʱ��,��λms
    u32 cut_tail_time;//¼���ļ�ȥβʱ��,��λms
    u32 limit_size;//¼���ļ���С��С���ƣ� ��λbyte
    u8  source;//¼������Դ
    void (*err_callback)(void);
};


int esco_enc_open(u32 coding_type, u8 frame_len);
void esco_enc_close();

enum enc_source {
    ENCODE_SOURCE_MIX = 0x0,
    ENCODE_SOURCE_MIC,
    ENCODE_SOURCE_LINE0_LR,
    ENCODE_SOURCE_LINE1_LR,
    ENCODE_SOURCE_LINE2_LR,
    ENCODE_SOURCE_USER,
};

void esco_enc_resume(void);
u32 recorder_get_encoding_time();
int recorder_is_encoding(void);
void recorder_device_offline_check(char *logo);
void recorder_encode_stop(void);
int mixer_recorder_encoding(void);
int mixer_recorder_start(void);
void mixer_recorder_stop(void);
int recorder_encode_start(struct record_file_fmt *f);
int recorder_userdata_to_enc(s16 *data, int len);

int audio_encoder_task_open(void);
void audio_encoder_task_close(void);

void init_audio_adc();
int audio_mic_open(struct adc_mic_ch *mic, u16 sample_rate, u8 gain);
void audio_mic_add_output(struct audio_adc_output_hdl *output);
void audio_mic_start(struct adc_mic_ch *mic);
void audio_mic_close(struct adc_mic_ch *mic, struct audio_adc_output_hdl *output);
void audio_mic_set_gain(u8 gain);

int audio_linein_open(struct audio_adc_ch *linein, u16 sample_rate, int gain);
void audio_linein_add_output(struct audio_adc_output_hdl *output);
void audio_linein_start(struct audio_adc_ch *linein);
void audio_linein_close(struct audio_adc_ch *linein, struct audio_adc_output_hdl *output);
void audio_linein_set_gain(int gain);
u8 get_audio_linein_ch_num(void);
//////////////////////////////////////////////////////////////////
#define REVERB_LADC_IRQ_POINTS (160-8)//(160)//(176)//(184)//(192)//(224)//(256)
#endif/*_AUDIO_ENC_H_*/
