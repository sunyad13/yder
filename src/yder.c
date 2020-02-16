/**
 * 
 * Yder Framework
 * 
 * Logging framework library
 * 
 * yder.h: structures and functions declarations
 * 
 * Copyright 2015-2018 Nicolas Mora <mail@babelouest.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation;
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU GENERAL PUBLIC LICENSE for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library.  If not, see <http://www.gnu.org/licenses/>.
 * 
 */

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include <orcania.h>
#include "yder.h"

#ifndef _WIN32
  #include <syslog.h>
  #ifndef Y_DISABLE_JOURNALD
    #include <systemd/sd-journal.h>
  #endif
#endif

static pthread_mutex_t y_message_lock;

#define Y_STATIC_BUFFER_SIZE 256

/**
 * Write log message to console output (stdout or stderr)
 */
static void y_write_log_console(const char * app_name, const time_t date, const unsigned long level, const char * message) {
  char * level_name = NULL, date_stamp[20];
  FILE * output = NULL;
  struct tm * tm_stamp;
  
  tm_stamp = localtime (&date);
  
#ifndef _WIN32
  strftime (date_stamp, sizeof(date_stamp), "%FT%TZ", tm_stamp);
#else
  strftime (date_stamp, sizeof(date_stamp), "%Y-%m-%dT%H:%M:%S", tm_stamp);
#endif
  switch (level) {
    case Y_LOG_LEVEL_ERROR:
      level_name = "ERROR";
      break;
    case Y_LOG_LEVEL_WARNING:
      level_name = "WARNING";
      break;
    case Y_LOG_LEVEL_INFO:
      level_name = "INFO";
      break;
    case Y_LOG_LEVEL_DEBUG:
      level_name = "DEBUG";
      break;
    default:
      level_name = "NONE";
      break;
  }
  if (level & Y_LOG_LEVEL_WARNING || level & Y_LOG_LEVEL_ERROR) {
    // Write to stderr
    output = stderr;
  } else {
    // Write to stdout
    output = stdout;
  }
  fprintf(output, "%s - %s %s: %s\n", date_stamp, app_name, level_name, message);
  fflush(output);
}

#ifndef _WIN32
/**
 * Write log message to syslog
 */
static void y_write_log_syslog(const char * app_name, const unsigned long level, const char * message) {
  openlog(app_name, LOG_PID|LOG_CONS, LOG_USER);
  switch (level) {
    case Y_LOG_LEVEL_ERROR:
      syslog( LOG_ERR, "%s", message );
      break;
    case Y_LOG_LEVEL_WARNING:
      syslog( LOG_WARNING, "%s", message );
      break;
    case Y_LOG_LEVEL_INFO:
      syslog( LOG_INFO, "%s", message );
      break;
    case Y_LOG_LEVEL_DEBUG:
      syslog( LOG_DEBUG, "%s", message );
      break;
  }
  closelog();
}

  #ifndef Y_DISABLE_JOURNALD
/**
 * Write log message to journald
 */
static void y_write_log_journald(const char * app_name, const unsigned long level, const char * message) {
  switch (level) {
    case Y_LOG_LEVEL_ERROR:
      sd_journal_print( LOG_ERR, "%s - %s", app_name, message );
      break;
    case Y_LOG_LEVEL_WARNING:
      sd_journal_print( LOG_WARNING, "%s - %s", app_name, message );
      break;
    case Y_LOG_LEVEL_INFO:
      sd_journal_print( LOG_INFO, "%s - %s", app_name, message );
      break;
    case Y_LOG_LEVEL_DEBUG:
      sd_journal_print( LOG_DEBUG, "%s - %s", app_name, message );
      break;
  }
}
  #endif
#endif

/**
 * Append log message to the log file
 */
