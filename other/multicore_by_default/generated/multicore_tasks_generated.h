// ==== GENERATED FILE DO NOT EDIT ====

#ifndef H_multicore_tasks_GEN
#define H_multicore_tasks_GEN
#include <stdarg.h>
#include "lib/task.h"
#include "multicore_tasks.h"

void _TaskWideSumInit_Exec(void* _data) {
    TaskWideSumInit* data = (TaskWideSumInit*)_data;
    TaskWideSumInit_Exec(data);
}

TaskHandle _TaskWideSumInit_Schedule(TaskQueue* queue, TaskWideSumInit* data, TaskHandle* deps, u8 deps_count) {
    TaskResourceAccess resource_access[1];
    resource_access[0] = TASK_ACCESS_WRITE(data->numbers.items, data->numbers.len);
    return _task_queue_append(queue, _TaskWideSumInit_Exec, data, resource_access, 1, deps, deps_count);
}

TaskHandle TaskWideSumInit_ScheduleV(TaskQueue* queue, TaskWideSumInit* data, u8 dep_count, ...) {
    printf("DEBUG TaskWideSumInit_ScheduleV: dep_count = %d\n", dep_count);
    TaskHandle deps[32];
    va_list args;
    va_start(args, dep_count);
    for (u8 i = 0; i < dep_count; i++) {
        deps[i] = va_arg(args, TaskHandle);
        printf("  dep[%d].h[0] = %u\n", i, deps[i].h[0]);
    }
    va_end(args);
    return _TaskWideSumInit_Schedule(queue, data, deps, dep_count);
}

#define TaskWideSumInit_Schedule(queue,data,...) TaskWideSumInit_ScheduleV(queue, data, _COUNT_ARGS(__VA_ARGS__), ##__VA_ARGS__)

void _TaskWideSumExec_Exec(void* _data) {
    TaskWideSumExec* data = (TaskWideSumExec*)_data;
    TaskWideSumExec_Exec(data);
}

TaskHandle _TaskWideSumExec_Schedule(TaskQueue* queue, TaskWideSumExec* data, TaskHandle* deps, u8 deps_count) {
    TaskResourceAccess resource_access[1];
    resource_access[0] = TASK_ACCESS_WRITE(data->numbers.items, data->numbers.len);
    return _task_queue_append(queue, _TaskWideSumExec_Exec, data, resource_access, 1, deps, deps_count);
}

TaskHandle TaskWideSumExec_ScheduleV(TaskQueue* queue, TaskWideSumExec* data, u8 dep_count, ...) {
    printf("DEBUG TaskWideSumExec_ScheduleV: dep_count = %d\n", dep_count);
    TaskHandle deps[32];
    va_list args;
    va_start(args, dep_count);
    for (u8 i = 0; i < dep_count; i++) {
        deps[i] = va_arg(args, TaskHandle);
        printf("  dep[%d].h[0] = %u\n", i, deps[i].h[0]);
    }
    va_end(args);
    return _TaskWideSumExec_Schedule(queue, data, deps, dep_count);
}

#define TaskWideSumExec_Schedule(queue,data,...) TaskWideSumExec_ScheduleV(queue, data, _COUNT_ARGS(__VA_ARGS__), ##__VA_ARGS__)

#endif
// ==== GENERATED FILE DO NOT EDIT ====

