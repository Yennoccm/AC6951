#include "gpio.h"
#include "app_config.h"
#include "system/includes.h"
#include "device/wireless_charge.h"
#include "chgbox_ctrl.h"
#include "asm/adc_api.h"
#include "chgbox_box.h"
#include "device/chargebox.h"
#include "chgbox_wireless.h"

#if (TCFG_CHARGE_BOX_ENABLE)

#define LOG_TAG_CONST       APP_CHGBOX
#define LOG_TAG             "[CHG_WL]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"

extern void delay_2ms(int cnt);
extern void wireless_port_set_wakeup_enable(u8 enable);

///////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////���߳�/////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
#if(defined TCFG_WIRELESS_ENABLE) && (TCFG_WIRELESS_ENABLE)

_wireless_hdl wl_hdl;
#define __this (&wl_hdl)

static u16 wl_send_buff[10] __attribute__((aligned(4))); //10 * =20byte
static int wl_100ms_timer, wl_ad_timer, wl_ad_timeout;
static u32 g_wireless_voltage;
static u8  wl_send_over_msg, wl_init_ok;

//���Ƴ��ڵ�ѹ��Χ
#define VOLTAGE_MIN     6100
#define VOLTAGE_MAX     6200

//ȡ�м� (AD_OK_COUNTS-AD_CUT_COUNTS*2)�����ֵ��ȥ��ͷ����β����
#define AD_OK_COUNTS    5
#define AD_CUT_COUNTS   1

//���߳����߼��
#define WL_ONLINE_VOLT     3500  ///��ѹ����3.5����Ϊ���г����������
#define WL_ONLINE_TIMES    3
#define WL_OFFLINE_TIMES   5

static u16  power_table[AD_OK_COUNTS];
static u16  power_table_tmp[AD_OK_COUNTS];

//���߳��ͨ�ŵ�ʱ���ܽ���͹���
static volatile u8 is_wl_comm_active = 0;
static u8 wl_comm_idle_query(void)
{
    return (!is_wl_comm_active);
}
REGISTER_LP_TARGET(wl_comm_lp_target) = {
    .name = "chgbox_wl_comm",
    .is_idle = wl_comm_idle_query,
};

/*------------------------------------------------------------------------------------*/
/**@brief    timer2�жϻص�
   @param    ��
   @return   ��
   @note     ע����õ����д��������ram��
*/
/*------------------------------------------------------------------------------------*/
SEC(.chargebox_code)
___interrupt
static void wl_timer2_isr(void)
{
    JL_TIMER2->CON |= BIT(14);
    wireless_250us_run();
}

static const u32 timer_div[] = {
    /*0000*/    1,
    /*0001*/    4,
    /*0010*/    16,
    /*0011*/    64,
    /*0100*/    2,
    /*0101*/    8,
    /*0110*/    32,
    /*0111*/    128,
    /*1000*/    256,
    /*1001*/    4 * 256,
    /*1010*/    16 * 256,
    /*1011*/    64 * 256,
    /*1100*/    2 * 256,
    /*1101*/    8 * 256,
    /*1110*/    32 * 256,
    /*1111*/    128 * 256,
};
#define MAX_TIME_CNT            0x7fff
#define MIN_TIME_CNT            0x100
#define WL_TIMER_UNIT_US        250  //��λus
/*------------------------------------------------------------------------------------*/
/**@brief    ����timer2ģ��
   @param    ��
   @return   ��
   @note
*/
/*------------------------------------------------------------------------------------*/
static void wl_timer2_open(void)
{
    u32 prd_cnt, timer_clk;
    u8 index;
#if (TCFG_CLOCK_SYS_SRC == SYS_CLOCK_INPUT_PLL_RCL)
    timer_clk = clk_get("lsb");
#else
    timer_clk = clk_get("timer");
#endif
    JL_TIMER2->CON = BIT(14);//��pending
    for (index = 0; index < (sizeof(timer_div) / sizeof(timer_div[0])); index++) {
        prd_cnt = WL_TIMER_UNIT_US * (timer_clk / 1000000) / timer_div[index];
        if (prd_cnt > MIN_TIME_CNT && prd_cnt < MAX_TIME_CNT) {
            break;
        }
    }
    JL_TIMER2->CNT = 0;
    JL_TIMER2->PRD = prd_cnt;
    request_irq(IRQ_TIME2_IDX, 3, wl_timer2_isr, 0);
#if (TCFG_CLOCK_SYS_SRC == SYS_CLOCK_INPUT_PLL_RCL)
    JL_TIMER2->CON = (index << 4) | BIT(0); //lsb clk
#else
    JL_TIMER2->CON = (index << 4) | BIT(0) | BIT(3); //osc clk
#endif
    is_wl_comm_active = 1;
}

