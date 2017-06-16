/*
 Copyright (C) 2017 Nikolay N. Karikh <knn@qmtunnel.com>

 This file is part of qmtunnel and is licensed under GNU General Public License 3.0, with
 the additional special exception to link portions of this program with the OpenSSL library.
 See LICENSE file for more details.
*/

#include "widget_usergroups.h"
#include "ui_widget_usergroups.h"
#include <QMessageBox>
#include <QFileDialog>
#include "gui_settings.h"
#include "../lib/ssl_helper.h"

#define UI_TAB_USER_GROUPS_VERSION                    2

//---------------------------------------------------------------------------
WidgetUserGroups::WidgetUserGroups(bool, QWidget *parent): QWidget(parent), ui(new Ui::WidgetUserGroups)
{
  unique_user_group_id = 1;
  unique_user_id = 1;
  ui->setupUi(this);
  ui->stackedWidget->setCurrentIndex(0);
  ui->treeWidget->sortByColumn(0, Qt::AscendingOrder);

  popupMenu = new QMenu(ui->treeWidget);
  action_group_new = popupMenu->addAction(tr("Add group..."), this, SLOT(treeWidget_on_popupMenu_group_new()));
  action_group_remove = popupMenu->addAction(tr("Remove group..."), this, SLOT(treeWidget_on_popupMenu_remove()));
  popupMenu->addSeparator();
  action_user_new = popupMenu->addAction(tr("Add user..."), this, SLOT(treeWidget_on_popupMenu_user_new()));
  action_user_remove = popupMenu->addAction(tr("Remove user..."), this, SLOT(treeWidget_on_popupMenu_remove()));

  connect(ui->edtUserName, SIGNAL(textEdited(QString)), this, SLOT(edt_textEdited(QString)));
  connect(ui->chkUserEnabled, SIGNAL(clicked(bool)), this, SLOT(chk_clicked(bool)));
  connect(ui->edtUserCert, SIGNAL(textChanged()), this, SLOT(edt_textChanged()));
  connect(ui->edtUserPassword, SIGNAL(textEdited(QString)), this, SLOT(edt_textEdited(QString)));

  connect(ui->edtGroupName, SIGNAL(textEdited(QString)), this, SLOT(edt_textEdited(QString)));
  connect(ui->chkGroupEnabled, SIGNAL(clicked(bool)), this, SLOT(chk_clicked(bool)));
  connect(ui->afConfigSet, SIGNAL(clicked(bool)), this, SLOT(chk_clicked(bool)));
  connect(ui->afTunnelCreate, SIGNAL(clicked(bool)), this, SLOT(chk_clicked(bool)));
  connect(ui->afTunnelSuperAdmin, SIGNAL(clicked(bool)), this, SLOT(chk_clicked(bool)));
  connect(ui->afTunnelGroupAdmin, SIGNAL(clicked(bool)), this, SLOT(chk_clicked(bool)));
  connect(ui->permittedTunnelList, SIGNAL(itemChanged(QListWidgetItem*)), this, SLOT(allowed_tunnelList_changed(QListWidgetItem*)));

  load_ui_settings();
}

//---------------------------------------------------------------------------
void WidgetUserGroups::load_ui_settings()
{
  cJSON *j_section = cJSON_GetObjectItem(j_gui_settings, "WidgetUserGroups");
  if (!j_section)
    return;
  cJSON *j = cJSON_GetObjectItem(j_section, "version");
  if (!j || j->type != cJSON_Number || j->valueint != UI_TAB_USER_GROUPS_VERSION)
    return;
  j = cJSON_GetObjectItem(j_section, "splitterState");
  if (j && j->type == cJSON_String)
    ui->splitter->restoreState(QByteArray::fromBase64(j->valuestring));
}

