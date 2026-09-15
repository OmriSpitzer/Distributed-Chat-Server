/**
 * Abstract server GUI panel base
 *
 * @date 15-09-2026
 */

#pragma once
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

class QLabel;
class Server;

class Panel : public QWidget {
  Q_OBJECT
public:
  // constructor
  explicit Panel(const QString &title, QWidget *parent = nullptr, Server *server = nullptr);

  // destructor
  ~Panel() override = default;

protected:
  // refresh interval
  static constexpr int REFRESH_INTERVAL = 500;

  // get the server instance
  Server *server() const { return serverObject; }

  // get the body layout
  QVBoxLayout *getBodyLayout() const { return bodyLayout; }

  // refresh the panel
  virtual void refresh() = 0;

private:
  Server *serverObject{nullptr};    // server instance
  QLabel *titleLabel{nullptr};      // title label
  QVBoxLayout *rootLayout{nullptr}; // root layout
  QVBoxLayout *bodyLayout{nullptr}; // body layout
};