static void y_write_log_file(const char * app_name, const time_t date, FILE * log_file, const unsigned long level, const char * message) {
  char * level_name = NULL, date_stamp[20];
  struct tm * tm_stamp;
  
  if (log_file != NULL) {
    tm_stamp = localtime (&date);
    strftime (date_stamp, sizeof(date_stamp), "%Y-%m-%d %H:%M:%S", tm_stamp);
    switch (level) {
      case Y_LOG_LEVEL_ERROR:
        level_name = "ERROR";
        break;
      case Y_LOG_LEVEL_WARNING:
        level_name = "WARNING";
        break;
      case Y_LOG_LEVEL_INFO:
        level_name = "INFO";
        break;
      case Y_LOG_LEVEL_DEBUG:
        level_name = "DEBUG";
        break;
      default:
        level_name = "NONE";
        break;
    }
    fprintf(log_file, "%s - %s %s: %s\n", date_stamp, app_name, level_name, message);
    fflush(log_file);
  }
}

/**
 * Main function for logging messages
 * Warning ! Contains static variables used for not having to pass general configuration values every time you call log_message
 */
static int y_write_log(const char * app_name,
                       const unsigned long init_mode,
                       const unsigned long init_level,
                       const char * init_log_file,
                       void (* y_callback_log_message) (void * cls, const char * app_name, const time_t date, const unsigned long level, const char * message),
                       void * cls,
                       const unsigned long level,
                       const char * message) {
  static unsigned long cur_mode = Y_LOG_MODE_NONE, cur_level = Y_LOG_LEVEL_NONE;
  FILE * cur_log_file = NULL;
  static char * cur_app_name = NULL;
  static const char * cur_log_file_path = NULL;
  static void (* cur_callback_log_message) (void * cls, const char * app_name, const time_t date, const unsigned long level, const char * message) = NULL;
  static void * cur_cls = NULL;
  time_t now;
  
  // Closing logs: free cur_app_name
  if (app_name == NULL &&
      init_mode == Y_LOG_MODE_NONE &&
      init_level == Y_LOG_LEVEL_NONE &&
      init_log_file == NULL &&
      level == Y_LOG_LEVEL_NONE &&
      message == NULL) {
    o_free(cur_app_name);
    cur_app_name = NULL;
    return 1;
  }
  
  time(&now);
  
  if (init_mode != Y_LOG_MODE_CURRENT) {
    cur_mode = init_mode;
  }
  
  if (init_level != Y_LOG_LEVEL_CURRENT) {
    cur_level = init_level;
  }
  
  if (y_callback_log_message != NULL) {
    cur_callback_log_message = y_callback_log_message;
    cur_cls = cls;
  }
  
  if (cur_mode == Y_LOG_MODE_NONE && cur_level == Y_LOG_LEVEL_NONE) {
    // Logs have not been initialized, cancel
    return 0;
  }

  if (init_log_file != NULL) {
    if (cur_log_file_path == NULL) {
      cur_log_file_path = init_log_file;
    } else {
      // Logs have already been initialized, cancel
      perror("Error - yder logs already initialized");
      return 0;
    }
  }
  
  if (app_name != NULL) {
    if (cur_app_name == NULL) {
      cur_app_name = o_strdup(app_name);
    } else {
      // Logs have already been initialized, cancel
      perror("Error - yder logs already initialized");
      return 0;
    }
  }
  
  if (init_mode == Y_LOG_MODE_FILE && !o_strlen(cur_log_file_path)) {
    // Logs have already been initialized, cancel
    perror("Error - log file path misssing");
    return 0;
  }
  
  if (cur_log_file_path != NULL) {
    if ((cur_log_file = fopen(cur_log_file_path, "a+")) == NULL) {
      fprintf(stderr, "%s - ", cur_log_file_path);
      perror("Error opening log file");
      cur_log_file_path = NULL;
      o_free(cur_app_name);
      cur_app_name = NULL;
      return 0;
    }
  }
  
  // write message to expected output if level expected
  if (cur_level >= level) {
    if (message != NULL) {
      if (cur_mode & Y_LOG_MODE_CONSOLE) {
        y_write_log_console(cur_app_name, now, level, message);
      }
#ifndef _WIN32
      if (cur_mode & Y_LOG_MODE_SYSLOG) {
        y_write_log_syslog(cur_app_name, level, message);
      }
#endif

#if !defined(_WIN32) && !defined(Y_DISABLE_JOURNALD)
      if (cur_mode & Y_LOG_MODE_JOURNALD) {
        y_write_log_journald(cur_app_name, level, message);
      }
#endif
      if (cur_mode & Y_LOG_MODE_FILE) {
        y_write_log_file(cur_app_name, now, cur_log_file, level, message);
      }
      if (cur_mode & Y_LOG_MODE_CALLBACK && cur_callback_log_message != NULL) {
        cur_callback_log_message(cur_cls, cur_app_name, now, level, message);
      }
    }
  }
  
  if (cur_log_file != NULL && cur_log_file_path != NULL) {
    if (fclose(cur_log_file) != 0) {
      perror("Error closing log file");
      return 0;
    }
  }
  
  return 1;
}

