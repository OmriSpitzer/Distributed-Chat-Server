/**
 * Client GUI button implementation
 *
 * @brief Client GUI button implementation
 * @date 17-09-2026
 */

#include "client/gui/components/button.h"

// constructor
Button::Button(const QString &title, std::function<void()> onClick, bool visible, QWidget *parent,
               const QString &objectName)
    : QPushButton(title, parent), onClick(std::move(onClick)) {
  // button properties
  setObjectName(objectName);
  setCursor(Qt::PointingHandCursor);
  setVisible(visible);

  // connect the clicked signal to the click handler
  connect(this, &QPushButton::clicked, this, [this]() {
    if (this->onClick) {
      this->onClick();
    }
  });
}

// set the click handler
void Button::setOnClick(std::function<void()> onClick) { this->onClick = std::move(onClick); }
