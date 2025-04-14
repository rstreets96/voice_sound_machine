#include "esp_audio.h"
#include "esp_log.h"

#include "audio_tone_uri.h"

#include "audio_manager.h"

static char *TAG = "audio_manager";

static audio_manager_t mngr_g = 
{
    .task = NULL,
    .check_pos_timer = NULL,
    .pos_timer_period_ms = AUDIO_CHECK_POS_PERIOD_MS,
    .loop_headroom_ms = AUDIO_LOOP_HEADROOM_MS,
    .loop_enabled = true,
    .cmd_queue = NULL,
    .cmd_ring_buff = {
        .msg_array = {},
        .size = AUDIO_MSG_QUEUE_SIZE,
        .head_idx = 0,
    },
    .player = NULL,
};

static audio_msg_t *ring_buff_get_msg(audio_manager_t *mngr, uint8_t *wait_ms, bool from_isr);

// Commands for other tasks to interact
//******************************************************************
bool cmd_wake_sound(audio_manager_t * manager, uint8_t delay_ms)
{
    ESP_LOGD(TAG, "Sending Wake Sound Command!");
    audio_msg_t *msg = ring_buff_get_msg(manager, &delay_ms, false);
    if(msg != NULL)
    {
        msg->type = AUDIO_CMD_WAKE_SOUND;
        if(xQueueSend(manager->cmd_queue, msg, pdMS_TO_TICKS(delay_ms)) != pdPASS)
        {
            ESP_LOGE(TAG, "Wake Sound CMD: Queue send failed");
            return false;
        }
        return true;
    }
    return false;
}

bool cmd_confirm_sound(audio_manager_t * manager, uint8_t delay_ms)
{
    ESP_LOGD(TAG, "Sending Confirm Sound Command!");
    audio_msg_t *msg = ring_buff_get_msg(manager, &delay_ms, false);
    if(msg != NULL)
    {
        msg->type = AUDIO_CMD_CONFIRM_SOUND;
        if(xQueueSend(manager->cmd_queue, msg, pdMS_TO_TICKS(delay_ms)) != pdPASS)
        {
            ESP_LOGE(TAG, "Confirm Sound CMD: Queue send failed");
            return false;
        }
        return true;
    }
    return false;
}

bool cmd_start_rain(audio_manager_t * manager, uint8_t delay_ms)
{
    ESP_LOGD(TAG, "Sending Rain Command!");
    audio_msg_t *msg = ring_buff_get_msg(manager, &delay_ms, false);
    if(msg != NULL)
    {
        msg->type = AUDIO_CMD_START_RAIN;
        if(xQueueSend(manager->cmd_queue, msg, pdMS_TO_TICKS(delay_ms)) != pdPASS)
        {
            ESP_LOGE(TAG, "Rain Sound CMD: Queue send failed");
            return false;
        }
        return true;
    }
    return false;
}

static void timers_done_cb(TimerHandle_t timer)
{
    audio_manager_t *manager = (audio_manager_t *)pvTimerGetTimerID(timer);
    uint8_t delay_ms = 0;
    audio_msg_t *msg = ring_buff_get_msg(manager, &delay_ms, false);
    if(msg != NULL)
    {
        msg->type = AUDIO_CMD_TIMER_DONE;
        if(xQueueSend(manager->cmd_queue, msg, 0) != pdPASS)
        {
            ESP_LOGE(TAG, "Timer queue send fail");
        }
    }
    else
    {
        ESP_LOGE(TAG, "Timer ring buffer fail");
    }

}

bool start_timer(audio_manager_t * manager, uint32_t timer_val_s, uint8_t queue_delay_ms)
{
    ESP_LOGI(TAG, "Starting timer for %u seconds.", timer_val_s);
    xTimerCreate("New Timer", pdMS_TO_TICKS(timer_val_s * 1000), pdFALSE, (void *)manager, timers_done_cb);  //TODO: Un-magic the number
    return cmd_confirm_sound(manager, queue_delay_ms);
}

// Static functions as helpers
//******************************************************************
static audio_msg_t * ring_buff_get_msg(audio_manager_t *mngr, uint8_t *wait_ms, bool from_isr)
{
    bool open_space;
    if(from_isr)
    {
        open_space = (uxQueueMessagesWaitingFromISR(mngr->cmd_queue) < mngr->cmd_ring_buff.size);
    }
    else
    {
        open_space = (uxQueueMessagesWaiting(mngr->cmd_queue) < mngr->cmd_ring_buff.size);
    }
    if(open_space)
    {
        mngr->cmd_ring_buff.head_idx += 1;
        mngr->cmd_ring_buff.head_idx %= mngr->cmd_ring_buff.size;
        mngr->cmd_ring_buff.msg_array[mngr->cmd_ring_buff.head_idx].type = 0;
        return &mngr->cmd_ring_buff.msg_array[mngr->cmd_ring_buff.head_idx];
    }
    else
    {
        if(from_isr || *wait_ms <= 0)
        {
            return NULL;
        }
        else
        {
            while(*wait_ms > 0)
            {
                if(uxQueueMessagesWaiting(mngr->cmd_queue) < mngr->cmd_ring_buff.size)
                {
                    mngr->cmd_ring_buff.head_idx += 1;
                    mngr->cmd_ring_buff.head_idx %= mngr->cmd_ring_buff.size;
                    mngr->cmd_ring_buff.msg_array[mngr->cmd_ring_buff.head_idx].type = 0;
                    return &mngr->cmd_ring_buff.msg_array[mngr->cmd_ring_buff.head_idx];
                }
                vTaskDelay(pdMS_TO_TICKS(1));
                *wait_ms -= 1;
            }
            return NULL;
        }
    }

}

