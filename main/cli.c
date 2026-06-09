#include "cli.h"

#include "esp_chip_info.h"
#include "esp_err.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_psram.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

#define CLI_MAX_LINE 128
#define CLI_MAX_ARGS 8

typedef int (*cli_handler)(int argc, char **argv);

typedef struct {
  const char *name;
  const char *desc;
  cli_handler handler;
} cli_command;

static int cmd_help(int argc, char **argv);
static int cmd_status(int argc, char **argv);
static int cmd_reboot(int argc, char **argv);

static const cli_command commands[] = {
    {"help", "Show commands", cmd_help},
    {"status", "Show device status", cmd_status},
    {"reboot", "Restart device", cmd_reboot},
};

static int cmd_help(int argc, char **argv) {
  (void)argc;
  (void)argv;

  printf("Commands:\n");

  for (int i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
    printf("\t%-10s %s\n", commands[i].name, commands[i].desc);
  }

  return 0;
}

static int cmd_status(int argc, char **argv) {
  (void)argc;
  (void)argv;

  esp_chip_info_t chip;
  esp_chip_info(&chip);

  uint32_t flash_size = 0;
  esp_flash_get_size(NULL, &flash_size);

  size_t internal_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  size_t internal_min = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);

  size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  size_t psram_min = heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM);
  size_t psram_total = esp_psram_get_size();

  size_t total_free = esp_get_free_heap_size();
  size_t total_min = esp_get_minimum_free_heap_size();

  int64_t uptime_s = esp_timer_get_time() / 1000000;

  printf("\nSystem info:\n");
  printf("  Chip: ESP32-S3, cores: %d, revision: %d\n", chip.cores,
         chip.revision);

  printf("  Flash: %lu MB\n", flash_size / (1024 * 1024));

  printf("  Uptime: %lld seconds\n", uptime_s);

  printf("\nHeap:\n");
  printf("  Total free heap:    %u bytes\n", (unsigned)total_free);
  printf("  Minimum free heap:  %u bytes\n", (unsigned)total_min);

  printf("\nInternal RAM:\n");
  printf("  Free:               %u bytes\n", (unsigned)internal_free);
  printf("  Minimum free:       %u bytes\n", (unsigned)internal_min);

  printf("\nPSRAM:\n");
  printf("  Total:              %u bytes\n", (unsigned)psram_total);
  printf("  Free:               %u bytes\n", (unsigned)psram_free);
  printf("  Used:               %u bytes\n",
         (unsigned)(psram_total - psram_free));
  printf("  Minimum free:       %u bytes\n", (unsigned)psram_min);

  return 0;
}

static int cmd_reboot(int argc, char **argv) {
  (void)argc;
  (void)argv;

  printf("Rebooting...\n");
  fflush(stdout);
  esp_restart();

  return 0;
}

/*-------------HELPERS-------------------*/

/* Split an input line into argc/argv arguments */
static int cli_parse_args(char *line, char **argv, int max_args) {
  int argc = 0;

  char *token = strtok(line, " ");

  while (token != NULL && argc < max_args) {
    argv[argc++] = token;
    token = strtok(NULL, " ");
  }

  return argc;
}

static void cli_execute(char *line) {
  char *argv[CLI_MAX_ARGS];

  int argc = cli_parse_args(line, argv, CLI_MAX_ARGS);

  if (argc == 0)
    return;

  for (int i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
    if (strcmp(argv[0], commands[i].name) == 0) {
      commands[i].handler(argc, argv);
      return;
    }
  }

  printf("Unknown command: %s\n", argv[0]);
}

static void cli_task(void *arg) {
  (void)arg;

  char line[CLI_MAX_LINE];
  int pos = 0;

  printf("Yahoo lets go\n");
  printf("espmaestro> ");
  fflush(stdout);

  while (1) {
    int c = getchar();

    /* No character available yet */
    if (c < 0) {
      vTaskDelay(pdMS_TO_TICKS(20));
      continue;
    }

    /* Execute the current line when Enter is pressed */
    if (c == '\r' || c == '\n') {
      printf("\n");

      line[pos] = '\0';

      if (pos > 0) {
        cli_execute(line);
      }

      pos = 0;
      printf("espmaestro> ");
      fflush(stdout);
      continue;
    }

    if (c == 127 || c == '\b') {
      if (pos > 0) {
        pos--;
        printf("\b \b");
        fflush(stdout);
      }
      continue;
    }

    if (pos < CLI_MAX_LINE - 1) {
      line[pos++] = (char)c;
      putchar(c);
      fflush(stdout);
    }
  }
}

void cli_init(void) { xTaskCreate(cli_task, "cli_task", 4096, NULL, 2, NULL); }
