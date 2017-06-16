/*
 Copyright (C) 2017 Nikolay N. Karikh <knn@qmtunnel.com>

 This file is part of qmtunnel and is licensed under GNU General Public License 3.0, with
 the additional special exception to link portions of this program with the OpenSSL library.
 See LICENSE file for more details.
*/

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "gui_settings.h"
#include <QFileDialog>
#include <QMessageBox>
#include "../lib/mgrclient-conn.h"
#include "../lib/mgrserver-parameters.h"

#define PROFILE_MAX_SIZE                1024*1024
#define MAX_RECENT_PROFILES                     5
#define PROFILE_PATH_MAX_LENGTH                40

QString cur_profile_filename;
QString cur_profile_short_filename;
bool cur_profile_modified=false;
QStringList recent_profiles;
extern QHash<quint32, QTreeWidgetItem *> mgrconn_list;
extern QHash<quint64, QTreeWidgetItem *> tunnel_list;
quint32 unique_mgrconn_id=1;

//---------------------------------------------------------------------------
void MainWindow::mod_profile_init()
{
  this->setWindowTitle(tr("qmtunnel GUI v%1.%2").arg(GUI_MAJOR_VERSION).arg(GUI_MINOR_VERSION));

  for (int i=0; i < MAX_RECENT_PROFILES; i++)
  {
    QAction *action = new QAction(this);
    ui->menu_File->insertAction(ui->action_exit, action);
    action->setVisible(false);
    connect(action, SIGNAL(triggered()), this, SLOT(profile_open_recent()));
    recent_profile_actions.append(action);
  }
  ui->menu_File->insertSeparator(ui->action_exit);

  cJSON *j_recentProfiles = cJSON_GetObjectItem(j_gui_settings, "recentProfiles");
  if (j_recentProfiles && j_recentProfiles->type == cJSON_Array)
  {
    for (int i=0; i < cJSON_GetArraySize(j_recentProfiles); i++)
    {
      cJSON *j = cJSON_GetArrayItem(j_recentProfiles, i);
      if (j->type == cJSON_String)
        recent_profiles.append(QString::fromUtf8(j->valuestring));
    }
    while (recent_profiles.count() > MAX_RECENT_PROFILES)
      recent_profiles.removeAt(0);
  }

  if ((gui_flags & GFL_AUTOOPEN_RECENT_PROFILE_ON_START) && !recent_profiles.isEmpty())
  {
    QString filename = recent_profiles.last();
    if (QFile::exists(filename))
      profile_load(filename);
  }
  update_recent_profile_actions();
}

//---------------------------------------------------------------------------
void MainWindow::profile_modified()
{
  cur_profile_modified = true;
  ui->action_profile_save->setEnabled(true);
  if (!cur_profile_short_filename.isEmpty())
    this->setWindowTitle(tr("qmtunnel GUI v%1.%2 - %3*").arg(GUI_MAJOR_VERSION).arg(GUI_MINOR_VERSION).arg(cur_profile_short_filename));
}

//---------------------------------------------------------------------------
void MainWindow::save_recent_profiles()
{
  cJSON *j_recentProfiles = cJSON_GetObjectItem(j_gui_settings, "recentProfiles");
  if (j_recentProfiles)
    cJSON_DeleteItemFromObject(j_gui_settings, "recentProfiles");
  if (!recent_profiles.isEmpty())
  {
    j_recentProfiles = cJSON_CreateArray();
    cJSON_AddItemToObject(j_gui_settings, "recentProfiles", j_recentProfiles);
    for (int i=0; i < recent_profiles.count(); i++)
      cJSON_AddItemToArray(j_recentProfiles, cJSON_CreateString(recent_profiles[i].toUtf8()));
  }
  ui_save();
}

//---------------------------------------------------------------------------
void MainWindow::update_recent_profile_actions()
{
  bool has_long_names = false;
  for (int i=0; i < recent_profiles.count(); i++)
  {
    if (recent_profiles[i].length() > PROFILE_PATH_MAX_LENGTH)
    {
      has_long_names = true;
      break;
    }
  }

  int action_index = 0;
  for (int i=0; i < MAX_RECENT_PROFILES; i++)
  {
    if (i < recent_profiles.count())
    {
      if (recent_profiles[i] == cur_profile_filename)
        continue;
      recent_profile_actions[action_index]->setText(has_long_names ? QFileInfo(recent_profiles[i]).fileName() : recent_profiles[i]);
      recent_profile_actions[action_index]->setData(recent_profiles[i]);
      recent_profile_actions[action_index]->setVisible(true);
    }
    else
      recent_profile_actions[action_index]->setVisible(false);
    action_index++;
  }
}

