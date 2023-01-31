#include "MemWatchDelegate.h"

#include <QLineEdit>

#include "../../../MemoryEngine/MemoryWatch/MemWatchTreeNode.h"
#include "../GUICommon.h"
#include "MemWatchModel.h"

QWidget* MemWatchDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& option,
                                        const QModelIndex& index) const
{
  const MemWatchModel* model = static_cast<const MemWatchModel*>(index.model());
  MemWatchTreeNode* node = model->getTreeNodeFromIndex(index);
  QLineEdit* editor = new QLineEdit(parent);
  if (index.column() == MemWatchModel::WATCH_COL_VALUE && !node->isGroup())
    node->setValueEditing(true);
  GUICommon::g_valueEditing = true;
  return editor;
}

void MemWatchDelegate::setModelData(QWidget* editor, QAbstractItemModel* model,
                                    const QModelIndex& index) const
{
  QLineEdit* lineEditor = static_cast<QLineEdit*>(editor);
  if (!lineEditor->text().isEmpty())
    QStyledItemDelegate::setModelData(editor, model, index);
}

void MemWatchDelegate::destroyEditor(QWidget* editor, const QModelIndex& index) const
{
  const MemWatchModel* model = static_cast<const MemWatchModel*>(index.model());
  MemWatchTreeNode* node = model->getTreeNodeFromIndex(index);
  if (index.column() == MemWatchModel::WATCH_COL_VALUE && !node->isGroup())
    node->setValueEditing(false);
  GUICommon::g_valueEditing = false;
  editor->deleteLater();
}
