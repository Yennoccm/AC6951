#include "effect_reg.h"
#include "app_config.h"
#include "audio_enc.h"
/**********************���칦��ѡ��*********************************/
#if (TCFG_MIC_EFFECT_ENABLE)
#if (TCFG_MIC_EFFECT_SEL == MIC_EFFECT_REVERB)
#define MIC_EFFECT_CONFIG 	  	  0	\
								| BIT(MIC_EFFECT_CONFIG_REVERB) \
								| BIT(MIC_EFFECT_CONFIG_PITCH) \
								| BIT(MIC_EFFECT_CONFIG_NOISEGATE) \
								| BIT(MIC_EFFECT_CONFIG_DVOL) \
								| BIT(MIC_EFFECT_CONFIG_ENERGY_DETECT) \
								| BIT(MIC_EFFECT_CONFIG_EQ) \
								| BIT(MIC_EFFECT_CONFIG_LINEIN) \


#define MIC_EFFECT_TOOL_CONFIG_ARRAY	reverb_tool_attr
#elif (TCFG_MIC_EFFECT_SEL == MIC_EFFECT_ECHO)
#define MIC_EFFECT_CONFIG 	  	  0	\
								| BIT(MIC_EFFECT_CONFIG_ECHO) \
								| BIT(MIC_EFFECT_CONFIG_NOISEGATE) \
								| BIT(MIC_EFFECT_CONFIG_PITCH) \
								| BIT(MIC_EFFECT_CONFIG_HOWLING) \
								| BIT(MIC_EFFECT_CONFIG_DVOL)\
								| BIT(MIC_EFFECT_CONFIG_EQ) \
								| BIT(MIC_EFFECT_CONFIG_SOFT_SRC)\


#define MIC_EFFECT_TOOL_CONFIG_ARRAY	echo_tool_attr
#endif//
/*********************************************************************/

/**********************����������ѡ��*********************************/
#if (TCFG_LOUDSPEAKER_ENABLE)
#undef MIC_EFFECT_CONFIG
#if (TCFG_MIC_EFFECT_SEL == MIC_EFFECT_REVERB)
#define MIC_EFFECT_CONFIG     0\
										| BIT(MIC_EFFECT_CONFIG_HOWLING) \
										| BIT(MIC_EFFECT_CONFIG_SOFT_SRC)\
										/* | BIT(MIC_EFFECT_CONFIG_REVERB) \ */
/* | BIT(MIC_EFFECT_CONFIG_PITCH) \ */
/* | BIT(MIC_EFFECT_CONFIG_EQ) \ */


#elif (TCFG_MIC_EFFECT_SEL == MIC_EFFECT_ECHO)
#define MIC_EFFECT_CONFIG     0\
										| BIT(MIC_EFFECT_CONFIG_HOWLING) \
										| BIT(MIC_EFFECT_CONFIG_SOFT_SRC)\
										/* | BIT(MIC_EFFECT_CONFIG_ECHO) \ */
/* | BIT(MIC_EFFECT_CONFIG_PITCH) \ */
/* | BIT(MIC_EFFECT_CONFIG_DVOL)\ */
/* | BIT(MIC_EFFECT_CONFIG_NOISEGATE) \ */
/* | BIT(MIC_EFFECT_CONFIG_EQ) \ */

#endif
#endif
/*********************************************************************/

const struct __mic_effect_parm effect_parm_default = {
    .effect_config = MIC_EFFECT_CONFIG,///����ͨ·֧����Щ����
    .effect_run = MIC_EFFECT_CONFIG,///�����֮ʱ�� Ĭ�ϴ򿪵Ĺ���
    .sample_rate = MIC_EFFECT_SAMPLERATE,
};

const struct __mic_stream_parm effect_mic_stream_parm_default = {
#if (TCFG_MIC_EFFECT_SEL == MIC_EFFECT_REVERB)
    .mic_gain			= 0,
#else
    .mic_gain			= 4,
#endif
    .sample_rate 		= MIC_EFFECT_SAMPLERATE,//������
#if (RECORDER_MIX_EN)
    .point_unit  		= 160,//һ�δ������ݵ����ݵ�Ԫ�� ��λ�� 4����(Ҫ���mic���жϵ����޸�)
    .dac_delay			= 20,//dacӲ��������ʱ�� ��λms Ҫ���� point_unit*2
#else
#if (TCFG_MIC_EFFECT_SEL == MIC_EFFECT_REVERB)
    .point_unit  		= REVERB_LADC_IRQ_POINTS,//һ�δ������ݵ����ݵ�Ԫ�� ��λ�� 4����(Ҫ���mic���жϵ����޸�)
    .dac_delay			= (int)((REVERB_LADC_IRQ_POINTS * 2) / (TCFG_REVERB_SAMPLERATE_DEFUAL / 1000.0)), //6,//8,//8,//10,//dacӲ��������ʱ�� ��λms Ҫ���� point_unit*2 // (REVERB_LADC_IRQ_POINTS*1.5)/(TCFG_REVERB_SAMPLERATE_DEFUAL/1000.0)
#else
    .point_unit  		= 80,//һ�δ������ݵ����ݵ�Ԫ�� ��λ�� 4����(Ҫ���mic���жϵ����޸�)
    .dac_delay			= 10,//dacӲ��������ʱ�� ��λms Ҫ���� point_unit*2
#endif
#endif
};

