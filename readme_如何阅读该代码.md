# 如何阅读该代码

## 0. 这份文档写给谁

假设你刚加入战队，第一次打开这个工程：目录几百个文件、函数名一堆缩写、`Chassis_Task` 里十几行调用看不出在干什么。

这份文档不解释每一行代码，而是回答三个问题：**先看什么、后看什么、每看完一块应该能回答什么问题**。照着走一遍，目标是在两三天内能够独立定位"要改某个功能该动哪个文件"。

## 1. 阅读前的准备

### 1.1 工具与基本动作

- 用 CLion 打开**工程根目录**，等右下角索引转完。之后 `Ctrl+点击` 跳转定义，`Ctrl+Shift+F` 全工程搜索，右键 → Find Usages 查某个函数/变量被谁用了。
- 嵌入式项目里最常用的动作就两个：**从调用处跳到定义**、**从定义看被谁调用**。迷路时先问自己"这个变量是谁写进去的？"
- **先读 `.h`，再读 `.c`**。头文件写的是"这个模块对外提供什么"（结构体、宏、函数声明），源文件写的是"怎么实现"。一个模块看不懂时，先把它的头文件读明白，通常就掌握了八成。
- 遇到不认识的宏/缩写，先在 `BSP/typedef.h`、各模块 `.h` 顶部找，绝大多数都在那里定义。

### 1.2 三个前置概念

如果下面三个词你只是听过，先花十分钟补一下，否则后面会一直卡壳：

| 概念 | 一句话解释 | 在本工程里的表现 |
| ------- | ------- | ------- |
| **HAL 库** | ST 官方对寄存器的封装，本身不含业务逻辑 | 所有 `MX_XXX_Init()` 都是 CubeMX 生成的 HAL 初始化代码 |
| **FreeRTOS 任务** | 一个任务就是一个死循环函数，由操作系统调度 | 所有 `XXX_Task()` 都是任务函数，它们看起来永不返回 |
| **中断与回调** | 硬件收到数据会打断 CPU，先执行中断服务函数 | 本工程不在中断里写业务逻辑，而是注册回调，由 BSP 层在中断里调用 |

## 2. 先建立全局印象：三层架构在分什么

一句话概括：**BSP 管"一个字节怎么收发"，Module 管"这串字节是什么意思"，APP 管"机器人该做什么动作"。**

| 层级 | 目录 | 回答的问题 | 典型文件 |
| ------- | ------- | ------- | ------- |
| APP | `APP/` | 机器人怎么动？ | `chassis.c`、`gimbal.c` |
| Module | `Module/` | 这个设备的数据是什么意思？ | `motor/DJI/M3508/m3508.c`、`DBUS/dbus.c` |
| BSP | `BSP/` | 数据怎么从硬件进来、怎么出去？ | `can/bsp_can.c`、`usart/bsp_uart.c` |
| HAL | `Drivers/` | 寄存器怎么配？ | ST 官方代码，**不需要读，也不要改** |

判断一个文件属于哪一层，问自己：**"换一个电机型号，哪些文件要改？"**
只改 `Module/motor/` 下对应文件的是设备细节；改 `APP/` 的是策略；只有换外设（比如 CAN 换成串口）才会动 `BSP/`。

### 2.1 目录地图

| 路径 | 内容 | 你要不要细读 |
| ------- | ------- | ------- |
| `Core/Src/main.c` | 启动流程、外设初始化 | **必读**，先读 |
| `Core/Src/freertos.c` | FreeRTOS 启动，调用 `task_init()` | **必读** |
| `Core/Src/stm32f4xx_it.c` | 所有中断服务函数 | 用到再看 |
| `APP/` | 业务逻辑 | **必读**，主战场 |
| `Module/` | 各类设备驱动与协议 | 按需精读 1~2 个 |
| `BSP/` | 外设封装 + 通用算法 | 先读 `can`、`usart` 两个 |
| `Drivers/`、`Middlewares/` | ST 官方 HAL、FreeRTOS 源码 | **不要读** |
| `cmake/`、`CMakeLists.txt` | 构建配置 | 改编译选项时看 |
| `MDK-ARM/` | Keil 工程 | 用 Keil 时才看 |
| `standard_robot_c.ioc` | CubeMX 工程文件 | 改外设引脚时用 |

### 2.2 模块清单（`Module/` 下都有什么）

