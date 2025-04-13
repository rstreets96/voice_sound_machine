#include "audio_manager.h"

static char *TAG = "audio_manager";

static audio_manager_t mngr_g = 
{
    .task = NULL,
    .check_pos_timer = NULL,
    .pos_timer_period_ms = AUDIO_CHECK_POS_PERIOD_MS,
    .cmd_queue = NULL,
    .cmd_ring_buff = {
        .msg_array = {0},
        .size = AUDIO_MSG_QUEUE_SIZE,
        .head_idx = 0,
    };
    .loop_enabled = true,
}

// Commands for other tasks to interact
//******************************************************************
bool cmd_start_rain(audio_manager_t * manger, uint8_t delay_ms)
{
    audio_msg_t msg;
    if(ring_buff_get_msg(manager, msg, delay_ms, false))
    {
        msg.type = AUDIO_CMD_START_RAIN;
        
    }
}

// Static functions as helpers
//******************************************************************
static bool ring_buff_get_msg(audio_manager_t *mngr, audio_msg_t *msg_ret, uint8_t wait_ms, bool from_isr)
{
    bool open_space;
    if(from_isr)
    {
        open_space = (uxQueueMessagesWaitingFromISR(mngr->cmd_queue) >= mngr->cmd_ring_buff.size);
    }
    else
    {
        open_space = (uxQueueMessagesWaiting(mngr->cmd_queue) >= mngr->cmd_ring_buff.size);
    }
    if(open_space)
    {
        msg_ret = &mngr->cmd_ring_buff.msg_array[mngr->cmd_ring_buff.head_idx];
        mngr->cmd_ring_buff.head_idx += 1;
        mngr->cmd_ring_buff.head_idx %= mngr->cmd_ring_buff.size;
        return true;
    }
    else
    {
        if(from_isr || wait_ms <= 0)
        {
            return false;
        }
        else
        {
            while(wait_ms > 0)
            {
                if(uxQueueMessagesWaiting(mngr->cmd_queue) < mngr->cmd_ring_buff.size)
                {
                    msg_ret = &mngr->cmd_ring_buff.msg_array[mngr->cmd_ring_buff.head_idx];
                    mngr->cmd_ring_buff.head_idx += 1;
                    mngr->cmd_ring_buff.head_idx %= mngr->cmd_ring_buff.size;
                    return true;
                }
                vTaskDelay(pdMS_TO_TICKS(1));
                wait_ms -= 1;
            }
            return false;
        }
    }

}

static void audio_pos_timer_cb(TimerHandle_t timer)
{
    audio_manager_t *manager = (audio_manager_t *)pvTimerGetTimerID(timer);
    audio_msg_t msg;
    if(ring_buff_get_msg(manager, msg, 0, false))
    {
        msg.type = AUDIO_CMD_CHECK_POS;
        if(xQueueSend(manager->cmd_queue, &msg, 0) != pdPASS)
        {
            ESP_LOGE(TAG, "Check Position Timer failed to send msg to queue.");
        }
    }
    else
    {
        ESP_LOGE(TAG, "Check Position Timer failed to get msg from ring buffer.");
    }
}

// Main Task Function
//******************************************************************
static void audio_manager_task(void *parameter)
{
    vTaskSetThreadLocalStoragePointer(NULL, 0, parameter);
    audio_manager_t * audio_manager = (audio_manager_t *)parameter;

    while(true)
    {
        audio_msg_t *msg;
        if(xQueueReceive(audio_manager->cmd_queue, msg, portMAX_DELAY) == pdPASS)
        {
            switch(msg.type)
            {
                case AUDIO_CMD_TIMER_DONE:
                    break;
                case AUDIO_CMD_START_RAIN:
                    break;
                case AUDIO_CMD_EN_LOOP:
                    break;
                case AUDIO_CMD_STOP_SOUNDS:
                    break;
                case AUDIO_CMD_CHECK_POS:
                    break;
            }

        }
    }
}

// Main Initialization Function
//******************************************************************
void audio_manager_task_init(audio_manager_t * manger)
{
    manager = &mngr_g;
    mngr_g.cmd_queue = xQueueCreate(mngr_g.cmd_ring_buff.size, sizeof(audio_msg_t));
    mngr_g.check_pos_timer = xTimerCreate(
        "Audio Position Check Timer", 
        pdMS_TO_TICKS(mngr_g.pos_timer_period_ms),
        pdTRUE,
        (void *)&mngr_g,
        audio_pos_timer_cb,
    );
    xTaskCreate(
        audio_manager_task,
        AUDIO_MANAGER_TASK_STACK_SIZE,
        (void *)&mngr_g,
        AUDIO_MANAGER_TASK_PRIORITY,
        &mngr_g.task
    )
}