//---------------------------------------------------------------------------
void MainWindow::profile_open_recent()
{
  QAction *action = qobject_cast<QAction *>(sender());
  if (!action)
    return;
  if (cur_profile_modified)
  {
    if (QMessageBox::warning(this, tr("Forgot to save?"), tr("Current profile has been modified.\n"
                                                             "Are you sure you want to discard changes and open another profile?"),
                         QMessageBox::Yes | QMessageBox::No) == QMessageBox::No)
    {
      return;
    }
  }
  QString filename = action->data().toString();
  if (QFile::exists(filename))
    profile_load(filename);
}

//---------------------------------------------------------------------------
void MainWindow::on_action_profile_new_triggered()
{
  if (cur_profile_modified)
  {
    if (QMessageBox::warning(this, tr("Forgot to save?"), tr("Current profile has been modified.\n"
                                                             "Are you sure you want to discard changes and begin new profile?"),
                         QMessageBox::Yes | QMessageBox::No) == QMessageBox::No)
    {
      return;
    }
  }
  cur_profile_short_filename.clear();
  cur_profile_filename.clear();
  this->setWindowTitle(tr("qmtunnel GUI v%1.%2").arg(GUI_MAJOR_VERSION).arg(GUI_MINOR_VERSION));
  ui->action_profile_save->setEnabled(false);
  ui->browserTree->clear();
  mgrconn_list.clear();
  tunnel_list.clear();
  page_mgrConn_clear();
  cur_profile_modified = false;
  unique_mgrconn_id = 1;
}

//---------------------------------------------------------------------------
void MainWindow::on_action_profile_open_triggered()
{
  if (cur_profile_modified)
  {
    if (QMessageBox::warning(this, tr("Forgot to save?"), tr("Current profile has been modified.\n"
                                                             "Are you sure you want to discard changes and open another profile?"),
                         QMessageBox::Yes | QMessageBox::No) == QMessageBox::No)
    {
      return;
    }
  }
  QFileDialog fileDialog(this);
  fileDialog.setFileMode(QFileDialog::ExistingFile);
  fileDialog.setAcceptMode(QFileDialog::AcceptOpen);
  fileDialog.setNameFilters(QStringList() << "qmtunnel profile (*.qmt)" << "Any files (*)");

  cJSON *j_fileDialogs = cJSON_GetObjectItem(j_gui_settings, "fileDialogs");
  if (!j_fileDialogs)
  {
    j_fileDialogs = cJSON_CreateObject();
    cJSON_AddItemToObject(j_gui_settings, "fileDialogs", j_fileDialogs);
  }
  cJSON *j_fileDialog_profile = cJSON_GetObjectItem(j_fileDialogs, "profile");
  if (j_fileDialog_profile)
  {
    cJSON *j;
    j = cJSON_GetObjectItem(j_fileDialog_profile, "geometry");
    if (j && j->type == cJSON_String)
      fileDialog.restoreGeometry(QByteArray::fromBase64(j->valuestring));
    j = cJSON_GetObjectItem(j_fileDialog_profile, "state");
    if (j && j->type == cJSON_String)
      fileDialog.restoreState(QByteArray::fromBase64(j->valuestring));
    j = cJSON_GetObjectItem(j_fileDialog_profile, "filter");
    if (j && j->type == cJSON_String)
      fileDialog.selectNameFilter(QString::fromUtf8(j->valuestring));
  }
  if (!fileDialog.exec())
    return;

  if (j_fileDialog_profile)
    cJSON_DeleteItemFromObject(j_fileDialogs, "profile");
  j_fileDialog_profile = cJSON_CreateObject();
  cJSON_AddItemToObject(j_fileDialogs, "profile", j_fileDialog_profile);

  cJSON_AddStringToObject(j_fileDialog_profile, "geometry", fileDialog.saveGeometry().toBase64());
  cJSON_AddStringToObject(j_fileDialog_profile, "state", fileDialog.saveState().toBase64());
  cJSON_AddStringToObject(j_fileDialog_profile, "filter", fileDialog.selectedNameFilter().toUtf8());

  ui_save();

  if (fileDialog.selectedFiles().isEmpty())
    return;
  QString filename = fileDialog.selectedFiles().at(0);
  profile_load(filename);
}