//---------------------------------------------------------------------------
void WidgetUserGroups::save_ui_settings()
{
  cJSON *j_section = cJSON_GetObjectItem(j_gui_settings, "WidgetUserGroups");
  if (j_section)
    cJSON_DeleteItemFromObject(j_gui_settings, "WidgetUserGroups");
  j_section = cJSON_CreateObject();
  cJSON_AddItemToObject(j_gui_settings, "WidgetUserGroups", j_section);

  cJSON_AddStringToObject(j_section, "splitterState", ui->splitter->saveState().toBase64());
  cJSON_AddNumberToObject(j_section, "version", UI_TAB_USER_GROUPS_VERSION);
}

//---------------------------------------------------------------------------
void WidgetUserGroups::on_treeWidget_customContextMenuRequested(const QPoint &pos)
{
  QPoint p = ui->treeWidget->mapToGlobal(pos);
  treeWidget_popupMenu_item = ui->treeWidget->itemAt(pos);
  p.setX(p.x()+10);
  action_group_remove->setEnabled(treeWidget_popupMenu_item != NULL && treeWidget_popupMenu_item->parent() == NULL);
  action_user_new->setEnabled(treeWidget_popupMenu_item != NULL);
  action_user_remove->setEnabled(treeWidget_popupMenu_item != NULL && treeWidget_popupMenu_item->parent() != NULL);
  popupMenu->exec(p);
}

//---------------------------------------------------------------------------
void WidgetUserGroups::treeWidget_on_popupMenu_group_new()
{
  UserGroup new_group;
  while (true)
  {
    new_group.id = unique_user_group_id++;
    if (new_group.id == 0)
      new_group.id = unique_user_group_id++;
    bool is_unique_id = true;
    for (int i=0; i < ui->treeWidget->topLevelItemCount(); i++)
    {
      if (ui->treeWidget->topLevelItem(i)->data(0, Qt::UserRole).value<UserGroup>().id == new_group.id)
      {
        is_unique_id = false;
        break;
      }
    }
    if (is_unique_id)
      break;
  }
  new_group.name = tr("Group%1").arg(new_group.id);

  QTreeWidgetItem *item = new QTreeWidgetItem(ui->treeWidget, QStringList() << new_group.name);
  item->setData(0, Qt::UserRole, QVariant::fromValue(new_group));

  ui->treeWidget->setCurrentItem(item);

  emit paramsModified();
}

//---------------------------------------------------------------------------
void WidgetUserGroups::treeWidget_on_popupMenu_user_new()
{
  if (!treeWidget_popupMenu_item)
    return;
  QTreeWidgetItem *parent_group_item = treeWidget_popupMenu_item;
  if (parent_group_item->parent())
    parent_group_item = parent_group_item->parent();

  User new_user;
  while (true)
  {
    new_user.id = unique_user_id++;
    if (new_user.id == 0)
      new_user.id = unique_user_id++;
    bool is_unique_id = true;
    for (int group_index=0; group_index < ui->treeWidget->topLevelItemCount() && is_unique_id; group_index++)
    {
      QTreeWidgetItem *group_item = ui->treeWidget->topLevelItem(group_index);
      for (int user_index=0; user_index < group_item->childCount(); user_index++)
      {
        if (group_item->child(user_index)->data(0, Qt::UserRole).value<User>().id == new_user.id)
        {
          is_unique_id = false;
          break;
        }
      }
    }
    if (is_unique_id)
      break;
  }
  new_user.name = tr("User%1").arg(new_user.id);
  new_user.group_id = parent_group_item->data(0, Qt::UserRole).value<UserGroup>().id;

  QTreeWidgetItem *item = new QTreeWidgetItem(parent_group_item, QStringList() << new_user.name);
  item->setData(0, Qt::UserRole, QVariant::fromValue(new_user));

  ui->treeWidget->setCurrentItem(item);

  emit paramsModified();
}

