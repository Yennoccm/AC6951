#include "audio_dec_midi_ctrl.h"

#if AUDIO_MIDI_CTRL_CONFIG
/*
 *�������ఴ�ټ�ͬʱ���»��ظ����ٰ�ĳ�ټ����ֿ��ٵ����
 *��������1������ͬʱ����MIDI_KEY_NUM ����
 *          2��������flash�� �����ʣ�60M��90M��,����spi���߿��Ϊ2��ģʽ��4��
 *          3�����ϵͳʱ��
 * */
extern struct audio_dac_hdl dac_hdl;

#define MIDI_KEY_NUM  (18)//(֧�ֶ��ٸ�keyͬʱ����(1~18)�� Խ����Ҫ��ʱ��Խ��)

const u16 midi_samplerate_tab[9] = {
    48000,
    44100,
    32000,
    24000,
    22050,
    16000,
    12000,
    11025,
    8000
};

struct _midi_obj {
    u8 channel;
    u32 sample_rate;
    u32 id;				// Ψһ��ʶ�������ֵ
    u32 start : 1;		// ���ڽ���
    char *path;         //��ɫ�ļ�·��
    struct audio_res_wait wait;		// ��Դ�ȴ����
    struct midi_ctrl_decoder midi_ctrl_dec;
    struct audio_mixer_ch mix_ch;	// ���Ӿ��
    struct audio_stream *stream;		// ��Ƶ��
};

struct _midi_obj *midi_ctrl_dec_hdl = NULL;

void midi_ctrl_ioctrl(u32 cmd, void *priv);
/*----------------------------------------------------------------------------*/
/**@brief    midi ctrl�����ͷ�
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void midi_ctrl_dec_relaese()
{
    if (midi_ctrl_dec_hdl) {
        audio_decoder_task_del_wait(&decode_task, &midi_ctrl_dec_hdl->wait);
        clock_remove(DEC_MIDI_CLK);
        local_irq_disable();
        free(midi_ctrl_dec_hdl);
        midi_ctrl_dec_hdl = NULL;
        local_irq_enable();
    }
}

/*----------------------------------------------------------------------------*/
/**@brief    midi��ɫ�ļ���ȡ
   @param
   @return
   @note     �ڲ�����
*/
/*----------------------------------------------------------------------------*/
static int midi_get_cfg_addr(u8 **addr)
{
    if (!midi_ctrl_dec_hdl) {
        return -1;
    }

#ifndef CONFIG_MIDI_DEC_ADDR
    //��ɫ�ļ�֧�����ⲿ�洢���������flash,sdkĬ��ʹ�ñ���ʽ
    /* FILE  *file = fopen("storage/sd0/C/MIDI.bin\0", "r"); */
    FILE  *file = fopen(midi_ctrl_dec_hdl->path, "r");

    /* FILE  *file = fopen(SDFILE_RES_ROOT_PATH"MIDI.bin\0", "r"); */
    if (!file) {
        log_e("MIDI.bin open err\n");
        return -1;
    }
    *addr = (u8 *)file;
    log_i("midi_file %x\n", file);
#else
    //��ɫ�ļ���֧��������flash
    /* FILE  *file = fopen(SDFILE_RES_ROOT_PATH"MIDI.bin\0", "r"); */
    FILE  *file = fopen(midi_ctrl_dec_hdl->path, "r");
    if (!file) {
        log_e("MIDI.bin open err\n");
        return -1;
    }

    struct vfs_attr attr = {0};
    fget_attrs(file, &attr);
    *addr = (u8 *)attr.sclust;
    fclose(file);
#endif

    return 0;
}



