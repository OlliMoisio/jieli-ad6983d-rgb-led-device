#ifndef _EQ_CONFIG_H_
#define _EQ_CONFIG_H_

#include "application/audio_eq.h"
#include "application/audio_drc.h"
#include "spinlock.h"
#include "math.h"

/*-----------------------------------------------------------*/
/*eq online cmd*/
enum {
    EQ_ONLINE_CMD_SECTION       = 1,
    EQ_ONLINE_CMD_GLOBAL_GAIN,
    EQ_ONLINE_CMD_LIMITER,
    EQ_ONLINE_CMD_INQUIRE,
    EQ_ONLINE_CMD_GETVER,
    EQ_ONLINE_CMD_GET_SOFT_SECTION,//br22专用
    EQ_ONLINE_CMD_GET_SECTION_NUM = 0x7,//工具查询 小机需要的eq段数
    EQ_ONLINE_CMD_GLOBAL_GAIN_SUPPORT_FLOAT = 0x8,


    EQ_ONLINE_CMD_PASSWORD = 0x9,
    EQ_ONLINE_CMD_VERIFY_PASSWORD = 0xA,
    EQ_ONLINE_CMD_FILE_SIZE = 0xB,
    EQ_ONLINE_CMD_FILE = 0xC,
    EQ_EQ_ONLINE_CMD_GET_SECTION_NUM = 0xD,//该命令新加与 0x7功能一样
    EQ_EQ_ONLINE_CMD_CHANGE_MODE = 0xE,//切模式

    EQ_ONLINE_CMD_MODE_COUNT = 0x100,//模式个数a  1
    EQ_ONLINE_CMD_MODE_NAME = 0x101,//模式的名字a eq
    EQ_ONLINE_CMD_MODE_GROUP_COUNT = 0x102,//模式下组的个数,4个字节 1
    EQ_ONLINE_CMD_MODE_GROUP_RANGE = 0x103,//模式下组的id内容  0x11
    EQ_ONLINE_CMD_EQ_GROUP_COUNT = 0x104,//eq组的id个数  1
    EQ_ONLINE_CMD_EQ_GROUP_RANGE = 0x105,//eq组的id内容 0x11
    EQ_ONLINE_CMD_MODE_SEQ_NUMBER = 0x106,//mode的编号  magic num


    EQ_ONLINE_CMD_PARAMETER_SEG = 0x11,
    EQ_ONLINE_CMD_PARAMETER_TOTAL_GAIN,
    EQ_ONLINE_CMD_PARAMETER_LIMITER,
    EQ_ONLINE_CMD_PARAMETER_DRC,
    EQ_ONLINE_CMD_PARAMETER_CHANNEL,//通道切换

    EQ_ONLINE_CMD_WRITE_FILE_SIZE = 0x20,  //准备写入文件
    EQ_ONLINE_CMD_WRITE_FILE_CHANNEL = 0x21,

    EQ_ONLINE_CMD_SONG_EQ_SEG = 0x2001,
    EQ_ONLINE_CMD_CALL_EQ_SEG = 0x2002,
    EQ_ONLINE_CMD_AEC_EQ_SEG = 0x2003,

    EQ_ONLINE_CMD_SONG_EQ_V1_SEG = 0x2011,// float gain, float q
    EQ_ONLINE_CMD_CALL_EQ_V1_SEG = 0x2012,
    EQ_ONLINE_CMD_AEC_EQ_V1_SEG = 0x2013,


    EQ_ONLINE_CMD_SONG_DRC = 0x2101,
    EQ_ONLINE_CMD_SONG_WDRC = 0x2102,
//add xx here

    EQ_ONLINE_CMD_MAX,//最后一个
};

#define CONSTRAINT_OP_DRC_L_R_CH  0x01
#define CONSTRAINT_OP_DRC_WDRC_SINGLE_CH 0x02
#define CONSTRAINT_OP_DRC_WDRC_DUAL_CH 0x03


/*eq online packet*/
typedef struct {
    int cmd;     			///<EQ_ONLINE_CMD
    int data[45];       	///<data
} EQ_ONLINE_PACKET;


/*EQ_ONLINE_CMD_PARAMETER_SEG*/
typedef struct eq_seg_info EQ_ONLINE_PARAMETER_SEG;

/*-----------------------------------------------------------*/
typedef struct eq_seg_info EQ_CFG_SEG;

typedef struct {
    float global_gain;    //总增益
    int seg_num;          //eq效果文件存储的段数
    int enable_section;   //
} CFG_PARM;

#ifdef CONFIG_CPU_BR23
#define SECTION_MAX 32
#else
#ifdef CONFIG_CPU_BR25
#define SECTION_MAX 10
#else
#define SECTION_MAX 20
#endif//CONFIG_CPU_BR25
#endif//CONFIG_CPU_BR23

