#include "typedef.h"
#include "irq.h"
#include "asm/gpio.h"

/**
 * ע�⣺JL_WAKEUP ������PMU����Ļ��ѡ������Ϊһ��������ģ��ʹ�á����ڵ͹��ĵ�����£��ж���Ч��
 */

static void (*port_wkup_irq_cbfun)(void) = NULL;
static u32 user_port = -1;;
/**
 * @brief �����жϺ���
 */
___interrupt
void port_wkup_irq_fun(void)
{
    if (JL_WAKEUP->CON3 & BIT(0)) {
        JL_WAKEUP->CON2 |= BIT(0);
        if (port_wkup_irq_cbfun) {
            port_wkup_irq_cbfun();
        }
    }
}
/**
 * @brief �����жϳ�ʼ��
 * @param port �������ŵ����źţ�IO_PORTA_00......
 * @param trigger_edge �������ء� 0�������ش����� 1���½��ش���
 * @param cbfun ��Ӧ���жϻص�����
 */
void port_wkup_interrupt_init(u32 port, u8 trigger_edge, void (*cbfun)(void))
{
    JL_WAKEUP->CON0 &= ~BIT(0);
    gpio_set_die(port, 1);
    gpio_set_direction(port, 1);
    if (trigger_edge == 0) {
        JL_WAKEUP->CON1 &= ~BIT(0);
        gpio_set_pull_up(port, 0);
        gpio_set_pull_down(port, 1);
    } else {
        JL_WAKEUP->CON1 |= BIT(0);
        gpio_set_pull_up(port, 1);
        gpio_set_pull_down(port, 0);
    }
    user_port = port;
    if (cbfun) {
        port_wkup_irq_cbfun = cbfun;
    }
    request_irq(IRQ_PORT_IDX, 3, port_wkup_irq_fun, 0); //ע���жϺ���
    JL_IOMAP->CON2 &= ~(0b111111 << 0);                 //ʹ��inputchannel 0
    JL_IOMAP->CON2 |= (port << 0);
    JL_WAKEUP->CON2 |= BIT(0);                          //��һ��pnd
    JL_WAKEUP->CON0 |= BIT(0);                          //�����ж�ʹ��
}

/**
 * @brief �ص������ŵ��жϹ���
 * @param port ���źţ�IO_PORTA_00......
 */
void port_wkup_interrupt_close(u32 port)
{
    JL_WAKEUP->CON0 &= ~BIT(0);
    if (port == user_port) {
        gpio_set_die(port, 0);
        gpio_set_direction(port, 1);
        gpio_set_pull_up(port, 0);
        gpio_set_pull_down(port, 0);
    }
}

/*********************************************************************************************************
 * ******************************           ʹ�þ�������           ***************************************
 * ******************************************************************************************************/
void port_irq_cbfun(void)
{
    printf("Hello world !\n");
}
void my_port_wkup_test()
{
    port_wkup_interrupt_init(IO_PORTA_03, 0, port_irq_cbfun);//�����ش���
    /* port_wkup_interrupt_init(IO_PORT_DP, 1, port_irq_cbfun);//�½��ش��� */
    while (1);
}

