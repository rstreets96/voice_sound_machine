#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#define AUDIO_MSG_QUEUE_SIZE                10
#define AUDIO_CHECK_POS_PERIOD_MS           50      
#define AUDIO_MANAGER_TASK_STACK_SIZE       4096
#define AUDIO_MANAGER_TASK_PRIORITY         2

typedef enum 
{
    AUDIO_CMD_TIMER_DONE,
    AUDIO_CMD_START_RAIN,
    AUDIO_CMD_EN_LOOP,
    AUDIO_CMD_STOP_SOUNDS,
    AUDIO_CMD_CHECK_POS,
}audio_cmd_e;

typedef struct
{
    audio_cmd_e type;

}audio_msg_t;

typedef struct
{
    audio_msg_t msg_array[AUDIO_MSG_QUEUE_SIZE];
    uint8_t size;
    uint8_t head_idx;
}audio_msg_ring_buff_t;

typedef struct
{
    TaskHandle_t task;

    TimerHandle_t check_pos_timer;
    uint8_t pos_timer_period_ms;

    QueueHandle_t cmd_queue;
    audio_msg_ring_buff_t cmd_ring_buff;
    bool loop_enabled;
}audio_manager_t;


void audio_manager_task_init(void);