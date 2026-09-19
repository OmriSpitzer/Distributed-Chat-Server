/**
 * Client GUI button
 *
 * @date 17-09-2026
 */

#pragma once
#include <QPushButton>
#include <QString>
#include <functional>

class Button : public QPushButton {
  Q_OBJECT
public:
  // constructor
  explicit Button(const QString &title = QStringLiteral("Default"),
                  std::function<void()> onClick = nullptr, bool visible = true,
                  QWidget *parent = nullptr,
                  const QString &objectName = QStringLiteral("GhostButton"));

  // destructor
  ~Button() override = default;

  // set the click handler
  void setOnClick(std::function<void()> onClick);

private:
  std::function<void()> onClick; // click handler
};
