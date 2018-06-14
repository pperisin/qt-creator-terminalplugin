DEFINES += TERMINALPLUGIN_LIBRARY

HEADERS += terminalplugin.h \
           terminalwindow.h \
           findsupport.h

SOURCES += terminalplugin.cpp \
           terminalwindow.cpp \
           findsupport.cpp

## set the QTC_SOURCE environment variable to override the setting here
QTCREATOR_SOURCES = $$(QTC_SOURCE)
isEmpty(QTCREATOR_SOURCES):QTCREATOR_SOURCES=NO_PATH_SPECIFIED_FOR_QTC_SOURCE

## set the QTC_BUILD environment variable to override the setting here
IDE_BUILD_TREE = $$(QTC_BUILD)
isEmpty(IDE_BUILD_TREE):IDE_BUILD_TREE=NO_PATH_SPECIFIED_FOR_QTC_BUILD

## uncomment to build plugin into user config directory
## <localappdata>/plugins/<ideversion>
##    where <localappdata> is e.g.
##    "%LOCALAPPDATA%\QtProject\qtcreator" on Windows Vista and later
##    "$XDG_DATA_HOME/data/QtProject/qtcreator" or "~/.local/share/data/QtProject/qtcreator" on Linux
##    "~/Library/Application Support/QtProject/Qt Creator" on OS X
#USE_USER_DESTDIR = yes

QTC_PLUGIN_NAME = TerminalPlugin
QTC_LIB_DEPENDS += \
    # nothing here at this time

QTC_PLUGIN_DEPENDS += \
    coreplugin texteditor

QTC_PLUGIN_RECOMMENDS += \
    # optional plugin dependencies. nothing here at this time

# Set the QTERMWIDGET environment variable to point to the install path
# of qtermwidget5, if it's not in the default library paths
QTERMWIDGET_PREFIX = $$(QTERMWIDGET)
!isEmpty(QTERMWIDGET_PREFIX) {
    INCLUDEPATH += -I$(QTERMWIDGET_PREFIX)/include
    LIBS += -L$(QTERMWIDGET_PREFIX)/lib
}
LIBS += -lqtermwidget5

include($$QTCREATOR_SOURCES/src/qtcreatorplugin.pri)