/*------------------------------------------------------------------------------------*/
/**@brief    �ر�timer2ģ��
   @param    ��
   @return   ��
   @note
*/
/*------------------------------------------------------------------------------------*/
static void wl_timer2_close(void)
{
    JL_TIMER2->CON = BIT(14);
    is_wl_comm_active = 0;
}

///dcdc en IO��ʼ��
static void dcdc_io_init(void)
{
#if TCFG_DCDC_CTRL_EN
    gpio_set_die(TCFG_DCDC_EN_IO, 0);
    gpio_set_pull_down(TCFG_DCDC_EN_IO, 0);
    gpio_set_pull_up(TCFG_DCDC_EN_IO, 0);
    gpio_direction_output(TCFG_DCDC_EN_IO, 0);
#endif
}

/*------------------------------------------------------------------------------------*/
/**@brief    DCDCʹ��
   @param    ��
   @return   ��
   @note     ʹ�ܺ�����������
*/
/*------------------------------------------------------------------------------------*/
static void dcdc_set_en(u8 en)
{
#if TCFG_DCDC_CTRL_EN
    log_info("dcdc en: %d\n", en);
    if (en) {
        gpio_direction_output(TCFG_DCDC_EN_IO, 1);
    } else {
        gpio_direction_output(TCFG_DCDC_EN_IO, 0);
    }
#endif
}

/*------------------------------------------------------------------------------------*/
/**@brief    ���߳�ͨ�ſڳ�ʼ��
   @param    ��
   @return   ��
*/
/*------------------------------------------------------------------------------------*/
static void wpc_io_init(void)
{
    gpio_set_die(TCFG_WPC_COMM_IO, 1);
    gpio_set_pull_down(TCFG_WPC_COMM_IO, 0);
    gpio_set_pull_up(TCFG_WPC_COMM_IO, 0);
    gpio_direction_output(TCFG_WPC_COMM_IO, 0);
}

/*------------------------------------------------------------------------------------*/
/**@brief    ���߳�AD���������IO��ʼ��
   @param    ��
   @return   ��
   @note
*/
/*------------------------------------------------------------------------------------*/
static void wl_ad_det_init(u8 die_en)
{
    gpio_set_die(TCFG_WL_AD_DET_IO, die_en);
    gpio_set_direction(TCFG_WL_AD_DET_IO, 1);
    gpio_set_pull_down(TCFG_WL_AD_DET_IO, 0);
    gpio_set_pull_up(TCFG_WL_AD_DET_IO, 0);
    if (!die_en) {
        gpio_set_pull_down(TCFG_WL_AD_DET_IO, 1);
        adc_add_sample_ch(TCFG_WL_AD_DET_CH);
        wireless_port_set_wakeup_enable(0);
    } else {
        wireless_port_set_wakeup_enable(1);
    }
}

SEC(.chargebox_code)
static u32 wl_read_ad_io_status(void)
{
    u32 v = 0;
    u32 mask = TCFG_WL_AD_DET_IO % IO_GROUP_NUM;
#if (TCFG_WL_AD_DET_IO <= IO_PORTA_15)
    v = !!(JL_PORTA->IN & BIT(mask));
#elif(TCFG_WL_AD_DET_IO <= IO_PORTB_15)
    v = !!(JL_PORTB->IN & BIT(mask));
#elif(TCFG_WL_AD_DET_IO <= IO_PORTC_15)
    v = !!(JL_PORTC->IN & BIT(mask));
#elif(TCFG_WL_AD_DET_IO <= IO_PORTD_7)
    v = !!(JL_PORTD->IN & BIT(mask));
#endif
    return v;
}

