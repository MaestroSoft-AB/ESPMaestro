#include "scheduler.h"

#include "unity.h"
#include "unity_test_runner.h"

static int s_callback_count;
static void *s_last_context;
static uint64_t s_last_montime;

uint64_t SystemMonotonicMS(void) { return 0; }

static void counting_callback(void *context, uint64_t montime) {
  s_callback_count++;
  s_last_context = context;
  s_last_montime = montime;
}

TEST_CASE("scheduler_init clears registered tasks", "[scheduler]") {
  TEST_ASSERT_EQUAL_INT(0, scheduler_init());
  TEST_ASSERT_EQUAL_INT(0, scheduler_get_task_count());
}

TEST_CASE("scheduler_create_task registers callback and work dispatches it",
          "[scheduler]") {
  int context = 1234;
  s_callback_count = 0;
  s_last_context = NULL;
  s_last_montime = 0;

  scheduler_init();

  Scheduler_Task *task = scheduler_create_task(&context, counting_callback);

  TEST_ASSERT_NOT_NULL(task);
  TEST_ASSERT_EQUAL_INT(1, scheduler_get_task_count());

  scheduler_work(98765);

  TEST_ASSERT_EQUAL_INT(1, s_callback_count);
  TEST_ASSERT_EQUAL_PTR(&context, s_last_context);
  TEST_ASSERT_EQUAL_UINT64(98765, s_last_montime);

  scheduler_dispose();
}

TEST_CASE("scheduler_destroy_task removes only the selected task",
          "[scheduler]") {
  int first_context = 1;
  int second_context = 2;

  scheduler_init();

  Scheduler_Task *first = scheduler_create_task(&first_context, counting_callback);
  Scheduler_Task *second =
      scheduler_create_task(&second_context, counting_callback);

  TEST_ASSERT_NOT_NULL(first);
  TEST_ASSERT_NOT_NULL(second);
  TEST_ASSERT_EQUAL_INT(2, scheduler_get_task_count());

  scheduler_destroy_task(first);

  TEST_ASSERT_EQUAL_INT(1, scheduler_get_task_count());
  TEST_ASSERT_NULL(first->context);
  TEST_ASSERT_NULL(first->callback);
  TEST_ASSERT_EQUAL_PTR(&second_context, second->context);
  TEST_ASSERT_EQUAL_PTR(counting_callback, second->callback);

  scheduler_dispose();
}

TEST_CASE("scheduler_create_task returns null when capacity is exhausted",
          "[scheduler]") {
  scheduler_init();

  for (int i = 0; i < SCHEDULER_MAX_TASKS; i++) {
    TEST_ASSERT_NOT_NULL(scheduler_create_task((void *)(intptr_t)(i + 1),
                                               counting_callback));
  }

  TEST_ASSERT_EQUAL_INT(SCHEDULER_MAX_TASKS, scheduler_get_task_count());
  TEST_ASSERT_NULL(scheduler_create_task((void *)0xfeed, counting_callback));

  scheduler_dispose();
}