//---------------------------------------------------------------------------
void WidgetUserGroups::treeWidget_on_popupMenu_remove()
{
  if (!treeWidget_popupMenu_item)
    return;
  if (treeWidget_popupMenu_item->childCount() > 0)
  {
    if (QMessageBox::question(this, tr("Remove group"), tr("Are you sure you want to remove group '%1'?\n"
                                                          "This group contains at least one user which will be also removed if you proceed.")
                                .arg(treeWidget_popupMenu_item->text(0)),
                                QMessageBox::Yes | QMessageBox::No) == QMessageBox::No)
      return;
  }
  else if (!treeWidget_popupMenu_item->parent())
  {
    if (QMessageBox::question(this, tr("Remove group"), tr("Are you sure you want to remove group '%1'?")
                            .arg(treeWidget_popupMenu_item->text(0)),
                            QMessageBox::Yes | QMessageBox::No) == QMessageBox::No)
    return;
  }
  else if (QMessageBox::question(this, tr("Remove user"), tr("Are you sure you want to remove user '%1'?")
                            .arg(treeWidget_popupMenu_item->text(0)),
                            QMessageBox::Yes | QMessageBox::No) == QMessageBox::No)
    return;
  delete treeWidget_popupMenu_item;
  treeWidget_popupMenu_item = NULL;

  emit paramsModified();
}

//---------------------------------------------------------------------------
void WidgetUserGroups::treeWidget_loadItem(QTreeWidgetItem *item)
{
  if (!item)
  {
    ui->stackedWidget->setCurrentIndex(0);
    return;
  }

  if (!item->parent())
  {
    ui->stackedWidget->setCurrentWidget(ui->page_group);
    UserGroup user_group = item->data(0, Qt::UserRole).value<UserGroup>();

    ui->edtGroupName->setText(user_group.name);
    ui->chkGroupEnabled->setChecked(user_group.enabled);
    ui->afConfigSet->setChecked(user_group.access_flags & UserGroup::AF_CONFIG_SET);
    ui->afTunnelCreate->setChecked(user_group.access_flags & UserGroup::AF_TUNNEL_CREATE);
    ui->afTunnelGroupAdmin->setChecked(user_group.access_flags & UserGroup::AF_TUNNEL_GROUP_ADMIN);
    ui->afTunnelSuperAdmin->setChecked(user_group.access_flags & UserGroup::AF_TUNNEL_SUPER_ADMIN);
    ui->permittedTunnelList->blockSignals(true);
    ui->permittedTunnelList->clear();
    for (int i=0; i < user_group.tunnel_allow_list.count(); i++)
    {
      QListWidgetItem *item = new QListWidgetItem(user_group.tunnel_allow_list[i], ui->permittedTunnelList);
      item->setFlags(item->flags() | Qt::ItemIsEditable);
    }
    QListWidgetItem *item = new QListWidgetItem(ui->permittedTunnelList);
    item->setFlags(item->flags() | Qt::ItemIsEditable);
    ui->permittedTunnelList->blockSignals(false);

    ui->edtGroupName->setFocus();
  }
  else
  {
    ui->stackedWidget->setCurrentWidget(ui->page_user);
    User user = item->data(0, Qt::UserRole).value<User>();

    ui->edtUserName->setText(user.name);
    ui->chkUserEnabled->setChecked(user.enabled);
    ui->edtUserCert->setPlainText(user.cert.toPem());
    ui->edtUserPassword->setText(user.password);

    ui->edtUserName->setFocus();
  }
}