const REVERBN_PARM_SET effect_reverb_parm_default = {
    .dry = 100,						//[0:200]%
    .wet = 80,	  					//[0:300]%
    .delay = 70,					//[0-100]ms
    .rot60 = 1400,					//[0:15000]ms  //����ϵ�� ֵԽ�� ����Խ���� ˥����
    .Erwet = 60,					//[5:250]%
    .Erfactor = 180,					//[50:250]%
    .Ewidth = 100,					//[-100:100]%
    .Ertolate  = 100,				//[0:100]%
    .predelay  = 0,					//[0:20]ms
    //���²�����Ч������ͨ��EQ����
    .width  = 100,					//[0:100]%
    .diffusion  = 70,				//[0:100]%
    .dampinglpf  = 15000,			//[0:18k]
    .basslpf = 500,					//[0:1.1k]
    .bassB = 10,					//[0:80]%
    .inputlpf  = 18000,				//[0:18k]
    .outputlpf  = 18000,			//[0:18k]
};


const EF_REVERB_FIX_PARM effect_echo_fix_parm_default = {
    .wetgain = 2048,			////ʪ�����棺[0:4096]
    .drygain = 4096,				////��������: [0:4096]
    .sr = MIC_EFFECT_SAMPLERATE,		////������
    .max_ms = 200,				////����Ҫ�������ʱ��Ӱ�� need_buf ��С

};

const ECHO_PARM_SET effect_echo_parm_default = {
    .delay = 200,				//��������ʱʱ�� 0-300ms
    .decayval = 50,				// 0-70%
    .direct_sound_enable = 1,	//ֱ����ʹ��  0/1
    .filt_enable = 0,			//��ɢ�˲���ʹ��
};


const  PITCH_SHIFT_PARM effect_pitch_parm_default = {
    .sr = MIC_EFFECT_SAMPLERATE,
    .shiftv = 100,
    .effect_v = EFFECT_PITCH_SHIFT,
    .formant_shift = 100,
};

const NOISEGATE_PARM effect_noisegate_parm_default = {
    .attackTime = 300,
    .releaseTime = 5,
    .threshold = -45000,
    .low_th_gain = 0,
    .sampleRate = MIC_EFFECT_SAMPLERATE,
    .channel = 1,
};

static const u16 r_dvol_table[] = {
    0	, //0
    93	, //1
    111	, //2
    132	, //3
    158	, //4
    189	, //5
    226	, //6
    270	, //7
    323	, //8
    386	, //9
    462	, //10
    552	, //11
    660	, //12
    789	, //13
    943	, //14
    1127, //15
    1347, //16
    1610, //17
    1925, //18
    2301, //19
    2751, //20
    3288, //21
    3930, //22
    4698, //23
    5616, //24
    6713, //25
    8025, //26
    9592, //27
    11466,//28
    15200,//29
    16000,//30
    16384 //31
};

audio_dig_vol_param effect_dvol_default_parm = {
#if (SOUNDCARD_ENABLE)
    .vol_start = 0,//30,
#else
    .vol_start = 30,
#endif
    .vol_max = ARRAY_SIZE(r_dvol_table) - 1,
    .ch_total = 1,
    .fade_en = 1,
    .fade_points_step = 2,
    .fade_gain_step = 2,
    .vol_list = (void *)r_dvol_table,
};


// �����˲���:
const SHOUT_WHEAT_PARM_SET effect_shout_wheat_default = {
    .center_frequency = 1500,//����Ƶ��: 800
    .bandwidth = 4000,//����:   4000
    .occupy = 80,//ռ��:   100%
};
// ������
const LOW_SOUND_PARM_SET effect_low_sound_default = {
    .cutoff_frequency = 700,//��ֹƵ�ʣ�600
    .highest_gain = 1000,//������棺0
    .lowest_gain = -12000,//�������:  -12000
};
// ������
const HIGH_SOUND_PARM_SET effect_high_sound_default = {
    .cutoff_frequency = 2000,//:��ֹƵ��:  1800
    .highest_gain = 1000,//������棺0
    .lowest_gain = -12000,// ������棺-12000
};

/* const struct audio_eq_param effect_eq_default_parm = { */
/* .channels  		= 1, */
/* .online_en 		= 1, */
/* .mode_en   		= 1, */
/* .remain_en 		= 1, */
/* .no_wait   		= 0, */
/* [> .eq_switch 		= 0, <] */
/* .eq_name 		= 3, */
/* .max_nsection 	= EQ_SECTION_MAX + 3,///�����������ߵ���������� */
/* .cb 			= mic_eq_get_filter_info, */
/* }; */

/* const  */
//����ʹ�� const ,��Աsamplerate��������
struct __effect_mode_cfg effect_tool_parm = {
    .debug_en = 1,
    .mode_max = ARRAY_SIZE(MIC_EFFECT_TOOL_CONFIG_ARRAY),
    .sample_rate = MIC_EFFECT_SAMPLERATE,
    .attr = (struct __effect_mode_attr *)MIC_EFFECT_TOOL_CONFIG_ARRAY,
    .file_path = SDFILE_RES_ROOT_PATH"effects_cfg.bin",
};



const struct __effect_dodge_parm dodge_parm = {
    .dodge_in_thread = 1000,//�������ܵ�������ֵ
    .dodge_in_time_ms = 1,//����ֵ��������dodge_in_thread �ʹ�������
    .dodge_out_thread = 1000,//�˳����ܵ�������ֵ
    .dodge_out_time_ms = 500,//����ֵ����С��dodge_out_thread ���˳�����
};



#endif//TCFG_MIC_EFFECT_ENABLE


