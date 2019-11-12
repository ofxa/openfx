#include <string>
#include <stdio.h>
#include <stdarg.h>

namespace OFX {

  /** @brief log file, yes I know, it could be an iostream, but I hate iostreams, call me an old fashion C programmer */
  static FILE *gLogFP = 0;
  static std::string gLogFileName("ofxTestLog.txt");
  
  /** @brief Sets the name of the log file. */
  void
  logSetFileName(const std::string &value)
  {
    gLogFileName = value;
  }
  
  /** @brief Gets the name of the log file. */
  const std::string &
  logGetFileName()
  {
    return gLogFileName;
  }

  /** @brief Opens the log file, returns whether this was successful or not. */
  bool
  logOpenFile(void)
  {
    if(!gLogFP) {
      gLogFP = fopen(gLogFileName.c_str(), "w");
    }
    return gLogFP != 0;
  }

  /** @brief Closes the log file. */
  void
  logCloseFile(void)
  {
    if(gLogFP) {
      fclose(gLogFP);
    }
    gLogFP = 0;
  }

  /** @brief Prints to the log file. */
  void
  logPrint(const char *format, ...)
  {
    if(logOpenFile()) {
      va_list args;
      va_start(args, format);
      vfprintf(gLogFP, format, args);
      fputc('\n', gLogFP);
      fflush(gLogFP);
      va_end(args);
    }  
  }

  /** @brief Prints to the log file only if the condition is true and prepends a warning notice. */
  void
  logWarning(bool condition, const char *format, ...)
  {
    if(condition && logOpenFile()) {
      fputs("WARNING : ", gLogFP);

      va_list args;
      va_start(args, format);
      vfprintf(gLogFP, format, args);
      fputc('\n', gLogFP);
      va_end(args);

      fflush(gLogFP);
    }  
  }

  /** @brief Prints to the log file only if the condition is true and prepends an error notice. */
  void
  logError(bool condition, const char *format, ...)
  {
    if(condition && logOpenFile()) {
      fputs("ERROR : ", gLogFP);

      va_list args;
      va_start(args, format);
      vfprintf(gLogFP, format, args);
      fputc('\n', gLogFP);
      va_end(args);

      fflush(gLogFP);
    }  
  }

};