typedef struct {
    CFG_PARM par;
    EQ_CFG_SEG seg[SECTION_MAX];   //eq系数存储地址
} EQ_CFG_PARAMETER;


typedef struct {
    struct drc_ch_org drc;    //drc系数地址
} DRC_CFG_PARAMETER;


typedef struct {
    /* unsigned short crc; */
    unsigned short id;    //eq效果文件存储的标识
    unsigned short len;   //当前标识结构长度
} EQ_FILE_HEAD;

typedef struct {
    EQ_FILE_HEAD head;
    EQ_CFG_PARAMETER parm;
}  EQ_CFG_PARM;           //效果文件中，eq存储结构

typedef struct {
    EQ_FILE_HEAD head;
    DRC_CFG_PARAMETER parm;
}  DRC_CFG_PARM;          //效果文件中，drc存储结构

typedef struct {
    EQ_CFG_PARM song_eq_parm;
    DRC_CFG_PARM drc_parm;
    u8 cur_mode;
} EQ_FILE_PARM;           //eq drc效果系数，统一管理

typedef struct {
    u16 tmr;
    u8 *fade_stu;
    u32 *sr;
    EQ_CFG_SEG *seg;
} EQ_FADE_CFG;            //系数eq系数调节,支持淡入淡出，防止增益跳跃过大，造成哒哒音

typedef struct _eq_tool_cfg {
    u8 mode_index;       //模式序号
    u8 *mode_name;       //模式名
    u32 mode_seq;        //模式的seq,用于识别离线文件功能类型
    u8 section;          //每个模式下eq的段数
    u8 fun_num;          //模式下有多少种功能
    u16 fun_type[2];     //模式下拥有哪些功能
} eq_tool_cfg;           //调试工具支持的功能

struct vm_save {
    u8 *file;
    u16 file_size;
    u16 offset;
    u8 vm;
};
typedef struct {
    u32 eq_type : 3;      //系数调试所处模式：在线模式、效果文件（离线）模式、默认系数表模式
    u32 *mode_updata;     //默认系数表切换更新标志
    u32 *drc_updata;      //drc系数更新标志
    u32 *online_updata;   //在线调试系数更新标志
    u32 *design_mask;     //在线调试，哪一段eq需要更新标志
    u32 *seg_num;         //当前模式(播歌、通话宽频、窄频上下行模式标号),所拥有的eq段数
    EQ_FILE_PARM *cfg_parm;//eq drc效果系数，统一管理
    u32 *cur_sr;          //当前模式(播歌、通话宽频、窄频上下行模式标号),记录的采样率
    spinlock_t lock;      //自旋锁
    u8 mode_id;           //当前模式(播歌、通话宽频、窄频上下行模式标号),在线调试时，内部使用

    u8 mode_num;          //模式总数(播歌、通话宽频、窄频上下行模式标号)
    u8 online_en: 1;      //是否支持在线调试
    u8 fade_en: 1;        //是否支持淡入淡出
    u8 stero: 1;          //左右声道效果是否支持拆分
    u8 drc: 1;            //是否支持drc调试
    u8 tws: 1;            //是否支持tws调试
    u8 app: 1;            //是否支持app调试
    u8 limit_zero: 1;     //是否将系数限制到0
    u8 eq_file_ver_err: 1;//记录效果文件版本是否匹配
    u8 eq_file_section_err : 1;//记录效果文件eq段数是否匹配
    u8 type_num;          //默认eq系数表总个数
    u8 section_max;       //eq最大段数
    u8 eq_mode;           //记录当前使用哪个eq系数表
    u8 parse_seq;         //记录app调试的seq
    uint8_t password_ok;  //在线调试记录密码是否正确
    int *eq_coeff_tab;    //eq在线多模式系数表,[MODE_NUM_MAX][SECTION_MAX * 5];

    void *tws_ci;         //tws收发句柄
    int tws_tmr;          //系数收发timer
    u16 *tws_pack;        //收发的数据包地址
    u8 pack_id;           //收发数据包标识

    EQ_FADE_CFG *fade_cfg;//系数淡入淡出句柄
    int *eq_type_tab;     //eq 默认系数表,计算前seg信息存储地址[EQ_MODE_MAX];
    float *type_gain_tab; //song_eq_mode eq 默认系数表的对应的总增益
    u8 *eq_mode_use_idx;  //10段内抽取部分段数
    eq_tool_cfg *eq_tool_tab;//在线调试工具支持的功能配置结构

    void *phone_eq_tab;//通话下行 eq系数表
    u8 phone_eq_tab_size;//通话下行eq系数表段数
    void *ul_eq_tab;//通话上行 eq系数表
    u8 ul_eq_tab_size;//通话上行eq系数表段数


    void *priv;
    int (*send_cmd)(void *priv, u32 id, u8 *packet, int size);//在线调试，命令应答
    struct vm_save vs;
    u8 custom_mode_id;   //记录自定义eq系数表的数组所在下标号
} EQ_CFG;

