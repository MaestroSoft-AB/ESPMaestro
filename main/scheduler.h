/** FORMERLY KNOWN AS SMW
 * Simple task scheduler util */

#ifndef _scheduler_h_
#define _scheduler_h_

#include "time_utils.h"
#include <stdint.h>
#include <string.h>

#ifndef SCHEDULER_MAX_TASKS
#define SCHEDULER_MAX_TASKS 32
#endif

#define MIN_LOOP_MS                                                            \
  10 // Defines how many ms a scheduler task-loop needs to take at a minimum

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  void *context;
  /*replace callback with work function from other module?*/
  void (*callback)(void *_context, uint64_t _montime);

} Scheduler_Task;

typedef struct {
  Scheduler_Task tasks[SCHEDULER_MAX_TASKS];

} Scheduler;

extern Scheduler Global_Scheduler;

int scheduler_init();
void scheduler_task(void *_null_for_now);
Scheduler_Task *scheduler_create_task(void *_context,
                                      void (*_callback)(void *_context,
                                                        uint64_t _montime));
void scheduler_destroy_task(Scheduler_Task *_Task);
void scheduler_work(uint64_t _montime);
int scheduler_get_task_count();
void scheduler_dispose();

#ifdef __cplusplus
}
#endif

#endif
