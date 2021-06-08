/*
 ****************************************************************
 *File : audio_dec_midi_file.c
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
#include "MIDI_DEC_API.h"
#include "audio_dec_file.h"


#if TCFG_APP_MUSIC_EN
#if TCFG_DEC_MIDI_ENABLE

u32 tmark_trigger(void *priv, u8 *val, u8 len)
{
    return 0;
}

u32 melody_trigger(void *priv, u8 key, u8 vel)
{
    return 0;
}

u32 timDiv_trigger(void *priv)
{
    return 0;
}

u32 beat_trigger(void *priv, u8 val1/*һ�ڶ�����*/, u8 val2/*ÿ�Ķ��ٷ�����*/)
{
    return 0;
}
/*----------------------------------------------------------------------------*/
/**@brief    midi��ɫ�ļ���
   @param
   @return
   @note     �ڲ�����
*/
/*----------------------------------------------------------------------------*/
int midi_fread(void *file, void *buf, u32 len)
{

#ifndef CONFIG_MIDI_DEC_ADDR
    FILE *hd = (FILE *)file;
    if (hd) {
        len = fread(hd, buf, len);
    }
#endif
    return len;
}
/*----------------------------------------------------------------------------*/
/**@brief    midi��ɫ�ļ�seek
   @param
   @return
   @note     �ڲ�����
*/
/*----------------------------------------------------------------------------*/
int midi_fseek(void *file, u32 offset, int seek_mode)
{
#ifndef CONFIG_MIDI_DEC_ADDR
    FILE *hd = (FILE *)file;
    if (hd) {
        fseek(hd, offset, seek_mode);
    }
#endif
    return 0;
}
/*----------------------------------------------------------------------------*/
/**@brief    midi��ʼ����������midi_init����
   @param    midi_init_info_v:midi��Ϣ
   @param    addr:��ɫ�ļ����
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void init_midi_info_val(MIDI_INIT_STRUCT  *midi_init_info_v, u8 *addr)
{
    //midi��ʼ����
    midi_init_info_v->init_info.player_t = 8;
    midi_init_info_v->init_info.sample_rate = 4;
    midi_init_info_v->init_info.spi_pos = (u32)addr;
    midi_init_info_v->init_info.fread = midi_fread;
    midi_init_info_v->init_info.fseek = midi_fseek;


    //midi��ģʽ��ʼ��
    midi_init_info_v->mode_info.mode = 0; //CMD_MIDI_CTRL_MODE_2;
    /* midi_init_info_v->mode_info.mode = 1;//CMD_MIDI_CTRL_MODE_2; */

    //midi�����ʼ��
    midi_init_info_v->tempo_info.tempo_val = 1042;

    midi_init_info_v->tempo_info.decay_val = ((u16)31 << 11) | 1024;
    midi_init_info_v->tempo_info.mute_threshold = (u16)1L << 29;

    //midi�������ʼ��
    midi_init_info_v->mainTrack_info.chn = 17; //���ĸ�������������

    //midi�ⲿ������ʼ��
    {
        u32 tmp_i;
        for (tmp_i = 0; tmp_i < 16; tmp_i++) {
            midi_init_info_v->vol_info.cc_vol[tmp_i] = 4096; //4096��ԭ��������
        }
    }

    //midi���������������
    midi_init_info_v->prog_info.prog = 0;
    midi_init_info_v->prog_info.ex_vol = 1024;
    midi_init_info_v->prog_info.replace_mode = 0;


    //midi��mark���Ƴ�ʼ��
    midi_init_info_v->mark_info.priv = NULL; //&file_mark;
    midi_init_info_v->mark_info.mark_trigger = tmark_trigger;

    //midi��melody���Ƴ�ʼ��
    midi_init_info_v->moledy_info.priv = NULL; //&file_melody;
    midi_init_info_v->moledy_info.melody_trigger = melody_trigger;

    //midi��С�ڻص����Ƴ�ʼ��
    midi_init_info_v->tmDiv_info.priv = NULL;
    midi_init_info_v->tmDiv_info.timeDiv_trigger = timDiv_trigger;

    //midi��С�Ļص����Ƴ�ʼ��
    midi_init_info_v->beat_info.priv = NULL;
    midi_init_info_v->beat_info.beat_trigger = beat_trigger;

    //ʹ��λ����
    midi_init_info_v->switch_info = MELODY_PLAY_ENABLE | MELODY_ENABLE | EX_VOL_ENABLE;            //���������ʹ��

    return;
}

