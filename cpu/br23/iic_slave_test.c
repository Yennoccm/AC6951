#include "system/includes.h"
#include "media/includes.h"
//#include "device/iic.h"
#include "asm/iic_hw.h"
#include "asm/iic_soft.h"

#if 0
/*
    [[[ README ]]]
    1. iic�ӻ���Ҫ��ֵhw_iic_cfg.role = IIC_SLAVE����roleû�и�ֵ��ֵ��
    IIC_MASTER����Ĭ��������ģʽ��
    2. iic�ӻ���demoʹ���ж�-����Ľ�����ʽ������while(!iic_pnd)��������ʽ��ɵ�
    CPU�˷Ѽ���Ӧ����ʱ������IICû��DMA��ÿ����1 byte�ͻᴥ��1���жϣ�����IIC����
    ����ÿ�������1�ֽ�delayһ��ʱ�䣬�����ٵȴ�IIC�ӻ���Ӧ�жϡ�iic stop������
    end�жϱ����ڼ�������ֽ�������λ״̬������Ҫȥ��end�жϵ�ʹ�ܼ�����IIC����
    ���յ������end�жϣ�����end�жϵĴ��������start-end��ס��
    3. demo�Ľ���/����ʹ��double buffer�����⴦�����ݹ����б��´ν���/���͵�����
    ���ǡ�
    4. IIC�ӻ��յ�IIC_S_RADDR����Ҫ����׼����Ҫ���͵����ݣ���Ϊ������ܶ�ʱ���ڷ�
    ����һ��IIC�ֽ�ʱ�����������ݡ�
    5. IIC������TX��STOP���¸�RX��START��Ҫdelayһ��ʱ�䣬������ն���һ������ȷ
    ���ֽڡ�
    6. δ����������е�ע�͡�
*/

#define IIC_S_RADDR                             0x61
#define IIC_S_WADDR                             0x60

#define IIC_S_DEV                                 0
#define IIC_S_TXBUF_SIZE                        128
#define IIC_S_RXBUF_SIZE                        128

enum {
    IIC_S_MSG_TX,
    IIC_S_MSG_RX,
};

struct iic_s_tx_statemachine {
    u8 *buf[2];
    u32 b_size;
    bool toggle;
    u32 cur_cnt;
    u32 tx_cnt;
};

struct iic_s_rx_statemachine {
    u8 *buf[2];
    u32 b_size;
    bool toggle;
    u32 cur_cnt;
    u32 rx_cnt;
};

struct iic_slave {
    enum {IIC_S_TX, IIC_S_RX} dir;
    struct iic_s_tx_statemachine tx;
    struct iic_s_rx_statemachine rx;
    u8 bus_occupy;
};

static struct iic_slave iic_s;
static u8 iic_s_txbuf[2][IIC_S_TXBUF_SIZE];
static u8 iic_s_rxbuf[2][IIC_S_RXBUF_SIZE];

