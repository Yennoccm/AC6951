#include "chargeIc_manage.h"
#include "device/device.h"
#include "app_config.h"
#include "app_main.h"
#include "user_cfg.h"
#include "chgbox_det.h"
#include "chgbox_ctrl.h"
#include "chgbox_wireless.h"

#if (TCFG_CHARGE_BOX_ENABLE)

#define LOG_TAG_CONST       APP_CHGBOX
#define LOG_TAG             "[CHG_IC]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"

//������ת�����ĵ�ѹ��--ʹ�����ó��ʱ��Ч
#define CHARGE_CURRENT_CHANGE   (3000)
#define CHARGE_FULL_MA          (CHARGE_FULL_mA_30)
#define CHARGE_SLOW_MA          (CHARGE_mA_40)
#define CHARGE_WL_FAST_MA       (CHARGE_mA_200)//���߳��ʱ,ʹ��200mA������
#define CHARGE_FAST_MA          (CHARGE_mA_320)//�߳���ʹ��320mA������

#if (TCFG_CHARGE_MOUDLE_OUTSIDE == DISABLE)
static volatile u8 inside_ma;
static int charge_timer;
#endif
#if TCFG_SHORT_PROTECT_ENABLE
extern void short_det_wakeup_disable();
extern void short_det_wakeup_enable();
#endif
static volatile u8 charge_en;

/*------------------------------------------------------------------------------------*/
/**@brief    ���ݵ�ص�ѹ���õ�����λ
   @param    ��
   @return   ��
   @note     ��ʼ���ʱ���øýӿ��е���Ӧ��λ
*/
/*------------------------------------------------------------------------------------*/
#if (TCFG_CHARGE_MOUDLE_OUTSIDE == DISABLE)
void chargebox_charge_change_current(void *priv)
{
    u8 inside_target = CHARGE_FAST_MA;
    u16 vbat_volt = get_vbat_voltage();
    charge_timer = 0;
    if (vbat_volt > CHARGE_CURRENT_CHANGE) {
#if TCFG_WIRELESS_ENABLE
        if (sys_info.status[WIRELESS_DET] == STATUS_ONLINE) {
            inside_target = CHARGE_WL_FAST_MA;
        }
#endif
        CHARGE_mA_SEL(inside_target);
        inside_ma = inside_target;
    } else {
        //������ʱ,10s�鿴һ�ε���
        CHARGE_mA_SEL(CHARGE_SLOW_MA);
        inside_ma = CHARGE_SLOW_MA;
        charge_timer = sys_timeout_add(NULL, chargebox_charge_change_current, 10000);
    }
}
#endif

/*------------------------------------------------------------------------------------*/
/**@brief    ��ʼ�Ե�زֳ��
   @param    ��
   @return   ��
   @note
*/
/*------------------------------------------------------------------------------------*/
void chargebox_charge_start(void)
{
    log_info("charge start!\n");
#if TCFG_CHARGE_MOUDLE_OUTSIDE
    gpio_direction_input(TCFG_STOP_CHARGE_IO);
    usb_charge_full_wakeup_deal();//��ҳ��,�����ʱ����ȥ��ѯһ���Ƿ����
#else
    //���и��ݵ�ѹ�л�������λ,�ٿ����
    chargebox_charge_change_current(NULL);
    charge_start();
#endif
    charge_en = 1;
}

/*------------------------------------------------------------------------------------*/
/**@brief    ֹͣ�Ե�زֳ��
   @param    ��
   @return   ��
   @note
*/
/*------------------------------------------------------------------------------------*/
void chargebox_charge_close(void)
{
    log_info("charge close!\n");
#if TCFG_CHARGE_MOUDLE_OUTSIDE
    gpio_direction_output(TCFG_STOP_CHARGE_IO, 1);
#else
    if (charge_timer) {
        sys_timer_del(charge_timer);
        charge_timer = 0;
    }
    charge_close();
    CHARGE_mA_SEL(CHARGE_SLOW_MA);
    inside_ma = CHARGE_SLOW_MA;
#endif
    charge_en = 0;
}

/*------------------------------------------------------------------------------------*/
/**@brief    ��ȡϵͳ�Ƿ��ڳ��
   @param    ��
   @return   1:���ڳ��, 0:���ر�
   @note
*/
/*------------------------------------------------------------------------------------*/
u8 chargebox_get_charge_en(void)
{
    return charge_en;
}

