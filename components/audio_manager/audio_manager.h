#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#define AUDIO_MSG_QUEUE_SIZE                10
#define AUDIO_CHECK_POS_PERIOD_MS           30      
#define AUDIO_MANAGER_TASK_STACK_SIZE       4096
#define AUDIO_MANAGER_TASK_PRIORITY         2
#define AUDIO_LOOP_HEADROOM_MS              200

#define US_PER_MS                            1000

typedef enum 
{
    AUDIO_CMD_TIMER_DONE,
    AUDIO_CMD_START_RAIN,
    AUDIO_CMD_EN_LOOP,
    AUDIO_CMD_STOP_SOUNDS,
    AUDIO_CMD_CHECK_POS,
    AUDIO_CMD_WAKE_SOUND,
    AUDIO_CMD_CONFIRM_SOUND,
}audio_cmd_e;

typedef struct
{
    audio_cmd_e type;
    TimerHandle_t timer;
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
    bool loop_enabled;
    uint32_t loop_headroom_ms;

    QueueHandle_t cmd_queue;
    audio_msg_ring_buff_t cmd_ring_buff;

    esp_audio_handle_t player;
}audio_manager_t;

bool cmd_wake_sound(audio_manager_t * manager, uint8_t delay_ms);
bool cmd_confirm_sound(audio_manager_t * manager, uint8_t delay_ms);
bool cmd_start_rain(audio_manager_t * manger, uint8_t delay_ms);
bool start_timer(audio_manager_t * manager, uint32_t timer_val_s, uint8_t queue_delay_ms);

audio_manager_t * audio_manager_task_init(esp_audio_handle_t audio_player);