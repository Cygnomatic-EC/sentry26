#include "gimbal.h"

const pid_config yaw_pid_config = {.mode = PID_POSITION, .kp = 1500.0f, .ki = 0.0f, .kd = 100.0f, .max_out = 25000.0f, .max_iout = 3000.0f,
    .out_limit_delta_P = 1000.0f, .out_limit_delta_N = 4000.0f, .deadzone = 0.0f};
const pid_config pitch_pid_config = {.mode = PID_POSITION, .kp = 1500.0f, .ki = 0.0f, .kd = 200.0f, .max_out = 25000.0f, .max_iout = 3000.0f,
    .out_limit_delta_P = 1000.0f, .out_limit_delta_N = 4000.0f, .deadzone = 0.0f};
const pid_config friction_pid_config = {.mode = PID_POSITION, .kp = 8.0f, .ki = 0.0f, .kd = 0.0f, .max_out = 16000.0f, .max_iout = 3000.0f,
    .out_limit_delta_P = 1000.0f, .out_limit_delta_N = 4000.0f, .deadzone = 0.0f};
const pid_config trigger_pid_config = {.mode = PID_POSITION, .kp = 8.0f, .ki = 0.0f, .kd = 0.0f, .max_out = 16000.0f, .max_iout = 3000.0f,
    .out_limit_delta_P = 1000.0f, .out_limit_delta_N = 4000.0f, .deadzone = 0.0f};

gimbal_t gimbal;

// ---------- 内部静态函数声明 ----------
static void gimbal_pid_init(gimbal_t* gimbal_ptr);
static uint8_t gimbal_check_game_start(const gimbal_t* gimbal_ptr) __attribute__((unused));
static void gimbal_switch_controller(gimbal_t* gimbal_ptr);
static void gimbal_calc_target_angle(gimbal_t* gimbal_ptr);
static void gimbal_calc_current_angle(gimbal_t* gimbal_ptr);
static void gimbal_fire_confirm(gimbal_t* gimbal_ptr);
static void gimbal_calc_rotate_pidout(gimbal_t* gimbal_ptr);
static void gimbal_calc_friction_pidout(gimbal_t* gimbal_ptr);
static void gimbal_calc_trigger_pidout(gimbal_t* gimbal_ptr);
static void gimbal_send_friction_cmd(const gimbal_t* gimbal_ptr);
static void gimbal_send_trigger_cmd(const gimbal_t* gimbal_ptr);
static void gimbal_send_rotate_cmd(const gimbal_t* gimbal_ptr);
static void gimbal_send_rc(const gimbal_t* gimbal_ptr);
static void gimbal_send_angle(const gimbal_t* gimbal_ptr);

void Gimbal_Init(gimbal_t* gimbal_ptr)
{
    memset(gimbal_ptr, 0, sizeof(gimbal_t));

    gimbal_ptr->cboard = Get_CBoard_Gimbal_Ptr();
    gimbal_ptr->nx_ctrl = Get_NX_Ctrl_Instance();
    gimbal_ptr->rc = Get_DBUS_Instance();
    gimbal_ptr->hi12 = Get_HI12_Instance();

    gimbal_ptr->angle_motor = Get_GM6020_Ptr(GM6020_TX_2);
    gimbal_ptr->trigger_motor = Get_M2006_Ptr(M2006_TX_1);
    gimbal_ptr->friction_motor = Get_M3508_Ptr(M3508_TX_2);

    gimbal_pid_init(gimbal_ptr);
}

void Gimbal_Task(const void* argument)
{
    Gimbal_Init(&gimbal);
    static uint16_t ctrl_loop = 0;
    while(1)
    {
        osDelay(1);
        //if (!gimbal_check_game_start(&gimbal)) continue;
        gimbal_switch_controller(&gimbal);
        if (ctrl_loop % 3 == 0) // 控制循环 300Hz左右
        {
            gimbal_calc_target_angle(&gimbal);
            gimbal_calc_current_angle(&gimbal);
            gimbal_calc_rotate_pidout(&gimbal);
            gimbal_send_rotate_cmd(&gimbal);
        }
        else if (ctrl_loop % 3 == 1)
        {
            gimbal_calc_friction_pidout(&gimbal);
            gimbal_send_friction_cmd(&gimbal);
        }
        else
        {
            gimbal_fire_confirm(&gimbal);
            gimbal_calc_trigger_pidout(&gimbal);
            gimbal_send_trigger_cmd(&gimbal);
        }

        if (ctrl_loop % 5 == 0) // 通信循环 200Hz左右
            gimbal_send_rc(&gimbal);
        else if (ctrl_loop % 5 == 1)
            gimbal_send_angle(&gimbal);

        ctrl_loop++;
        if (ctrl_loop > 3000) ctrl_loop = 0;
    }
}

