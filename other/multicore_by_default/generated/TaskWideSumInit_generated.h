void _TaskWideSumInit_Exec(void* _data) {
    TaskWideSumInit* data = (TaskWideSumInit*)_data;
    TaskWideSumInit_Exec(data);
}

TaskHandle _TaskWideSumInit_Schedule(TaskQueue* queue, TaskWideSumInit* data, TaskHandle* deps, u8 deps_count) {
    TaskResourceAccess resource_access[1];
    resource_access[0] = TASK_ACCESS_WRITE(data->numbers.items, data->numbers.len);
    _task_queue_append(queue, _TaskWideSumInit_Exec, data, resource_access, 1, deps, deps_count);
}

#define TaskWideSumInit_Schedule(queue,data,...) _TaskWideSumInit_Schedule(queue,data,ARGS_ARRAY(TaskHandle, __VA_ARGS__), ARGS_COUNT(TaskHandle, __VA_ARGS__))