//---------------------------------------------------------------------------
void MainWindow::on_action_profile_save_triggered()
{
  if (cur_profile_filename.isEmpty())
  {
    on_action_profile_saveAs_triggered();
    return;
  }
  profile_save(cur_profile_filename);
}

//---------------------------------------------------------------------------
void MainWindow::on_action_profile_saveAs_triggered()
{
  QFileDialog fileDialog(this);
  fileDialog.setFileMode(QFileDialog::AnyFile);
  fileDialog.setDefaultSuffix("qmt");
  fileDialog.setAcceptMode(QFileDialog::AcceptSave);
  fileDialog.setNameFilters(QStringList() << "qmtunnel profile (*.qmt)" << "Any files (*)");

  cJSON *j_fileDialogs = cJSON_GetObjectItem(j_gui_settings, "fileDialogs");
  if (!j_fileDialogs)
  {
    j_fileDialogs = cJSON_CreateObject();
    cJSON_AddItemToObject(j_gui_settings, "fileDialogs", j_fileDialogs);
  }
  cJSON *j_fileDialog_profile = cJSON_GetObjectItem(j_fileDialogs, "profile");
  if (j_fileDialog_profile)
  {
    cJSON *j;
    j = cJSON_GetObjectItem(j_fileDialog_profile, "geometry");
    if (j && j->type == cJSON_String)
      fileDialog.restoreGeometry(QByteArray::fromBase64(j->valuestring));
    j = cJSON_GetObjectItem(j_fileDialog_profile, "state");
    if (j && j->type == cJSON_String)
      fileDialog.restoreState(QByteArray::fromBase64(j->valuestring));
    j = cJSON_GetObjectItem(j_fileDialog_profile, "filter");
    if (j && j->type == cJSON_String)
      fileDialog.selectNameFilter(QString::fromUtf8(j->valuestring));
  }
  if (!fileDialog.exec())
    return;

  if (j_fileDialog_profile)
    cJSON_DeleteItemFromObject(j_fileDialogs, "profile");
  j_fileDialog_profile = cJSON_CreateObject();
  cJSON_AddItemToObject(j_fileDialogs, "profile", j_fileDialog_profile);

  cJSON_AddStringToObject(j_fileDialog_profile, "geometry", fileDialog.saveGeometry().toBase64());
  cJSON_AddStringToObject(j_fileDialog_profile, "state", fileDialog.saveState().toBase64());
  cJSON_AddStringToObject(j_fileDialog_profile, "filter", fileDialog.selectedNameFilter().toUtf8());

  ui_save();

  if (fileDialog.selectedFiles().isEmpty())
    return;
  QString filename = fileDialog.selectedFiles().at(0);
  if (QFile::exists(filename) && filename != cur_profile_filename)
  {
    if (QMessageBox::warning(this, tr("Overwriting existing file"), tr("File %1 already exists.\n"
                                                                 "Are you sure you want to overwrite it?").arg(filename),
                         QMessageBox::Yes | QMessageBox::No) == QMessageBox::No)
      return;
  }
  if (profile_save(filename))
  {
    if (recent_profiles.contains(filename))
      recent_profiles.removeOne(filename);
    recent_profiles.append(filename);
    while (recent_profiles.count() > MAX_RECENT_PROFILES)
      recent_profiles.removeAt(0);
    save_recent_profiles();
    update_recent_profile_actions();
  }
}

