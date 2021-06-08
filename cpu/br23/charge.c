#include "asm/clock.h"
#include "timer.h"
#include "asm/power/p33.h"
#include "asm/charge.h"
#include "uart.h"
#include "device/device.h"
#include "asm/power_interface.h"
/*#include "power/power_hw.h"*/
#include "system/event.h"
#include "asm/efuse.h"
#include "gpio.h"
#include "app_config.h"

#define LOG_TAG_CONST   CHARGE
#define LOG_TAG         "[CHARGE]"
#define LOG_INFO_ENABLE
#define LOG_DUMP_ENABLE
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#include "debug.h"

#define IO_PORT_LDOIN   58//IO���

#define BIT_LDO5V_IN		BIT(0)
#define BIT_LDO5V_OFF		BIT(1)
#define BIT_LDO5V_ERR		BIT(2)

enum {
    CHARGE_TIMER_10_MS = 0,
    CHARGE_TIMER_10_S,
} CHARGE_CHECK_TIME;

typedef struct _CHARGE_VAR {
    struct charge_platform_data *data;
    volatile u8 charge_online_flag;
    volatile u8 init_ok;
    volatile u8 timer_period;
    volatile int ldo5v_timer;
    volatile int charge_timer;
    volatile int charge_timer_pre;
} CHARGE_VAR;

#define __this 	(&charge_var)
static CHARGE_VAR charge_var;
static u8 charge_flag;
extern void charge_set_callback(void (*wakup_callback)(void), void (*sub_callback)(void));

//оƬ�Ĵ���Ĭ��ֵ,�⼸���Ĵ���ֻ��д,���ܶ�
volatile u8 chg_con0 = 0x03;
volatile u8 chg_con1 = 0x00;
volatile u8 chg_con2 = 0x00;
volatile u8 chg_wkup = 0x00;

void chg_reg_set(u8 addr, u8 start, u8 len, u8 data)
{
    switch (addr) {
    case P3_CHG_CON0:
        SFR(chg_con0, start, len, data);
        p33_tx_1byte(P3_CHG_CON0, chg_con0);
        break;
    case P3_CHG_CON1:
        SFR(chg_con1, start, len, data);
        p33_tx_1byte(P3_CHG_CON1, chg_con1);
        break;
    case P3_CHG_CON2:
        SFR(chg_con2, start, len, data);
        p33_tx_1byte(P3_CHG_CON2, chg_con2);
        break;
    case P3_CHG_WKUP:
        SFR(chg_wkup, start, len, data);
        p33_tx_1byte(P3_CHG_WKUP, chg_wkup);
        break;
    }
}

u8 chg_reg_get(u8 addr)
{
    switch (addr) {
    case P3_CHG_CON0:
        return chg_con0;
    case P3_CHG_CON1:
        return chg_con1;
    case P3_CHG_CON2:
        return chg_con2;
    case P3_CHG_WKUP:
        return chg_wkup;
    }
    return 0;
}

u8 get_charge_poweron_en(void)
{
    return __this->data->charge_poweron_en;
}

void charge_check_and_set_pinr(u8 level)
{
    u8 pinr_io;
    if (P33_CON_GET(P3_PINR_CON) & BIT(0)) {
        pinr_io = P33_CON_GET(P3_PORT_SEL10);
        if (pinr_io == IO_PORT_LDOIN) {
            if (level == 0) {
                P33_CON_SET(P3_PINR_CON, 2, 1, 0);
            } else {
                P33_CON_SET(P3_PINR_CON, 2, 1, 1);
            }
        }
    }
}

static void udelay(u32 usec)
{
    JL_TIMER0->CON = BIT(14);
    JL_TIMER0->CNT = 0;
    JL_TIMER0->PRD = clk_get("lsb") / 1000000L  * usec; //1us
    JL_TIMER0->CON = BIT(0); //sys clk
    while ((JL_TIMER0->CON & BIT(15)) == 0);
    JL_TIMER0->CON = BIT(14);
}

static u8 check_charge_state(void)
{
    u8 online_cnt = 0;
    u8 i = 0;

    //only for br23 fix charge load too small
    if (__this->data->ldo5v_pulldown_en) {
        gpio_set_pull_down(IO_PORTB_05, 1);
    }

    __this->charge_online_flag = 0;

    for (i = 0; i < 20; i++) {
        if (LVCMP_DET_GET() || LDO5V_DET_GET()) {
            online_cnt++;
        }
        udelay(1000);
    }
    log_info("online_cnt = %d\n", online_cnt);
    if (online_cnt > 5) {
        __this->charge_online_flag = 1;
    }

    return __this->charge_online_flag;
}