//---------------------------------------------------------------------------
void WidgetUserGroups::edt_textEdited(const QString &text)
{
  QTreeWidgetItem *cur_item = ui->treeWidget->currentItem();
  QLineEdit *edt = qobject_cast<QLineEdit *>(sender());
  if (!cur_item || !edt)
    return;

  if (!cur_item->parent())
  {
    UserGroup user_group = cur_item->data(0, Qt::UserRole).value<UserGroup>();
    if (edt == ui->edtGroupName)
    {
      user_group.name = text.trimmed();
      bool is_unique_name = true;
      for (int group_index=0; group_index < ui->treeWidget->topLevelItemCount() && is_unique_name; group_index++)
      {
        QTreeWidgetItem *group_item = ui->treeWidget->topLevelItem(group_index);
        if (group_item != cur_item &&
            group_item->data(0, Qt::UserRole).value<UserGroup>().name == user_group.name)
        {
          is_unique_name = false;
          break;
        }
      }
      if (user_group.name.isEmpty() || !is_unique_name)
      {
        ui->lblGroupName->setText(tr("<font color=red><b>Group name</b></font>:"));
        return;
      }
      else
        ui->lblGroupName->setText(tr("Group name:"));
      cur_item->setText(0, text);
    }
    cur_item->setData(0, Qt::UserRole, QVariant::fromValue(user_group));
  }
  else
  {
    User user = cur_item->data(0, Qt::UserRole).value<User>();
    if (edt == ui->edtUserName)
    {
      user.name = text.trimmed();
      bool is_unique_name = true;
      for (int group_index=0; group_index < ui->treeWidget->topLevelItemCount() && is_unique_name; group_index++)
      {
        QTreeWidgetItem *group_item = ui->treeWidget->topLevelItem(group_index);
        for (int user_index=0; user_index < group_item->childCount(); user_index++)
        {
          if (group_item->child(user_index) != cur_item &&
              group_item->child(user_index)->data(0, Qt::UserRole).value<User>().name == user.name)
          {
            is_unique_name = false;
            break;
          }
        }
      }
      if (user.name.isEmpty() || !is_unique_name)
      {
        ui->lblUserName->setText(tr("<font color=red><b>User name</b></font>:"));
        return;
      }
      else
        ui->lblUserName->setText(tr("User name:"));
      cur_item->setText(0, user.name);
    }
    else if (edt == ui->edtUserPassword)
      user.password = text;
    cur_item->setData(0, Qt::UserRole, QVariant::fromValue(user));
  }
  emit paramsModified();
}

//---------------------------------------------------------------------------
void WidgetUserGroups::on_btnUserPasswordClear_clicked()
{
  QTreeWidgetItem *cur_item = ui->treeWidget->currentItem();
  if (!cur_item)
    return;

  if (cur_item->parent())
  {
    ui->edtUserPassword->clear();
    User user = cur_item->data(0, Qt::UserRole).value<User>();
    user.password = QString(USER_PASSWORD_SPECIAL_CLEAR);
    cur_item->setData(0, Qt::UserRole, QVariant::fromValue(user));
  }
  emit paramsModified();
}

//---------------------------------------------------------------------------
void WidgetUserGroups::edt_textChanged()
{
  QTreeWidgetItem *cur_item = ui->treeWidget->currentItem();
  QTextEdit *edt = qobject_cast<QTextEdit *>(sender());
  if (!cur_item || !edt)
    return;

  if (!cur_item->parent())
  {
  }
  else
  {
    User user = cur_item->data(0, Qt::UserRole).value<User>();
    if (edt == ui->edtUserCert)
    {
      QSslCertificate old_cert = user.cert;
      QList<QSslCertificate> certs = QSslCertificate::fromData(edt->toPlainText().toUtf8());
      if (!certs.isEmpty())
      {
        user.cert = certs.first();
        ui->lblUserCertificate->setText(tr("X.509 SSL certificate in PEM format:"));
      }
      else
      {
        user.cert = QSslCertificate();
        ui->lblUserCertificate->setText(tr("<font color=red><b>X.509 SSL certificate in PEM format:</b></font>"));
      }
      if (old_cert == user.cert)
        return;
    }
    cur_item->setData(0, Qt::UserRole, QVariant::fromValue(user));
  }

  emit paramsModified();
}

