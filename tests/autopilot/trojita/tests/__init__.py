# -*- Mode: Python; coding: utf-8; indent-tabs-mode: nil; tab-width: 4 -*-

#   Copyright (C) 2014 Dan Chapman <dpniel@ubuntu.com>

#   This file is part of the Trojita Qt IMAP e-mail client,
#   http://trojita.flaska.net/

#   This program is free software; you can redistribute it and/or
#   modify it under the terms of the GNU General Public License as
#   published by the Free Software Foundation; either version 2 of
#   the License or (at your option) version 3 or any later version
#   accepted by the membership of KDE e.V. (or its successor approved
#   by the membership of KDE e.V.), which shall act as a proxy
#   defined in Section 14 of version 3 of the license.

#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.

#   You should have received a copy of the GNU General Public License
#   along with this program.  If not, see <http://www.gnu.org/licenses/>.

import os
import os.path
import logging

from autopilot.input import Mouse, Touch, Pointer
from autopilot.platform import model
from autopilot.testcase import AutopilotTestCase

from ubuntuuitoolkit import (
    base,
    emulators as toolkit_emulators
)

from trojita import emulators

logger = logging.getLogger(__name__)

class TrojitaAppTestCase(AutopilotTestCase):
    """A common test case class that provides several useful methods for
       trojita tests."""

    if model() == 'Desktop':
        scenarios = [('with mouse', dict(input_device_class=Mouse))]
    else:
        scenarios = [('with touch', dict(input_device_class=Touch))]

    local_location_binary = "../trojita"
    local_main_qml_file = "../qml/trojita/main.qml"
    installed_location_binary= "/usr/bin/trojita"

    def setUp(self):
        self.pointing_device = Pointer(self.input_device_class.create())
        super(TrojitaAppTestCase, self).setUp()

        if os.path.exists(self.local_location_binary):
            self.launch_test_local()
        elif os.path.exists(self.installed_location_binary):
            self.launch_test_installed()
        else:
            self.launch_test_click()

    def launch_test_local(self):
        logger.debug("Launching via local")
        self.app = self.launch_test_application(
            self.local_location_binary,
            "-a", self.local_main_qml_file,
            app_type='qt',
            emulator_base=toolkit_emulators.UbuntuUIToolkitEmulatorBase)

    def launch_test_installed(self):
        logger.debug("Launching via installation")
        self.app = self.launch_test_application(
            self.installed_location_binary,
            "--desktop_file_hint=/usr/share/applications/"
            "trojita.desktop",
            app_type='qt',
            emulator_base=toolkit_emulators.UbuntuUIToolkitEmulatorBase)


    def launch_test_click(self):
        logger.debug("Launching via click")
        self.app = self.launch_click_package(
            "com.ubuntu.trojita",
            emulator_base=toolkit_emulators.UbuntuUIToolkitEmulatorBase)

    @property
    def main_view(self):
        return self.app.select_single(emulators.MainView)