typedef struct _eq_parm {
    u8 mode_num;           //模式总数(播歌、通话宽频、窄频上下行模式标号)
    u8 online_en: 1;       //是否支持在线调试
    u8 fade_en: 1;         //是否支持淡入淡出
    u8 file_en: 1;         //是否支持效果文件解析
    u8 stero: 1;           //是否左右声道做不同eq
    u8 drc: 1;             //是否有drc
    u8 tws: 1;             //是否tws
    u8 app: 1;             //是否手机app在线调试
    u8 limit_zero: 1;      //是否将系数限制到0
    u8 type_num: 4;        //默认eq系数表总个数
    u8 section_max;        //eq最大段数
    void *eq_type_tab;     //eq 默认系数表,计算前seg信息存储地址[EQ_MODE_MAX];
    float *type_gain_tab;  //song_eq_mode eq 默认系数表的对应的总增益
    u8 *eq_mode_use_idx;   //10段内抽取部分段数
    eq_tool_cfg *eq_tool_tab;//在线调试工具支持的功能配置结构

    void *phone_eq_tab;    //通话下行 eq系数表
    u8 phone_eq_tab_size;  //通话下行eq系数表段数
    void *ul_eq_tab;       //通话上行 eq系数表
    u8 ul_eq_tab_size;     //通话上行eq系数表段数

} eq_adjust_parm;


/*----------------------------------------------------------------------------*/
/**@brief    设置 eq总增益
   @param   global_gain:总增益
   @return
*/
/*----------------------------------------------------------------------------*/
void set_global_gain(EQ_MODE mode, float global_gain);

/*----------------------------------------------------------------------------*/
/**@brief    获取 eq总增益
   @return
*/
/*----------------------------------------------------------------------------*/
float get_global_gain(EQ_MODE mode);

/*----------------------------------------------------------------------------*/
/**@brief   获取某eq效果模式的增益
   @param   mode:哪个模式
   @param   index:哪一段
   @return  增益
   @note    外部使用
*/
/*----------------------------------------------------------------------------*/
s8 eq_mode_get_gain(EQ_MODE mode, u16 index);
/*----------------------------------------------------------------------------*/
/**@brief   设置用户自定义eq效果模式时的增益
   @param   index:哪一段
   @param   gain:增益
   @return
   @note    外部使用
*/
/*----------------------------------------------------------------------------*/
int eq_mode_set_custom_param(u16 index, int gain);
/*----------------------------------------------------------------------------*/
/**@brief   获取用户自定义eq效果模式时的增益、频率
   @param   index:哪一段
   @param   freq:中心截止频率
   @param   gain:增益
   @return
   @note    外部使用
*/
/*----------------------------------------------------------------------------*/
int eq_mode_set_custom_info(u16 index, int freq, int gain);
/*----------------------------------------------------------------------------*/
/**@brief   用默认eq系数表的eq效果模式设置(设置模式,更新系数)
   @param   mode:EQ_MODE_NORMAL, EQ_MODE_ROCK,EQ_MODE_POP,EQ_MODE_CLASSIC,EQ_MODE_JAZZ,EQ_MODE_COUNTRY, EQ_MODE_CUSTOM
   @return
   @note    外部使用
*/
/*----------------------------------------------------------------------------*/
int eq_mode_set(EQ_MODE mode);

/*----------------------------------------------------------------------------*/
/**@brief   eq模式切换
   @param
   @return
   @note    外部使用
*/
/*----------------------------------------------------------------------------*/
int eq_mode_sw(void);

/*----------------------------------------------------------------------------*/
/**@brief   获取eq效果模式
   @param
   @return
   @note    外部使用
*/
/*----------------------------------------------------------------------------*/
EQ_MODE eq_mode_get_cur(void);



/*----------------------------------------------------------------------------*/
/**@brief   获取某eq效果模式的中心截止频率
   @param   index:哪一段
   @return  中心截止频率
   @note    外部使用
*/
/*----------------------------------------------------------------------------*/
int eq_mode_get_freq(u8 mode, u16 index);



#if 0
/*设置总增益后，需要设置更新*/
void update_global_gain_demo(float global_gain)
{
    set_global_gain(EQ_MODE_CUSTOM, global_gain);//设置总增益，
    eq_mode_set(EQ_MODE_CUSTOM);//设置自定义模式、更新系数以及总增益
}

void update_custom_info_demo2(u16 index, int freq, int gain)
{
    eq_mode_set_custom_info(index, freq, gain);/*改某一段eq的的中心截止频率 以及增益*/
    eq_mode_set(EQ_MODE_CUSTOM);//设置自定义模式、更新系数以及总增益
}

#endif

#endif