//only for br23 fix charge load too small
void charge_reset_pb5_pd_status(void)
{
    if (__this->data == NULL) {
        return;
    }
    switch (charge_flag) {
    case 0:
    case BIT_LDO5V_OFF:
        if (__this->data->ldo5v_pulldown_en) {
            gpio_set_pull_down(IO_PORTB_05, 1);
        }
        break;

    case BIT_LDO5V_IN:
    case BIT_LDO5V_ERR:
        if (__this->data->ldo5v_pulldown_en) {
            gpio_set_pull_down(IO_PORTB_05, 0);
        }
        break;
    }
}

//only for br23,���Ǹ���������
bool charge_check_wakeup_is_set_ok(void)
{
#if TCFG_CHARGE_ENABLE
    //�жϲ�μ�������Ƿ���ȷ
    if (LDO5V_IS_EN() && LDO5V_EDGE_WKUP_IS_EN()) {
        //����Ϊ�½���ʱ(bit1 = 1),��ǰ״̬Ӧ��Ϊ����(bit5 = 1)
        //����Ϊ������ʱ(bit1 = 0),��ǰ״̬Ӧ��Ϊ�γ�(bit5 = 0)
        if (LDO5V_EDGE_GET() ^ LDO5V_DET_GET()) {
            return FALSE;
        }
    }
#endif
    return TRUE;
}

void set_charge_online_flag(u8 flag)
{
    __this->charge_online_flag = flag;
}

u8 get_charge_online_flag(void)
{
    return __this->charge_online_flag;
}


u8 get_ldo5v_online_hw(void)
{
    return LDO5V_DET_GET();
}

u8 get_lvcmp_det(void)
{
    return LVCMP_DET_GET();
}

u8 get_ldo5v_pulldown_en(void)
{
    return __this->data->ldo5v_pulldown_en;
}

void charge_event_to_user(u8 event)
{
    struct sys_event e;
    e.type = SYS_DEVICE_EVENT;
    e.arg  = (void *)DEVICE_EVENT_FROM_CHARGE;
    e.u.dev.event = event;
    e.u.dev.value = 0;
    sys_event_notify(&e);
}

static void set_charge_wkup_source(u8 source)
{
    CHARGE_WKUP_EDGE_SEL(0);	//0:������,�ߵ�ƽ 1:�½��أ��͵�ƽ
    CHARGE_WKUP_SOURCE_SEL(source);
    CHARGE_EDGE_DETECT_EN(1);
    CHARGE_LEVEL_DETECT_EN(1);
    CHARGE_WKUP_EN(1);
    CHARGE_WKUP_PND_CLR();
}

void charge_start(void)
{
    log_info("%s\n", __func__);

    if (__this->charge_timer_pre) {
        sys_hi_timer_del(__this->charge_timer_pre);
        __this->charge_timer_pre = 0;
    }

    if (__this->charge_timer) {
        sys_timer_del(__this->charge_timer);
        __this->charge_timer = 0;
    }

    set_charge_wkup_source(CHARGE_FULL_33V);

    CHGBG_EN(1);
    CHARGE_EN(1);
    /* PDIO_KEEP(1);//��ֹ����pdown����ͨ·���ж� */

    charge_event_to_user(CHARGE_EVENT_CHARGE_START);
}

void charge_close(void)
{
    log_info("%s\n", __func__);

    CHARGE_WKUP_EN(0);
    CHARGE_EDGE_DETECT_EN(0);
    CHARGE_LEVEL_DETECT_EN(0);
    CHGBG_EN(0);
    CHARGE_EN(0);
    CHARGE_WKUP_PND_CLR();
    /* PDIO_KEEP(0); */

    charge_event_to_user(CHARGE_EVENT_CHARGE_CLOSE);

    if (__this->charge_timer_pre) {
        sys_hi_timer_del(__this->charge_timer_pre);
        __this->charge_timer_pre = 0;
    }

    if (__this->charge_timer) {
        sys_timer_del(__this->charge_timer);
        __this->charge_timer = 0;
    }
}