//---------------------------------------------------------------------------
void MainWindow::parseBrowserTreeItems(cJSON *j_items, QTreeWidgetItem *parent_item)
{
  int item_count = cJSON_GetArraySize(j_items);
  for (int item_index=0; item_index < item_count; item_index++)
  {
    cJSON *j_item = cJSON_GetArrayItem(j_items, item_index);
    cJSON *j = cJSON_GetObjectItem(j_item, "folder_name");
    QTreeWidgetItem *item;
    if (j && j->type == cJSON_String)
    {
      if (parent_item)
        item = new QTreeWidgetItem(parent_item, QStringList() << QString::fromUtf8(j->valuestring));
      else
        item = new QTreeWidgetItem(ui->browserTree, QStringList() << QString::fromUtf8(j->valuestring));
      item->setFlags(item->flags() | Qt::ItemIsEditable);
      item->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
      item->setData(0, ItemTypeRole, (int)ITEM_TYPE_FOLDER);
    }
    else
    {
      MgrClientParameters *mgrconn_params = new MgrClientParameters;
      mgrconn_params->parseJSON(j_item);
      while (mgrconn_params->id == 0)
        mgrconn_params->id = unique_mgrconn_id++;
      // ensure all items have distinct unique IDs
      while (mgrconn_list.contains(mgrconn_params->id))
      {
        mgrconn_params->id = unique_mgrconn_id++;
        if (mgrconn_params->id == 0)
          mgrconn_params->id = unique_mgrconn_id++;
      }
      if (parent_item)
        item = new QTreeWidgetItem(parent_item, QStringList() << mgrconn_params->name);
      else
        item = new QTreeWidgetItem(ui->browserTree, QStringList() << mgrconn_params->name);
      mgrconn_list.insert(mgrconn_params->id, item);
      item->setFlags(item->flags() | Qt::ItemIsEditable);
      item->setIcon(0, style()->standardIcon(QStyle::SP_ComputerIcon));
      item->setData(0, ItemTypeRole, (int)ITEM_TYPE_MGRCONN);
      item->setData(0, MgrClientParametersRole, QVariant::fromValue(mgrconn_params));
      item->setData(0, MgrServerConfigRole, QVariant::fromValue(((cJSON*)NULL)));
      item->setIcon(1, QIcon(":/images/icons/circle16-gray.png"));
      item->setData(1, MgrConnStateRole, (int)MgrClientConnection::MGR_NONE);
    }

    cJSON *j_subitems = cJSON_GetObjectItem(j_item, "items");
    if (j_subitems && j_subitems->type == cJSON_Array)
      parseBrowserTreeItems(j_subitems, item);
  }
}

//---------------------------------------------------------------------------
void MainWindow::profile_load(const QString &filename)
{
  QApplication::setOverrideCursor(Qt::WaitCursor);
  QFile f(filename);
  if (!f.open(QIODevice::ReadOnly))
  {
    QApplication::restoreOverrideCursor();
    QMessageBox::critical(this, tr("Unable to open file"), tr("Unable to open file %1 for reading").arg(filename));
    return;
  }
  QByteArray buffer = f.read(PROFILE_MAX_SIZE);
  f.close();
  if (buffer.isEmpty())
  {
    QApplication::restoreOverrideCursor();
    QMessageBox::critical(this, tr("File is empty"), tr("File %1 is empty").arg(filename));
    return;
  }
  cJSON *j_profile = cJSON_Parse(buffer);
  if (!j_profile)
  {
    QApplication::restoreOverrideCursor();
    QMessageBox::critical(this, tr("Parsing error"), tr("Unable to parse profile %1").arg(filename));
    return;
  }
  cJSON *j_items = cJSON_GetObjectItem(j_profile, "items");
  if (!j_items || j_items->type != cJSON_Array)
  {
    QApplication::restoreOverrideCursor();
    QMessageBox::critical(this, tr("Parsing error"), tr("Unable to parse profile %1").arg(filename));
    return;
  }
  cJSON *j = cJSON_GetObjectItem(j_profile, "unique_id");
  if (j && j->type == cJSON_Number)
    unique_mgrconn_id = j->valueint;
  else
    unique_mgrconn_id = 1;
  ui->browserTree->clear();
  mgrconn_list.clear();
  tunnel_list.clear();
  page_mgrConn_clear();
  parseBrowserTreeItems(j_items, NULL);
  if (this->receivers(SIGNAL(gui_profileLoaded(cJSON *))) == 1)
    emit gui_profileLoaded(j_profile);
  else
    cJSON_Delete(j_profile);

  ui->browserTree->expandAll();

  if (recent_profiles.contains(filename))
    recent_profiles.removeOne(filename);
  recent_profiles.append(filename);
  while (recent_profiles.count() > MAX_RECENT_PROFILES)
    recent_profiles.removeAt(0);
  save_recent_profiles();
  update_recent_profile_actions();

  cur_profile_filename = cur_profile_short_filename = filename;
  if (cur_profile_filename.length() > PROFILE_PATH_MAX_LENGTH)
    cur_profile_short_filename = QFileInfo(cur_profile_filename).fileName();
  this->setWindowTitle(tr("qmtunnel GUI v%1.%2 - %3").arg(GUI_MAJOR_VERSION).arg(GUI_MINOR_VERSION).arg(cur_profile_short_filename));
  ui->action_profile_save->setEnabled(false);
  cur_profile_modified = false;

  QApplication::restoreOverrideCursor();

  if (ui->browserTree->topLevelItemCount() > 0 && this->isVisible())
    ui->browserTree->setCurrentItem(ui->browserTree->topLevelItem(0));
}