看 `Module/` 的时候不用全读，先知道每个模块负责什么：

| 模块 | 职责 | 通信方式 |
| ------- | ------- | ------- |
| `motor/DJI/M3508` | 底盘轮毂电机、摩擦轮 | CAN |
| `motor/DJI/M2006` | 拨弹盘电机 | CAN |
| `motor/DJI/GM6020` | 云台 Yaw/Pitch 电机 | CAN |
| `motor/LK/MF9025` | 云台电机 | CAN |
| `motor/DAMIAO/DM8009P`、`motor/RS/rs02` | 其他型号电机 | CAN |
| `DBUS/dbus.c` | 遥控器数据解析 | UART3 直连 或 CAN 转发 |
| `COMM/comm.c` | 已废弃 | UART |
| `COMM/CBoard_chassis.c`、`CBoard_gimbal.c` | 底盘板 / 云台板之间的通信 | CAN |
| `NUC/nuc.c` | 导航上位机通信 | UART6 |
| `NX/nx.c` | 自瞄上位机通信 | CAN |
| `Referee/referee.c` | 裁判系统官方协议 | UART1 |
| `IMU/BMI088` | 板载 IMU 原始数据 | SPI |
| `IMU/INS` | 姿态解算（四元数 → Roll/Pitch/Yaw） | 纯计算 |
| `IMU/EKF` | 四元数卡尔曼滤波 | 纯计算 |
| `IMU/HI12` | 外置 IMU 模块 | UART |
| `Power_Ctrl` | 功率控制与分配 | 纯计算 |
| `Super_Cap` | 超级电容 | CAN |
| `LED` | RGB 灯 | PWM |
| `VOFA` | 上位机波形调试 | UART |
| `Servo` | 舵机（预留空文件） | — |

## 3. 程序是怎么跑起来的：启动流程

按下面的顺序读，每一步都打开对应文件对着看。

**第 1 步：`Core/Src/main.c` 的 `main()`**

```c
HAL_Init();               // HAL 库初始化，配置时基
SystemClock_Config();     // 系统时钟 168MHz
MX_GPIO_Init();           // ↓ 以下都是 CubeMX 生成的
MX_DMA_Init();
MX_CAN1_Init(); MX_CAN2_Init();
MX_USART1_UART_Init(); MX_USART3_UART_Init(); MX_USART6_UART_Init();
...
Modules_Init();           // ← 注意：这行是自己写的，不是 CubeMX 生成的
MX_FREERTOS_Init();       // 创建任务
osKernelStart();          // 启动调度器，从此再也不会返回 main
```

这里要分清两件事：
- `MX_XXX_Init()` 是 CubeMX 生成的，**只负责把外设寄存器配好**（波特率、引脚、DMA），它不知道你接的是什么设备。
- `Modules_Init()`（在 `APP/module_init.c`）是**自己写的**，它才负责"把设备和外设绑定起来"。

**第 2 步：`APP/module_init.c` —— 全工程的"接线图"**

```c
void Modules_Init(void) {
    DWT_Init(DWT_CLOCK_FREQ);
#ifdef CHASSIS
    dbus_init(RC_CAN, &hcan2);        // 遥控器走 CAN2 转发
    m3508_init(&hcan1, M3508_TX_1, 4);// 4 个 M3508 挂在 CAN1
    BMI088_Init();
    NUC_Init(&huart6);                // 上位机接在 USART6
    Referee_Init(&huart1);            // 裁判系统接在 USART1
    ...
#else
    ...                               // 云台分支，另一套设备
#endif
}
```

**这个文件是最值得先读的一个文件**：它一次性交代了"哪个设备挂在哪条总线、用哪个 ID、有几个"。读 `chassis.c` 时看到 `Get_M3508_Ptr(M3508_TX_1)` 一头雾水，回来这里就知道它的出处了。

**第 3 步：`Core/Src/freertos.c` → `task_init()`**

`MX_FREERTOS_Init()` 里调用 `task_init()`（`APP/task_init.c`），创建任务：

```c
osThreadDef(ChassisTask, Chassis_Task, osPriorityNormal, 0, 256);
chassis_taskHandle = osThreadCreate(osThread(ChassisTask), NULL);
```

注意这里的 `#ifdef CHASSIS`：**底盘和云台是两套代码，编译期二选一**，所以你要先确认当前编译的是哪一套（见第 8 节第 1 条）。

