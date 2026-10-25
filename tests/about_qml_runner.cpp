// SPDX-FileCopyrightText: 2026 Dennis Burgermeister
// SPDX-License-Identifier: GPL-3.0-or-later

// Some SDK targets ship Qt Quick Test's runtime library and QML plugin without
// the qmltestrunner executable or QuickTest development headers.
extern int quick_test_main(int argc, char **argv, const char *name, const char *sourceDir);

int main(int argc, char **argv)
{
    return quick_test_main(argc, argv, "dencla_about", nullptr);
}