//---------------------------------------------------------------------------
void MainWindow::printBrowserTreeItems(cJSON *j_items, QTreeWidgetItem *parent_item)
{
  int n_items = parent_item ? parent_item->childCount() : ui->browserTree->topLevelItemCount();
  for (int i=0; i < n_items; i++)
  {
    QTreeWidgetItem *item;
    if (parent_item)
      item = parent_item->child(i);
    else
      item = ui->browserTree->topLevelItem(i);
    cJSON *j_item = cJSON_CreateObject();
    if (item->data(0, ItemTypeRole).toInt() == ITEM_TYPE_MGRCONN)
    {
      MgrClientParameters *mgrconn_params = item->data(0, MgrClientParametersRole).value<MgrClientParameters *>();
      mgrconn_params->printJSON(j_item);
    }
    else if (item->data(0, ItemTypeRole).toInt() == ITEM_TYPE_FOLDER)
    {
      cJSON_AddStringToObject(j_item, "folder_name", item->text(0).toUtf8());
    }
    else
    {
      cJSON_Delete(j_item);
      continue;
    }
    cJSON_AddItemToArray(j_items, j_item);

    if (item->childCount() > 0 && item->data(0, ItemTypeRole).toInt() == ITEM_TYPE_FOLDER)
    {
      cJSON *j_subitems = cJSON_CreateArray();
      cJSON_AddItemToObject(j_item, "items", j_subitems);
      printBrowserTreeItems(j_subitems, item);
    }
  }
}

//---------------------------------------------------------------------------
bool MainWindow::profile_save(const QString &filename)
{
  QApplication::setOverrideCursor(Qt::WaitCursor);
  cJSON *j_profile = cJSON_CreateObject();
  cJSON *j_items = cJSON_CreateArray();
  cJSON_AddItemToObject(j_profile, "items", j_items);
  printBrowserTreeItems(j_items, NULL);
  cJSON_AddNumberToObject(j_profile, "unique_id", unique_mgrconn_id);

  char *buffer = cJSON_PrintBuffered(j_profile, PROFILE_MAX_SIZE, 1);
  cJSON_Delete(j_profile);

  QFile f(filename);
  if (!f.open(QIODevice::WriteOnly))
  {
    free(buffer);
    QApplication::restoreOverrideCursor();
    QMessageBox::critical(this, tr("Unable to open file"), tr("Unable to open file %1 for writing").arg(filename));
    return false;
  }
  if (f.write(buffer) <= 0)
  {
    free(buffer);
    f.close();
    QApplication::restoreOverrideCursor();
    QMessageBox::critical(this, tr("Unable to write"), tr("Unable to write to file %1").arg(filename));
    return false;
  }
  free(buffer);
  f.close();
  QApplication::restoreOverrideCursor();

  cur_profile_filename = cur_profile_short_filename = filename;
  if (cur_profile_filename.length() > PROFILE_PATH_MAX_LENGTH)
    cur_profile_short_filename = QFileInfo(cur_profile_filename).fileName();
  ui->action_profile_save->setEnabled(false);
  this->setWindowTitle(tr("qmtunnel GUI v%1.%2 - %3").arg(GUI_MAJOR_VERSION).arg(GUI_MINOR_VERSION).arg(cur_profile_short_filename));
  cur_profile_modified = false;
  update_mgrConn_buttons();
  return true;
}
