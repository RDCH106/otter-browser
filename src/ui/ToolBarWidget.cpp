/**************************************************************************
* Otter Browser: Web browser controlled by the user, not vice-versa.
* Copyright (C) 2015 Michal Dutkiewicz aka Emdek <michal@emdek.pl>
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
**************************************************************************/

#include "ToolBarWidget.h"
#include "ContentsWidget.h"
#include "MainWindow.h"
#include "Menu.h"
#include "TabBarWidget.h"
#include "Window.h"
#include "toolbars/AddressWidget.h"
#include "toolbars/BookmarkWidget.h"
#include "toolbars/GoBackActionWidget.h"
#include "toolbars/GoForwardActionWidget.h"
#include "toolbars/MenuActionWidget.h"
#include "toolbars/PanelChooserWidget.h"
#include "toolbars/SearchWidget.h"
#include "toolbars/StatusMessageWidget.h"
#include "toolbars/ZoomWidget.h"
#include "../core/BookmarksManager.h"
#include "../core/BookmarksModel.h"
#include "../core/Utils.h"
#include "../core/WindowsManager.h"

#include <QtGui/QMouseEvent>

namespace Otter
{

ToolBarWidget::ToolBarWidget(const ToolBarDefinition &definition, Window *window, QWidget *parent) : QToolBar(parent),
	m_mainWindow(qobject_cast<MainWindow*>(parent)),
	m_window(window),
	m_definition(definition)
{
	setObjectName(definition.name);
	setStyleSheet(QLatin1String("QToolBar {padding:0 3px;spacing:3px;}"));
	setAllowedAreas(Qt::AllToolBarAreas);
	setFloatable(false);
	setToolButtonStyle(definition.iconStyle);

	if (definition.iconSize > 0)
	{
		setIconSize(QSize(definition.iconSize, definition.iconSize));
	}

	if (!definition.bookmarksPath.isEmpty())
	{
		updateBookmarks();

		connect(BookmarksManager::getInstance(), SIGNAL(modelModified()), this, SLOT(updateBookmarks()));

		return;
	}

	bool hasTabBar = false;

	for (int i = 0; i < definition.actions.count(); ++i)
	{
		if (definition.actions.at(i).action == QLatin1String("separator"))
		{
			addSeparator();
		}
		else if (definition.actions.at(i).action == QLatin1String("spacer"))
		{
			QWidget *spacer = new QWidget(this);
			spacer->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);

			addWidget(spacer);
		}
		else if (definition.actions.at(i).action == QLatin1String("ClosedWindowsWidget"))
		{
			QAction *closedWindowsAction = new QAction(Utils::getIcon(QLatin1String("user-trash")), tr("Closed Tabs"), this);
			Menu *closedWindowsMenu = new Menu(this);
			closedWindowsMenu->setRole(ClosedWindowsMenu);

			closedWindowsAction->setMenu(closedWindowsMenu);
			closedWindowsAction->setEnabled(false);

			QToolButton *closedWindowsMenuButton = new QToolButton(this);
			closedWindowsMenuButton->setDefaultAction(closedWindowsAction);
			closedWindowsMenuButton->setAutoRaise(true);
			closedWindowsMenuButton->setPopupMode(QToolButton::InstantPopup);

			addWidget(closedWindowsMenuButton);
		}
		else if (definition.actions.at(i).action == QLatin1String("MenuWidget"))
		{
			addWidget(new MenuActionWidget(this));
		}
		else if (definition.actions.at(i).action == QLatin1String("AddressWidget"))
		{
			addWidget(new AddressWidget(m_window, this));
		}
		else if (definition.actions.at(i).action == QLatin1String("PanelChooserWidget"))
		{
			addWidget(new PanelChooserWidget(this));
		}
		else if (definition.actions.at(i).action == QLatin1String("SearchWidget"))
		{
			addWidget(new SearchWidget(m_window, this));
		}
		else if (definition.actions.at(i).action == QLatin1String("StatusMessageWidget"))
		{
			addWidget(new StatusMessageWidget(this));
		}
		else if (definition.actions.at(i).action == QLatin1String("ZoomWidget"))
		{
			addWidget(new ZoomWidget(this));
		}
		else if (definition.actions.at(i).action == QLatin1String("TabBarWidget") && !hasTabBar && definition.name == QLatin1String("TabBar"))
		{
			hasTabBar = true;

			TabBarWidget *tabBar = new TabBarWidget(this);
			MainWindow *window = qobject_cast<MainWindow*>(parent);

			if (window)
			{
				window->setTabBar(tabBar);
			}

			addWidget(tabBar);
		}
		else if (definition.actions.at(i).action.startsWith(QLatin1String("bookmarks:")))
		{
			BookmarksItem *bookmark = (definition.actions.at(i).action.startsWith(QLatin1String("bookmarks:/")) ? BookmarksManager::getModel()->getItem(definition.actions.at(i).action.mid(11)) : BookmarksManager::getBookmark(definition.actions.at(i).action.mid(11).toULongLong()));

			if (bookmark)
			{
				addWidget(new BookmarkWidget(bookmark, this));
			}
		}
		else
		{
			const int identifier = ActionsManager::getActionIdentifier(definition.actions.at(i).action.left(definition.actions.at(i).action.length() - 6));

			if (identifier >= 0)
			{
				Action *action = NULL;

				if (m_window && Action::isLocal(identifier))
				{
					action = m_window->getContentsWidget()->getAction(identifier);
				}
				else
				{
					action = ActionsManager::getAction(identifier, this);
				}

				if (action)
				{
					if (m_window)
					{
						action->setWindow(m_window);
					}

					addAction(action);
				}
			}
		}
	}

