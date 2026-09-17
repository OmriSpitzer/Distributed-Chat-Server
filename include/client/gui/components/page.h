/**
 * Abstract client GUI page base
 *
 * @date 16-09-2026
 */

#pragma once
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

class Header;
class QLabel;
class Client;

class Page : public QWidget {
  Q_OBJECT
public:
  // constructor
  explicit Page(const QString &title, QWidget *parent = nullptr, Client *client = nullptr);

  // destructor
  ~Page() override = default;

protected:
  // get the client instance
  Client *client() const { return clientObject; }

  // get the page header
  Header *header() const { return headerObject; }

  // get the main body layout (below the header)
  QVBoxLayout *getBodyLayout() const { return bodyLayout; }

  // refresh the page
  virtual void refresh() = 0;

private:
  Client *clientObject{nullptr};    // client instance
  Header *headerObject{nullptr};    // page header
  QVBoxLayout *rootLayout{nullptr}; // root layout
  QVBoxLayout *bodyLayout{nullptr}; // body layout
};