/*----------------------------------------------------------------------------*/
/**@brief    midi��ɫ�ļ���
   @param
   @return
   @note     �ڲ�����
*/
/*----------------------------------------------------------------------------*/
static int midi_fread(void *file, void *buf, u32 len)
{
#ifndef CONFIG_MIDI_DEC_ADDR
    FILE *hd = (FILE *)file;
    if (hd) {
        len = fread(hd, buf, len);
    }
#endif
    return len;
}
/*----------------------------------------------------------------------------*/
/**@brief    midi��ɫ�ļ�seek
   @param
   @return
   @note     �ڲ�����
*/
/*----------------------------------------------------------------------------*/
static int midi_fseek(void *file, u32 offset, int seek_mode)
{
#ifndef CONFIG_MIDI_DEC_ADDR
    FILE *hd = (FILE *)file;
    if (hd) {
        fseek(hd, offset, seek_mode);
    }
#endif
    return 0;
}
/*----------------------------------------------------------------------------*/
/**@brief    midi ctrl��ʼ���������ú����ɿ����
   @param    �������ص�ַ
   @return   0
   @note     �ú����������庯���������޸Ķ���
*/
/*----------------------------------------------------------------------------*/
int midi_ctrl_init(void *info)
{
    if (!midi_ctrl_dec_hdl) {
        return -1;
    }
    u8 *cache_addr;
    if (midi_get_cfg_addr(&cache_addr)) {
        return -1;
    }
    midi_ctrl_open_parm *parm = (midi_ctrl_open_parm *)info;
    parm->ctrl_parm.tempo = 1024;//�����ı䲥���ٶ�,1024������ֵ
    parm->ctrl_parm.track_num = 1;//֧�������������0~15

    parm->sample_rate = midi_ctrl_dec_hdl->midi_ctrl_dec.sample_rate;//midi_samplerate_tab[5];
    parm->cfg_parm.player_t = MIDI_KEY_NUM; //(֧�ֶ��ٸ�keyͬʱ����,�û����޸�)
    parm->cfg_parm.spi_pos = (unsigned int)cache_addr;
    parm->cfg_parm.fread = midi_fread;
    parm->cfg_parm.fseek = midi_fseek;

    for (int i = 0; i < ARRAY_SIZE(midi_samplerate_tab); i++) {
        if (parm->sample_rate == midi_samplerate_tab[i]) {
            parm->cfg_parm.sample_rate = i;
            break;
        }
    }
    return 0;
}
/*----------------------------------------------------------------------------*/
/**@brief    midi ctrl��ɫ�ļ��ر�
   @param
   @return
   @note     �ú�����midi�ر�ʱ����,�ú����������庯���������޸Ķ���
*/
/*----------------------------------------------------------------------------*/
int midi_ctrl_uninit(void *priv)
{
#ifndef CONFIG_MIDI_DEC_ADDR
    if (priv) {
        fclose((FILE *)priv);
        priv = NULL;
    }
#endif
    return 0;
}