**第 4 步：`APP/chassis.c` 的 `Chassis_Task()`** —— 从这里开始就是真正的业务逻辑了。

## 4. 数据是怎么流动的：两条主线

任何嵌入式项目都可以拆成"下行控制"和"上行反馈"两条线，本工程也不例外。

### 4.1 下行：从决策到电流

以底盘轮毂电机为例，每一层只做自己那一层的事：

```
Chassis_Task()                    任务循环，1ms 一次
  └─ chassis_switch_controller()  决定听谁的：遥控器 / 上位机
  └─ chassis_calc_move_speed()    把摇杆值换算成底盘速度
  └─ chassis_calc_wheelmotor_speed()  麦轮/全向轮解算 → 每个轮子该转多快
  └─ chassis_calc_wheelmotor_pidout() PID 计算 → 每个轮子该给多少电流
  └─ chassis_send_wheelmotor_cmd()
       └─ m3508_ctrl()            把电流值打包成 8 字节 CAN 报文
            └─ BSP_CAN_Transmit() 往 CAN 邮箱里塞数据
                 └─ HAL_CAN_AddTxMessage()   HAL 层，写寄存器
```

**读 APP 层代码的技巧**：`chassis.c` 里全是 `static` 函数，看起来很长，但 `Chassis_Task()` 就是一份目录。按它调用的顺序读，每个函数只做一件事。

### 4.2 上行：从中断到数据

以 M3508 上报的转速为例，方向完全相反：

```
硬件收到 CAN 报文
  └─ HAL_CAN_RxFifo0MsgPendingCallback()   中断触发（HAL 提供）
       └─ BSP_CAN_Rx_IRQHandler()          从 FIFO 读出报文
            └─ 按 ID 查表找到注册的回调
                 └─ get_m3508_measure()    解析字节 → 直接写进 m3508[i].ecd[j]
                      ↓
              下一次 Chassis_Task 循环里直接读 m3508->ecd[i].speed
```

关键点：**回调在中断里执行，直接把数据写进模块自己的结构体**。所以 APP 层从来不用"主动去问电机现在转速多少"，直接读结构体就行。

## 5. 六个反复出现的代码套路

看懂这六个套路，再看任何模块都能对上号。

### 套路 1：句柄结构体（instance）

每个模块都有一个 `xxx_instance` 结构体，里面装着"这个设备的全部状态"：

```c
typedef struct {
    CAN_Instance_t *can_ins;  // 挂在哪个 BSP 实例上
    uint8_t num;              // 有几个电机
    uint32_t txid;            // 发送用的 CAN ID
    m3508_ecd_t ecd[4];       // 收到的反馈数据
    uint8_t init;             // 是否已初始化
} m3508_instance;
```

看到 `init` 成员就明白：这个模块的每个函数开头都会检查它，没初始化就直接返回错误。这是本工程的统一约定。

### 套路 2：单例 + Getter

模块内部维护一个 `static` 实例，对外只暴露一个 `Get_XXX_Ptr()`：

```c
static m3508_instance m3508[2];              // 文件内私有

m3508_instance *Get_M3508_Ptr(uint16_t txid) // 对外唯一入口
{
    if (txid == M3508_TX_1) return &m3508[0];
    if (txid == M3508_TX_2) return &m3508[1];
    return NULL;
}
```

APP 层在 `Chassis_Init()` 里把这些指针一次性挂到自己的总结构体上：

```c
chassis_ptr->m3508 = Get_M3508_Ptr(M3508_TX_1);
chassis_ptr->rc    = Get_DBUS_Instance();
chassis_ptr->referee = Get_Referee_Instance();
```

**为什么要这样设计**：模块之间不需要层层传参，谁想用谁去拿；同一个设备被多个任务使用（比如遥控器既给底盘又给云台），拿到的也是同一个实例，数据天然共享。这也是本框架相对旧版本的核心改动（见 `readme.md` 末尾「接下来要做什么」第一条）。

### 套路 3：回调 + `void* arg`

这是 C 语言里模拟"继承"的关键手法。注册回调时，除了传函数指针，还把"数据该写到哪"的地址一起传进去：

```c
BSP_CAN_RegisterStdCallback(m3508[index].can_ins, M3508_RX_1 + i,
                            get_m3508_measure, &m3508[index].ecd[i]);
```