SET_INTERRUPT
static void iic_slave_isr()
{
    u8 is_addr = 0;
    u8 byte;

    if (hw_iic_get_pnd(IIC_S_DEV)) {
        hw_iic_clr_pnd(IIC_S_DEV);
        putchar('a');
        if (iic_s.dir == IIC_S_RX) {
            byte = hw_iic_slave_rx_byte(IIC_S_DEV, &is_addr);  //�ж��Ƿ�Ϊ��ַ
            if (is_addr) {
                iic_s.rx.cur_cnt = 0;
                iic_s.rx.rx_cnt = 0;
                iic_s.tx.cur_cnt = 0;
                iic_s.tx.tx_cnt = 0;
                if (byte == IIC_S_WADDR) {
                    putchar('b');
                    hw_iic_slave_rx_prepare(IIC_S_DEV, 1);  //������1 byte���գ���ʹ��recv ack
                    iic_s.bus_occupy = 1;
                } else if (byte == IIC_S_RADDR) {
                    putchar('c');
                    iic_s.dir = IIC_S_TX;
                    os_taskq_post_msg("iic_slave", 1, IIC_S_MSG_TX);
                    iic_s.bus_occupy = 1;  //��סstart-stop�����⴦�������stop����
                }
            } else {
                putchar('d');
                if (iic_s.rx.cur_cnt < iic_s.rx.b_size) {
                    iic_s.rx.buf[iic_s.rx.toggle][iic_s.rx.cur_cnt++] = byte;
                }
                hw_iic_slave_rx_prepare(IIC_S_DEV, 1);  //������1 byte���գ���ʹ��recv ack
            }
        } else {
            putchar('e');
            //�����������ACK���������1 byte����NACK��������1 byte�������ط���ǰbyte
            if (hw_iic_slave_tx_check_ack(IIC_S_DEV) ||
                (iic_s.tx.tx_cnt - iic_s.tx.cur_cnt == 1)) {
                iic_s.tx.cur_cnt++;
            }
            if (iic_s.tx.cur_cnt < iic_s.tx.tx_cnt) {
                hw_iic_slave_tx_byte(IIC_S_DEV, iic_s.tx.buf[iic_s.tx.toggle][iic_s.tx.cur_cnt]);
            } else {
                //������������ֽ�����ʵ��IIC�����ֽ����࣬����0xff����ֹ����while(!iic_pnd)��������
                hw_iic_slave_tx_byte(IIC_S_DEV, 0xff);
            }
        }
    }
    if (hw_iic_get_end_pnd(IIC_S_DEV)) {
        hw_iic_clr_end_pnd(IIC_S_DEV);
        putchar('f');
        //start-stop��ס���յ�stopʱ�Ĵ���
        if (iic_s.bus_occupy) {
            iic_s.bus_occupy = 0;
            if (iic_s.dir == IIC_S_RX) {
                putchar('g');
                iic_s.rx.toggle = !iic_s.rx.toggle;
                iic_s.rx.rx_cnt = iic_s.rx.cur_cnt;
                os_taskq_post_msg("iic_slave", 2, IIC_S_MSG_RX, iic_s.rx.rx_cnt);
            } else {
                putchar('h');
                iic_s.tx.toggle = !iic_s.tx.toggle;
            }
            iic_s.dir = IIC_S_RX;
            iic_s.rx.cur_cnt = 0;
            iic_s.tx.cur_cnt = 0;
            iic_s.tx.tx_cnt = 0;
            hw_iic_slave_rx_prepare(IIC_S_DEV, 0);  //������1 byte���գ���NACK
        }
    }
}

static void iic_slave_task(void *arg)
{
    int res;
    int msg[8];
    u32 rxlen = 0;
    u32 txlen = 0;
    u8 *addr;

    printf("iic_slave_task run\n");
    memset(&iic_s, 0, sizeof(struct iic_slave));
    iic_s.dir = IIC_S_RX;
    iic_s.rx.buf[0] = iic_s_rxbuf[0];
    iic_s.rx.buf[1] = iic_s_rxbuf[1];
    iic_s.rx.b_size = IIC_S_RXBUF_SIZE;
    iic_s.tx.buf[0] = iic_s_txbuf[0];
    iic_s.tx.buf[1] = iic_s_txbuf[1];
    iic_s.tx.b_size = IIC_S_TXBUF_SIZE;

    hw_iic_init(IIC_S_DEV);
    //����IIC�ӻ���ַ������ʹ�ܵ�ַ���Զ�ACK
    hw_iic_slave_set_addr(IIC_S_DEV, IIC_S_WADDR, 1);
    //ע���ж�isr
    request_irq(IRQ_IIC_IDX, 3, iic_slave_isr, 0);
    //ʹ��byte�����ж�
    hw_iic_set_ie(IIC_S_DEV, 1);
    //ʹ��stop�ж�
    hw_iic_set_end_ie(IIC_S_DEV, 1);
    //������գ���ֹACK���������ַ���Զ�ACK��ͻ��bit ACK��Ҫ�ڽ�������ǰ���ã���
    //��ACK���򿪵�ַ���Զ�ACK��Ϊ�˹Ҷ��IIC�ӻ�ʱ����IIC�ӻ�����ACK����IIC�豸
    //��ַ����ɶ�ӻ�ʧЧ����ַ���Զ�ACKֻ�е��ӻ��յ����õĵ�ַ�Ż�ACK������
    //NACK
    hw_iic_slave_rx_prepare(IIC_S_DEV, 0);
    __asm__ volatile("%0 = icfg" : "=r"(res));
    printf("icfg = %08x\n", res);

    while (1) {
        res = os_taskq_pend("taskq", msg, 8);
        switch (res) {
        case OS_TASKQ:
            switch (msg[0]) {
            case Q_MSG:
                switch (msg[1]) {
                case IIC_S_MSG_RX:
                    puts(">>>>>> iic_s rx msg\n");
                    rxlen = msg[2];
                    addr = iic_s.rx.buf[!iic_s.rx.toggle];
                    printf("rx len: %d\n", rxlen);
                    //put_buf(addr, rxlen);
                    for (int i = 0; i < rxlen; i++) {
                        putchar(*(addr + i));
                        putchar(0x20);
                        if (i % 16 == 15) {
                            putchar('\n');
                        }
                    }
                    break;
                case IIC_S_MSG_TX:
                    puts(">>>>>> iic_s tx msg\n");
                    txlen = rxlen;
                    memcpy(iic_s.tx.buf[!iic_s.tx.toggle], iic_s.rx.buf[!iic_s.rx.toggle], txlen);
                    iic_s.tx.tx_cnt = txlen;
                    hw_iic_slave_tx_byte(IIC_S_WADDR, iic_s.tx.buf[!iic_s.tx.toggle][0]);
                    iic_s.tx.toggle = !iic_s.tx.toggle;
                    printf("tx req len: %d\n", txlen);
                    break;
                }
                break;
            }
            break;
        }
    }
}

