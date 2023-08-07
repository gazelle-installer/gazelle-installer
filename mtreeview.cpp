/**********************************************************************
 * MTreeView class - QTreeView with usability improvements.
 *
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

MTreeView::MTreeView(QWidget *parent) noexcept
    : QTreeView(parent), gridon(false)
{
    setTabKeyNavigation(true);
    setSelectionBehavior(SelectRows);
    setEditTriggers(CurrentChanged | DoubleClicked | SelectedClicked | EditKeyPressed | AnyKeyPressed);
}

QModelIndex MTreeView::moveCursor(CursorAction cursorAction, Qt::KeyboardModifiers modifiers) noexcept
{
    // Usability improvement: move horizontally instead of vertically.
    // Also, only move to the next cell that is editable.
    QModelIndex index = currentIndex();
    const bool spanned = isFirstColumnSpanned(index.row(), index.parent());
    if (cursorAction == MoveNext || cursorAction == MovePrevious // Tab keys
        || cursorAction == MoveLeft || cursorAction == MoveRight) { // Arrow keys
        while (index.isValid()) {
            const int vcol = header()->visualIndex(index.column());
            if (cursorAction == MovePrevious || cursorAction == MoveLeft) {
                if (vcol > 0 && !spanned) {
                    index = index.sibling(index.row(), header()->logicalIndex(vcol - 1));
                } else if (cursorAction == MoveLeft && model()->hasChildren(index) && isExpanded(index)) {
                    collapse(index);
                } else if (index.row() > 0) {
                    index = index.sibling(index.row() - 1,
                        header()->logicalIndex(model()->columnCount(index) - 1));
                } else {
                    return QModelIndex();
                }
            } else if (cursorAction == MoveNext || cursorAction == MoveRight) {
                const QModelIndex &stump = index.siblingAtColumn(0);
                if ((vcol + 1) < model()->columnCount(index) && !spanned) {
                    index = index.sibling(index.row(), header()->logicalIndex(vcol + 1));
                } else if (cursorAction == MoveRight && model()->hasChildren(stump) && !isExpanded(stump)) {
                    expand(stump);
                } else if ((index.row() + 1) < model()->rowCount(index.parent())) {
                    index = index.sibling(index.row() + 1, header()->logicalIndex(0));
                } else {
                    return QModelIndex();
                }
            }
            if (index.flags() & Qt::ItemIsEnabled) {
                if (index.flags() & (Qt::ItemIsEditable | Qt::ItemIsUserCheckable)) break;
                if (cursorAction == MoveLeft || cursorAction == MoveRight) break;
            }
        }
        return index;
    } else if (cursorAction == MoveUp || cursorAction == MoveDown) {
        // Prevent the cursor latching onto an invalid cell of a spanned item.
        const bool down = (cursorAction == MoveDown);
        int newrow = index.row() + (down ? 1 : -1);
        int newcol = spanned ? lastColumn : index.column();
        const QModelIndex &sibling = index.sibling(down ? index.row() : newrow, 0);
        if (cursorAction == MoveUp && model()->hasChildren(sibling) && isExpanded(sibling)) {
            newrow = model()->rowCount(sibling) - 1;
            index = model()->index(newrow, newcol, sibling);
        } else if (cursorAction == MoveDown && model()->hasChildren(sibling) && isExpanded(sibling)) {
            newrow = 0;
            index = model()->index(0, 0, sibling);
        } else if (newrow < 0 || newrow >= model()->rowCount(index.parent())) {
            index = index.parent();
            if (!index.isValid()) return QModelIndex();
            if (newrow < 0) newrow = index.row();
            else newrow = index.row() + 1;
        }
        if (isFirstColumnSpanned(newrow, index.parent())) {
            if (!spanned) lastColumn = currentIndex().column();
            newcol = 0;
        }
        return index.sibling(newrow, newcol);
    }
    return QTreeView::moveCursor(cursorAction, modifiers);
}

// Draw the grid.
void MTreeView::drawRow(QPainter *painter,
    const QStyleOptionViewItem &option, const QModelIndex &index) const noexcept
{
    QTreeView::drawRow(painter, option, index);
    QColor color = option.palette.color(QPalette::Active, QPalette::Text);
    color.setAlpha(128);
    QPen pen = painter->pen();
    pen.setColor(color);
    pen.setDashPattern({3, 5});
    int rowindent = 0;
    for (QModelIndex in = index; in.isValid(); in = in.parent()) {
        rowindent += indentation();
    }
    pen.setDashOffset(rowindent);
    pen.setJoinStyle(Qt::MiterJoin);
    if (gridon && !isFirstColumnSpanned(index.row(), index.parent())) {
        painter->save();
        painter->setPen(pen);
        const int x = visualRect(index).x();
        int y = visualRect(index).y();
        // Horizontal lines
        int width = option.rect.width();
        if (x < 0) width -= x;
        painter->drawLine(x, y, width, y);
        y = visualRect(index).bottom();
        painter->drawLine(x, y, width, y);
        // Vertical lines
        y = option.rect.y() + pen.width();
        const int y2 = option.rect.bottom() - pen.width();
        painter->drawLine(x, y, x, y2);
        const int last = header()->count() - 1;
        const bool spanned = isFirstColumnSpanned(index.row(), index.parent());
        painter->translate(visualRect(model()->index(0, 0)).x() - indentation() - 0.5, 0);
        for (int ixi = 0; ixi <= last; ++ixi) {
            painter->translate(header()->sectionSize(header()->logicalIndex(ixi)), 0);
            if (!spanned || ixi == last) painter->drawLine(0, y, 0, y2);
        }
        painter->restore();
    }
    // Cursor
    const QRect &cirect = visualRect(currentIndex());
    if (currentIndex().isValid() && cirect.isValid()) {
        painter->save();
        const QColor &c = option.palette.color(QPalette::Active, QPalette::Highlight);
        pen.setColor(QColor(255 - c.red(), 255 - c.green(), 255 - c.blue()));
        pen.setWidth(2);
        painter->setPen(pen);
        painter->translate(pen.widthF() / 2, pen.widthF() / 2);
        painter->drawRect(cirect.adjusted(0, 0, -pen.width(), -pen.width()));
        painter->restore();
    }
}

void MTreeView::selectionChanged(const QItemSelection &selected, const QItemSelection &deselected) noexcept
{
    viewport()->update();
    QTreeView::selectionChanged(selected, deselected);
}

void MTreeView::setGrid(bool on) noexcept
{
    if (gridon != on) {
        gridon = on;
        viewport()->update();
    }
}
