#include "app_config.h"
#include "system/includes.h"
#include "fm_emitter/fm_emitter_manage.h"
#include "fm_emitter_inside.h"

#if(TCFG_FM_EMITTER_INSIDE_ENABLE == ENABLE)

#define FM_EMITTER_INSIDE_USE_STEREO		0	// 1:fm ����ʹ��������  0:��������


//*****************************************************************
//695 1:pb11 ��ǿ��������  0:pb10 ��ͨ����
//696 1:pb3 ��ǿ��������   0:pb1 ��ͨ����,ʹ��pb1��Ҫ��Ҫ�ı份��IO
//*****************************************************************
#define FM_EMITTER_INSIDE_USE_CH    		1

static void fm_emitter_inside_init(u16 fre)
{
    printf("fm emitter inside init \n");
    extern void fm_emitter_init(void);
    extern void fm_emitter_set_freq(u16 fre);
    extern void fmtx_stereo_onoff(u8 onoff);
    extern void fm_emitter_set_ch(u8 ch);
    extern void fm_emitter_set_power(u8 level);
#if FM_EMITTER_INSIDE_USE_STEREO
    fmtx_stereo_onoff(1);
#endif
#if FM_EMITTER_INSIDE_USE_CH
    fm_emitter_set_ch(1);
    fm_emitter_init();
    fm_emitter_set_power(3);//���ʵȼ�0~3����߷��书��Ϊ3�����ڳ�ʼ��֮������,ֻ����ǿ��
#else
    fm_emitter_init();
#endif

    if (fre) {
        fm_emitter_set_freq(fre);
    }

}

static void fm_emitter_inside_start(void)
{
    extern void fmtx_on();
    fmtx_on();
}
static void fm_emitter_inside_stop(void)
{
    extern void fmtx_off();
    fmtx_off();
}

static void fm_emitter_inside_set_fre(u16 fre)
{
    extern void fm_emitter_set_freq(u16 fre);
    fm_emitter_set_freq(fre);
}

//**************************************************
//���ʵ���ֻ�����ڳ�ǿ����,ʹ����ͨIO���䲻�����������
//�ȼ�0-3,3Ϊ���������ʣ�Ĭ��ʹ�õȼ�3��߹���
//�������÷��ڳ�ʼ��֮��
//�ù��ʵ���ΪȫƵ�㹦�ʵ��ڣ����Բ��� fre ��Ч����0����
//**************************************************
static void fm_emitter_inside_set_power(u8 level, u16 fre)
{
    extern void fm_emitter_set_power(u8 level);
    fm_emitter_set_power(level);
}

static void fm_emitter_inside_set_data_cb(void *cb)
{
    extern void fm_emitter_set_data_cb(void *cb);
    fm_emitter_set_data_cb(cb);
}

REGISTER_FM_EMITTER(fm_emitter_inside) = {
    .name      = "fm_emitter_inside",
    .init      = fm_emitter_inside_init,
    .start     = fm_emitter_inside_start,
    .stop      = fm_emitter_inside_stop,
    .set_fre   = fm_emitter_inside_set_fre,
    .set_power = fm_emitter_inside_set_power,
    .data_cb   = fm_emitter_inside_set_data_cb,
};

#endif