/*------------------------------------------------------------------------------------*/
/**@brief    ��ȡ���߳������ѹ
   @param    ��
   @return   ��
   @note
*/
/*------------------------------------------------------------------------------------*/
static u16 get_wireless_voltage(void)
{
    u16 volt;
    //ע���ѹ,���������ⲿ·��أ� Ĭ�Ͽ������ķ�һ
    volt = adc_get_voltage(TCFG_WL_AD_DET_CH) * 4;
    return volt;
}

/*------------------------------------------------------------------------------------*/
/**@brief    ���߳�100ms����
   @param    ��
   @return   ��
   @note
*/
/*------------------------------------------------------------------------------------*/
static void wireless_100ms_run_app(void *priv)
{
    if (sys_info.temperature_limit) {
        if (sys_info.status[WIRELESS_DET] == STATUS_ONLINE) {
            sys_info.status[WIRELESS_DET] = STATUS_OFFLINE;
            app_chargebox_event_to_user(CHGBOX_EVENT_WIRELESS_OFFLINE);
        }
        return;
    }
    //�鿴�Ƿ���������Ҫ����
    wireless_100ms_run();
    if (__this->info.busy) {
        wl_timer2_open();
    }
}
/*------------------------------------------------------------------------------------*/
/**@brief    ͨ����ѹ��������ֵ
   @param    ��
   @return   ��
   @note
*/
/*------------------------------------------------------------------------------------*/
static u16 wireless_calc_power(void)
{
    u8 i, j, k;
    u16 ad_sum, ad_min;
    memcpy(power_table_tmp, power_table, sizeof(power_table));
    for (i = 1; i < AD_OK_COUNTS; i++) {
        for (j = i; j > 0; j--) {
            if (power_table_tmp[j] < power_table_tmp[j - 1]) {
                ad_min = power_table_tmp[j];
                power_table_tmp[j] = power_table_tmp[j - 1];
                power_table_tmp[j - 1] = ad_min;
            }
        }
    }
    ad_sum = 0;
    for (k = AD_CUT_COUNTS; k < (AD_OK_COUNTS - AD_CUT_COUNTS); k++) {
        ad_sum = power_table_tmp[k] + ad_sum;
    }
    ad_sum = (ad_sum / (AD_OK_COUNTS - (AD_CUT_COUNTS * 2)));
    return ad_sum;
}

static void wireless_ad_detect_run(void *priv)
{
    static u8 power_cnt = 0;
    u16 power_tmp;
    power_tmp = get_wireless_voltage();
    power_table[power_cnt++] = power_tmp;
    if (power_cnt >= AD_OK_COUNTS) {
        usr_timer_del(wl_ad_timer);
        wl_ad_timer = 0;
        wl_ad_timeout = 0;
        power_cnt = 0;
        g_wireless_voltage = wireless_calc_power();
        if (g_wireless_voltage < WL_ONLINE_VOLT) {
            sys_info.status[WIRELESS_DET] = STATUS_OFFLINE;
            app_chargebox_event_to_user(CHGBOX_EVENT_WIRELESS_OFFLINE);
        }
        log_info("wireless_power: %d mV\n", g_wireless_voltage);
    }
}

static void wireless_ad_detect_start(void *priv)
{
    if (wl_ad_timer == 0) {
        wl_ad_timer = usr_timer_add(NULL, wireless_ad_detect_run, 10, 1);//10ms
    }
}

static void wireless_ad_detect_init(void)
{
    if (wl_ad_timeout == 0) {
        wl_ad_timeout = sys_timeout_add(NULL, wireless_ad_detect_start, 25);
    }
}

/*------------------------------------------------------------------------------------*/
/**@brief    ��ȡ���߳䵱ǰ�ṩ�ĵ�ѹ
   @param    ��
   @return   ��
   @note
*/
/*------------------------------------------------------------------------------------*/
u16 get_wireless_power(void)
{
    return g_wireless_voltage;
}