/*----------------------------------------------------------------------------*/
/**@brief   ��������
   @param   prog:������
   @param   trk_num :���� (0~15)
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void midi_ctrl_set_porg(u8 prog, u8 trk_num)
{
    struct set_prog_parm parm = {0};
    parm.prog = prog;
    parm.trk_num = trk_num;
    midi_ctrl_ioctrl(MIDI_CTRL_SET_PROG, &parm);
}

/*----------------------------------------------------------------------------*/
/**@brief   ��������
   @param   nkey:������ţ�0~127��
   @param   nvel:�������ȣ�0~127��
   @param   chn :ͨ��(0~15)
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void midi_ctrl_note_on(u8 nkey, u8 nvel, u8 chn)
{
    struct note_on_parm parm = {0};
    parm.nkey = nkey;
    parm.nvel = nvel;
    parm.chn = chn;
    midi_ctrl_ioctrl(MIDI_CTRL_NOTE_ON, &parm);
}
/*----------------------------------------------------------------------------*/
/**@brief   �����ɿ�
   @param   nkey:������ţ�0~127��
   @param   chn :ͨ��(0~15)
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void midi_ctrl_note_off(u8 nkey, u8 chn)
{
    struct note_off_parm parm = {0};
    parm.nkey = nkey;
    parm.chn = chn;
    midi_ctrl_ioctrl(MIDI_CTRL_NOTE_OFF, &parm);
}
/*----------------------------------------------------------------------------*/
/**@brief  midi ���ýӿ�
   @param   cmd:����
   @param   priv:��Ӧcmd�Ľṹ��
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void midi_ctrl_confing(u32 cmd, void *priv)
{
    midi_ctrl_ioctrl(cmd, priv);
}

/*----------------------------------------------------------------------------*/
/**@brief   midi keyboard ���ð�����������������˥��ϵ��
   @param   obj:���ƾ��
   @param   samp:��Ӧsamplerate_tab����
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void midi_ctrl_confing_set_melody_decay(u16 val)
{
    u32 cmd = CMD_MIDI_CTRL_TEMPO;
    MIDI_PLAY_CTRL_TEMPO tempo = {0};
    tempo.decay_val = val;
    tempo.tempo_val = 1024;//����Ϊ�̶�1024����
    midi_ctrl_confing(cmd, (void *)&tempo);
}
/*----------------------------------------------------------------------------*/
/**@brief  ����������
   @param   pitch_val:������ֵ,1 - 65535 ��256������ֵ,������������
   @param   chn :ͨ��(0~15)
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void midi_ctrl_pitch_bend(u16 pitch_val, u8 chn)
{
    struct pitch_bend_parm parm = {0};
    parm.pitch_val = pitch_val;
    parm.chn = chn;
    midi_ctrl_ioctrl(MIDI_CTRL_PITCH_BEND, &parm);
}



/*----------------------------------------------------------------------------*/
/**@brief    midi ctrl��������������
   @param    *p: ˽�о��
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
static void midi_ctrl_dec_out_stream_resume(void *p)
{
    struct _midi_obj *dec = p;
#if FILE_DEC_USE_OUT_TASK
    if (dec->midi_ctrl_dec.dec_no_out_sound == 0) {
        audio_decoder_resume_out_task(&dec->midi_ctrl_dec.decoder);
        return ;
    }
#endif
    audio_decoder_resume(&dec->midi_ctrl_dec.decoder);
}


/*----------------------------------------------------------------------------*/
/**@brief    midi ctrl�����¼�����
   @param    *decoder: ���������
   @param    argc: ��������
   @param    *argv: ����
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
static void midi_ctrl_dec_event_handler(struct audio_decoder *decoder, int argc, int *argv)
{
    switch (argv[0]) {
    case AUDIO_DEC_EVENT_END:
        if (!midi_ctrl_dec_hdl) {
            log_i("midi_ctrl_dec_hdl handle err ");
            break;
        }

        if (midi_ctrl_dec_hdl->id != argv[1]) {
            log_w("midi_ctrl_dec_hdl id err : 0x%x, 0x%x \n", midi_ctrl_dec_hdl->id, argv[1]);
            break;
        }

        midi_ctrl_dec_close();
        //audio_decoder_resume_all(&decode_task);
        break;
    }
}
/*----------------------------------------------------------------------------*/
/**@brief    midi ctrl���뿪ʼ
   @param
   @return   0���ɹ�
   @return   ��0��ʧ��
   @note
*/
/*----------------------------------------------------------------------------*/
int midi_ctrl_dec_start()
{
    int err;
    struct _midi_obj *dec = midi_ctrl_dec_hdl;

    struct audio_mixer *p_mixer = &mixer;

    if (!dec) {
        return -EINVAL;
    }

    log_i("midi_ctrl dec start: in\n");
// ��midi ctrl������
    err = midi_ctrl_decoder_open(&dec->midi_ctrl_dec, &decode_task);
    if (err) {
        goto __err1;
    }

    midi_ctrl_decoder_set_event_handler(&dec->midi_ctrl_dec, midi_ctrl_dec_event_handler, dec->id);

// ���õ��ӹ���
    audio_mixer_ch_open_head(&dec->mix_ch, p_mixer);
    audio_mixer_ch_set_src(&dec->mix_ch, 1, 0);

#if FILE_DEC_USE_OUT_TASK
    if (dec->midi_ctrl_dec.dec_no_out_sound == 0) {
        audio_decoder_out_task_ch_enable(&dec->midi_ctrl_dec.decoder);
    }
#endif


// ����������
    struct audio_stream_entry *entries[8] = {NULL};
    u8 entry_cnt = 0;

    entries[entry_cnt++] = &dec->midi_ctrl_dec.decoder.entry;
    entries[entry_cnt++] = &dec->mix_ch.entry;

    // �����������������нڵ���������

    dec->stream = audio_stream_open(dec, midi_ctrl_dec_out_stream_resume);
    audio_stream_add_list(dec->stream, entries, entry_cnt);

// ������Ƶ�������
    audio_output_set_start_volume(APP_AUDIO_STATE_MUSIC);
// ����ʱ��
    clock_set_cur();

    // ��ʼ����
    dec->midi_ctrl_dec.status = FILE_DEC_STATUS_PLAY;
    err = audio_decoder_start(&dec->midi_ctrl_dec.decoder);
    dec->start = 1;
    if (err) {
        goto __err3;
    }
    return 0;
__err3:
    audio_mixer_ch_close(&dec->mix_ch);

    midi_ctrl_decoder_close(&dec->midi_ctrl_dec);

    if (dec->stream) {
        audio_stream_close(dec->stream);
        dec->stream = NULL;
    }
__err1:
    log_w(" start err close\n");
    midi_ctrl_dec_relaese();
    return -1;
}
/*----------------------------------------------------------------------------*/
/**@brief    midi ctrl����ر�
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
static void __midi_ctrl_dec_close(void)
{
    if (midi_ctrl_dec_hdl && midi_ctrl_dec_hdl->start) {
        midi_ctrl_dec_hdl->start = 0;

        midi_ctrl_decoder_close(&midi_ctrl_dec_hdl->midi_ctrl_dec);

        audio_mixer_ch_close(&midi_ctrl_dec_hdl->mix_ch);

        // �ȹرո����ڵ㣬����close������
        if (midi_ctrl_dec_hdl->stream) {
            audio_stream_close(midi_ctrl_dec_hdl->stream);
            midi_ctrl_dec_hdl->stream = NULL;
        }

    }
}


/*----------------------------------------------------------------------------*/
/**@brief    �ر�midi ctrl����
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void midi_ctrl_dec_close(void)
{
    if (!midi_ctrl_dec_hdl) {
        return;
    }
    __midi_ctrl_dec_close();
    midi_ctrl_dec_relaese();
    clock_set_cur();
    log_i("midi ctrl dec close \n\n ");
}

/*----------------------------------------------------------------------------*/
/**@brief    midi ctrl������Դ�ȴ�
   @param    *wait: ���
   @param    event: �¼�
   @return   0���ɹ�
   @note     ���ڶ�����ϴ���
*/
/*----------------------------------------------------------------------------*/
static void __midi_ctrl_dec_close(void);
static int midi_ctrl_wait_res_handler(struct audio_res_wait *wait, int event)
{
    int err = 0;
    log_i("midi_ctrl_wait_res_handler, event:%d\n", event);
    if (event == AUDIO_RES_GET) {
        // ��������
        err = midi_ctrl_dec_start();
    } else if (event == AUDIO_RES_PUT) {
        // �����
        __midi_ctrl_dec_close();
    }

    return err;
}

