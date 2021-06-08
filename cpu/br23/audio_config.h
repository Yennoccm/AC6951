#ifndef _AUDIO_CONFIG_H_
#define _AUDIO_CONFIG_H_

#include "generic/typedef.h"
#include "board_config.h"

#if(defined(CONFIG_MIXER_CYCLIC)&&(SOUNDCARD_ENABLE))
#define AUDIO_MIXER_LEN			(128 * 2 * 2 * 6)
#else
#define AUDIO_MIXER_LEN			(128 * 2 * 2)
#endif

#if (SOUNDCARD_ENABLE)
#define AUDIO_SYNTHESIS_LEN     (AUDIO_MIXER_LEN * 4)
#else
#define AUDIO_SYNTHESIS_LEN     (AUDIO_MIXER_LEN * 2)
#endif

/*��������cpuռ���ʸ���*/
#define TCFG_AUDIO_DECODER_OCCUPY_TRACE		1

#if BT_SUPPORT_MUSIC_VOL_SYNC
#define TCFG_MAX_VOL_PROMPT						 0
#else
#define TCFG_MAX_VOL_PROMPT						 1
#endif

/*
 *������������û�����������Ĳ�Ʒ����ֹ������ͬ��֮��
 *����֧������ͬ�����豸����������С�������Ӳ�֧����
 *��ͬ�����豸������û�лָ�����������С������
 *Ĭ����û������ͬ���ģ����������õ����ֵ��������vol_sync.c
 *�ú������޸���Ӧ�����á�
 */
#define TCFG_VOL_RESET_WHEN_NO_SUPPORT_VOL_SYNC	 0	//��֧������ͬ�����豸Ĭ���������

/*
 *ʡ����micƫ�õ�ѹ�Զ�����(��ΪУ׼��Ҫʱ�䣬�����в�ͬ�ķ�ʽ)
 *1�����������ȫ���£�����������������У׼һ��
 *2���ϵ縴λ��ʱ��У׼,���ϵ������ϵ�ͻ�У׼�Ƿ���ƫ��(Ĭ��)
 *3��ÿ�ο�����У׼��������û�жϹ��磬��У׼����ÿ�ζ���
 */
#define MC_BIAS_ADJUST_DISABLE		0	//ʡ����micƫ��У׼�ر�
#define MC_BIAS_ADJUST_ONE			1	//ʡ����micƫ��ֻУ׼һ�Σ���dac trimһ����
#define MC_BIAS_ADJUST_POWER_ON		2	//ʡ����micƫ��ÿ���ϵ縴λ��У׼(Power_On_Reset)
#define MC_BIAS_ADJUST_ALWAYS		3	//ʡ����micƫ��ÿ�ο�����У׼(�����ϵ縴λ��������λ)
#if TCFG_MIC_CAPLESS_ENABLE
#define TCFG_MC_BIAS_AUTO_ADJUST	MC_BIAS_ADJUST_POWER_ON
#else
#define TCFG_MC_BIAS_AUTO_ADJUST	MC_BIAS_ADJUST_DISABLE
#endif/*TCFG_MIC_CAPLESS_ENABLE*/
#define TCFG_MC_CONVERGE_TRACE		0	//ʡ����mic����ֵ����
/*
 *ʡ����mic������������
 *0:����Ӧ��������, >0:�����������ֵ
 *ע����mic��ģ�����������������ܴ��ʱ��mic_caplessģʽ��������,
 *�仯�ĵ�ѹ�Ŵ�󣬿��ܻ����������������ʱ��Ϳ�������ס�����������
 *������ƽ������(ǰ����Ԥ�����ɹ��������)
 */
#define TCFG_MC_DTB_STEP_LIMIT		15  //�����������ֵ

#if TCFG_USER_BLE_ENABLE
#define TCFG_AEC_SIMPLEX			0  //ͨ������ģʽ����
#else
#define TCFG_AEC_SIMPLEX			0  //ͨ������ģʽ����
#endif

#define TCFG_ESCO_PLC				1  //ͨ�������޸�

#define TCFG_ESCO_LIMITER			0  	//ͨ�����˵���/�޷���

#if (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_MONO_LR_DIFF || \
     TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_DUAL_LR_DIFF)
#define MAX_ANA_VOL             (21)
#else
#define MAX_ANA_VOL             (27)
#endif/*TCFG_AUDIO_DAC_CONNECT_MODE*/

#define MAX_COM_VOL             (22)    // ������ֵӦС�����������ȼ��������С (combined_vol_list)
#define MAX_DIG_VOL             (100)

#define MAX_DIG_GR_VOL          (30)//��������������ֵ

