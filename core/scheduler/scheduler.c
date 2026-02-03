#include "scheduler.h"

static const char* const priority_strings[] = {
    [TASK_PRIORITY_CRITICAL] = "Critical",
    [TASK_PRIORITY_HIGH] = "High",
    [TASK_PRIORITY_MEDIUM] = "Medium",
    [TASK_PRIORITY_LOW] = "Low",
    [TASK_PRIORITY_BACKGROUND] = "Background"
};

static const char* const state_strings[] = {
    [TASK_STATE_IDLE] = "Idle",
    [TASK_STATE_PENDING] = "Pending",
    [TASK_STATE_RUNNING] = "Running",
    [TASK_STATE_BLOCKED] = "Blocked",
    [TASK_STATE_DISABLED] = "Disabled"
};

static SchedulerTask_t* find_task(Scheduler_t* sched, u8 task_id)
{
    for (u8 i = 0U; i < sched->task_count; i++) {
        if (sched->tasks[i].id == task_id) {
            return &sched->tasks[i];
        }
    }
    return NULL_PTR;
}

static const SchedulerTask_t* find_task_const(const Scheduler_t* sched, u8 task_id)
{
    for (u8 i = 0U; i < sched->task_count; i++) {
        if (sched->tasks[i].id == task_id) {
            return &sched->tasks[i];
        }
    }
    return NULL_PTR;
}

static u8 get_next_task_id(const Scheduler_t* sched)
{
    u8 max_id = 0U;
    
    for (u8 i = 0U; i < sched->task_count; i++) {
        if (sched->tasks[i].id > max_id) {
            max_id = sched->tasks[i].id;
        }
    }
    
    return max_id + 1U;
}

