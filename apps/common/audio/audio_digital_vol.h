#ifndef _AUDIO_DIGITAL_VOL_H_
#define _AUDIO_DIGITAL_VOL_H_

#include "generic/typedef.h"
#include "os/os_type.h"
#include "os/os_api.h"
#include "generic/list.h"

#define BG_DVOL_FADE_ENABLE		1	/*��·�������ӣ����������Զ�����С��*/

typedef struct {
    u8 toggle;					/*������������*/
    u8 fade;					/*���뵭����־*/
    u8 vol;						/*���뵭����ǰ����(level)*/
    u8 vol_max;					/*���뵭���������(level)*/
    s16 vol_fade;				/*���뵭����Ӧ����ʼ����*/
#if BG_DVOL_FADE_ENABLE
    s16 vol_bk;					/*��̨�Զ�����ǰ����ֵ*/
    struct list_head entry;
#endif
    volatile s16 vol_target;	/*���뵭����Ӧ��Ŀ������*/
    volatile u16 fade_step;		/*���뵭���Ĳ���*/
} dvol_handle;


int audio_digital_vol_init(void);
void audio_digital_vol_bg_fade(u8 fade_out);
dvol_handle *audio_digital_vol_open(u8 vol, u8 vol_max, u16 fade_step);
void audio_digital_vol_close(dvol_handle *dvol);
void audio_digital_vol_set(dvol_handle *dvol, u8 vol);
u8 audio_digital_vol_get(void);
int audio_digital_vol_run(dvol_handle *dvol, void *data, u32 len);
void audio_digital_vol_reset_fade(dvol_handle *dvol);

/*************************�Զ���֧�������������������****************************/
void *user_audio_digital_volume_open(u8 vol, u8 vol_max, u16 fade_step);
int user_audio_digital_volume_close(void *_d_volume);
u8 user_audio_digital_volume_get(void *_d_volume);
int user_audio_digital_volume_set(void *_d_volume, u8 vol);
int user_audio_digital_volume_reset_fade(void *_d_volume);
int user_audio_digital_volume_run(void *_d_volume, void *data, u32 len, u8 ch_num);
void user_audio_digital_handler_run(void *_d_volume, void *data, u32 len);
void user_audio_digital_set_volume_tab(void *_d_volume, u16 *user_vol_tab, u8 user_vol_max);

void *user_audio_process_open(void *parm, void *priv, void (*handler)(void *priv, void *data, int len, u8 ch_num));
int user_audio_process_close(void *_uparm_hdl);
void user_audio_process_handler_run(void *_uparm_hdl, void *data, u32 len, u8 ch_num);

struct user_audio_digital_parm {
    u8 en;
    u8 vol;
    u8 vol_max;
    u16 fade_step;
};

struct digital_volume {
    u8 toggle;					/*������������*/
    u8 fade;					/*���뵭����־*/
    u8 vol;						/*���뵭����ǰ����*/
    u8 vol_max;					/*���뵭���������*/
    s16 vol_fade;				/*���뵭����Ӧ����ʼ����*/
    volatile s16 vol_target;	/*���뵭����Ӧ��Ŀ������*/
    volatile u16 fade_step;				/*���뵭���Ĳ���*/

    OS_MUTEX mutex;
    u8 ch_num;
    void *priv;
    u8 user_vol_max;                                 /*�Զ�����������*/
    volatile s16 *user_vol_tab;	                     /*�Զ���������*/

};

struct user_audio_parm {
    void *priv;
    void (*handler)(void *priv, void *data, int len, u8 ch_num);/*�û��Զ���ص�����*/
    struct digital_volume *dvol_hdl;
};

#endif
