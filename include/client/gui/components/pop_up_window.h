/**
 * Client GUI pop-up window
 *
 * @date 17-09-2026
 */

#pragma once
#include <QDialog>
#include <QString>

class QWidget;

class PopUpWindow : public QDialog {
  Q_OBJECT
public:
  // constructor
  explicit PopUpWindow(const QString &title, QWidget *form, QWidget *parent = nullptr,
                       QWidget *focus = nullptr);

  // destructor
  ~PopUpWindow() override = default;

  // run the pop-up window
  static bool run(QWidget *parent, const QString &title, QWidget *form, QWidget *focus = nullptr);
};
