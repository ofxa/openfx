#ifndef _ofxLog_H_
#define _ofxLog_H_

/** @file This file contains code that wraps OFX objects with C++ classes
 */
#include <string.h>
#include <string>

namespace OFX {
  /** @brief Sets the name of the log file. */
  void logSetFileName(const std::string &value);
  
  /** @brief Gets the name of the log file. */
  const std::string & logGetFileName();

  /** @brief Opens the log file, returns whether this was successful or not. */
  bool logOpenFile(void);

  /** @brief Closes the log file. */
  void logCloseFile(void);

  /** @brief Prints to the log file. */
  void logPrint(const char *format, ...);

  /** @brief Prints to the log file only if the condition is true and prepends a warning notice. */
  void logWarning(bool condition, const char *format, ...);

  /** @brief Prints to the log file only if the condition is true and prepends an error notice. */
  void logError(bool condition, const char *format, ...);

  /** @brief, checks two C strings for equality */
  inline bool strEquals(const char *a, const char *b)
  {
    return strcmp(a, b) == 0;
  }
};
#endif
