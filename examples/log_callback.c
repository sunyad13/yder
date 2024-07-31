/**
 * 
 * Yder example program
 * 
 * This example program describes the main features 
 * that are available in a callback function
 * 
 * Copyright 2014-2020 Nicolas Mora <mail@babelouest.org>
 * 
 * License MIT
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "yder.h"

#define DATE_STAMP_SIZE 20

const char* log_level_names[] = {
    "NONE",
    "ERROR",
    "WARNING",
    "INFO",
    "DEBUG"
};

void y_callback_log_message(void * cls, const char * app_name, const time_t date, const unsigned long level, const char * message) {
  char date_stamp[DATE_STAMP_SIZE];
  struct tm * tm_stamp = localtime(&date);

#ifndef _WIN32
  strftime(date_stamp, sizeof(date_stamp), "%FT%TZ", tm_stamp);
#else
  strftime(date_stamp, sizeof(date_stamp), "%Y-%m-%dT%H:%M:%S", tm_stamp);
#endif
  
  printf("Here is my callback log function\n- cls is %s\n- app_name is %s\n- date is %s\n- level is %s\n- message is '%s'\n\n",
         (char *)cls, app_name, date_stamp, log_level_names[level], message);
}

void write_logs(const char * level) {
  y_log_message(Y_LOG_LEVEL_ERROR, "This is an error message while level is %s", level);
  y_log_message(Y_LOG_LEVEL_WARNING, "This is a warning message while level is %s", level);
  y_log_message(Y_LOG_LEVEL_INFO, "This is an information message while level is %s", level);
  y_log_message(Y_LOG_LEVEL_DEBUG, "This is a debug message while level is %s", level);
}

int main(int argc, char ** argv) {
  const char * cls = "my cls";
  const unsigned long log_levels[] = { Y_LOG_LEVEL_ERROR, Y_LOG_LEVEL_WARNING, Y_LOG_LEVEL_INFO, Y_LOG_LEVEL_DEBUG };
  const char * log_level_strs[] = { "error", "warning", "info", "debug" };

  y_log_message(Y_LOG_LEVEL_ERROR, "This is a test error message without initialized logs");

  for (int i = 0; i < sizeof(log_levels) / sizeof(log_levels[0]); i++) {
    if (y_init_logs("Yder Tests", Y_LOG_MODE_CALLBACK, log_levels[i], NULL, "Initializing logs mode: callback") &&
        y_set_logs_callback(&y_callback_log_message, (void*)cls, "callback init")) {
      write_logs(log_level_strs[i]);
      y_close_logs();
    }
  }
  
  return 0;
}