static u8 CHARGE_FULL_FLAG_GET(void)
{
    u8 sfr;
    //����֤timer�ж����ȼ���RTC�ж����ȼ�һ�����ÿ������ж�
    local_irq_disable();
    CHARGE_LEVEL_DETECT_EN(1);
    sfr = p33_rx_1byte(P3_WKUP_SRC);
    CHARGE_LEVEL_DETECT_EN(0);
    if (sfr & BIT(6)) {
        CHARGE_WKUP_PND_CLR();
    }
    local_irq_enable();
    return (sfr & BIT(6));
}

static void charge_full_detect(void *priv)
{
    static u16 charge_full_cnt = 0;

    if (__this->timer_period == CHARGE_TIMER_10_S) {
        sys_timer_modify(__this->charge_timer, 10);
        __this->timer_period = CHARGE_TIMER_10_MS;
    }

    if (CHARGE_FULL_FLAG_GET() && LVCMP_DET_GET()) {
        /* putchar('F'); */
        if (charge_full_cnt < 5) {
            charge_full_cnt++;
        } else {
            charge_full_cnt = 0;
            sys_timer_del(__this->charge_timer);
            __this->charge_timer = 0;
            charge_event_to_user(CHARGE_EVENT_CHARGE_FULL);
        }
    } else {
        /* putchar('K'); */
        charge_full_cnt = 0;
        sys_timer_modify(__this->charge_timer, 10000);
        __this->timer_period = CHARGE_TIMER_10_S;
    }
}

static void charge_full_detect_pre(void *priv)
{
    u8 full_flag, lvcmp_lvl;
    static u16 charge_no_full_cnt = 0;
    sys_hi_timer_del(__this->charge_timer_pre);
    __this->charge_timer_pre = 0;
    full_flag = CHARGE_FULL_FLAG_GET();
    lvcmp_lvl = LVCMP_DET_GET();
    if (full_flag && lvcmp_lvl) {
        goto __exit_det_pre;
    } else {
        if ((!full_flag) && lvcmp_lvl) {
            //��ѹ��������������n�������жϺ�,�����ѯ��ʽ
            charge_no_full_cnt++;
            if (charge_no_full_cnt > 50) {
                goto __exit_det_pre;
            }
        } else {
            charge_no_full_cnt = 0;
        }
        CHARGE_EDGE_DETECT_EN(1);
        CHARGE_LEVEL_DETECT_EN(1);
    }
    return;
__exit_det_pre:
    //�����źż�⵽֮��,�л���ⷽʽ,���ò�ѯ����(Ϊ����߳��Ч��)
    //���������ѯ��ʽ,��ʱ�����жϻ�Ƶ������ϵͳ,ϵͳ���Ļ�����,��ɳ��Ч�ʵ�
    charge_no_full_cnt = 0;
    __this->charge_timer = sys_timer_add(NULL, charge_full_detect, 10);
    __this->timer_period = CHARGE_TIMER_10_MS;
    return;
}