Result_t Scheduler_Init(Scheduler_t* sched, const SchedulerConfig_t* config)
{
    if (sched == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (config == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (config->get_timestamp_ms == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    sched->task_count = 0U;
    sched->running = false;
    sched->get_timestamp_ms = config->get_timestamp_ms;
    sched->error_handler = config->error_handler;
    sched->complete_callback = config->complete_callback;
    sched->callback_context = config->callback_context;
    sched->total_runs = 0U;
    sched->total_errors = 0U;
    
    if (config->min_interval_ms < SCHEDULER_MIN_INTERVAL_MS) {
        sched->min_interval_ms = SCHEDULER_MIN_INTERVAL_MS;
    } else {
        sched->min_interval_ms = config->min_interval_ms;
    }
    
    for (u8 i = 0U; i < SCHEDULER_MAX_TASKS; i++) {
        sched->tasks[i].id = 0U;
        sched->tasks[i].name = NULL_PTR;
        sched->tasks[i].function = NULL_PTR;
        sched->tasks[i].enabled = false;
        sched->tasks[i].state = TASK_STATE_DISABLED;
    }
    
    sched->initialized = true;
    
    return RESULT_OK;
}

Result_t Scheduler_AddTask(Scheduler_t* sched,
                           const char* name,
                           TaskFunction_t function,
                           void* context,
                           TaskPriority_t priority,
                           u16 interval_ms,
                           bool one_shot,
                           u8* task_id)
{
    if (sched == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (function == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (sched->initialized == false) {
        return RESULT_NOT_READY;
    }
    
    if (sched->task_count >= SCHEDULER_MAX_TASKS) {
        if (sched->error_handler != NULL_PTR) {
            ERROR_REPORT(sched->error_handler, ERR_SCHEDULER_QUEUE_FULL, ERR_SEV_ERROR);
        }
        return RESULT_BUFFER_FULL;
    }
    
    if (priority >= TASK_PRIORITY_MAX) {
        return RESULT_INVALID_PARAM;
    }
    
    u16 actual_interval = interval_ms;
    if ((actual_interval > 0U) && (actual_interval < sched->min_interval_ms)) {
        actual_interval = sched->min_interval_ms;
    }
    
    u8 new_id = get_next_task_id(sched);
    SchedulerTask_t* task = &sched->tasks[sched->task_count];
    
    task->id = new_id;
    task->name = name;
    task->function = function;
    task->context = context;
    task->priority = priority;
    task->state = TASK_STATE_IDLE;
    task->interval_ms = actual_interval;
    task->last_run_ms = 0U;
    task->run_count = 0U;
    task->error_count = 0U;
    task->enabled = true;
    task->one_shot = one_shot;
    
    u32 current_time = sched->get_timestamp_ms();
    task->next_run_ms = current_time + actual_interval;
    
    sched->task_count++;
    
    if (task_id != NULL_PTR) {
        *task_id = new_id;
    }
    
    return RESULT_OK;
}

Result_t Scheduler_RemoveTask(Scheduler_t* sched, u8 task_id)
{
    if (sched == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (sched->initialized == false) {
        return RESULT_NOT_READY;
    }
    
    for (u8 i = 0U; i < sched->task_count; i++) {
        if (sched->tasks[i].id == task_id) {
            for (u8 j = i; j < (sched->task_count - 1U); j++) {
                sched->tasks[j] = sched->tasks[j + 1U];
            }
            
            sched->task_count--;
            return RESULT_OK;
        }
    }
    
    if (sched->error_handler != NULL_PTR) {
        ERROR_REPORT(sched->error_handler, ERR_SCHEDULER_TASK_NOT_FOUND, ERR_SEV_WARNING);
    }
    
    return RESULT_ERROR;
}

Result_t Scheduler_EnableTask(Scheduler_t* sched, u8 task_id)
{
    if (sched == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (sched->initialized == false) {
        return RESULT_NOT_READY;
    }
    
    SchedulerTask_t* task = find_task(sched, task_id);
    
    if (task == NULL_PTR) {
        return RESULT_ERROR;
    }
    
    task->enabled = true;
    task->state = TASK_STATE_IDLE;
    task->next_run_ms = sched->get_timestamp_ms() + task->interval_ms;
    
    return RESULT_OK;
}

Result_t Scheduler_DisableTask(Scheduler_t* sched, u8 task_id)
{
    if (sched == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (sched->initialized == false) {
        return RESULT_NOT_READY;
    }
    
    SchedulerTask_t* task = find_task(sched, task_id);
    
    if (task == NULL_PTR) {
        return RESULT_ERROR;
    }
    
    task->enabled = false;
    task->state = TASK_STATE_DISABLED;
    
    return RESULT_OK;
}

Result_t Scheduler_SetInterval(Scheduler_t* sched, u8 task_id, u16 interval_ms)
{
    if (sched == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (sched->initialized == false) {
        return RESULT_NOT_READY;
    }
    
    SchedulerTask_t* task = find_task(sched, task_id);
    
    if (task == NULL_PTR) {
        return RESULT_ERROR;
    }
    
    u16 actual_interval = interval_ms;
    if ((actual_interval > 0U) && (actual_interval < sched->min_interval_ms)) {
        actual_interval = sched->min_interval_ms;
    }
    
    task->interval_ms = actual_interval;
    
    return RESULT_OK;
}

Result_t Scheduler_SetPriority(Scheduler_t* sched, u8 task_id, TaskPriority_t priority)
{
    if (sched == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (sched->initialized == false) {
        return RESULT_NOT_READY;
    }
    
    if (priority >= TASK_PRIORITY_MAX) {
        return RESULT_INVALID_PARAM;
    }
    
    SchedulerTask_t* task = find_task(sched, task_id);
    
    if (task == NULL_PTR) {
        return RESULT_ERROR;
    }
    
    task->priority = priority;
    
    return RESULT_OK;
}

Result_t Scheduler_TriggerTask(Scheduler_t* sched, u8 task_id)
{
    if (sched == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (sched->initialized == false) {
        return RESULT_NOT_READY;
    }
    
    SchedulerTask_t* task = find_task(sched, task_id);
    
    if (task == NULL_PTR) {
        return RESULT_ERROR;
    }
    
    task->state = TASK_STATE_PENDING;
    task->next_run_ms = sched->get_timestamp_ms();
    
    return RESULT_OK;
}

Result_t Scheduler_Update(Scheduler_t* sched)
{
    if (sched == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (sched->initialized == false) {
        return RESULT_NOT_READY;
    }
    
    if (sched->running == false) {
        return RESULT_OK;
    }
    
    u32 current_time = sched->get_timestamp_ms();
    
    SchedulerTask_t* best_task = NULL_PTR;
    TaskPriority_t best_priority = TASK_PRIORITY_MAX;
    u32 earliest_due = 0xFFFFFFFFU;
    
    for (u8 i = 0U; i < sched->task_count; i++) {
        SchedulerTask_t* task = &sched->tasks[i];
        
        if ((task->enabled == false) || (task->state == TASK_STATE_DISABLED)) {
            continue;
        }
        
        if (task->state == TASK_STATE_RUNNING) {
            continue;
        }
        
        bool is_due = false;
        
        if (current_time >= task->next_run_ms) {
            is_due = true;
        } else if ((task->next_run_ms - current_time) > 0x7FFFFFFFU) {
            is_due = true;
        }
        
        if (is_due == true) {
            if ((task->priority < best_priority) ||
                ((task->priority == best_priority) && (task->next_run_ms < earliest_due))) {
                best_task = task;
                best_priority = task->priority;
                earliest_due = task->next_run_ms;
            }
        }
    }
    
    if (best_task != NULL_PTR) {
        best_task->state = TASK_STATE_RUNNING;
        
        Result_t result = best_task->function(best_task->context);
        
        best_task->last_run_ms = current_time;
        best_task->run_count++;
        sched->total_runs++;
        
        if (result != RESULT_OK) {
            best_task->error_count++;
            sched->total_errors++;
        }
        
        if (sched->complete_callback != NULL_PTR) {
            sched->complete_callback(best_task->id, result, sched->callback_context);
        }
        
        if (best_task->one_shot == true) {
            best_task->enabled = false;
            best_task->state = TASK_STATE_DISABLED;
        } else {
            best_task->state = TASK_STATE_IDLE;
            best_task->next_run_ms = current_time + best_task->interval_ms;
        }
    }
    
    return RESULT_OK;
}

Result_t Scheduler_Start(Scheduler_t* sched)
{
    if (sched == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (sched->initialized == false) {
        return RESULT_NOT_READY;
    }
    
    u32 current_time = sched->get_timestamp_ms();
    
    for (u8 i = 0U; i < sched->task_count; i++) {
        if (sched->tasks[i].enabled == true) {
            sched->tasks[i].state = TASK_STATE_IDLE;
            sched->tasks[i].next_run_ms = current_time + sched->tasks[i].interval_ms;
        }
    }
    
    sched->running = true;
    
    return RESULT_OK;
}

Result_t Scheduler_Stop(Scheduler_t* sched)
{
    if (sched == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (sched->initialized == false) {
        return RESULT_NOT_READY;
    }
    
    sched->running = false;
    
    return RESULT_OK;
}

bool Scheduler_IsRunning(const Scheduler_t* sched)
{
    if (sched == NULL_PTR) {
        return false;
    }
    
    if (sched->initialized == false) {
        return false;
    }
    
    return sched->running;
}

u8 Scheduler_GetTaskCount(const Scheduler_t* sched)
{
    if (sched == NULL_PTR) {
        return 0U;
    }
    
    if (sched->initialized == false) {
        return 0U;
    }
    
    return sched->task_count;
}

Result_t Scheduler_GetTaskInfo(const Scheduler_t* sched, u8 task_id, SchedulerTask_t* info)
{
    if (sched == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (info == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (sched->initialized == false) {
        return RESULT_NOT_READY;
    }
    
    const SchedulerTask_t* task = find_task_const(sched, task_id);
    
    if (task == NULL_PTR) {
        return RESULT_ERROR;
    }
    
    *info = *task;
    
    return RESULT_OK;
}

Result_t Scheduler_GetNextTask(const Scheduler_t* sched, u8* task_id, u32* time_until_ms)
{
    if (sched == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (sched->initialized == false) {
        return RESULT_NOT_READY;
    }
    
    u32 current_time = sched->get_timestamp_ms();
    u32 earliest_time = 0xFFFFFFFFU;
    u8 earliest_id = 0xFFU;
    
    for (u8 i = 0U; i < sched->task_count; i++) {
        const SchedulerTask_t* task = &sched->tasks[i];
        
        if ((task->enabled == false) || (task->state == TASK_STATE_DISABLED)) {
            continue;
        }
        
        if (task->next_run_ms < earliest_time) {
            earliest_time = task->next_run_ms;
            earliest_id = task->id;
        }
    }
    
    if (earliest_id == 0xFFU) {
        return RESULT_NO_DATA;
    }
    
    if (task_id != NULL_PTR) {
        *task_id = earliest_id;
    }
    
    if (time_until_ms != NULL_PTR) {
        if (earliest_time > current_time) {
            *time_until_ms = earliest_time - current_time;
        } else {
            *time_until_ms = 0U;
        }
    }
    
    return RESULT_OK;
}

const char* Scheduler_GetPriorityString(TaskPriority_t priority)
{
    if (priority >= TASK_PRIORITY_MAX) {
        return "Unknown";
    }
    
    return priority_strings[priority];
}

const char* Scheduler_GetStateString(TaskState_t state)
{
    if (state >= TASK_STATE_MAX) {
        return "Unknown";
    }
    
    return state_strings[state];
}
