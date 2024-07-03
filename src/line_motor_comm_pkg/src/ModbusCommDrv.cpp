/*
 * 文件名：ModbusCommDrv.cpp
 * 作者：卢洪磊
 * 日期：2024-6-26
 * 文件描述：该文件产生了一个直线电机类，用于配置直线电机的相关数据，对应驱动器为汇川SV660C
 *          需说明的是PDO的映射没有写在该类里面，需在上位机界面手动实现映射
 * Version：1.0
 * 第一次修改：将汇川驱动器修改为SV660C
 * 第二次修改
 */
#include <iostream>
#include <string.h>
#include "Modbus_Comm.h"

using namespace std;
/*
 * 函数名称：LineMotor
 * 函数描述：直线电机类的构造函数,使用SDO方法建立PDO映射
 * 函数输入：addr - 驱动器在总线上的地址
 * 函数返回：None
 */
LineMotor::LineMotor(uint8_t addr)
{
    //开启CAN通信
    uint8_t open_signal = DRIVER_OpenCanComm();
    if(open_signal == 0)
    {
        ROS_INFO_STREAM(">>开启CAN通信失败!");
        exit(1);
    }

    addr_BUS = addr;

    Sdo_COB_ID = 0x600 + addr_BUS;      // SDO
    TPdo1_COB_ID = 0x180 + addr_BUS;       // 启用TPDO1
    uint8_t Tx_COB_ID_H = (uint8_t)(TPdo1_COB_ID >> 8) & 0x00ff;
    uint8_t Tx_COB_ID_L = (uint8_t)(TPdo1_COB_ID & 0x00ff);

    RPdo1_COB_ID = 0x200 + addr_BUS;       // 启用RPDO1
    uint8_t Rx_COB_ID_H = (uint8_t)(RPdo1_COB_ID >> 8) & 0x00ff;
    uint8_t Rx_COB_ID_L = (uint8_t)(RPdo1_COB_ID & 0x00ff);

    RPdo2_COB_ID = 0x300 + addr_BUS;       // 启用RPDO2
    uint8_t Rx2_COB_ID_H = (uint8_t)(RPdo2_COB_ID >> 8) & 0x00ff;
    uint8_t Rx2_COB_ID_L = (uint8_t)(RPdo2_COB_ID & 0x00ff);

    sendTodriver_msg.SendType = 1;      // 发送类型为单次发送
    sendTodriver_msg.RemoteFlag = 0;    // 远程帧不开启
    sendTodriver_msg.ExternFlag = 0;    // 扩展帧不开启
    sendTodriver_msg.DataLen = 8;       // 数据长度为8个字节
    /*
    * 1. PDO1的Communication Parameters(0x1400)中COB-ID(01)，MSB写1，失能PDO1
    * 2. PDO1的Communication Parameters(0x1400)中transmission type(02)，写1，同步
    * 3. PDO1的Mapping Object(0x1600)中sub-index(0x00)，先写0，清空原先PDO映射
    * 4. PDO1的Mapping Object(0x1600)中sub-index(0x01~0x0n)，建立n个映射
    * 5. PDO1的Mapping Object(0x1600)中sub-index(0x00)，写n，指示映射变量的个数
    */
    // 配置RPDO1，流程如下
    // sub-index = 0x01:目标位置
    // sub-index = 0x01:轮廓速度
    uint8_t temp_data[8][8] = {{0x23,0x00,0x14,0x01,Rx_COB_ID_L,Rx_COB_ID_H,0x00,0x80}, \
                                {0x2F,0x00,0x14,0x02,0x01,0x00,0x00,0x00},\
                                {0x2F,0x00,0x16,0x00,0x00,0x00,0x00,0x00},\
                                {0x23,0x00,0x16,0x01,0x20,0x00,0x7A,0x60},\
                                {0x23,0x00,0x16,0x02,0x20,0x00,0x81,0x60},\
                                {0x2F,0x00,0x16,0x00,0x02,0x00,0x00,0x00},\
                                {0x23,0x00,0x14,0x01,Rx_COB_ID_L,Rx_COB_ID_H,0x00,0x00},\
                                };
    sendTodriver_msg.ID = Sdo_COB_ID;
    for(int i = 0; i < 8; i++)
    {
        array_copy(sendTodriver_msg.Data, temp_data[i], 8);
        VCI_Transmit(VCI_USBCAN2, 0, 0, &sendTodriver_msg, 1);
        usleep(10000);
    }
    // 配置RPDO2，流程如下
    // sub-index = 0x01:控制字
    uint8_t temp_data_5[6][8] = {{0x23,0x00,0x14,0x01,Rx2_COB_ID_L,Rx2_COB_ID_H,0x00,0x80}, \
                                {0x2F,0x00,0x14,0x02,0x01,0x00,0x00,0x00},\
                                {0x2F,0x00,0x16,0x00,0x00,0x00,0x00,0x00},\
                                {0x23,0x00,0x16,0x01,0x2b,0x00,0x40,0x60},\
                                {0x2F,0x00,0x16,0x00,0x01,0x00,0x00,0x00},\
                                {0x23,0x00,0x14,0x01,Rx2_COB_ID_L,Rx2_COB_ID_H,0x00,0x00},\
                                };
    sendTodriver_msg.ID = Sdo_COB_ID;
    for(int i = 0; i < 8; i++)
    {
        array_copy(sendTodriver_msg.Data, temp_data[i], 8);
        VCI_Transmit(VCI_USBCAN2, 0, 0, &sendTodriver_msg, 1);
        usleep(10000);
    }
    // 配置TPDO1
    // sub-index = 0x01:状态字
    // sub-index = 0x02:反馈位置
    uint8_t temp_data_2[7][8] = {{0x23,0x00,0x18,0x01,Tx_COB_ID_L,Tx_COB_ID_H,0x00,0x80}, \
                                {0x2F,0x00,0x18,0x02,0x01,0x00,0x00,0x00},\
                                {0x2F,0x00,0x1A,0x00,0x00,0x00,0x00,0x00},\
                                {0x23,0x00,0x1A,0x01,0x10,0x00,0x41,0x60},\
                                {0x23,0x00,0x1A,0x02,0x20,0x00,0x64,0x60},\
                                {0x2F,0x00,0x1A,0x00,0x02,0x00,0x00,0x00},\
                                {0x23,0x00,0x18,0x01,Tx_COB_ID_L,Tx_COB_ID_H,0x00,0x00},\
                                };
    sendTodriver_msg.ID = Sdo_COB_ID;
    for(int i = 0; i < 7; i++)
    {
        array_copy(sendTodriver_msg.Data, temp_data_2[i], 8);
        VCI_Transmit(VCI_USBCAN2, 0, 0, &sendTodriver_msg, 1);
        usleep(10000);
    }
    // 伺服驱动器启动流程设置
    uint8_t temp_data_3[3][8] = {{0x2b,0x40,0x60,0x00,0x06,0x00,0x00,0x00},\
                                {0x2b,0x40,0x60,0x00,0x07,0x00,0x00,0x00},\
                                {0x2b,0x40,0x60,0x00,0x0f,0x00,0x00,0x00},\
                                };
    sendTodriver_msg.ID = Sdo_COB_ID;
    for(int i = 0; i < 7; i++)
    {
        array_copy(sendTodriver_msg.Data, temp_data_3[i], 8);
        VCI_Transmit(VCI_USBCAN2, 0, 0, &sendTodriver_msg, 1);
        usleep(10000);
    }
    // 设置到轮廓位置模式
    uint8_t temp_data_4[8] = {0x2F,0x60,0x60,0x00,0x01,0x00,0x00,0x00};
    sendTodriver_msg.ID = Sdo_COB_ID;
    array_copy(sendTodriver_msg.Data, temp_data_4, 8);
    VCI_Transmit(VCI_USBCAN2, 0, 0, &sendTodriver_msg, 1);
    usleep(10000);
}
/*
 * 函数名称：DRIVER_OpenCanComm
 * 函数描述：开启PC端与驱动器端的can通信
 * 函数输入：None
 * 函数返回：can通信状态
 *          0 - 使能失败
 *          1 - 使能成功
 */
