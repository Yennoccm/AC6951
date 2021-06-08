#ifndef _AUDIO_LINK_H_
#define _AUDIO_LINK_H_

//ch_num
#define ALINK_CH0    	0
#define ALINK_CH1    	1
#define ALINK_CH2    	2
#define ALINK_CH3    	3

#define ALNK_BUF_POINTS_NUM 		128

typedef enum {
    ALINK0_PORTA,            //MCLK:PA8 SCLK:PA2  LRCK:PA3 CH0:PA4 CH1:PA5 CH2:PA6 CH3:PA7
    ALINK0_PORTB,            //MCLK:PA15 SCLK:PA9  LRCK:PA10 CH0:PA11 CH1:PA12 CH2:PA13 CH3:PA14
    ALINK1_PORTA,            //MCLK:PB0 SCLK:PC0  LRCK:PC1 CH0:PC2 CH1:PC3 CH2:PC4 CH3:PC5
} ALINK_PORT;

//ch_dir
typedef enum {
    ALINK_DIR_TX	= 0u,
    ALINK_DIR_RX		,
} ALINK_DIR;

typedef enum {
    ALINK_LEN_16BIT = 0u,
    ALINK_LEN_24BIT		, //ALINK_FRAME_MODE��Ҫѡ��: ALINK_FRAME_64SCLK
} ALINK_DATA_WIDTH;

//ch_mode
typedef enum {
    ALINK_MD_NONE	= 0u,
    ALINK_MD_IIS		,
    ALINK_MD_IIS_LALIGN	,
    ALINK_MD_IIS_RALIGN	,
    ALINK_MD_DSP0		,
    ALINK_MD_DSP1		,
} ALINK_MODE;

//ch_mode
typedef enum {
    ALINK_ROLE_MASTER, //����
    ALINK_ROLE_SLAVE,  //�ӻ�
} ALINK_ROLE;

typedef enum {
    ALINK_CLK_FALL_UPDATE_RAISE_SAMPLE, //�½��ظ�������, �����ز�������
    ALINK_CLK_RAISE_UPDATE_FALL_SAMPLE, //�Ͻ��ظ�������, �����ز�������
} ALINK_CLK_MODE;

typedef enum {
    ALINK_FRAME_32SCLK, 	//32 sclk/frame
    ALINK_FRAME_64SCLK, 	//64 sclk/frame
} ALINK_FRAME_MODE;

typedef enum {
    ALINK_SR_48000 = 48000,
    ALINK_SR_44100 = 44100,
    ALINK_SR_32000 = 32000,
    ALINK_SR_24000 = 24000,
    ALINK_SR_22050 = 22050,
    ALINK_SR_16000 = 16000,
    ALINK_SR_12000 = 12000,
    ALINK_SR_11025 = 11025,
    ALINK_SR_8000  = 8000,
} ALINK_SR;

struct alnk_ch_cfg {
    u8 enable;
    ALINK_DIR dir; 				//ͨ���������ݷ���: Tx, Rx
    void *buf;					//dma buf��ַ
    void (*isr_cb)(u8 ch, s16 *buf, u32 len);
};

//===================================//
//���ͨ��ʹ����Ҫע��:
//1.����λ����Ҫ����һ��
//2.buf������ͬ
//===================================//
typedef struct _ALINK_PARM {
    u8 port_select;
    struct alnk_ch_cfg ch_cfg[4];		//ͨ���ڲ�����
    ALINK_MODE mode; 					//IIS, left, right, dsp0, dsp1
    ALINK_ROLE role; 			//����/�ӻ�
    ALINK_CLK_MODE clk_mode; 			//���ºͲ�������
    ALINK_DATA_WIDTH  bitwide;   //����λ��16/32bit
    ALINK_FRAME_MODE sclk_per_frame;  	//32/64 sclk/frame
    u16 dma_len; 						//buf����: byte
    ALINK_SR sample_rate;					//������
} ALINK_PARM;

int alink_init(ALINK_PARM *parm);  //iis ��ʼ��
int	alink_start(ALINK_PORT port);             //iis ����
void alink_channel_init(ALINK_PORT port, u8 ch_idx, u8 dir, void (*handle)(u8 ch, s16 *buf, u32 len));   //iis ��ͨ��
void alink_channel_close(ALINK_PORT port, u8 ch_idx); //iis �ر�ͨ��
int alink_sr_set(ALINK_PORT port, u16 sr); 			//iis ���ò�����
void alink_uninit(ALINK_PORT port); 			//iis �˳�

void audio_link_init(ALINK_PORT port);
void audio_link_uninit(ALINK_PORT port);

#endif