static void audio_pos_timer_cb(TimerHandle_t timer)
{
    audio_manager_t *manager = (audio_manager_t *)pvTimerGetTimerID(timer);
    uint8_t delay_ms = 0;
    audio_msg_t *msg = ring_buff_get_msg(manager, &delay_ms, false);
    if(msg != NULL)
    {
        msg->type = AUDIO_CMD_CHECK_POS;
        if(xQueueSend(manager->cmd_queue, msg, 0) != pdPASS)
        {
            ESP_LOGE(TAG, "Timer queue send fail");
        }
    }
    else
    {
        ESP_LOGE(TAG, "Timer ring buffer fail");
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
        audio_msg_t msg = {};
        if(xQueueReceive(audio_manager->cmd_queue, &msg, portMAX_DELAY) == pdPASS)
        {
            esp_audio_state_t state;
            ESP_LOGD(TAG, "Received audio command: %u", msg.type);
            switch(msg.type)
            {
                case AUDIO_CMD_TIMER_DONE:
                    esp_audio_state_get(audio_manager->player, &state);
                    if(state.status == AUDIO_STATUS_RUNNING)
                    {
                        esp_audio_stop(audio_manager->player, TERMINATION_TYPE_NOW);
                    }
                    esp_audio_play(audio_manager->player, AUDIO_CODEC_TYPE_DECODER, tone_uri[TONE_TYPE_TIMER], 0);
                    xTimerDelete(msg.timer, pdMS_TO_TICKS(50)); //TODO: Un-magic number
                    xTimerStart(audio_manager->check_pos_timer, pdMS_TO_TICKS(100)); //TODO: Check for loop enabled?
                    break;
                case AUDIO_CMD_START_RAIN:
                    esp_audio_state_get(audio_manager->player, &state);
                    if(state.status == AUDIO_STATUS_RUNNING)
                    {
                        esp_audio_stop(audio_manager->player, TERMINATION_TYPE_NOW);
                    }
                    esp_audio_play(audio_manager->player, AUDIO_CODEC_TYPE_DECODER, tone_uri[TONE_TYPE_RAIN], 0);
                    xTimerStart(audio_manager->check_pos_timer, pdMS_TO_TICKS(100)); //TODO: Check for loop enabled?
                    break;
                case AUDIO_CMD_EN_LOOP:
                    break;
                case AUDIO_CMD_STOP_SOUNDS:
                    break;
                case AUDIO_CMD_CHECK_POS:
                    if(audio_manager->loop_enabled)
                    {
                        int tot_duration_ms = 0;
                        int curr_duration_ms = 0;
                        esp_audio_duration_get(audio_manager->player, &tot_duration_ms);
                        esp_audio_time_get(audio_manager->player, &curr_duration_ms);
                        ESP_LOGI(TAG, "Curr Duration: %u, Total Duration: %u", curr_duration_ms, tot_duration_ms);
                        if(tot_duration_ms - audio_manager->loop_headroom_ms <= curr_duration_ms)
                        {
                            esp_audio_seek(audio_manager->player, 0);
                            ESP_LOGI(TAG, "Looping Audio!");
                        }

                    }
                    break;
                case AUDIO_CMD_WAKE_SOUND:
                    esp_audio_state_get(audio_manager->player, &state);
                    if(state.status == AUDIO_STATUS_RUNNING)
                    {
                        xTimerStop(audio_manager->check_pos_timer, pdMS_TO_TICKS(100));     //TODO: Is there something I should check here?
                        esp_audio_stop(audio_manager->player, TERMINATION_TYPE_NOW);
                    }
                    esp_audio_sync_play(audio_manager->player, tone_uri[TONE_TYPE_DINGDONG], 0);
                    break;
                case AUDIO_CMD_CONFIRM_SOUND:
                    esp_audio_state_get(audio_manager->player, &state);
                    if(state.status == AUDIO_STATUS_RUNNING)
                    {
                        xTimerStop(audio_manager->check_pos_timer, pdMS_TO_TICKS(100));     //TODO: Is there something I should check here?
                        esp_audio_stop(audio_manager->player, TERMINATION_TYPE_NOW);
                    }
                    esp_audio_sync_play(audio_manager->player, tone_uri[TONE_TYPE_DINGDONG], 0);
                    break;
            }

        }
    }
}

// Main Initialization Function
//******************************************************************
audio_manager_t * audio_manager_task_init(esp_audio_handle_t audio_player)
{
    mngr_g.player = audio_player;
    mngr_g.cmd_queue = xQueueCreate(mngr_g.cmd_ring_buff.size, sizeof(audio_msg_t));
    mngr_g.check_pos_timer = xTimerCreate("Audio Position Check Timer", pdMS_TO_TICKS(mngr_g.pos_timer_period_ms), pdTRUE, (void *)&mngr_g, audio_pos_timer_cb);
    xTaskCreate(audio_manager_task, "audio_manager", AUDIO_MANAGER_TASK_STACK_SIZE, (void *)&mngr_g, AUDIO_MANAGER_TASK_PRIORITY, &mngr_g.task);

    return &mngr_g;
}