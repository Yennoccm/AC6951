#include "system/includes.h"
#include "media/includes.h"
#include "asm/spi.h"
#include "generic/log.h"

#if 0
/*
    [[[ README ]]]
    1. ��spi����demo�ṩ��spi.c��APIʹ�����̣����Է�ʽΪ����spi�Ļ��ز��ԡ�
    spi1����Ϊ����ģʽ��spi2����Ϊ�ӻ�ģʽ��spi1�������ݵ�spi2��Ȼ�����spi2
    ԭ�����ص����ݣ�Ȼ��ȽϷ��ͳ�ȥ����������յ������Ƿ�һ�£�һ����˵��
    ��֤ͨ����
    2. ��demo�漰BYTE�շ����Լ�DMA�շ����ԣ�ͨ����SPI_TEST_MODEѡ������
    demo���漰��spi�ж��е��ÿ������жϵ�spi API��ʹ�á�
    3. spi.c��API������CS���ţ�CS��API������ơ�
    4. ����board_xxx.c�ж������ýṹ�壬�����õ�spi1����Ҫ����spi1_p_data��
    ����������
    5. spi��DMA��ַ��Ҫ4�ֽڶ��롣
    6. ��Ȼspi.c��API����spi0�������й�spi flash��оƬ��ʹ�ÿ��ܻ�����⣬
    ����ʹ��spi0��

*/

#define SPI1_CS_OUT() \
    do { \
        JL_PORTB->DIR &= ~BIT(4); \
        JL_PORTB->DIE |= BIT(4); \
        JL_PORTB->PU &= ~BIT(4); \
        JL_PORTB->PD &= ~BIT(4); \
    } while(0)
#define SPI1_CS_L()     (JL_PORTB->OUT &= ~BIT(4))
#define SPI1_CS_H()     (JL_PORTB->OUT |= BIT(4))

#define SPI2_CS_IN() \
    do { \
        JL_PORTA->DIR |= BIT(3); \
        JL_PORTA->DIE |= BIT(3); \
        JL_PORTA->PU &= ~BIT(3); \
        JL_PORTA->PD &= ~BIT(3); \
    } while (0)
#define SPI2_READ_CS()     (JL_PORTA->IN & BIT(3))

static u8 slave_dir = 1;
static u8 spi1_send_buf[100] __attribute__((aligned(4)));
static u8 spi1_recv_buf[100] __attribute__((aligned(4)));
static u8 spi2_send_buf[100] __attribute__((aligned(4)));
static u8 spi2_recv_buf[100] __attribute__((aligned(4)));

static spi_dev spi1_hdl = 1;
static spi_dev spi2_hdl = 2;

#define SPI_TEST_BYTE_MODE      0x01
#define SPI_TEST_DMA_MODE       0x02
//����ģʽѡ��
#define SPI_TEST_MODE           SPI_TEST_BYTE_MODE

static void my_put_u8hex(u8 b)
{
    u8 dat;
    dat = b / 16;
    if (dat >= 0 && dat <= 9) {
        putchar('0' + dat);
    } else {
        putchar('A' + dat - 10);
    }
    dat = b % 16;
    if (dat >= 0 && dat <= 9) {
        putchar('0' + dat);
    } else {
        putchar('A' + dat - 10);
    }
    putchar(' ');
}

//�жϺ�������������������
__attribute__((interrupt("")))
static void spi2_isr()
{
    static int i = 0;
    if (spi_get_pending(spi2_hdl)) {
        spi_clear_pending(spi2_hdl);
        if (SPI2_READ_CS()) {
            return;
        }
#if SPI_TEST_MODE == SPI_TEST_BYTE_MODE
        if (slave_dir == 1) {
            spi2_recv_buf[i] = spi_recv_byte_for_isr(spi2_hdl);
            spi_send_byte_for_isr(spi2_hdl, spi2_recv_buf[i]);
            i >= 100 ? i = 0 : i++;
            slave_dir = 0;
        } else {
            slave_dir = 1;
        }
#elif SPI_TEST_MODE == SPI_TEST_DMA_MODE
        if (slave_dir == 1) {
            spi_dma_set_addr_for_isr(spi2_hdl, spi2_recv_buf, 100, 0);
            slave_dir = 0;
        } else {
            slave_dir = 1;
        }
#endif
    }
}

#if 1  //������spi demo����ʽ������ŵ�board_xxx.c�ļ���
/* const struct spi_platform_data spi0_p_data = { */
/* .port = 'A', */
/* .mode = SPI_MODE_BIDIR_1BIT, */
/* .clk = 1000000, */
/* .role = SPI_ROLE_MASTER, */
/* }; */

const struct spi_platform_data spi1_p_data = {
    .port = 'A',
    .mode = SPI_MODE_BIDIR_1BIT,
    .clk = 1000000,
    .role = SPI_ROLE_MASTER,
};

const struct spi_platform_data spi2_p_data = {
    .port = 'A',
    .mode = SPI_MODE_BIDIR_1BIT,
    .clk = 1000000,
    .role = SPI_ROLE_SLAVE,
};
#endif


void spi_test_main()
{
    int i;
    int err;

    spi_open(spi1_hdl);
    spi_open(spi2_hdl);
    spi_set_ie(spi2_hdl, 1);
    //�����ж����ȼ����жϺ���
    request_irq(IRQ_SPI2_IDX, 3, spi2_isr, 0);

    SPI1_CS_OUT();
    SPI2_CS_IN();
    SPI1_CS_H();
    for (i = 0; i < 100; i++) {
        spi1_send_buf[i] = i % 26 + 'A';
        spi1_recv_buf[i] = 0;
    }
    puts(">>> spi test start\n");
#if SPI_TEST_MODE == SPI_TEST_BYTE_MODE
    SPI1_CS_L();
    for (i = 0; i < 100; i++) {
        err = spi_send_byte(spi1_hdl, spi1_send_buf[i]);
        if (err) {
            puts("spi1 byte send timeout\n");
            break;
        }
        delay(100);
        spi1_recv_buf[i] = spi_recv_byte(spi1_hdl, &err);
        if (err) {
            puts("spi1 byte recv timeout\n");
            break;
        }
        delay(100);
    }
    SPI1_CS_H();
#elif SPI_TEST_MODE == SPI_TEST_DMA_MODE
    spi_dma_set_addr_for_isr(spi2_hdl, spi2_recv_buf, 100, 1);
    SPI1_CS_L();
    err = spi_dma_send(spi1_hdl, spi1_send_buf, 100);
    if (err < 0) {
        puts("spi1 dma send timeout\n");
        goto __out_dma;
    }
    //delay(100);
    err = spi_dma_recv(spi1_hdl, spi1_recv_buf, 100);
    if (err < 0) {
        puts("spi1 dma recv timeout\n");
        goto __out_dma;
    }
    //delay(100);
__out_dma:
    SPI1_CS_H();
#endif
    puts("<<< spi test end\n");

    puts("\nspi master receivce buffer:\n");
    for (i = 0; i < 100; i++) {
        //my_put_u8hex(spi1_recv_buf[i]);
        putchar(spi1_recv_buf[i]), putchar(0x20);
        if (i % 16 == 15) {
            putchar('\n');
        }
    }
    if (i % 16) {
        putchar('\n');
    }

    if (!memcmp(spi1_send_buf, spi1_recv_buf, 100)) {
        puts("\nspi test pass\n");
    } else {
        puts("\nspi test fail\n");
    }

    spi_close(spi1_hdl);
    spi_close(spi2_hdl);
}
#endif