FILE  *midi_file = NULL;
int midi_get_cfg_addr(u8 **addr)
{
#ifndef CONFIG_MIDI_DEC_ADDR
    //��ɫ�ļ�֧�����ⲿ�洢���������flash,sdkĬ��ʹ�ñ���ʽ
    //��ȡ��ɫ�ļ�
    /* FILE  *file = fopen("storage/sd0/C/MIDI.bin\0", "r"); */
    FILE  *file = fopen(SDFILE_RES_ROOT_PATH"MIDI.bin\0", "r");
    if (!file) {
        log_e("MIDI.bin open err\n");
        return -1;
    }
    *addr = file;
    midi_file = file;
    printf("midi_file %x\n", midi_file);
#else
    //��ɫ�ļ���֧��������flash
    FILE  *file = fopen(SDFILE_RES_ROOT_PATH"MIDI.bin\0", "r");
    if (!file) {
        log_e("MIDI.bin open err\n");
        return -1;
    }

    struct vfs_attr attr = {0};
    fget_attrs(file, &attr);
    *addr = (u8 *)attr.sclust;
    fclose(file);
#endif

    return 0;
}

/*----------------------------------------------------------------------------*/
/**@brief    midi��ɫ�ļ��ر�
   @param
   @return
   @note     �ú�����midi�ر�ʱ����,�ú����������庯���������޸Ķ���
*/
/*----------------------------------------------------------------------------*/
int midi_uninit()
{
#ifndef CONFIG_MIDI_DEC_ADDR
    if (midi_file) {
        fclose(midi_file);
        midi_file = NULL;
    }
#endif
    return 0;
}
/*----------------------------------------------------------------------------*/
/**@brief    midi��ʼ���������ú����ɿ����
   @param    �������ص�ַ
   @return   0
   @note     �ú����������庯���������޸Ķ���
*/
/*----------------------------------------------------------------------------*/
int midi_init(void *info)
{
    u8 *cache_addr;
    if (midi_get_cfg_addr(&cache_addr)) {
        log_e("get midi addr err\n");
        return -1;
    }
    //��ʼ��midi����
    init_midi_info_val(info, cache_addr);  //��Ҫ�ⲿд
    return 0;
}

/*----------------------------------------------------------------------------*/
/**@brief    midi���ƺ���
	@param   cmd:
		     CMD_MIDI_SET_CHN_PROG,  //��������,�����ṹ��Ӧ MIDI_PROG_CTRL_STRUCT
		     CMD_MIDI_CTRL_TEMPO,    //�ı����,�����ṹ��Ӧ MIDI_PLAY_CTRL_TEMPO
		     CMD_MIDI_GOON,          //one key one note��ʱ����Ų���ʹ��,����Ϊ��
		     CMD_MIDI_CTRL_MODE,     //�ı�ģʽ,�����ṹ��Ӧ MIDI_PLAY_CTRL_MO
		     CMD_MIDI_SET_SWITCH,    //���ÿ���ʹ�ܣ�Ҫ��Ҫ�滻������ʹ���ⲿ����,������Ӧ MIDI_SET_SWITCH
		     CMD_MIDI_SET_EX_VOL,    //�����ⲿ��������,�����ṹ��Ӧ EX_CH_VOL_PARM
   @param    priv:��Ӧcmd�Ĳ�����ַ
   @return   0
   @note    midi�����������
*/
/*----------------------------------------------------------------------------*/
void midi_ioctrl(u32 cmd, void *priv)
{
    struct file_dec_hdl *dec = get_file_dec_hdl();	// �ļ�������
    if (dec) {
        log_e("file dec NULL\n");
        return ;
    }

    int status = file_dec_get_status();
    if (status == FILE_DEC_STATUS_STOP) {
        log_w("file dec is stop\n");
        return ;
    }

    log_i("midi cmd %x", cmd);
    audio_decoder_ioctrl(&dec->file_dec.decoder, cmd, priv);
}
#if 0
//��������
void *ex_vol_test()
{
    static int val = 4096;
    EX_CH_VOL_PARM ex_vol;
    for (int test_ci = 0; test_ci < CTRL_CHANNEL_NUM; test_ci++) {
        ex_vol.cc_vol[test_ci] = val;
    }
    val -= 64;
    if (val <= 0) {
        val = 4096;
    }
    midi_ioctrl(CMD_MIDI_SET_EX_VOL, &ex_vol);
    return NULL;
}
#endif

#endif /*TCFG_DEC_MIDI_ENABLE*/
#endif /*TCFG_APP_MUSIC_EN*/

