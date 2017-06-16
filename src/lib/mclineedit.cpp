/*
 Copyright (C) 2017 Nikolay N. Karikh <knn@qmtunnel.com>

 This file is part of qmtunnel and is licensed under GNU General Public License 3.0, with
 the additional special exception to link portions of this program with the OpenSSL library.
 See LICENSE file for more details.
*/

#include "mclineedit.h"
#include <QCompleter>
#include <QTextCursor>
#include <QAbstractItemView>
#include <QScrollBar>
#include <QKeyEvent>

//---------------------------------------------------------------------------
MCLineEdit::MCLineEdit(QWidget *parent): QLineEdit(parent)
{
  c = NULL;
}

//---------------------------------------------------------------------------
void MCLineEdit::keyPressEvent(QKeyEvent *event)
{
  QLineEdit::keyPressEvent(event);
  if (!c)
    return;

  c->setCompletionPrefix(cursorWord(this->text()));
  if (c->completionPrefix().length() < 1 &&
      event->key() != Qt::Key_Space &&
      event->key() != Qt::Key_Colon)
  {
    c->popup()->hide();
    return;
  }
  QRect cr = cursorRect();
  cr.setWidth(c->popup()->sizeHintForColumn(0)
              + c->popup()->verticalScrollBar()->sizeHint().width());
  c->complete(cr);

}

//---------------------------------------------------------------------------
QString MCLineEdit::cursorWord(const QString &sentence) const
{
  int pos = sentence.left(cursorPosition()).lastIndexOf(separator);
  return sentence.mid(pos + 1, cursorPosition() - pos - 1);
}

//---------------------------------------------------------------------------
void MCLineEdit::insertCompletion(const QString &arg)
{
  int pos = text().left(cursorPosition()).lastIndexOf(separator);
  setText(text().replace(pos + 1, cursorPosition() - pos - 1, arg+separator));
}

//---------------------------------------------------------------------------
void MCLineEdit::setSeparator(const QString &_sep)
{
  separator = _sep;
}

//---------------------------------------------------------------------------
void MCLineEdit::setMultipleCompleter(QCompleter* _completer)
{
  c = _completer;
  c->setWidget(this);
  connect(c, SIGNAL(activated(QString)),
          this, SLOT(insertCompletion(QString)));
}