int8_t LineMotor::DRIVER_OpenCanComm(void)
{
    motor_can_config.AccCode = 0; 
    motor_can_config.AccMask = 0xffffffff;
    motor_can_config.Filter = 1; // 接收所有帧
    motor_can_config.Timing0 = 0x00; // Timing0 = 0x00, Timing1 = 0x1C
    motor_can_config.Timing1 = 0x1C; // 默认500kbps 波特率
    motor_can_config.Mode = 0; // 正常收发模式
    if(VCI_InitCAN(VCI_USBCAN2, 0, 0, &motor_can_config) != 1)
    {
        ROS_ERROR_STREAM(">>CAN initialization failed!");
        return 0;
    }
    if(VCI_StartCAN(VCI_USBCAN2, 0, 0)!=1)
    {
        ROS_ERROR_STREAM(">>Starting CAN failed!");
        return 0;
    }
    return 1;
}
/*
 * 函数名称：AbsPos_Set
 * 函数描述：以绝对式位置控制方式控制电机运动
 * 函数输入：None
 * 函数返回：None
 */
void LineMotor::AbsPos_Set(float absPos)
{
}
/*
 * 函数名称：RelPos_Set
 * 函数描述：以相对位移控制方式控制电机运动
 * 函数输入：relPos - 直线电机伸缩距离
 *         refSpeed - 点对点运动时的参考速度
 * 函数返回：None
 */