中断里这样调用：

```c
can_ins->rx_callbacks[idpos](rx_data, rxid, arg);   // arg 就是上面传的地址
```

所以 `get_m3508_measure` 不需要知道自己是给哪个电机服务的，拿到 `arg` 直接往里写：

```c
void get_m3508_measure(const uint8_t* rx_data, uint32_t id, void* arg)
{
    m3508_ecd_t *ecd = (m3508_ecd_t*)arg;   // 还原成自己的结构体
    ecd->ecd = (uint16_t)(rx_data[0] << 8 | rx_data[1]);
    ...
}
```

这就是概述里说的 Module "继承自 BSP 类"：**持有 BSP 实例指针 + 注册回调 + 用 `arg` 携带自己的 `this` 指针**。

> 读 Module 层代码的通用方法：**先找 `xxx_init()` 里注册了哪些回调，再顺着回调函数体往下读**，数据流就通了。

### 套路 4：BSP 层的收发机制

| 层 | 发送 | 接收 |
| ------- | ------- | ------- |
| CAN | `BSP_CAN_Transmit()` 等邮箱空闲 → 塞进邮箱 | HAL 回调 → `BSP_CAN_Rx_IRQHandler()` → 查 ID 表 → 调回调 |
| UART | `BSP_UART_Transmit_To_Mail()` 投递到邮箱 → 独立任务 `UART_TxTask` 用 DMA 发出 | DMA 空闲中断 → 调回调 |

两个细节值得注意：
- **CAN 的过滤器被配成"全通"**（掩码全 0），所有报文都会进 FIFO0，然后在软件里用一张 ID 表分发。所以 `bsp_can.h` 里的 `MAX_CAN_FILTERS` 其实是"回调槽位数量"，不是硬件过滤器。
- **UART 发送是异步的**：调用发送函数只是把数据丢进邮箱，真正的 DMA 发送由 `UART_TxTask` 完成，所以不会阻塞控制循环。

### 套路 5：APP 层的"总结构体 + 静态函数"

`chassis_t` / `gimbal_t` 是全局唯一的"数据中心"，里面分两块：

```c
typedef struct {
    ctrl_data_t ctrl;          // 所有计算中间量：给定量、PID、滤波器
    rc_instance *rc;           // 外部模块的指针（通过 Getter 拿）
    m3508_instance *m3508;
    ...
} chassis_t;
```

`chassis.c` 里的函数一律是 `static`，只对外暴露 `Chassis_Task()`。这是一种"文件即类"的写法：**结构体是成员变量，static 函数是私有方法，头文件里声明的两个函数是公开接口**。

### 套路 6：控制源切换用枚举

```c
typedef enum {
    CHASSIS_RC_OFFLINE = 0,  // 遥控器掉线
    CHASSIS_RC = 1,          // 遥控器控制
    CHASSIS_UPC = 2,         // 上位机控制
    CHASSIS_SHUTDOWN = 3     // 关机
} Chassis_Controller;
```

`chassis_switch_controller()` 每个循环都会跑，根据这个状态决定"当前听谁的命令"。**调试时发现车不动，先看这个状态是几**。

## 6. RTOS 视角：任务、周期与中断

### 6.1 任务一览

| 任务 | 文件 | 优先级 | 干什么 |
| ------- | ------- | ------- | ------- |
| `Chassis_Task` / `Gimbal_Task` | `APP/chassis.c`、`APP/gimbal.c` | Normal | 底盘/云台控制，**二选一编译** |
| `Daemon_Task` | `APP/daemon.c` | Low | LED 呼吸灯，指示系统还活着 |
| `defaultTask` | `Core/Src/freertos.c` | Normal | CubeMX 生成，初始化 USB |
| `UART_TxTask` | `BSP/usart/bsp_uart.c` | — | 每个已初始化串口的异步发送 |

### 6.2 时间：1ms 节拍与分时复用

`FreeRTOSConfig.h` 里 `configTICK_RATE_HZ = 1000`，所以 **1 个 tick = 1ms**。

控制循环的写法是：

```c
while(1)
{
    osDelay(1);                    // 固定 1ms 节拍
    if (ctrl_loop % 3 == 0) {      // 第 0、3、6… 帧 → 底盘
        ...
    } else if (ctrl_loop % 3 == 1) { // 第 1、4、7… 帧 → 云台
        ...
    } else {                        // 第 2、5、8… 帧 → 功率
        ...
    }
    ctrl_loop++;
}
```

