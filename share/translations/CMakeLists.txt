#  Copyright (C) 2017 KeePassXC Team <team@keepassxc.org>
#  Copyright (C) 2014 Felix Geyer <debfx@fobos.de>
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 2 or (at your option)
#  version 3 of the License.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.

file(GLOB TRANSLATION_FILES *.ts)
message(STATUS "Including translations...\n")

qt6_add_translation(QM_FILES ${TRANSLATION_FILES})

if(WIN32)
    file(GLOB QTBASE_TRANSLATIONS ${Qt6_PREFIX}/share/qt6/translations/qtbase_*.qm)
elseif(APPLE OR KEEPASSXC_DIST_APPIMAGE)
    file(GLOB QTBASE_TRANSLATIONS
            /usr/share/qt/translations/qtbase_*.qm
            /usr/share/qt6/translations/qtbase_*.qm
            ${Qt6_PREFIX}/translations/qtbase_*.qm)
endif()
set(QM_FILES ${QM_FILES} ${QTBASE_TRANSLATIONS})

if(WIN32)
    set(_lang_dest "languages")
else()
    set(_lang_dest "share/arsenic/languages")
endif()

install(FILES ${QM_FILES} DESTINATION ${_lang_dest} COMPONENT arsenic)

# Add arsenic_en.qm as a fallback for uncommon english locales
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/arsenic_en_US.qm
        DESTINATION ${_lang_dest} RENAME arsenic_en.qm COMPONENT arsenic)

add_custom_target(translations DEPENDS ${QM_FILES})
add_dependencies(arsenic translations)
