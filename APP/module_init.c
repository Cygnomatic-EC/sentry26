#include "module_init.h"
#include "can.h"
#include "motor/DJI/M3508/m3508.h"
#include "motor/LK/MF9025/mf9025.h"
#include "DBUS/dbus.h"
#include "IMU/INS/ins.h"
#include "Power_Ctrl/power_ctrl.h"
#include "Power_Ctrl/power_ctrl_param_get.h"
#include "usart.h"
#include "NUC/nuc.h"
#include "Referee/referee.h"
#include "COMM/CBoard_chassis.h"
#include "COMM/CBoard_gimbal.h"
#include "dwt/bsp_dwt.h"
#include "IMU/HI12/hi12.h"
#include "motor/DJI/GM6020/gm6020.h"
#include "motor/DJI/M2006/m2006.h"
#include "NX/nx.h"
#include "Super_Cap/super_cap.h"

void Modules_Init(void) {
    DWT_Init(DWT_CLOCK_FREQ);
#ifdef CHASSIS
    dbus_init(RC_CAN, &hcan2);
    initPowerControllerConfig(M3508_TORQUE_CONST, M3508_CURRENT_LIMIT, M3508_OUTPUT_LIMIT,
        K1_CONST,  K2_CONST, K3_CONST, sentinelMaxPower[0]);
    PowerControl_Init();
    mf9025_init(&hcan1, MF9025_TX_MIN);
    m3508_init(&hcan1, M3508_TX_1, 4);
    BMI088_Init();
    super_cap_init(&hcan1);
    NUC_Init(&huart6);
    Referee_Init(&huart1);
    CBoard_Chassis_Init(&hcan2);
#else
    dbus_init(RC_DIRECT, &hcan2);
    CBoard_Gimbal_Init(&hcan2);
    NX_Init(&hcan2);
    HI12_Init(&huart1);
    m3508_init(&hcan1, M3508_TX_2, 2);
    m2006_init(&hcan1, M2006_TX_1, 1);
    gm6020_init(&hcan1, GM6020_TX_2, 2);
#endif
}