**这不是三个任务，而是一个任务在三个帧里轮流做三件事**，每件事的等效周期是 3ms（约 300Hz）。目的是把 CAN 发送错开，减少总线拥塞。

> 这是当前框架的一个遗留问题（详见 `readme.md` 末尾「接下来要做什么」第一条），后续会把它们拆成独立的 FreeRTOS 任务。读的时候不要误以为它们在同一帧里执行——**它们之间的数据传递依赖上一帧的结果，调试时序问题时这一点很关键**。

### 6.3 中断上下文 vs 任务上下文

| | 中断上下文（ISR） | 任务上下文 |
| ------- | ------- | ------- |
| 谁在跑 | `HAL_CAN_RxFifo0MsgPendingCallback()`、`USARTx_IRQHandler()` 及其回调 | `Chassis_Task()`、`Gimbal_Task()` |
| 能做什么 | 解析报文、赋值给结构体 | 控制解算、PID、发 CAN |
| 注意 | 不能阻塞、不能调 `osDelay` | 读写 ISR 写过的变量时要注意原子性 |

本工程里 UART 的中断服务函数放在 `Core/Src/stm32f4xx_it.c`，但它只调用 `BSP_UART_IRQHandler(get_uart_ins_map(n))` 就转交给 BSP 层了——**CubeMX 重新生成代码时不会把 BSP 里的代码删掉**，这是个很实用的规避技巧。

## 7. 建议的阅读路线（分四步，每步带自检问题）

### 第一步：搞清程序怎么跑起来

读：`Core/Src/main.c` → `Core/Src/freertos.c` → `APP/task_init.c` → `APP/module_init.c` → `APP/daemon.c`

读完后你应该能回答：
- [ ] 程序从复位到进入第一个任务循环，依次经过了哪些函数？
- [ ] 当前编译的是底盘分支还是云台分支？依据是什么宏？
- [ ] 遥控器、裁判系统、M3508 分别接在哪条总线上？
- [ ] 一共有几个任务？各自优先级是多少？

### 第二步：吃透一个 BSP 和一个 Module

读：`BSP/can/bsp_can.h` → `BSP/can/bsp_can.c` → `Module/motor/DJI/M3508/m3508.h` → `m3508.c`

读完后你应该能回答：
- [ ] 一条 CAN 报文从进中断到被写入 `m3508[i].ecd[j]`，经过哪几个函数？
- [ ] 如果我要加一个新型号电机，需要照着 `m3508.c` 写哪些函数？
- [ ] `void* arg` 在这条链路里起什么作用？
- [ ] 为什么 `BSP_CAN_Transmit()` 里要 `while` 等邮箱空闲？

### 第三步：通读一个 APP 模块

读：`APP/chassis.h` → `APP/chassis.c`（或 `gimbal`，取决于当前分支）

方法：**先只看 `Chassis_Task()`，把里面每个函数的名字和注释抄成一份清单，再逐个展开**。不要一上来从第 1 行顺序读到底。

读完后你应该能回答：
- [ ] 摇杆往前推 → 轮子转起来，中间经过了哪些换算？
- [ ] 遥控器掉线时会发生什么？在哪一行判断的？
- [ ] PID 参数定义在哪个文件？改参数要动哪里？
- [ ] 小陀螺（`chassis_spin`）模式下，代码走的分支和平常有什么不同？

### 第四步：按需深入

到这一步你已经能自己找路了。剩下按需求查：
- 裁判系统 → `Module/Referee/referee.h`（协议结构体很全，注释也清楚）
- 姿态解算 → `Module/IMU/INS/ins.c`、`Module/IMU/EKF/`
- 功率控制 → `Module/Power_Ctrl/power_ctrl.h`
- 通用算法 → `BSP/algorithm/`（PID、CRC、RLS、滤波器）

## 8. 术语表

