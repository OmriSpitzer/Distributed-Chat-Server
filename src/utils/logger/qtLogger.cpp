/**
 * Qt Logger class implementation file
 *
 * @brief Qt Logger class that emits log messages to the Qt console
 * @date 24-09-2026
 */
#include "utils/logger/qtLogger.h"
#include "utils/logger/log_message.h"
#include <QDateTime>
#include <QString>

// constructor
QtLogger::QtLogger(QObject *parent) : QObject(parent) {}

// emit the log message to the Qt UI
void QtLogger::log(const LogMessage &message) {
  const QString timestampString =
      QDateTime::fromSecsSinceEpoch(static_cast<qint64>(message.getTimestamp()))
          .toString("hh:mm:ss");
  const QString typeString = QString::fromStdString(LogMessage::typeToString(message.getType()));
  const QString sourceString = QString::fromStdString(message.getSource());
  const QString messageString = QString::fromStdString(message.getMessage());

  const QString line = QString("[%1] (%2) %3: %4")
                           .arg(timestampString)
                           .arg(typeString)
                           .arg(sourceString)
                           .arg(messageString);

  emit logLine(line);
}
