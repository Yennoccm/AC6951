
#ifndef _AUDIO_DEC_FILE_H_
#define _AUDIO_DEC_FILE_H_

#include "asm/includes.h"
#include "media/includes.h"
#include "media/file_decoder.h"
#include "system/includes.h"
#include "media/audio_decoder.h"
#include "app_config.h"
#include "music/music_decrypt.h"
#include "music/music_id3.h"
#include "application/audio_vocal_tract_synthesis.h"
#include "application/audio_pitchspeed.h"
#include "application/audio_surround.h"
#include "application/audio_vbass.h"


#define FILE_DEC_REPEAT_EN			0 // �޷�ѭ������

#define FILE_DEC_AB_REPEAT_EN		1 // AB�㸴��

#define FILE_DEC_DEST_PLAY			0 // ָ��ʱ�䲥��

enum {
    FILE_DEC_STREAM_CLOSE = 0,
    FILE_DEC_STREAM_OPEN,
};

struct file_dec_hdl {
    struct audio_stream *stream;	// ��Ƶ��
    struct file_decoder file_dec;	// file������
    struct audio_res_wait wait;		// ��Դ�ȴ����
    struct audio_mixer_ch mix_ch;	// ���Ӿ��
    struct audio_eq_drc *eq_drc;//eq drc���
    struct audio_dec_breakpoint *dec_bp; // �ϵ�
    s_pitchspeed_hdl *p_pitchspeed_hdl; // ���ٱ�����
#if TCFG_EQ_DIVIDE_ENABLE
    struct audio_eq_drc *eq_drc_rl_rr;//eq drc���
    struct audio_vocal_tract vocal_tract;//�����ϲ�Ŀ����
    struct audio_vocal_tract_ch synthesis_ch_fl_fr;//�����ϲ����
    struct audio_vocal_tract_ch synthesis_ch_rl_rr;//�����ϲ����
    struct channel_switch *ch_switch;//�����任
#endif
    surround_hdl *surround;         //������Ч���
    vbass_hdl *vbass;               //����������

    u32 id;					// Ψһ��ʶ�������ֵ
    void *file;				// �ļ����
    u32 pick_flag : 1;		// ��������֡���ͣ���MP3��)���������pcm���󼶲��ܽ��κ���Ч�����
    u32 pcm_enc_flag : 1;	// pcmѹ��������֡���ͣ���WAV�ȣ�
    u32 read_err : 2;		// �������� 0:no err�� 1:fat err,  2:disk err
    u32 ab_repeat_status : 3;	// AB����״̬

#if TCFG_DEC_DECRYPT_ENABLE
    CIPHER mply_cipher;		// ���ܲ���
#endif
#if TCFG_DEC_ID3_V1_ENABLE
    MP3_ID3_OBJ *p_mp3_id3_v1;	// id3_v1��Ϣ
#endif
#if TCFG_DEC_ID3_V2_ENABLE
    MP3_ID3_OBJ *p_mp3_id3_v2;	// id3_v2��Ϣ
#endif

#if FILE_DEC_REPEAT_EN
    u8 repeat_num;			// �޷�ѭ������
    struct fixphase_repair_obj repair_buf;	// �޷�ѭ�����
#endif

    struct audio_dec_breakpoint *bp;	// �ϵ���Ϣ

    void *evt_priv;			// �¼��ص�˽�в���
    void (*evt_cb)(void *, int argc, int *argv);	// �¼��ص����

    void (*stream_handler)(void *priv, int event, struct file_dec_hdl *);	// ���������ûص�
    void *stream_priv;						// ���������ûص�˽�о��

};



struct file_decoder *file_dec_get_file_decoder_hdl(void);

#define file_dec_is_stop()				file_decoder_is_stop(file_dec_get_file_decoder_hdl())
#define file_dec_is_play()				file_decoder_is_play(file_dec_get_file_decoder_hdl())
#define file_dec_is_pause()				file_decoder_is_pause(file_dec_get_file_decoder_hdl())
#define file_dec_pp()				    file_decoder_pp(file_dec_get_file_decoder_hdl())
#define file_dec_FF(x)					file_decoder_FF(file_dec_get_file_decoder_hdl(),x)
#define file_dec_FR(x)					file_decoder_FR(file_dec_get_file_decoder_hdl(),x)
#define file_dec_get_breakpoint(x)		file_decoder_get_breakpoint(file_dec_get_file_decoder_hdl(),x)
#define file_dec_get_total_time()		file_decoder_get_total_time(file_dec_get_file_decoder_hdl())
#define file_dec_get_cur_time()			file_decoder_get_cur_time(file_dec_get_file_decoder_hdl())
#define file_dec_get_decoder_type()		file_decoder_get_decoder_type(file_dec_get_file_decoder_hdl())

// ����һ���ļ�����
int file_dec_create(void *priv, void (*handler)(void *, int argc, int *argv));
// ���ļ�����
int file_dec_open(void *file, struct audio_dec_breakpoint *bp);
// �ر��ļ�����
void file_dec_close();

// �ļ��������¿�ʼ
int file_dec_restart(int id);
// �����ļ��������¿�ʼ����
int file_dec_push_restart(void);
// ��ȡfile_dec״̬
int file_dec_get_status(void);

// ���ý������������ûص��ӿ�
void file_dec_set_stream_set_hdl(struct file_dec_hdl *dec,
                                 void (*stream_handler)(void *priv, int event, struct file_dec_hdl *),
                                 void *stream_priv);

// ��ȡ�ļ�����hdl
void *get_file_dec_hdl();

#if (FILE_DEC_AB_REPEAT_EN)
int file_dec_ab_repeat_switch(void);
int file_dec_ab_repeat_close(void);
#else
#define file_dec_ab_repeat_switch()
#define file_dec_ab_repeat_close()
#endif/*FILE_DEC_AB_REPEAT_EN*/

#if (FILE_DEC_DEST_PLAY)
int file_dec_set_start_play(u32 start_time);
int file_dec_set_start_dest_play(u32 start_time, u32 dest_time, u32(*cb)(void *), void *cb_priv);
#else
#define file_dec_set_start_play(a)
#define file_dec_set_start_dest_play(a,b,c,d)
#endif/*FILE_DEC_DEST_PLAY*/


#endif /*TCFG_APP_MUSIC_EN*/