void LineMotor::RelPos_Set(float relPos, uint32_t refSpeed)
{
    int32_t drvPos = fPos_to_iPos_Fcn(relPos);
    uint32_t drvSpeed = realSpeed_to_drvSpeed(refSpeed);

    uint8_t drvPos_8 = (drvPos & 0x000000ff);
    uint8_t drvPos_16 = (((uint32_t)drvPos>>8) & 0x000000ff);
    uint8_t drvPos_24 = (((uint32_t)drvPos>>16) & 0x000000ff);
    uint8_t drvPos_32 = (((uint32_t)drvPos>>24) & 0x000000ff);

    uint8_t drvSpeed_8 = (drvSpeed & 0x000000ff);
    uint8_t drvSpeed_16 = (((uint32_t)drvSpeed>>8) & 0x000000ff);
    uint8_t drvSpeed_24 = (((uint32_t)drvSpeed>>16) & 0x000000ff);
    uint8_t drvSpeed_32 = (((uint32_t)drvSpeed>>24) & 0x000000ff);

    uint8_t temp_data_1[8] = {drvPos_32,drvPos_24,drvPos_16,drvPos_8,\
                              drvSpeed_32,drvSpeed_24,drvSpeed_16,drvSpeed_8\
                             };
    uint8_t temp_data_2[2] = {0x1f,0x00};
    // 先设置轮廓速度和目标位置
    sendTodriver_msg.ID = RPdo1_COB_ID;
    sendTodriver_msg.DataLen = 8;
    array_copy(sendTodriver_msg.Data, temp_data_1, 8);
    VCI_Transmit(VCI_USBCAN2, 0, 0, &sendTodriver_msg, 1);
    usleep(1000);
    // 再设置控制字
    sendTodriver_msg.ID = RPdo2_COB_ID;
    sendTodriver_msg.DataLen = 2;
    array_copy(sendTodriver_msg.Data, temp_data_2, 2);
    VCI_Transmit(VCI_USBCAN2, 0, 0, &sendTodriver_msg, 1);
    usleep(1000);
    // 注意事项，发送一个PDO后，需要发送一个同步对象到总线上，待测试
    // uint8_t temp_data_1[8] = {0x00}; 
    // sendTodriver_msg.ID = 0x80;
    // sendTodriver_msg.DataLen = 0x00;
    // array_copy(sendTodriver_msg.Data, temp_data_1, 8);
}
/*
 * 函数名称：fPos_to_iPos_Fcn
 * 函数描述：浮点数位置指令转为供驱动器使用的整型位置指令
 * 函数输入：None
 * 函数返回：返回供驱动器使用的32位整型变量
 */
int32_t LineMotor::fPos_to_iPos_Fcn(float target_position)
{
    float num_of_rotation = target_position / LEAD_SCREW;
    int32_t position_Q32 = (int32_t)(num_of_rotation * MAX_INSTRUCTION_UNIT); 
    if(position_Q32 > MAX_TARGET_POSITION)
    {
        position_Q32 = MAX_TARGET_POSITION;
    }
    else if(position_Q32 < MIN_TARGET_POSITION)
    {
        position_Q32 = MIN_TARGET_POSITION;
    }
    return position_Q32;
}
/*
 * 函数名称：realSpeed_to_drvSpeed
 * 函数描述：将实际的设定速度(r/min)转为驱动器使用的速度指令(指令单位/s)
 * 函数输入：None
 * 函数返回：供驱动器使用的32位无符号整型变量
 */
uint32_t LineMotor::realSpeed_to_drvSpeed(float relSpeed)
{
    uint32_t speed_Q31 = (uint32_t)(relSpeed / 60 * MAX_INSTRUCTION_UNIT);
    if(speed_Q31 > MAX_PROFILE_SPEED)
    {
        speed_Q31 = MAX_PROFILE_SPEED;
    }
    else if(speed_Q31 < MIN_PROFILE_SPEED)
    {
        speed_Q31 = MIN_PROFILE_SPEED;
    }
    return speed_Q31;
}
/*
 * 函数名称：array_copy
 * 函数描述：数组复制
 * 函数输入：send_data - 目标数组
 *          given_data - 给定数组
 *          length - 数组长度
 * 函数返回：void
 */
void LineMotor::array_copy(uint8_t send_data[], uint8_t given_data[], uint8_t length)
{
    for(int i = 0; i < length; i++)
    {
        send_data[i] = given_data[i];
    }
}