/**********************************************************************
 * MTreeView class - QTreeView with usability improvements.
 **********************************************************************
 *   Copyright (C) 2021 by AK-47
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 *
 * This file is part of the gazelle-installer.
 *********************************************************************/

#include <QPainter>
#include <QHeaderView>
#include "mtreeview.h"

MTreeView::MTreeView(QWidget *parent)
    : QTreeView(parent)
{
    setTabKeyNavigation(true);
    setSelectionBehavior(SelectItems);
    setEditTriggers(DoubleClicked | SelectedClicked | EditKeyPressed | AnyKeyPressed);
}

QModelIndex MTreeView::moveCursor(CursorAction cursorAction, Qt::KeyboardModifiers modifiers)
{
    // Usability improvement: move horizontally instead of vertically.
    // Also, only move to the next cell that is editable.
    if (cursorAction == MoveNext || cursorAction == MovePrevious) {
        QModelIndex index = currentIndex();
        const Qt::ItemFlags reqflags = Qt::ItemIsEditable | Qt::ItemIsUserCheckable;
        do {
            if (cursorAction == MoveNext) {
                if ((index.column() + 1) < model()->columnCount()) {
                    index = index.sibling(index.row(), index.column() + 1);
                } else if ((index.row() + 1) < model()->rowCount(index.parent())) {
                    index = index.sibling(index.row() + 1, 0);
                } else {
                    return QModelIndex();
                }
            } else if (cursorAction == MovePrevious) {
                if (index.column() > 0) {
                    index = index.sibling(index.row(), index.column() - 1);
                } else if (index.row() > 0) {
                    index = index.sibling(index.row() - 1, model()->columnCount() - 1);
                } else {
                    return QModelIndex();
                }
            }
        } while (index.isValid() && !((index.flags() & Qt::ItemIsEnabled) && (index.flags() & reqflags)));
        return index;
    }
    return QTreeView::moveCursor(cursorAction, modifiers);
}

// Draw the grid.
void MTreeView::drawRow(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QTreeView::drawRow(painter, option, index);
    QPalette::ColorRole crole = QPalette::Text;
    if (selectionModel()->rowIntersectsSelection(index.row(), index.parent())) {
        crole = QPalette::Highlight;
    }
    QColor color = option.palette.color(QPalette::Active, crole);
    color.setAlpha(128);
    QPen pen = painter->pen();
    pen.setColor(color);
    pen.setStyle(Qt::DashDotDotLine);
    painter->setPen(pen);
    const int x = visualRect(index).x();
    int y = option.rect.y();
    // Vertical lines
    painter->drawLine(x, y, x, y + option.rect.height());
    painter->save();
    painter->translate(visualRect(model()->index(0, 0)).x() - indentation() - 0.5, -0.5);
    for (int ixi = 0; ixi < header()->count(); ++ixi) {
        painter->translate(header()->sectionSize(ixi), 0);
        painter->drawLine(0, y, 0, y + option.rect.height());
    }
    painter->restore();
    // Horizontal lines
    int width = option.rect.width();
    if (x < 0) width -= x;
    y = visualRect(index).top();
    painter->drawLine(x, y, width, y);
    y = visualRect(index).bottom();
    painter->drawLine(x, y, width, y);
}

void MTreeView::selectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    viewport()->update();
    QTreeView::selectionChanged(selected, deselected);
}