void iic_demo_slave_main()
{
    printf("%s() %d\n", __func__, __LINE__);
    os_task_create(iic_slave_task, NULL, 30, 1024, 64, "iic_slave");
}




/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~ iic host below ~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

#define IIC_H_DEV                                 0
#define IIC_H_TXBUF_SIZE                        128
#define IIC_H_RXBUF_SIZE                        128
#define IIC_H_DELAY                             500

#define iic_h_check_ack(ack, pCnt) \
    if (!(ack)) { \
        printf("nack %d\n", __LINE__); \
        if (*(pCnt) > 0 && --(*(pCnt))) { \
            continue; \
        } else { \
            break; \
        } \
    }

static u8 iic_h_txbuf[IIC_H_TXBUF_SIZE];
static u8 iic_h_rxbuf[IIC_H_RXBUF_SIZE];

void iic_demo_host_main()
{
    int i;
    int ret;
    u32 retry;
    u8 byte;

    printf("%s() %d\n", __func__, __LINE__);
    hw_iic_init(IIC_H_DEV);
    for (i = 0; i < IIC_H_TXBUF_SIZE; i++) {
        iic_h_txbuf[i] = 'A' + i % 26;
    }
    for (u8 times = 0; times < 3; times++) {  //���Դ���
        retry = 10;
        do {
            hw_iic_start(IIC_H_DEV);
            putchar('a');
            ret = hw_iic_tx_byte(IIC_H_DEV, IIC_S_WADDR);
            putchar('b');
            iic_h_check_ack(ret, &retry);
            delay(IIC_H_DELAY);
            i = 0;
            while (i < IIC_H_TXBUF_SIZE) {
                ret = hw_iic_tx_byte(IIC_H_DEV, iic_h_txbuf[i]);
                putchar('c');
                iic_h_check_ack(ret, &retry);
                delay(IIC_H_DELAY);
                i++;
            }
            break;
        } while (1);
        hw_iic_stop(IIC_H_DEV);
        delay(IIC_H_DELAY);  //stop����Ҫdelayһ��ʱ�����start

        retry = 10;
        do {
            hw_iic_start(IIC_H_DEV);
            putchar('d');
            ret = hw_iic_tx_byte(IIC_H_DEV, IIC_S_RADDR);
            putchar('e');
            iic_h_check_ack(ret, &retry);
            delay(IIC_H_DELAY);
            i = 0;
            while (i < IIC_H_RXBUF_SIZE - 1) {
                iic_h_rxbuf[i] = hw_iic_rx_byte(IIC_H_DEV, 1);
                putchar('f');
                delay(IIC_H_DELAY);
                i++;
            }
            iic_h_rxbuf[i] = hw_iic_rx_byte(IIC_H_DEV, 0);  //IIC�����������1 byte NACK
            putchar('g');
            delay(IIC_H_DELAY);
            break;
        } while (1);
        hw_iic_stop(IIC_H_DEV);  //stop����Ҫdelayһ��ʱ�����start
        delay(IIC_H_DELAY);
        putchar('\n');

        ret = 0;
        for (i = 0; i < IIC_H_RXBUF_SIZE; i++) {
            putchar(iic_h_rxbuf[i]);
            putchar(0x20);
            if (i % 16 == 15) {
                putchar('\n');
            }
            if (iic_h_txbuf[i] != iic_h_rxbuf[i]) {
                ret = 1;
            }
        }
        if (!ret) {
            puts("iic slave test pass\n");
        } else {
            puts("iic slave test fail\n");
        }
    }
}
#endif
