/*
 Copyright (C) 2017 Nikolay N. Karikh <knn@qmtunnel.com>

 This file is part of qmtunnel and is licensed under GNU General Public License 3.0, with
 the additional special exception to link portions of this program with the OpenSSL library.
 See LICENSE file for more details.
*/

#ifndef WIDGET_USERGROUPS_H
#define WIDGET_USERGROUPS_H

#include <QWidget>
#include <QTreeWidgetItem>
#include <QListWidgetItem>
#include <QMenu>
#include "../lib/mgrserver-parameters.h"

namespace Ui {
class WidgetUserGroups;
}

class WidgetUserGroups : public QWidget
{
  Q_OBJECT

public:
  explicit WidgetUserGroups(bool remote=false, QWidget *parent = 0);
  ~WidgetUserGroups();

  bool canApply();
  void loadParams(MgrServerParameters *params);
  void saveParams(MgrServerParameters *params);

signals:
  void paramsModified();

private slots:
  void treeWidget_on_popupMenu_group_new();
  void treeWidget_on_popupMenu_user_new();
  void treeWidget_on_popupMenu_remove();
  void on_treeWidget_customContextMenuRequested(const QPoint &pos);
  void on_treeWidget_currentItemChanged(QTreeWidgetItem *cur_item, QTreeWidgetItem *prev_item);

  void edt_textEdited(const QString &text);
  void chk_clicked(bool checked);
  void allowed_tunnelList_changed(QListWidgetItem *item);
  void edt_textChanged();
  void on_btnLoadUserCert_clicked();
  void on_btnUserPasswordClear_clicked();

private:
  Ui::WidgetUserGroups *ui;
  QMenu *popupMenu;
  QAction *action_group_new;
  QAction *action_group_remove;
  QAction *action_user_new;
  QAction *action_user_remove;
  QTreeWidgetItem *treeWidget_popupMenu_item;

  quint32 unique_user_group_id;
  quint32 unique_user_id;

  void treeWidget_loadItem(QTreeWidgetItem *item);

  void load_ui_settings();
  void save_ui_settings();

};

Q_DECLARE_METATYPE(UserGroup);
Q_DECLARE_METATYPE(User);

#endif // WIDGET_USERGROUPS_H
