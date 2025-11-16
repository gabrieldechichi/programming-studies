#ifndef H_MULTICORE_TASK
#define H_MULTICORE_TASK

#include "lib/typedefs.h"
#include "lib/array.h"

arr_define(i64);

#define HZ_TASK()
#define HZ_READ()
#define HZ_WRITE()
// BEGIN USER CODE

HZ_TASK()
typedef struct {
  u64 values_start;

  HZ_WRITE()
  i64_Array numbers;
} TaskWideSumInit;

HZ_TASK()
typedef struct {
  i64 lane_sum;

  HZ_WRITE()
  i64_Array numbers;
} TaskWideSumExec;

void TaskWideSumInit_Exec(TaskWideSumInit *data);
void TaskWideSumExec_Exec(TaskWideSumExec *data);
#endif