static void gimbal_pid_init(gimbal_t* gimbal_ptr)
{
    PID_init(&gimbal_ptr->ctrl.angle_ctrl[0].pid, yaw_pid_config);
    PID_init(&gimbal_ptr->ctrl.angle_ctrl[1].pid, pitch_pid_config);
    PID_init(&gimbal_ptr->ctrl.friction_ctrl[0].pid, friction_pid_config);
    PID_init(&gimbal_ptr->ctrl.friction_ctrl[1].pid, friction_pid_config);
    PID_init(&gimbal_ptr->ctrl.trigger_ctrl.pid, trigger_pid_config);
}

static uint8_t gimbal_check_game_start(const gimbal_t* gimbal_ptr)
{
    return gimbal_ptr->cboard->game_start;
}

static void gimbal_switch_controller(gimbal_t* gimbal_ptr)
{
    gimbal_ptr->ctrl.gimbal_controller = gimbal_ptr->rc->rc_data.s1;
}

static void gimbal_calc_target_angle(gimbal_t* gimbal_ptr)
{
    if (gimbal_ptr->ctrl.gimbal_controller == GIMBAL_RC)
    {
        gimbal_ptr->ctrl.target_yaw -= (fp32)gimbal_ptr->rc->rc_data.ch0 * SMALL_GIMBAL_ANGLE_DELTA_MAX / RC_VAL_MAX;
        gimbal_ptr->ctrl.target_pitch += (fp32)gimbal_ptr->rc->rc_data.ch1 * SMALL_GIMBAL_ANGLE_DELTA_MAX / RC_VAL_MAX;
        gimbal_ptr->ctrl.fire = gimbal_ptr->rc->rc_data.ch3 > (RC_CH_VALUE_MAX - RC_CH_VALUE_OFFSET) / 2 ? 1 : 0;
        gimbal_ptr->ctrl.open_friction = (gimbal_ptr->rc->rc_data.s2 == 2) ? 1 : 0;
    }
    else if (gimbal_ptr->ctrl.gimbal_controller == GIMBAL_UPC)
    {
        gimbal_ptr->ctrl.target_yaw = gimbal_ptr->nx_ctrl->target_yaw;
        gimbal_ptr->ctrl.target_pitch = gimbal_ptr->nx_ctrl->target_pitch;
        gimbal_ptr->ctrl.fire = gimbal_ptr->nx_ctrl->fire;
        gimbal_ptr->ctrl.open_friction = (gimbal_ptr->rc->rc_data.s2 == 2) ? 1 : 0; //测试完改为等于fire
    }
    else
    {
        gimbal_ptr->ctrl.fire = 0;
        gimbal_ptr->ctrl.open_friction = 0;
    }
    if (gimbal_ptr->ctrl.target_yaw > YAW_LIMIT)
        gimbal_ptr->ctrl.target_yaw = YAW_LIMIT;
    else if (gimbal_ptr->ctrl.target_yaw < -YAW_LIMIT)
        gimbal_ptr->ctrl.target_yaw = -YAW_LIMIT;
    gimbal_ptr->ctrl.target_pitch = loop_float_constrain(gimbal_ptr->ctrl.target_pitch, -PITCH_LIMIT, PITCH_LIMIT);
}

static void gimbal_calc_current_angle(gimbal_t* gimbal_ptr)
{
    fp32 ypr[3];
    ypr[0] = gimbal_ptr->hi12->imu_data.yaw;
    ypr[1] = gimbal_ptr->hi12->imu_data.pitch;
    ypr[2] = gimbal_ptr->hi12->imu_data.roll;
    Angle_Update(&gimbal_ptr->angle, ypr);
}

static void gimbal_fire_confirm(gimbal_t* gimbal_ptr)
{
    gimbal_ptr->ctrl.friction_ready = 1;
    if (!(gimbal_ptr->ctrl.open_friction
        && gimbal_ptr->friction_motor->ecd[0].speed > FRICTION_READY_SPEED
        && gimbal_ptr->friction_motor->ecd[1].speed < -FRICTION_READY_SPEED ))
        gimbal_ptr->ctrl.friction_ready = 0;
    // if (!(gimbal_ptr->cboard_gimbal.bullet_allow > 0 && gimbal_ptr->cboard_gimbal.shoot_heat < 140))
    //     gimbal_ptr->ctrl.friction_ready = 0;
}