/*----------------------------------------------------------------------------*/
/**@brief    ��midi ctrl����
   @param    sample_rate: ������
   @param    *path:��ɫ�ļ�·��
   @return   0���ɹ�
   @return   ��0��ʧ��
   @note
*/
/*----------------------------------------------------------------------------*/
int midi_ctrl_dec_open(u32 sample_rate, char *path)
{
    int err = 0;
    int i = 0;
    struct _midi_obj *dec;
    dec = zalloc(sizeof(*dec));
    if (!dec) {
        return -ENOMEM;
    }
    midi_ctrl_dec_hdl = dec;
    dec->id = rand32();
    dec->path = path;

    dec->midi_ctrl_dec.ch_num = 2;

    dec->midi_ctrl_dec.output_ch_num = audio_output_channel_num();
    dec->midi_ctrl_dec.output_ch_type = audio_output_channel_type();
#if TCFG_MIC_EFFECT_ENABLE
    dec->midi_ctrl_dec.sample_rate = MIC_EFFECT_SAMPLERATE;
#else
    for (i = 0; i < ARRAY_SIZE(midi_samplerate_tab); i++) {
        if (sample_rate == midi_samplerate_tab[i]) {
            dec->midi_ctrl_dec.sample_rate = midi_samplerate_tab[i];
            break;
        }
    }
    if (i >= ARRAY_SIZE(midi_samplerate_tab)) {
        dec->midi_ctrl_dec.sample_rate = 16000;
        log_e("midi sample_rate check err ,will set default 16000 Hz\n");
    }
#endif
    dec->wait.priority = 2;
    dec->wait.preemption = 0;
    dec->wait.snatch_same_prio = 1;
    /* dec->wait.protect = 1; */
    dec->wait.handler = midi_ctrl_wait_res_handler;
    clock_add(DEC_MIDI_CLK);


    err = audio_decoder_task_add_wait(&decode_task, &dec->wait);
    return err;
}
/*----------------------------------------------------------------------------*/
/**@brief    midi ctrl���ƺ���
	@param   cmd:
			 MIDI_CTRL_NOTE_ON,     //�������£������ṹ��Ӧstruct note_on_parm
			 MIDI_CTRL_NOTE_OFF,    //�����ɿ��������ṹ��Ӧstruct note_off_parm
			 MIDI_CTRL_SET_PROG,    //���������������ṹ��Ӧstruct set_prog_parm
			 MIDI_CTRL_PITCH_BEND,  //�����֣������ṹ��Ӧstruct pitch_bend_parm
		     CMD_MIDI_CTRL_TEMPO,    //�ı����,�����ṹ��Ӧ MIDI_PLAY_CTRL_TEMPO
   @param    priv:��Ӧcmd�Ĳ�����ַ
   @return   0
   @note    midi�����������
*/
/*----------------------------------------------------------------------------*/
void midi_ctrl_ioctrl(u32 cmd, void *priv)
{
    struct _midi_obj *dec = midi_ctrl_dec_hdl;
    if (!dec) {
        log_e("midi ctrl dec NULL\n");
        return ;
    }

    log_i("midi ctrl cmd %x", cmd);
    audio_decoder_ioctrl(&dec->midi_ctrl_dec.decoder, cmd, priv);
}

