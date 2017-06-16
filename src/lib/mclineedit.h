/*
 Copyright (C) 2017 Nikolay N. Karikh <knn@qmtunnel.com>

 This file is part of qmtunnel and is licensed under GNU General Public License 3.0, with
 the additional special exception to link portions of this program with the OpenSSL library.
 See LICENSE file for more details.
*/

#ifndef MCLINEEDIT_H
#define MCLINEEDIT_H

#include <QLineEdit>

class MCLineEdit: public QLineEdit
{
    Q_OBJECT
public:
    explicit MCLineEdit(QWidget *parent=0);
    void setMultipleCompleter(QCompleter *_completer);
    void setSeparator(const QString &_sep);

protected:
    void keyPressEvent(QKeyEvent *e);

private:
    QString cursorWord(const QString& sentence) const;

private slots:
    void insertCompletion(const QString &arg);

private:
    QCompleter* c;
    QString separator;
};

#endif // MCLINEEDIT_H