#if ((SYS_VOL_TYPE == VOL_TYPE_DIGITAL) || (SYS_VOL_TYPE == VOL_TYPE_DIGITAL_HW))
#define SYS_MAX_VOL             MAX_DIG_VOL
#elif (SYS_VOL_TYPE == VOL_TYPE_ANALOG)
#define SYS_MAX_VOL             MAX_ANA_VOL
#elif (SYS_VOL_TYPE == VOL_TYPE_AD)
#define SYS_MAX_VOL             MAX_COM_VOL
#elif (SYS_VOL_TYPE == VOL_TYPE_DIGGROUP)
#define SYS_MAX_VOL             MAX_DIG_GR_VOL
#else
#error "SYS_VOL_TYPE define error"
#endif


#define SYS_DEFAULT_VOL         	0  //(SYS_MAX_VOL/2)
#define SYS_DEFAULT_TONE_VOL    	18 //(SYS_MAX_VOL)
#define SYS_DEFAULT_SIN_VOL     	17

#define APP_AUDIO_STATE_IDLE        0
#define APP_AUDIO_STATE_MUSIC       1
#define APP_AUDIO_STATE_CALL        2
#define APP_AUDIO_STATE_WTONE       3
#define APP_AUDIO_STATE_LINEIN      4
#define APP_AUDIO_CURRENT_STATE     5

#define APP_AUDIO_MAX_STATE    (APP_AUDIO_CURRENT_STATE + 1)

#ifdef TCFG_IIS_ENABLE
#define AUDIO_OUTPUT_ONLY_IIS \
    (TCFG_IIS_ENABLE && TCFG_IIS_OUTPUT_EN && AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_IIS)

#define AUDIO_OUTPUT_DAC_AND_IIS \
    (AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_DAC_IIS && TCFG_IIS_ENABLE && TCFG_IIS_OUTPUT_EN)
#else
#define AUDIO_OUTPUT_ONLY_IIS   0

#define AUDIO_OUTPUT_DAC_AND_IIS (AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_DAC_IIS)

#endif

#define AUDIO_OUTPUT_INCLUDE_IIS (AUDIO_OUTPUT_ONLY_IIS || AUDIO_OUTPUT_DAC_AND_IIS)

#define AUDIO_OUTPUT_ONLY_DAC 		(AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_DAC)
#define AUDIO_OUTPUT_INCLUDE_DAC 	((AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_DAC) \
		|| (AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_BT) \
		|| (AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_DONGLE) \
		|| (AUDIO_OUTPUT_DAC_AND_IIS))


u8 get_max_sys_vol(void);
u8 get_tone_vol(void);

void app_audio_output_init(void);
void app_audio_output_sync_buff_init(void *sync_buff, int len);
int app_audio_output_channel_set(u8 output);
int app_audio_output_channel_get(void);
int app_audio_output_mode_set(u8 output);
int app_audio_output_mode_get(void);
int app_audio_output_write(void *buf, int len);
int app_audio_output_samplerate_select(u32 sample_rate, u8 high);
int app_audio_output_samplerate_set(int sample_rate);
int app_audio_output_samplerate_get(void);
int app_audio_output_start(void);
int app_audio_output_stop(void);
int app_audio_output_reset(u32 msecs);
int app_audio_output_state_get(void);
int app_audio_output_get_cur_buf_points(void);
void app_audio_output_ch_mute(u8 ch, u8 mute);
int app_audio_output_ch_analog_gain_set(u8 ch, u8 again);
s8 app_audio_get_volume(u8 state);
void app_audio_set_volume(u8 state, s8 volume, u8 fade);
void app_audio_volume_up(u8 value);
void app_audio_volume_down(u8 value);
void app_audio_state_switch(u8 state, s16 max_volume);
void app_audio_mute(u8 value);
s16 app_audio_get_max_volume(void);
void app_audio_state_switch(u8 state, s16 max_volume);
void app_audio_state_exit(u8 state);
u8 app_audio_get_state(void);
void volume_up_down_direct(s8 value);
void app_audio_set_mix_volume(u8 front_volume, u8 back_volume);
void app_audio_volume_init(void);
void app_audio_set_digital_volume(s16 volume);
void dac_trim_hook(u8 pos);

int audio_output_buf_time(void);
int audio_output_dev_is_working(void);
int audio_output_sync_start(void);
int audio_output_sync_stop(void);

void app_set_sys_vol(s16 vol_l, s16  vol_r);
void app_set_max_vol(s16 vol);

u32 phone_call_eq_open();
int eq_mode_sw();
int mic_test_start();
int mic_test_stop();

void dac_power_on(void);
void dac_power_off(void);

#endif/*_AUDIO_CONFIG_H_*/
