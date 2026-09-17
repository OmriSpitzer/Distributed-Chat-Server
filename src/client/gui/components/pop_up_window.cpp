/**
 * Client GUI pop-up window implementation
 *
 * @brief Modal form pop-up with OK / Cancel.
 * @date 17-09-2026
 */

#include "client/gui/components/pop_up_window.h"
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QWidget>

// constructor
PopUpWindow::PopUpWindow(const QString &title, QWidget *form, QWidget *parent, QWidget *focus)
    : QDialog(parent) {
  auto *root = new QVBoxLayout(this); // create the root layout
  auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                                       this); // create the buttons

  // layout properties
  setWindowTitle(title);
  setModal(true);
  resize(380, 0);
  setObjectName("PopUpWindow");

  // root layout properties
  root->setContentsMargins(16, 16, 16, 16);
  root->setSpacing(12);

  // add the form to the root layout
  if (form) {
    form->setParent(this);
    root->addWidget(form);
  }

  // add the buttons to the root layout
  root->addWidget(buttons);

  // connect the buttons to the dialog
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

  // set the focus to the focus widget
  if (focus) {
    focus->setFocus();
  }
}

// run the pop-up window
bool PopUpWindow::run(QWidget *parent, const QString &title, QWidget *form, QWidget *focus) {
  PopUpWindow dialog(title, form, parent, focus);
  return dialog.exec() == QDialog::Accepted;
}