static void ldo5v_detect(void *priv)
{
    /* log_info("%s\n",__func__); */

    static u16 ldo5v_in_normal_cnt = 0;
    static u16 ldo5v_in_err_cnt = 0;
    static u16 ldo5v_off_cnt = 0;

    if (LVCMP_DET_GET()) {	//ldoin > vbat
        /* putchar('X'); */
        ldo5v_off_cnt = 0;
        ldo5v_in_err_cnt = 0;
        if (ldo5v_in_normal_cnt < 50) {
            ldo5v_in_normal_cnt++;
        } else {
            /* printf("ldo5V_IN\n"); */
            set_charge_online_flag(1);
            ldo5v_in_normal_cnt = 0;
            sys_hi_timer_del(__this->ldo5v_timer);
            __this->ldo5v_timer = 0;
            if ((charge_flag & BIT_LDO5V_IN) == 0) {
                charge_flag = BIT_LDO5V_IN;

                //only for br23 fix charge load too small
                if (__this->data->ldo5v_pulldown_en) {
                    gpio_set_pull_down(IO_PORTB_05, 0);
                }

                charge_event_to_user(CHARGE_EVENT_LDO5V_IN);
                LVCMP_EDGE_SEL(1);	//���ldoin��vbat��ѹ�͵����(���ָ���س������ضϣ���ʱ��ѹ�������)
                LDO5V_EDGE_SEL(1);
                //only for br23 fix charge wkup bug (ldoin 3.3v to 5v)
                power_wakeup_index_disable(7);
            }
        }
    } else if (LDO5V_DET_GET() == 0) {	//ldoin<�γ���ѹ��0.6��
        /* putchar('Q'); */
        ldo5v_in_normal_cnt = 0;
        ldo5v_in_err_cnt = 0;
        if (ldo5v_off_cnt < (__this->data->ldo5v_off_filter + 10)) {
            ldo5v_off_cnt++;
        } else {
            /* printf("ldo5V_OFF\n"); */
            set_charge_online_flag(0);
            ldo5v_off_cnt = 0;
            sys_hi_timer_del(__this->ldo5v_timer);
            __this->ldo5v_timer = 0;
            if ((charge_flag & BIT_LDO5V_OFF) == 0) {
                charge_flag = BIT_LDO5V_OFF;
                LVCMP_EDGE_SEL(0);//�γ������¼�����
                LDO5V_EDGE_SEL(0);

                //only for br23 fix charge load too small
                if (__this->data->ldo5v_pulldown_en) {
                    gpio_set_pull_down(IO_PORTB_05, 1);
                }

                charge_event_to_user(CHARGE_EVENT_LDO5V_OFF);
                //only for br23 fix charge wkup bug (ldoin 3.3v to 5v)
                power_wakeup_index_disable(7);
            }
        }
    } else {	//�γ���ѹ��0.6���ң�< ldoin < vbat
        ldo5v_in_normal_cnt = 0;
        /* putchar('E'); */
        ldo5v_off_cnt = 0;
        if (ldo5v_in_err_cnt < 50) {
            ldo5v_in_err_cnt++;
        } else {
            /* printf("ldo5V_ERR\n"); */
            set_charge_online_flag(1);
            ldo5v_in_err_cnt = 0;
            sys_hi_timer_del(__this->ldo5v_timer);
            __this->ldo5v_timer = 0;
            if ((charge_flag & BIT_LDO5V_ERR) == 0) {
                charge_flag = BIT_LDO5V_ERR;
                LVCMP_EDGE_SEL(0);
                LDO5V_EDGE_SEL(1);

                //only for br23 fix charge load too small
                if (__this->data->ldo5v_pulldown_en) {
                    gpio_set_pull_down(IO_PORTB_05, 0);
                }

                //only for br23 fix charge wkup bug (ldoin 3.3v to 5v)
                power_wakeup_index_enable(7);
                if (__this->data->ldo5v_off_filter) {
                    charge_event_to_user(CHARGE_EVENT_LDO5V_KEEP);
                }
            }
        }
    }
}


void sub_wakeup_isr(void)
{
    if (__this->ldo5v_timer == 0) {
        __this->ldo5v_timer = sys_hi_timer_add(0, ldo5v_detect, 2);
    }
}

void charge_wakeup_isr(void)
{
    /* printf("\ncharge_wkup_deal\n"); */
    if (__this->charge_timer_pre == 0) {
        __this->charge_timer_pre = sys_hi_timer_add(0, charge_full_detect_pre, 10);
    }
}

#if 0
static void test_func(void *priv)
{
    if (P33_CON_GET(P3_CHG_READ) & BIT(0)) {
        JL_PORTA->DIR &= ~BIT(3);
        JL_PORTA->OUT |= BIT(3);
    } else {
        JL_PORTA->OUT &= ~BIT(3);
    }
}
#endif

u8 get_charge_mA_config(void)
{
    return __this->data->charge_mA;
}

void set_charge_mA(u8 charge_mA)
{
    static u8 charge_mA_old = 0xff;
    if (charge_mA_old != charge_mA) {
        charge_mA_old = charge_mA;
        CHARGE_mA_SEL(charge_mA);
    }
}

const u16 full_table[CHARGE_FULL_V_MAX] = {
    3869, 3907, 3946, 3985, 4026, 4068, 4122, 4157,
    4202, 4249, 4295, 4350, 4398, 4452, 4509, 4567,
};
u16 get_charge_full_value(void)
{
    ASSERT(__this->init_ok, "charge not init ok!\n");
    ASSERT(__this->data->charge_full_V < CHARGE_FULL_V_MAX);
    return full_table[__this->data->charge_full_V];
}