/*------------------------------------------------------------------------------------*/
/**@brief    �����ѹ����
   @param    ��
   @return   ��
   @note
*/
/*------------------------------------------------------------------------------------*/
void chargeIc_boost_ctrl(u8 en)
{
#if TCFG_SHORT_PROTECT_ENABLE
    if (en == 0) {
        short_det_wakeup_disable();
    }
#endif
    gpio_direction_output(TCFG_BOOST_CTRL_IO, en);
    log_debug("boost_ctrl:%s\n", en ? "open" : "close");
#if TCFG_LDO_DET_ENABLE
    ldo_wakeup_deal(NULL);
#else
    sys_info.status[LDO_DET] = en ? STATUS_ONLINE : STATUS_OFFLINE;
#endif
#if TCFG_CURRENT_LIMIT_ENABLE
    //����ѹʱ��ȥ������������
    ear_current_detect_enable(en);
#endif
#if TCFG_SHORT_PROTECT_ENABLE
    if (en) {
        short_det_wakeup_enable();
    }
#endif
}

/*------------------------------------------------------------------------------------*/
/**@brief    ��������Դ����
   @param    ��
   @return   ��
   @note
*/
/*------------------------------------------------------------------------------------*/
void chargeIc_pwr_ctrl(u8 en)
{
#if (TCFG_PWR_CTRL_TYPE == PWR_CTRL_TYPE_PU_PD)
    if (en == 0) {
        gpio_set_pull_up(TCFG_PWR_CTRL_IO, 0);
        gpio_set_pull_down(TCFG_PWR_CTRL_IO, 1);
    } else {
        gpio_set_pull_down(TCFG_PWR_CTRL_IO, 0);
        gpio_set_pull_up(TCFG_PWR_CTRL_IO, 1);
    }
#elif (TCFG_PWR_CTRL_TYPE == PWR_CTRL_TYPE_OUTPUT_0)
    if (en == 0) {
        gpio_direction_input(TCFG_PWR_CTRL_IO);
    } else {
        gpio_direction_output(TCFG_PWR_CTRL_IO, 0);
    }
#elif (TCFG_PWR_CTRL_TYPE == PWR_CTRL_TYPE_OUTPUT_1)
    if (en == 0) {
        gpio_direction_input(TCFG_PWR_CTRL_IO);
    } else {
        gpio_direction_output(TCFG_PWR_CTRL_IO, 1);
    }
#endif
    log_debug("vol_ctrl:%s\n", en ? "open" : "close");
}

/*------------------------------------------------------------------------------------*/
/**@brief    ���IC��ʼ��
   @param    null
   @return   null
   @note     ��ʼ�����IC��ص�����
*/
/*------------------------------------------------------------------------------------*/
void chargeIc_init(void)
{
    //ʹ��IO����͵�ƽ
    gpio_set_die(TCFG_BOOST_CTRL_IO, 0);
    gpio_set_pull_down(TCFG_BOOST_CTRL_IO, 0);
    gpio_set_pull_up(TCFG_BOOST_CTRL_IO, 0);
    gpio_direction_output(TCFG_BOOST_CTRL_IO, 0);

    //PWR_CTRL��ʼ��
    gpio_set_die(TCFG_PWR_CTRL_IO, 0);
    gpio_set_pull_up(TCFG_PWR_CTRL_IO, 0);
    gpio_direction_input(TCFG_PWR_CTRL_IO);
#if (TCFG_PWR_CTRL_TYPE == PWR_CTRL_TYPE_PU_PD)
    gpio_set_pull_down(TCFG_PWR_CTRL_IO, 1);
#else
    gpio_set_pull_down(TCFG_PWR_CTRL_IO, 0);
#endif

#if TCFG_CHARGE_MOUDLE_OUTSIDE
    //�����ƽ�,����ʱ�������,���1ʱ����ֹ
    charge_en = 1;
    gpio_set_die(TCFG_STOP_CHARGE_IO, 1);
    gpio_set_pull_down(TCFG_STOP_CHARGE_IO, 0);
    gpio_set_pull_up(TCFG_STOP_CHARGE_IO, 0);
    if (charge_en == 1) {
        gpio_direction_input(TCFG_STOP_CHARGE_IO);
    } else {
        gpio_direction_output(TCFG_STOP_CHARGE_IO, 1);
    }
#else
    //���ó���ʼ��
    CHGBG_EN(0);
    CHARGE_EN(0);
    CHARGE_WKUP_PND_CLR();

    LVCMP_EN(1);
    LVCMP_EDGE_SEL(0);
    LVCMP_PND_CLR();
    LVCMP_EDGE_WKUP_EN(1);
    LVCMP_CMP_SEL(1);
    u16 charge_4202_trim_val = get_vbat_trim();
    if (charge_4202_trim_val == 0xf) {
        log_info("vbat not trim, use default config!!!!!!");
    }
    CHARGE_FULL_V_SEL(charge_4202_trim_val);
    CHARGE_FULL_mA_SEL(CHARGE_FULL_MA);
    CHARGE_mA_SEL(CHARGE_SLOW_MA);
    inside_ma = CHARGE_SLOW_MA;
    charge_en = 0;
#endif
}

#endif