static void gimbal_calc_rotate_pidout(gimbal_t* gimbal_ptr)
{
    PID_calc(&gimbal_ptr->ctrl.angle_ctrl[0].pid, gimbal_ptr->angle.pitch_deg, gimbal_ptr->ctrl.target_pitch);
    gimbal_ptr->ctrl.angle_ctrl[0].out = gimbal_ptr->ctrl.angle_ctrl[0].pid.out[0];
    PID_calc(&gimbal_ptr->ctrl.angle_ctrl[1].pid, gimbal_ptr->angle.yaw_deg, gimbal_ptr->ctrl.target_yaw);
    gimbal_ptr->ctrl.angle_ctrl[1].out = gimbal_ptr->ctrl.angle_ctrl[1].pid.out[0];
}
static void gimbal_calc_friction_pidout(gimbal_t* gimbal_ptr)
{
    gimbal_ptr->ctrl.friction_ctrl[0].given_speed = (gimbal_ptr->ctrl.open_friction) ? FRICTION_SPEED_MAX : 0;
    gimbal_ptr->ctrl.friction_ctrl[1].given_speed = (gimbal_ptr->ctrl.open_friction) ? -FRICTION_SPEED_MAX : 0;

    PID_calc(&gimbal_ptr->ctrl.friction_ctrl[0].pid, gimbal_ptr->friction_motor->ecd[0].speed, gimbal_ptr->ctrl.friction_ctrl[0].given_speed);
    PID_calc(&gimbal_ptr->ctrl.friction_ctrl[1].pid, gimbal_ptr->friction_motor->ecd[1].speed, gimbal_ptr->ctrl.friction_ctrl[1].given_speed);

    gimbal_ptr->ctrl.friction_ctrl[0].out = gimbal_ptr->ctrl.friction_ctrl[0].pid.out[0];
    gimbal_ptr->ctrl.friction_ctrl[1].out = gimbal_ptr->ctrl.friction_ctrl[1].pid.out[0];
}
static void gimbal_calc_trigger_pidout(gimbal_t* gimbal_ptr)
{
    gimbal_ptr->ctrl.trigger_ctrl.given_speed = (gimbal_ptr->ctrl.fire && gimbal_ptr->ctrl.friction_ready) ? TRIGGER_SPEED_MAX : 0;
    PID_calc(&gimbal_ptr->ctrl.trigger_ctrl.pid, gimbal_ptr->trigger_motor->ecd[0].speed, gimbal_ptr->ctrl.trigger_ctrl.given_speed);
    gimbal_ptr->ctrl.trigger_ctrl.out = gimbal_ptr->ctrl.trigger_ctrl.pid.out[0];
}

static void gimbal_send_friction_cmd(const gimbal_t* gimbal_ptr)
{
    int16_t m3508_iq[4];
    m3508_iq[0] = (int16_t)gimbal_ptr->ctrl.friction_ctrl[0].out;
    m3508_iq[1] = (int16_t)gimbal_ptr->ctrl.friction_ctrl[1].out;
    m3508_iq[2] = 0; m3508_iq[3] = 0;

    m3508_ctrl(gimbal_ptr->friction_motor, m3508_iq);
}
static void gimbal_send_rotate_cmd(const gimbal_t* gimbal_ptr)
{
    int16_t gm6020_v[4];

    gm6020_v[0] = (int16_t)gimbal_ptr->ctrl.angle_ctrl[0].out;
    gm6020_v[1] = (int16_t)gimbal_ptr->ctrl.angle_ctrl[1].out;
    gm6020_v[2] = 0; gm6020_v[3] = 0;

    gm6020_ctrl_voltage(gimbal_ptr->angle_motor, gm6020_v);
}

static void gimbal_send_trigger_cmd(const gimbal_t* gimbal_ptr)
{
    int16_t m2006_iq[4];

    m2006_iq[0] = (int16_t)gimbal_ptr->ctrl.trigger_ctrl.out;
    m2006_iq[1] = 0; m2006_iq[2] = 0; m2006_iq[3] = 0;

    m2006_ctrl(gimbal_ptr->trigger_motor, m2006_iq);
}

static void gimbal_send_rc(const gimbal_t* gimbal_ptr)
{
    RemoteData_UART2CAN();
}
static void gimbal_send_angle(const gimbal_t* gimbal_ptr)
{
    const fp32 gimbal_yaw = gimbal_ptr->hi12->imu_data.yaw;
    CBoard_IMU_Transmit(gimbal_yaw);

    const fp32 quaternion[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    NX_SendAngle(quaternion);
}