static void charge_config(void)
{
    u8 charge_4202_trim_val = CHARGE_FULL_V_4202;
    u8 offset = 0;
    u8 charge_full_v_val = 0;

    if (get_vbat_trim() == 0xf) {
        log_info("vbat not trim, use default config!!!!!!");
    } else {
        charge_4202_trim_val = get_vbat_trim();		//4.2V��Ӧ��trim������ʵ�ʵ�λ
    }

    log_info("charge_4202_trim_val = %d\n", charge_4202_trim_val);

    if (__this->data->charge_full_V >= CHARGE_FULL_V_4202) {
        offset = __this->data->charge_full_V - CHARGE_FULL_V_4202;
        charge_full_v_val = charge_4202_trim_val + offset;
        if (charge_full_v_val > 0xf) {
            charge_full_v_val = 0xf;
        }
    } else {
        offset = CHARGE_FULL_V_4202 - __this->data->charge_full_V;
        if (charge_4202_trim_val >= offset) {
            charge_full_v_val = charge_4202_trim_val - offset;
        } else {
            charge_full_v_val = 0;
        }
    }

    log_info("charge_full_v_val = %d\n", charge_full_v_val);

    CHARGE_FULL_V_SEL(charge_full_v_val);
    CHARGE_FULL_mA_SEL(__this->data->charge_full_mA);
    /* CHARGE_mA_SEL(__this->data->charge_mA); */
    CHARGE_mA_SEL(CHARGE_mA_20);
}

int charge_init(const struct dev_node *node, void *arg)
{
    log_info("%s\n", __func__);

    __this->data = (struct charge_platform_data *)arg;

    ASSERT(__this->data);

    //�����ؼĴ�������Ĭ��ֵ
    p33_tx_1byte(P3_CHG_CON0, chg_con0);
    p33_tx_1byte(P3_CHG_CON1, chg_con1);
    p33_tx_1byte(P3_CHG_CON2, chg_con2);
    p33_tx_1byte(P3_CHG_WKUP, chg_wkup);

    __this->init_ok = 0;
    __this->charge_online_flag = 0;

    /*�ȹرճ��ʹ�ܣ������⵽�������ٿ���*/
    /* CHGBG_EN(0); */
    /* CHARGE_EN(0); */

    /*LDO5V��100K��������ʹ��*/
    L5V_LOAD_EN(__this->data->ldo5v_pulldown_en);

    /*LDO5V,��������أ����ڼ��ldoin����*/
    LDO5V_EN(1);
    LDO5V_EDGE_SEL(0);
    LDO5V_PND_CLR();
    LDO5V_EDGE_WKUP_EN(1);

    /*LVCMP,��������أ����ڼ��ldoin����*/
    LVCMP_EN(1);
    LVCMP_EDGE_SEL(0);
    LVCMP_PND_CLR();
    LVCMP_EDGE_WKUP_EN(1);
    /*ʹ��COMPH,��LDOIN��VBAT�ȽϽӽ�ʱ����źŻᶶ��,�����г�ʹ��COMPL,ע��softoff��Ҫ�г�COMPH*/
    /*COMPH:LDO5V<->VBAT COMPL:LDO5V<->VDD50*/
    LVCMP_CMP_SEL(1);

    charge_config();

    //only for br23 fix charge wkup bug (ldoin 3.3v to 5v)
    //ԭ��:softoff��û��rcʱ�ӻ��ѵ��˲��߼������������������ź�
    struct port_wakeup port;
    port.pullup_down_enable = 0;
    port.edge = RISING_EDGE;
    port.attribute = 0;
    port.iomap = IO_LDOIN_DET;
    power_wakeup_set_wakeup_io(7, &port);
    power_wakeup_index_disable(7);

    if (check_charge_state()) {
        if (__this->ldo5v_timer == 0) {
            __this->ldo5v_timer = sys_hi_timer_add(0, ldo5v_detect, 2);
        }
    } else {
        charge_flag = BIT_LDO5V_OFF;
        CHARGE_WKUP_PND_CLR();
        CHGBG_EN(0);
        CHARGE_EN(0);
    }
    charge_set_callback(charge_wakeup_isr, sub_wakeup_isr);
    /* sys_hi_timer_add(0,test_func,1); */

    __this->init_ok = 1;

    return 0;
}

int charge_api_init(void *arg)
{
    return charge_init(NULL, arg);
}
/* const struct device_operations charge_dev_ops = { */
/*     .init  = charge_init, */
/* }; */