//---------------------------------------------------------------------------
void WidgetUserGroups::chk_clicked(bool checked)
{
  QTreeWidgetItem *cur_item = ui->treeWidget->currentItem();
  QCheckBox *chk = qobject_cast<QCheckBox *>(sender());
  if (!cur_item || !chk)
    return;

  if (!cur_item->parent())
  {
    UserGroup user_group = cur_item->data(0, Qt::UserRole).value<UserGroup>();
    if (chk == ui->chkGroupEnabled)
      user_group.enabled = checked;
    else if (chk == ui->afConfigSet)
    {
      if (checked)
        user_group.access_flags |= UserGroup::AF_CONFIG_SET;
      else
        user_group.access_flags &= ~UserGroup::AF_CONFIG_SET;
    }
    else if (chk == ui->afTunnelCreate)
    {
      if (checked)
        user_group.access_flags |= UserGroup::AF_TUNNEL_CREATE;
      else
        user_group.access_flags &= ~UserGroup::AF_TUNNEL_CREATE;
    }
    else if (chk == ui->afTunnelGroupAdmin)
    {
      if (checked)
        user_group.access_flags |= UserGroup::AF_TUNNEL_GROUP_ADMIN;
      else
        user_group.access_flags &= ~UserGroup::AF_TUNNEL_GROUP_ADMIN;
    }
    else if (chk == ui->afTunnelSuperAdmin)
    {
      if (checked)
        user_group.access_flags |= UserGroup::AF_TUNNEL_SUPER_ADMIN;
      else
        user_group.access_flags &= ~UserGroup::AF_TUNNEL_SUPER_ADMIN;
    }
    cur_item->setData(0, Qt::UserRole, QVariant::fromValue(user_group));
  }
  else
  {
    User user = cur_item->data(0, Qt::UserRole).value<User>();
    if (chk == ui->chkUserEnabled)
      user.enabled = checked;
    cur_item->setData(0, Qt::UserRole, QVariant::fromValue(user));
  }

  emit paramsModified();
}

//---------------------------------------------------------------------------
void WidgetUserGroups::allowed_tunnelList_changed(QListWidgetItem *item)
{
  QTreeWidgetItem *cur_item = ui->treeWidget->currentItem();
  if (!cur_item || cur_item->parent())
    return;
  UserGroup user_group = cur_item->data(0, Qt::UserRole).value<UserGroup>();
  bool item_modified = true;
  int row = ui->permittedTunnelList->row(item);
  if ((user_group.tunnel_allow_list.count() > row &&
       user_group.tunnel_allow_list.at(row) == item->text()) ||
      (user_group.tunnel_allow_list.count() <= row &&
       item->text().trimmed().isEmpty()))
    item_modified = false;
  user_group.tunnel_allow_list.clear();
  bool has_empty_line = false;
  for (int i=0; i < ui->permittedTunnelList->count(); i++)
  {
    if (!ui->permittedTunnelList->item(i)->text().trimmed().isEmpty())
      user_group.tunnel_allow_list.append(ui->permittedTunnelList->item(i)->text().trimmed());
    else
      has_empty_line = true;
  }
  cur_item->setData(0, Qt::UserRole, QVariant::fromValue(user_group));
  if (!has_empty_line)
  {
    QListWidgetItem *new_item = new QListWidgetItem(ui->permittedTunnelList);
    new_item->setFlags(new_item->flags() | Qt::ItemIsEditable);
  }

  if (!has_empty_line || item_modified)
    emit paramsModified();
}

//---------------------------------------------------------------------------
void WidgetUserGroups::on_treeWidget_currentItemChanged(QTreeWidgetItem *cur_item, QTreeWidgetItem *)
{
  treeWidget_loadItem(cur_item);
}

//---------------------------------------------------------------------------
void WidgetUserGroups::loadParams(MgrServerParameters *params)
{
  ui->treeWidget->clear();
  unique_user_group_id = params->unique_user_group_id;
  unique_user_id = params->unique_user_id;

  for (int group_index=0; group_index < params->user_groups.count(); group_index++)
  {
    QTreeWidgetItem *group_item = new QTreeWidgetItem(ui->treeWidget, QStringList() << params->user_groups[group_index].name);
    group_item->setData(0, Qt::UserRole, QVariant::fromValue(params->user_groups[group_index]));
    for (int user_index=0; user_index < params->users.count(); user_index++)
    {
      if (params->users[user_index].group_id == params->user_groups[group_index].id)
      {
        QTreeWidgetItem *user_item = new QTreeWidgetItem(group_item, QStringList() << params->users[user_index].name);
        user_item->setData(0, Qt::UserRole, QVariant::fromValue(params->users[user_index]));
      }
    }
  }

  ui->treeWidget->expandAll();
}