/*------------------------------------------------------------------------------------*/
/**@brief    ���߳�ͨ��io����
   @param    ��
   @return   ��
   @note     ������ã�ע�����Ҫ��ram
*/
/*------------------------------------------------------------------------------------*/
SEC(.chargebox_code)
void wpc_io_set(u8 mode)
{
    u8 io_num;
    switch (mode) {
    case IO_HIGH:
        gpio_direction_output(TCFG_WPC_COMM_IO, 1);
        break;
    case IO_LOW:
        gpio_direction_output(TCFG_WPC_COMM_IO, 0);
        wl_timer2_close(); //���һ��������ر�
        if (wl_send_over_msg) {
            app_chargebox_event_to_user(CHGBOX_EVENT_WL_DATA_OVER);
        }
        break;
    case IO_INIT:
        gpio_set_die(TCFG_WPC_COMM_IO, 1);
        gpio_set_pull_down(TCFG_WPC_COMM_IO, 0);
        gpio_set_pull_up(TCFG_WPC_COMM_IO, 0);
        gpio_direction_output(TCFG_WPC_COMM_IO, 0);
        break;
    case IO_OVERTURN://��ת
        io_num = TCFG_WPC_COMM_IO % IO_GROUP_NUM;
#if(TCFG_WPC_COMM_IO <= IO_PORTA_15)
        JL_PORTA->OUT ^= BIT(io_num);
#elif(TCFG_WPC_COMM_IO <= IO_PORTB_15)
        JL_PORTB->OUT ^= BIT(io_num);
#elif(TCFG_WPC_COMM_IO <= IO_PORTC_15)
        JL_PORTC->OUT ^= BIT(io_num);
#elif(TCFG_WPC_COMM_IO <= IO_PORTD_7)
        JL_PORTD->OUT ^= BIT(io_num);
#endif
        break;
    case IO_DIR_IN: //����
        gpio_direction_input(TCFG_WPC_COMM_IO);
        break;
    }
}

/*------------------------------------------------------------------------------------*/
/**@brief    ���߳䷢��һ�����ݰ���װ
   @param    ��
   @return   ��
   @note     �������ֽ׶ε���!!!
*/
/*------------------------------------------------------------------------------------*/
static bool wireless_send_pack_deal(void)
{
    bool ret = true;
    local_irq_disable();
    wl_timer2_open();
    __this->info.busy = 1;
    while (__this->info.busy) {
        asm("nop");
        if (JL_TIMER2->CON & BIT(15)) {
            JL_TIMER2->CON |= BIT(14);
            wireless_250us_run();
        }
        if (wl_read_ad_io_status() == 0) {
            ret = false;
            break;
        }
    }
    wl_timer2_close();
    local_irq_enable();
    if (ret == false) {
        sys_info.status[WIRELESS_DET] = STATUS_OFFLINE;
        app_chargebox_event_to_user(CHGBOX_EVENT_WIRELESS_OFFLINE);
    }
    return ret;
}

/*------------------------------------------------------------------------------------*/
/**@brief    �����߳�ģ��
   @param    ��
   @return   ��
   @note     ��⵽���߳�����ʱ���ã�������صĳ�ʼ��ָ��
*/
/*------------------------------------------------------------------------------------*/
void wireless_api_open(void)
{
    if (sys_info.temperature_limit) {
        sys_info.status[WIRELESS_DET] = STATUS_OFFLINE;
        app_chargebox_event_to_user(CHGBOX_EVENT_WIRELESS_OFFLINE);
        return;
    }

    wireless_open(VOLTAGE_MIN, VOLTAGE_MAX);

    //�����ź�ǿ�Ȱ�
    get_signal_value();
    if (wireless_send_pack_deal() == false) {
        goto __err_exit;
    }

    //������ݰ�
    delay_2ms(3);
    get_identification();
    if (wireless_send_pack_deal() == false) {
        goto __err_exit;
    }

    //�������ð�
    delay_2ms(3);
    get_configuration();
    if (wireless_send_pack_deal() == false) {
        goto __err_exit;
    }

    wl_ad_det_init(0);//��ʼ����ģ���
    app_chargebox_event_to_user(CHGBOX_EVENT_WL_DATA_OVER);
    wl_100ms_timer = sys_timer_add(NULL, wireless_100ms_run_app, 100);
    wl_send_over_msg = 1;
    return;

__err_exit:
    wireless_close();
    return;
}