#if 0
/*----------------------------------------------------------------------------*/
/**@brief   midi key ��������
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void midi_paly_test(u32 key)
{
    static u8 open_close = 0;
    static u8 change_prog = 0;
    static u8 note_on_off = 0;
    switch (key) {
    case KEY_IR_NUM_0:
        if (!open_close) {
            /* midi_ctrl_dec_open(16000);//����midi key */
            midi_ctrl_dec_open(16000, "storage/sd0/C/MIDI.bin\0");//����midi key
            //midi_ctrl_dec_open(16000, SDFILE_RES_ROOT_PATH"MIDI.bin\0");//����midi key

        } else {
            midi_ctrl_dec_close();//�ر�midi key
        }
        open_close = !open_close;
        break;
    case KEY_IR_NUM_1:
        if (!change_prog) {
            midi_ctrl_set_porg(0, 0);//����0������������0
        } else {
            midi_ctrl_set_porg(22, 0);//����22������������0
        }
        change_prog = !change_prog;
        break;
    case KEY_IR_NUM_2:
        if (!note_on_off) {
            //ģ�ⰴ��57��58��59��60��61��62,������127��ͨ��0�����²���
            midi_ctrl_note_on(57, 127, 0);
            midi_ctrl_note_on(58, 127, 0);
            midi_ctrl_note_on(59, 127, 0);
            midi_ctrl_note_on(60, 127, 0);
            midi_ctrl_note_on(61, 127, 0);
            midi_ctrl_note_on(62, 127, 0);
        } else {
            //ģ�ⰴ��57��58��59��60��61��62�ɿ�����
            midi_ctrl_note_off(57,  0);
            midi_ctrl_note_off(58,  0);
            midi_ctrl_note_off(59,  0);
            midi_ctrl_note_off(60,  0);
            midi_ctrl_note_off(61,  0);
            midi_ctrl_note_off(62,  0);
        }
        note_on_off = !note_on_off;
        break;
    default:
        break;
    }
}
#endif
#endif