//---------------------------------------------------------------------------
void WidgetUserGroups::saveParams(MgrServerParameters *params)
{
  params->user_groups.clear();
  params->users.clear();

  for (int group_index=0; group_index < ui->treeWidget->topLevelItemCount(); group_index++)
  {
    QTreeWidgetItem *group_item = ui->treeWidget->topLevelItem(group_index);
    params->user_groups.append(group_item->data(0, Qt::UserRole).value<UserGroup>());
    for (int user_index=0; user_index < group_item->childCount(); user_index++)
    {
      QTreeWidgetItem *user_item = group_item->child(user_index);
      User user = user_item->data(0, Qt::UserRole).value<User>();
      if (user.password == USER_PASSWORD_SPECIAL_SECRET)
        user.password.clear();
      params->users.append(user);
    }
  }

  params->unique_user_group_id = unique_user_group_id;
  params->unique_user_id = unique_user_id;
}

//---------------------------------------------------------------------------
bool WidgetUserGroups::canApply()
{
  return true;
}

//---------------------------------------------------------------------------
void WidgetUserGroups::on_btnLoadUserCert_clicked()
{
  QFileDialog fileDialog(this);
  fileDialog.setFileMode(QFileDialog::ExistingFile);
  fileDialog.setAcceptMode(QFileDialog::AcceptOpen);
  fileDialog.setNameFilters(QStringList() << "CRT files (*.crt)" << "PEM format (*.pem)" << "Any files (*)");

  cJSON *j_fileDialogs = cJSON_GetObjectItem(j_gui_settings, "fileDialogs");
  if (!j_fileDialogs)
  {
    j_fileDialogs = cJSON_CreateObject();
    cJSON_AddItemToObject(j_gui_settings, "fileDialogs", j_fileDialogs);
  }
  cJSON *j_fileDialog_SSLCert = cJSON_GetObjectItem(j_fileDialogs, "SSLCert");
  if (j_fileDialog_SSLCert)
  {
    cJSON *j;
    j = cJSON_GetObjectItem(j_fileDialog_SSLCert, "geometry");
    if (j && j->type == cJSON_String)
      fileDialog.restoreGeometry(QByteArray::fromBase64(j->valuestring));
    j = cJSON_GetObjectItem(j_fileDialog_SSLCert, "state");
    if (j && j->type == cJSON_String)
      fileDialog.restoreState(QByteArray::fromBase64(j->valuestring));
    j = cJSON_GetObjectItem(j_fileDialog_SSLCert, "filter");
    if (j && j->type == cJSON_String)
      fileDialog.selectNameFilter(QString::fromUtf8(j->valuestring));
  }
  if (!fileDialog.exec())
    return;

  if (j_fileDialog_SSLCert)
    cJSON_DeleteItemFromObject(j_fileDialogs, "SSLCert");
  j_fileDialog_SSLCert = cJSON_CreateObject();
  cJSON_AddItemToObject(j_fileDialogs, "SSLCert", j_fileDialog_SSLCert);

  cJSON_AddStringToObject(j_fileDialog_SSLCert, "geometry", fileDialog.saveGeometry().toBase64());
  cJSON_AddStringToObject(j_fileDialog_SSLCert, "state", fileDialog.saveState().toBase64());
  cJSON_AddStringToObject(j_fileDialog_SSLCert, "filter", fileDialog.selectedNameFilter().toUtf8());

  ui_save();

  if (fileDialog.selectedFiles().isEmpty())
    return;
  QString filename = fileDialog.selectedFiles().at(0);
  QSslCertificate crt = loadSSLCertFromFile(filename);
  if (crt.isNull())
  {
    QMessageBox::critical(this, tr("Invalid SSL certificate"), tr("File %1 does not contain any valid SSL certificate").arg(filename));
    return;
  }
  ui->edtUserCert->setPlainText(crt.toPem());

//  emit paramsModified();
}

//---------------------------------------------------------------------------
WidgetUserGroups::~WidgetUserGroups()
{
  save_ui_settings();
  delete ui;
}