/*------------------------------------------------------------------------------------*/
/**@brief    �ر����߳�ģ��
   @param    ��
   @return   ��
   @note     ���߳�����ʱ����
*/
/*------------------------------------------------------------------------------------*/
void wireless_api_close(void)
{
    wireless_close();
    dcdc_set_en(0);
    wl_ad_det_init(1);
    wl_send_over_msg = 0;
    if (wl_100ms_timer) {
        sys_timer_del(wl_100ms_timer);
        wl_100ms_timer = 0;
    }
}

/*------------------------------------------------------------------------------------*/
/**@brief    ���߳����߼��
   @param    ��
   @return   ��
   @note     ����ADֵ������߳������
*/
/*------------------------------------------------------------------------------------*/
static int wireless_on_det_timer;
static void wireless_online_det(void *priv)
{
    u16 power_voltage;
    static u16 wl_on_detect_cnt = 0;
    static u16 wl_off_detect_cnt = 0;
    sys_info.life_cnt = 0;
    power_voltage = get_wireless_voltage();
    if (power_voltage >= WL_ONLINE_VOLT) {
        wl_on_detect_cnt++;
        wl_off_detect_cnt = 0;
        g_wireless_voltage += power_voltage;
        if (wl_on_detect_cnt >= WL_ONLINE_TIMES) {
            wl_on_detect_cnt = 0;
            g_wireless_voltage = g_wireless_voltage / WL_ONLINE_TIMES;
            sys_info.status[WIRELESS_DET] = STATUS_ONLINE;
            app_chargebox_event_to_user(CHGBOX_EVENT_WIRELESS_ONLINE);
            goto __online_det_exit;
        }
    } else {
        wl_off_detect_cnt++;
        wl_on_detect_cnt = 0;
        g_wireless_voltage = 0;
        if (wl_off_detect_cnt >= WL_OFFLINE_TIMES) {
            wl_off_detect_cnt = 0;
            goto __online_det_exit;
        }
    }
    return;
__online_det_exit:
    wl_ad_det_init(1);//��ʼ�������ֿ�
    usr_timer_del(wireless_on_det_timer);
    wireless_on_det_timer = 0;
}

void wireless_io_wakeup_deal(void)
{
    if (!wl_init_ok) {
        return;
    }
    if ((sys_info.status[WIRELESS_DET] == STATUS_OFFLINE) && (wireless_on_det_timer == 0)) {
        wl_ad_det_init(0);//��ʼ����AD��
        wireless_on_det_timer = usr_timer_add(NULL, wireless_online_det, 5, 1);//6ms
    }
}

void wireless_data_over_run(void)
{
    if (sys_info.status[WIRELESS_DET] == STATUS_OFFLINE) {
        return;
    }
    wireless_ad_detect_init();
}

/*------------------------------------------------------------------------------------*/
/**@brief    ���߳�ģ���ʼ��
   @param    ��
   @return   ��
   @note     ��ʼ�����ƽṹ�塢ad����Ӧ��io��ע����ؽӿ�
*/
/*------------------------------------------------------------------------------------*/
void wireless_init_api(void)
{
    ///ע��˳���ܸı�
    memset(__this, 0x0, sizeof(_wireless_hdl));
    //AD�ɼ���ʼ��
    wl_ad_det_init(1);
    dcdc_io_init();
    wpc_io_init();
    wl_send_over_msg = 0;
    __this->get_wl_power = get_wireless_power;
    __this->dcdc_en_set = dcdc_set_en;
    __this->wpc_set = wpc_io_set;
    __this->send_buff = wl_send_buff;
    wireless_lib_init(__this, VOLTAGE_MIN, VOLTAGE_MAX);
    wl_init_ok = 1;
}
#endif//end of WIRELESS_ENABLE

#endif
