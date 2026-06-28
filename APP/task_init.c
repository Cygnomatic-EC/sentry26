#include "task_init.h"
#include "cmsis_os.h"
#include "robovolley.h"
#include "daemon.h"
osThreadId chassis_taskHandle;
osThreadId gimbal_taskHandle;
osThreadId datasend_taskHandle;
osThreadId daemon_taskHandle;
void task_init()
{
    osThreadDef(ChassisTask, Chassis_Task, osPriorityNormal, 0, 256);
    chassis_taskHandle = osThreadCreate(osThread(ChassisTask), NULL);

    osThreadDef(GimbalTask, Gimbal_Task, osPriorityNormal, 0, 256);
    gimbal_taskHandle = osThreadCreate(osThread(GimbalTask), NULL);

    osThreadDef(DataSendTask, DataSend_Task, osPriorityNormal, 0, 256);
    datasend_taskHandle = osThreadCreate(osThread(DataSendTask), NULL);

    osThreadDef(DaemonTask, Daemon_Task, osPriorityLow, 0, 128);
    daemon_taskHandle = osThreadCreate(osThread(DaemonTask), NULL);

}
