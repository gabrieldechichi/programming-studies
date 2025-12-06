// ==== GENERATED FILE DO NOT EDIT ====

#ifndef H_multicore_tasks_GEN
#define H_multicore_tasks_GEN
#include <stdarg.h>
#include "lib/multicore_runtime.h"
#include "multicore_tasks.h"

void _TaskWideSumInit_Exec(void *_data)
{
    TaskWideSumInit *data = (TaskWideSumInit *)_data;
    TaskWideSumInit_Exec(data);
}

MCRHandle _TaskWideSumInit_Schedule(MCRQueue *queue, TaskWideSumInit *data, MCRHandle *deps, u8 deps_count)
{
    MCRResourceAccess resource_access[1];
    resource_access[0] = MCR_ACCESS_WRITE(data->numbers.items, data->numbers.len);
    return _mcr_queue_append(queue, _TaskWideSumInit_Exec, data, resource_access, 1, deps, deps_count);
}

#define TaskWideSumInit_Schedule(queue, data, ...) _TaskWideSumInit_Schedule(queue, data, ARGS_ARRAY(MCRHandle, __VA_ARGS__), ARGS_COUNT(MCRHandle, __VA_ARGS__))

void _TaskWideSumExec_Exec(void *_data)
{
    TaskWideSumExec *data = (TaskWideSumExec *)_data;
    TaskWideSumExec_Exec(data);
}

MCRHandle _TaskWideSumExec_Schedule(MCRQueue *queue, TaskWideSumExec *data, MCRHandle *deps, u8 deps_count)
{
    MCRResourceAccess resource_access[1];
    resource_access[0] = MCR_ACCESS_WRITE(data->numbers.items, data->numbers.len);
    return _mcr_queue_append(queue, _TaskWideSumExec_Exec, data, resource_access, 1, deps, deps_count);
}

#define TaskWideSumExec_Schedule(queue, data, ...) _TaskWideSumExec_Schedule(queue, data, ARGS_ARRAY(MCRHandle, __VA_ARGS__), ARGS_COUNT(MCRHandle, __VA_ARGS__))

#endif
// ==== GENERATED FILE DO NOT EDIT ====
