#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "../types.h"
#include "../error/error_handler.h"

#define SCHEDULER_MAX_TASKS 16
#define SCHEDULER_MIN_INTERVAL_MS 10

typedef enum {
    TASK_PRIORITY_CRITICAL = 0,
    TASK_PRIORITY_HIGH = 1,
    TASK_PRIORITY_MEDIUM = 2,
    TASK_PRIORITY_LOW = 3,
    TASK_PRIORITY_BACKGROUND = 4,
    TASK_PRIORITY_MAX
} TaskPriority_t;

typedef enum {
    TASK_STATE_IDLE = 0,
    TASK_STATE_PENDING = 1,
    TASK_STATE_RUNNING = 2,
    TASK_STATE_BLOCKED = 3,
    TASK_STATE_DISABLED = 4,
    TASK_STATE_MAX
} TaskState_t;

typedef Result_t (*TaskFunction_t)(void* context);
typedef void (*TaskCompleteCallback_t)(u8 task_id, Result_t result, void* context);

typedef struct {
    u8 id;
    const char* name;
    TaskFunction_t function;
    void* context;
    TaskPriority_t priority;
    TaskState_t state;
    u16 interval_ms;
    u32 last_run_ms;
    u32 next_run_ms;
    u16 run_count;
    u16 error_count;
    bool enabled;
    bool one_shot;
} SchedulerTask_t;

typedef struct {
    u32 (*get_timestamp_ms)(void);
    ErrorHandler_t* error_handler;
    TaskCompleteCallback_t complete_callback;
    void* callback_context;
    u16 min_interval_ms;
} SchedulerConfig_t;

typedef struct {
    SchedulerTask_t tasks[SCHEDULER_MAX_TASKS];
    u8 task_count;
    bool running;
    bool initialized;
    u32 (*get_timestamp_ms)(void);
    ErrorHandler_t* error_handler;
    TaskCompleteCallback_t complete_callback;
    void* callback_context;
    u16 min_interval_ms;
    u32 total_runs;
    u32 total_errors;
} Scheduler_t;

Result_t Scheduler_Init(Scheduler_t* sched, const SchedulerConfig_t* config);

Result_t Scheduler_AddTask(Scheduler_t* sched,
                           const char* name,
                           TaskFunction_t function,
                           void* context,
                           TaskPriority_t priority,
                           u16 interval_ms,
                           bool one_shot,
                           u8* task_id);

Result_t Scheduler_RemoveTask(Scheduler_t* sched, u8 task_id);

Result_t Scheduler_EnableTask(Scheduler_t* sched, u8 task_id);

Result_t Scheduler_DisableTask(Scheduler_t* sched, u8 task_id);

Result_t Scheduler_SetInterval(Scheduler_t* sched, u8 task_id, u16 interval_ms);

Result_t Scheduler_SetPriority(Scheduler_t* sched, u8 task_id, TaskPriority_t priority);

Result_t Scheduler_TriggerTask(Scheduler_t* sched, u8 task_id);

Result_t Scheduler_Update(Scheduler_t* sched);

Result_t Scheduler_Start(Scheduler_t* sched);

Result_t Scheduler_Stop(Scheduler_t* sched);

bool Scheduler_IsRunning(const Scheduler_t* sched);

u8 Scheduler_GetTaskCount(const Scheduler_t* sched);

Result_t Scheduler_GetTaskInfo(const Scheduler_t* sched, u8 task_id, SchedulerTask_t* info);

Result_t Scheduler_GetNextTask(const Scheduler_t* sched, u8* task_id, u32* time_until_ms);

const char* Scheduler_GetPriorityString(TaskPriority_t priority);

const char* Scheduler_GetStateString(TaskState_t state);

#endif