	if (!hasTabBar && definition.name == QLatin1String("TabBar"))
	{
		TabBarWidget *tabBar = new TabBarWidget(this);
		MainWindow *window = qobject_cast<MainWindow*>(parent);

		if (window)
		{
			window->setTabBar(tabBar);
		}

		addWidget(tabBar);
	}

	if (m_mainWindow)
	{
		connect(m_mainWindow->getWindowsManager(), SIGNAL(currentWindowChanged(qint64)), this, SLOT(notifyWindowChanged(qint64)));
	}

	connect(this, SIGNAL(topLevelChanged(bool)), this, SLOT(notifyAreaChanged()));
}

void ToolBarWidget::contextMenuEvent(QContextMenuEvent *event)
{
	QMenu menu(this);

	if (objectName() != QLatin1String("TabBar"))
	{
		menu.addAction(ActionsManager::getAction(Action::LockToolBarsAction, this));
		menu.exec(event->globalPos());

		return;
	}

	menu.addAction(ActionsManager::getAction(Action::NewTabAction, this));
	menu.addAction(ActionsManager::getAction(Action::NewTabPrivateAction, this));
	menu.addSeparator();

	QMenu *customizeMenu = menu.addMenu(tr("Customize"));
	QAction *cycleAction = customizeMenu->addAction(tr("Switch tabs using the mouse wheel"));
	cycleAction->setCheckable(true);
	cycleAction->setChecked(!SettingsManager::getValue(QLatin1String("TabBar/RequireModifierToSwitchTabOnScroll")).toBool());
	cycleAction->setEnabled(m_mainWindow->getTabBar());

	customizeMenu->addSeparator();
	customizeMenu->addAction(ActionsManager::getAction(Action::LockToolBarsAction, this));

	if (m_mainWindow->getTabBar())
	{
		connect(cycleAction, SIGNAL(toggled(bool)), m_mainWindow->getTabBar(), SLOT(setCycle(bool)));
	}

	menu.exec(event->globalPos());
}

void ToolBarWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
	if (event->button() == Qt::LeftButton && objectName() == QLatin1String("TabBar"))
	{
		ActionsManager::triggerAction((event->modifiers().testFlag(Qt::ShiftModifier) ? Action::NewTabPrivateAction : Action::NewTabAction), this);
	}
}

void ToolBarWidget::notifyAreaChanged()
{
	if (m_mainWindow)
	{
		emit areaChanged(m_mainWindow->toolBarArea(this));
	}
}

void ToolBarWidget::notifyWindowChanged(qint64 identifier)
{
	m_window = m_mainWindow->getWindowsManager()->getWindowByIdentifier(identifier);

	emit windowChanged(m_window);
}

void ToolBarWidget::updateBookmarks()
{
	clear();

	BookmarksItem *item = (m_definition.bookmarksPath.startsWith(QLatin1Char('#')) ? BookmarksManager::getBookmark(m_definition.bookmarksPath.mid(1).toULongLong()) : BookmarksManager::getModel()->getItem(m_definition.bookmarksPath));

	if (!item)
	{
		return;
	}

	for (int i = 0; i < item->rowCount(); ++i)
	{
		BookmarksItem *bookmark = dynamic_cast<BookmarksItem*>(item->child(i));

		if (bookmark)
		{
			if (static_cast<BookmarksItem::BookmarkType>(bookmark->data(BookmarksModel::TypeRole).toInt()) == BookmarksItem::SeparatorBookmark)
			{
				addSeparator();
			}
			else
			{
				addWidget(new BookmarkWidget(bookmark, this));
			}
		}
	}
}

}
