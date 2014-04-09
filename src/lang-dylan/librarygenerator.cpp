/****************************************************************************
**
** Copyright (C) 1992-2009 Nokia. All rights reserved.
**
** This file is part of Qt Jambi.
**
** ** $BEGIN_LICENSE$
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain
** additional rights. These rights are described in the Nokia Qt LGPL
** Exception version 1.0, included in the file LGPL_EXCEPTION.txt in this
** package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
** $END_LICENSE$
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
****************************************************************************/

#include "librarygenerator.h"
#include "reporthandler.h"
#include "fileout.h"
#include "typesystem/typedatabase.h"

void LibraryGenerator::addBinding(const QString &module, const QString &binding) {
    m_modules[module].bindings << binding;
}

static void generateUseStatements(QTextStream &s, const QString &package) {
    QList<CodeSnip> snips =
        ((TypeSystemTypeEntry *) TypeDatabase::instance()->findType(package))->snips;

    foreach(const CodeSnip &snip, snips) {
        if (snip.position == CodeSnip::Beginning) {
            s << snip.code();
        }
    }
}

void LibraryGenerator::generateLibraryFile(const QString &package, const QStringList &bindings)
{
    FileOut file(resolveOutputDirectory() + "/" + package + "/library.dylan");

    file.stream << "module: dylan-user\n";
    file.stream << "copyright: See LICENSE file in this distribution.\n";
    file.stream << "\ndefine library " << package << endl;
    file.stream << "  use dylan;\n";
    file.stream << "  use common-dylan;\n";
    file.stream << "  use c-ffi;\n";

    generateUseStatements(file.stream, package);
    file.stream << "\n";

    file.stream << "  export " << package << ";\n";
    file.stream << "end library;\n";

    file.stream << "\ndefine module " << package << endl;
    file.stream << "  use dylan;\n";
    file.stream << "  use common-dylan;\n";
    file.stream << "  use c-ffi;\n";

    generateUseStatements(file.stream, package);
    file.stream << "\n";

    file.stream << "  export\n";
    bool is_first = true;
    foreach(const QString &entry, bindings) {
        if (!is_first) {
            file.stream << ",\n";
        }
        is_first = false;
        file.stream << "    " << entry;
    }
    file.stream << ";\n";
    file.stream << "end module;\n";

    if (file.done()) {
        ++m_num_generated_written;
    }
    ++m_num_generated;
}

void LibraryGenerator::generateRegistryEntry(const QString &package)
{
    FileOut file(resolveOutputDirectory() + "/registry/generic/" + package);
    file.stream << "abstract://dylan/" + package + "/" + package + ".lid";
    if (file.done()) {
        ++m_num_generated_written;
    }
    ++m_num_generated;
}

void LibraryGenerator::generate() {
    QHashIterator<QString, Module> module(m_modules);

    while (module.hasNext()) {
        module.next();

        QStringList bindings = module.value().bindings.values();
        qSort(bindings.begin(), bindings.end());

        generateLibraryFile(module.key(), bindings);
        generateRegistryEntry(module.key());
    }
}