| 缩写 | 全称 / 含义 | 备注 |
| ------- | ------- | ------- |
| BSP | Board Support Package | 硬件支持包，最底层封装 |
| HAL | Hardware Abstraction Layer | ST 官方库 |
| DWT | Data Watchpoint and Trace | 用于高精度计时（`bsp_dwt.c`） |
| INS | Inertial Navigation System | 惯性导航，姿态解算 |
| EKF | Extended Kalman Filter | 扩展卡尔曼滤波 |
| IMU | Inertial Measurement Unit | 惯性测量单元 |
| DBUS | DJI 遥控器接收机协议 | 18 字节一帧 |
| NUC | Next Unit of Computing | 上位机（跑导航的小电脑） |
| NX | Jetson orin nx | 上位机（跑自瞄的小电脑） |
| CBoard | 大疆C型开发板 | 底盘/云台的控制板 |
| VOFA | VOFA+ 上位机软件 | 波形调试工具 |
| RLS | Recursive Least Squares | 递推最小二乘 |
| CRC | Cyclic Redundancy Check | 校验 |
| PID | 比例-积分-微分控制器 | `BSP/algorithm/pid.c` |
| 云台 Yaw / Pitch | 偏航 / 俯仰 | 两个旋转轴 |
| 小陀螺 | 底盘自旋 | `chassis_spin` |
| 摩擦轮 / 拨弹盘 | 发射机构部件 | 云台分支 |

## 9. 想改某处时，该动哪个文件

| 想做的事 | 要改的文件 |
| ------- | ------- |
| 调 PID 参数 | `APP/chassis.c` 或 `APP/gimbal.c` 顶部的 `const pid_config` |
| 换电机型号 | `Module/motor/<型号>/` 新增驱动 + `APP/module_init.c` 初始化 + APP 层换 Getter |
| 改通信协议 | `Module/NX/nx.h`/`Module/NUC/nuc.h`/`Module/COMM/CBoard_chassis.h`/`Module/COMM/CBoard_gimbal.h` 的枚举/结构体 + `nx.c`/`nuc.c`/`CBoard_chassis.c`/`CBoard_gimbal.c` |
| 加一个新设备 | ① 仿照现有 Module 写驱动 ② 在 `module_init.c` 里初始化 ③ 在 APP 层 `Get_XXX_Ptr()` 挂指针 |
| 改控制周期 | `APP/chassis.h` 的 `CHASSIS_CONTROL_TIME`，同时检查 PID 和滤波器参数 |
| 改引脚 / 波特率 | `standard_robot_c.ioc`（CubeMX）重新生成，或直接改 `Core/Src/` 下对应文件 |
| 加一个任务 | `APP/task_init.c` 里 `osThreadDef` + `osThreadCreate` |

## 10. 新手容易踩的坑

1. **底盘 / 云台是两套代码**。`CHASSIS` 和 `GIMBAL` 宏二选一，决定 `task_init.c`、`module_init.c` 里哪套代码被编译。注意 Keil 走 `APP/robot_config.h`（当前定义了 `CHASSIS`），而 CMake 走 `CMakeLists.txt`（当前 `CHASSIS` 被注释，实际编译的是**云台分支**）。换兵种时两个文件要同步改，否则会出现"我明明改了代码怎么没生效"。

2. **`.c` 里找不到 `main` 的直接调用链很正常**。任务函数是 `osThreadCreate()` 注册进去的，不会有人"调用"它，得从 `task_init.c` 反查。

3. **改数据的地方可能不在你看的文件里**。`m3508.ecd[]` 是中断里的回调写的，不是 `m3508.c` 里某个函数主动更新的。找不到某变量在哪赋值时，用 Find Usages 全局搜。

4. **模块指针是全局单例**，任何地方 `Get_XXX_Ptr()` 拿到的都是同一个对象。所以在 A 任务里改了，B 任务立刻能看到——这既是方便，也是坑（多任务同时写会出问题）。

5. **结构体里带 `#pragma pack` 的是通信协议**（如 `referee.h`、`hi12.h`），字节对齐必须和对方一致，不要随手加/删字段或调顺序。

6. **`CMakeLists.txt` 里的路径大小写和实际目录不完全一致**（例如写的是 `Module/motor/dji/m3508/m3508.c`，实际目录是 `Module/motor/DJI/M3508/`）。Windows 上不区分大小写所以能编过，**换到 Linux 或 CI 上会直接编译失败**。如果要上 Linux 构建，记得把路径改对。

7. **调参前先确认控制源状态**。车不动的原因经常是 `chassis_controller` 停在 `CHASSIS_RC_OFFLINE`，而不是 PID 调坏了。