/**
 * Initialize logs
 */
int y_init_logs(const char * app, const unsigned long init_mode, const unsigned long init_level, const char * init_log_file, const char * message) {
  pthread_mutexattr_t mutexattr;
#ifdef _WIN32
  if (init_mode & Y_LOG_MODE_SYSLOG) {
    perror("syslog mode not supported on your architecture");
    return 0;
  }
#endif

#if defined(_WIN32) || defined(Y_DISABLE_JOURNALD)
  if (init_mode & Y_LOG_MODE_JOURNALD) {
    perror("journald mode not supported on your architecture");
    return 0;
  }
#endif

  pthread_mutexattr_init ( &mutexattr );
  pthread_mutexattr_settype( &mutexattr, PTHREAD_MUTEX_RECURSIVE );
  if (!pthread_mutex_init(&y_message_lock, &mutexattr)) {
    return y_write_log(app, init_mode, init_level, init_log_file, NULL, NULL, Y_LOG_LEVEL_INFO, message);
  } else {
    perror("Error initializing mutex");
    return 0;
  }
}

/**
 * Specify a callback function that will catch all log messages
 * In addition to other logs output already defined in y_init_logs
 */
int y_set_logs_callback(void (* y_callback_log_message) (void * cls, const char * app_name, const time_t date, const unsigned long level, const char * message),
                        void * cls,
                        const char * message) {
  if (y_callback_log_message != NULL) {
    return y_write_log(NULL, Y_LOG_MODE_CURRENT, Y_LOG_LEVEL_CURRENT, NULL, y_callback_log_message, cls, Y_LOG_LEVEL_INFO, message);
  } else {
    return 0;
  }
}

/**
 * Close logs
 */
int y_close_logs() {
  if (pthread_mutex_destroy(&y_message_lock)) {
    perror("Error destroying mutex");
  }
  return y_write_log(NULL, 0, 0, NULL, NULL, NULL, 0, NULL);
}

/**
 * Write the message given in parameters to the current outputs if the current level matches
 */
void y_log_message(const unsigned long level, const char * message, ...) {
  va_list args, args_cpy;
  size_t out_len = 0;
  char * out = NULL;
  static char y_buffer_message[Y_STATIC_BUFFER_SIZE+1];
  
  if (!pthread_mutex_lock(&y_message_lock)) {
    va_start(args, message);
    // Use va_copy to make a new args pointer to avoid problems with vsnprintf which can change args parameter on some architectures
    va_copy(args_cpy, args);
    out_len = vsnprintf(NULL, 0, message, args);
    if (out_len <= Y_STATIC_BUFFER_SIZE) {
      vsnprintf(y_buffer_message, (out_len + 1), message, args_cpy);
      y_write_log(NULL, Y_LOG_MODE_CURRENT, Y_LOG_LEVEL_CURRENT, NULL, NULL, NULL, level, y_buffer_message);
    } else {
      out = o_malloc((out_len + 1)*sizeof(char));
      if (out != NULL) {
        vsnprintf(out, (out_len + 1), message, args_cpy);
        y_write_log(NULL, Y_LOG_MODE_CURRENT, Y_LOG_LEVEL_CURRENT, NULL, NULL, NULL, level, out);
        o_free(out);
      }
    }
    va_end(args);
    va_end(args_cpy);
    pthread_mutex_unlock(&y_message_lock);
  } else {
    perror("Error locking mutex");
  }
}
