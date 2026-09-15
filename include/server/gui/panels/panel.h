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
  explicit Panel(const QString &title, QWidget *parent = nullptr, Server *server = nullptr);
  ~Panel() override = default;

  // refresh panel contents (GUI thread)
  virtual void refresh() = 0;

protected:
  Server *server() const { return server_; }
  QVBoxLayout *bodyLayout() const { return bodyLayout_; }

private:
  Server *server_{nullptr};
  QLabel *titleLabel_{nullptr};
  QVBoxLayout *rootLayout_{nullptr};
  QVBoxLayout *bodyLayout_{nullptr};
};
