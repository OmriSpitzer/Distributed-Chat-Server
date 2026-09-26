/**
 * Qt Logger class header file
 *
 * @date 24-09-2026
 */
#pragma once
#include "utils/logger/i_logger.h"
#include "utils/logger/log_message.h"
#include <QObject>
#include <QString>

class QtLogger : public QObject, public ILogger {
  Q_OBJECT

public:
  // constructor
  explicit QtLogger(QObject *parent = nullptr);

  // destructor
  ~QtLogger() override = default;

  // emit the log message to the Qt UI
  void log(const LogMessage &message) override;

signals:
  void logLine(const QString &line);
};
