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

#include "plaincppimplgenerator.h"
#include "reporthandler.h"
#include <qnativepointer.h>

#include <QDir>
#include <QtDebug>
#include <QVariant>

#define VOID_POINTER_ORDINAL 8
#include "typesystem/typedatabase.h"

static Indentor INDENT;

QString c_signature(const AbstractMetaFunction *function, JNISignatureFormat format) {
    QString returned = "(";
    AbstractMetaArgumentList arguments = function->arguments();
    foreach(const AbstractMetaArgument *argument, arguments) {
        if (!function->argumentRemoved(argument->argumentIndex() + 1)) {
            QString modified_type = function->typeReplaced(argument->argumentIndex() + 1);

            if (modified_type.isEmpty())
                returned += c_signature(argument->type(), format);
            else
                returned += c_signature(modified_type, format);
        }
    }

    returned += ")";

    QString modified_type = function->typeReplaced(0);
    if (modified_type.isEmpty())
        returned += c_signature(function->type(), format);
    else
        returned += c_signature(modified_type, format);

    return returned;
}

QString c_signature(const QString &_full_name, JNISignatureFormat format) {
    QString signature;
    QString full_name = _full_name;

    if (full_name.endsWith("[]")) {
        full_name.chop(2);
        signature = "[";
    }

    int start = 0, end = -1;
    while ((start = full_name.indexOf("<")) >= 0 && (end = full_name.indexOf(">")) >= 0) {
        full_name.remove(start, end - start + 1);
    }

    static QHash<QString, QString> table;
    if (table.isEmpty()) {
        table["boolean"] = "Z";
        table["byte"] = "B";
        table["char"] = "C";
        table["short"] = "S";
        table["int"] = "I";
        table["long"] = "J";
        table["float"] = "F";
        table["double"] = "D";
    }

    if (format == Underscores)
        signature.replace("[", "_3");

    if (table.contains(full_name)) {
        signature += table[full_name];
    } else if (format == Underscores) {
        signature.replace("[", "_3");
        signature += "L";
        signature += QString(full_name).replace("_", "_1").replace('.', '_').replace("$", "_00024");
        signature += "_2";
    } else {
        signature += "L";
        signature += QString(full_name).replace('.', '/');
        signature += ";";
    }

    return signature;
}

QString c_signature(const AbstractMetaType *java_type, JNISignatureFormat format) {
    if (!java_type)
        return "V";

    if (java_type->isArray()) {
        return "_3" + c_signature(java_type->arrayElementType(), format);
    } else if (java_type->isNativePointer()) {
        if (format == Underscores)
            return "Lcom_trolltech_qt_QNativePointer_2";
        else
            return "Lcom/trolltech/qt/QNativePointer;";
    } else if (java_type->isIntegerEnum() || java_type->isIntegerFlags()
               || (format == Underscores && (java_type->isEnum() || java_type->isFlags()))) {
        return "I";
    } else if (java_type->isThread()) {
        if (format == Underscores)
            return "Ljava_lang_Thread_2";
        else
            return "Ljava/lang/Thread;";
    }

    QString name = java_type->name();
    if (java_type->isObject()) {
        if (const InterfaceTypeEntry *ie
                = static_cast<const ObjectTypeEntry *>(java_type->typeEntry())->designatedInterface())
            name = ie->targetLangName();
    } else if (java_type->isTargetLangEnum()) {
        const EnumTypeEntry *et = static_cast<const EnumTypeEntry *>(java_type->typeEntry());
        name = et->javaQualifier() + "$" + et->targetLangName();

    } else if (java_type->isTargetLangFlags()) {
        const FlagsTypeEntry *ft = static_cast<const FlagsTypeEntry *>(java_type->typeEntry());
        name = ft->originator()->javaQualifier() + "$" + ft->targetLangName();
    }

    return c_signature((java_type->package().isEmpty() ? QString() : java_type->package() + ".") + name, format);
}

static QHash<QString, QString> table;
static QString default_return_statement_qt(const AbstractMetaType *java_type,
                                           Generator::Option options = Generator::NoOption) {
    QString returnStr = ((options & Generator::NoReturnStatement) == 0 ? "return" : "");
    if (!java_type)
        return returnStr;

    if (table.isEmpty()) {
        table["boolean"] = "false";
        table["byte"] = "0";
        table["char"] = "0";
        table["short"] = "0";
        table["int"] = "0";
        table["long"] = "0";
        table["float"] = "0.0f";
        table["double"] = "0.0";
        table["java.lang.Object"] = "0";
    }

    QString signature = table.value(java_type->typeEntry()->targetLangName());

    if (!signature.isEmpty())
        return returnStr + " " + signature;

    Q_ASSERT(!java_type->isPrimitive());
    if (java_type->isJObjectWrapper())
        return returnStr + " JObjectWrapper()";
    if (java_type->isVariant())
        return returnStr + " QVariant()";
    if (java_type->isTargetLangString())
        return returnStr + " QString()";
    if (java_type->isTargetLangStringRef())
        return returnStr + " QStringRef()";
    if (java_type->isTargetLangChar())
        return returnStr + " QChar()";
    else if (java_type->isEnum())
        return returnStr + " " + java_type->typeEntry()->name() + "(0)";
    else if (java_type->isContainer() &&
             ((ContainerTypeEntry *)java_type->typeEntry())->type() ==
             ContainerTypeEntry::StringListContainer)
        return returnStr + " " + java_type->typeEntry()->name() + "()";
    else if (java_type->isValue() || java_type->isContainer())
        return returnStr + " " + java_type->cppSignature() + "()";
    else
        return returnStr + " 0";
}

static QString default_return_statement_java(const AbstractMetaType *java_type) {
    if (!java_type)
        return "return";
    if (java_type->isArray())
        return "return null";

    if (table.isEmpty()) {
        table["boolean"] = "false";
        table["byte"] = "0";
        table["char"] = "0";
        table["short"] = "0";
        table["int"] = "0";
        table["long"] = "0";
        table["float"] = "0.0f";
        table["double"] = "0.0";
        table["java.lang.Object"] = "0";
    }

    QString signature = table.value(java_type->typeEntry()->targetLangName());
    if (!signature.isEmpty())
        return "return " + signature;

    Q_ASSERT(!java_type->isPrimitive());
    return "return 0";
}

/* Used to decide how which of the Call[Xxx]Method functions to call
 */
QByteArray cTypeName(const QString &name) {
    static QHash<QString, const char *> table;
    if (table.isEmpty()) {
        table["jboolean"] = "Boolean";
        table["jbyte"] = "Byte";
        table["jchar"] = "Char";
        table["jshort"] = "Short";
        table["jint"] = "Int";
        table["jlong"] = "Long";
        table["jfloat"] = "Float";
        table["jdouble"] = "Double";
        table["jobject"] = "Object";
    }

    return table[name];
}

QByteArray cName(const QString &name) {
    TypeEntry *entry = TypeDatabase::instance()->findType(name);
    if (entry)
        return entry->jniName().toLatin1();
    else
        return "void *";
}

QString PlainCppImplGenerator::jniReturnName(const AbstractMetaFunction *java_function) {
    QString return_type = translateType(java_function->type(), EnumAsInts);
    QString new_return_type = java_function->typeReplaced(0);
    if (!new_return_type.isEmpty()) {
        return_type = cName(new_return_type);
    }
    return return_type;
}


QByteArray cTypeName(const AbstractMetaType *java_type) {
    if (!java_type) {
        return "Void";
    } else if (java_type->isTargetLangChar()) {
        return "Char";
    } else if (java_type->isPrimitive()) {
        return cTypeName(java_type->typeEntry()->jniName());
    } else if (java_type->isIntegerEnum() || java_type->isIntegerFlags()) {
        return "Int";
    } else {
        return "Object";
    }
}

#if 0 // JNI -> C TODO
QByteArray newXxxArray(const AbstractMetaType *java_type) {
    return "New" + cTypeName(java_type) + "Array";
}

QByteArray setXxxArrayElement(const AbstractMetaType *java_type) {
    Q_ASSERT(java_type);
    return "Set" + cTypeName(java_type) + "ArrayElement";
}

QByteArray getXxxArrayElement(const AbstractMetaType *java_type) {
    Q_ASSERT(java_type);
    return "Get" + cTypeName(java_type) + "ArrayElement";
}

QByteArray getXxxArrayRegion(const AbstractMetaType *java_type) {
    Q_ASSERT(java_type);
    return "Get" + cTypeName(java_type) + "ArrayRegion";
}

QByteArray setXxxArrayRegion(const AbstractMetaType *java_type) {
    Q_ASSERT(java_type);
    return "Set" + cTypeName(java_type) + "ArrayRegion";
}

QByteArray callXxxMethod(const AbstractMetaType *java_type) {
    return "Call" + cTypeName(java_type) + "Method";
}

QByteArray callXxxMethod(const QString &name) {
    TypeEntry *entry = TypeDatabase::instance()->findType(name);
    if (entry && entry->isPrimitive())
        return "Call" + cTypeName(entry->jniName()) + "Method";
    else
        return "CallObjectMethod";
}
#endif

QString c_function_signature(QString package,
                             QString class_name,
                             const QString &function_name,
                             const QString &return_type,
                             const QString &mangled_arguments = QString(),
                             uint options = PlainCppImplGenerator::StandardJNISignature) {
    QString s;

    if (options & PlainCppImplGenerator::ExternC)
        s += "extern \"C\" ";

    if (options & PlainCppImplGenerator::ReturnType) {
        s += return_type;
        s += " ";
    }

    s += QString(function_name);

    return s;
}

QString PlainCppImplGenerator::fileNameForClass(const AbstractMetaClass *java_class) const {
    return QString("qtc_%1.cpp").arg(java_class->name());
}

void PlainCppImplGenerator::writeSignalFunction(QTextStream &s, const AbstractMetaFunction *signal, const AbstractMetaClass *cls,
        int pos) {
/*
    writeFunctionSignature(s, signal, cls, signalWrapperPrefix(),
                           Option(OriginalName | OriginalTypeDescription),
                           "QtJambi_SignalWrapper_");
    s << endl << "{" << endl;
    {
        AbstractMetaArgumentList arguments = signal->arguments();
        Indentation indent(INDENT);

        s << INDENT << "  QTJAMBI_DEBUG_TRACE(\"(shell) entering: ";
        writeFunctionSignature(s, signal, cls, signalWrapperPrefix(),
                           Option(OriginalName | OriginalTypeDescription),
                           "QtJambi_SignalWrapper_");
        s << "\");" << endl;

        if (arguments.size() > 0)
            s << INDENT << "jvalue arguments[" << arguments.size() << "];" << endl;
        else
            s << INDENT << "jvalue *arguments = 0;" << endl;
        s << INDENT << "JNIEnv *__jni_env = qtjambi_current_environment();" << endl
        << INDENT << "__jni_env->PushLocalFrame(100);" << endl;

        writeCodeInjections(s, signal, cls, CodeSnip::Beginning, TypeSystem::Signal);

        for (int i = 0; i < arguments.size(); ++i) {
            const AbstractMetaArgument *argument = arguments.at(i);
            writeQtToJava(s,
                          argument->type(),
                          argument->indexedName(),
                          "__java_" + argument->indexedName(),
                          signal,
                          argument->argumentIndex() + 1,
                          BoxedPrimitive);
            s << INDENT << "arguments[" << i << "].l = __java_" << argument->indexedName() << ";" << endl;
        }
        s << INDENT << "qtjambi_call_java_signal(__jni_env, m_signals[" << pos << "], arguments);"
        << endl;

        writeCodeInjections(s, signal, cls, CodeSnip::End, TypeSystem::Signal);
        s << INDENT << "__jni_env->PopLocalFrame(0);" << endl;

        s << INDENT << "  QTJAMBI_DEBUG_TRACE(\"(shell)  leaving: ";
        writeFunctionSignature(s, signal, cls, signalWrapperPrefix(),
                           Option(OriginalName | OriginalTypeDescription),
                           "QtJambi_SignalWrapper_");
        s << "\");" << endl;

        if (signal->type() != 0)
            s << INDENT << default_return_statement_qt(signal->type()) << ";" << endl;
    }
    s << "}" << endl << endl;
*/

    if (signal->implementingClass() == signal->ownerClass())
        writeFinalFunction(s, signal, cls);
}

bool PlainCppImplGenerator::hasCustomDestructor(const AbstractMetaClass *java_class) const {
    return !java_class->isQObject() && !java_class->typeEntry()->isValue();
}

void PlainCppImplGenerator::write(QTextStream &s, const AbstractMetaClass *abstract_class) {
    MetaDylanClass *java_class = (MetaDylanClass *)abstract_class;

    bool shellClass = java_class->generateShellClass();

    // Includes
    writeExtraIncludes(s, java_class);
    bool shellInclude = (java_class->generateShellClass()
                         || java_class->queryFunctions(AbstractMetaClass::Signals
                                                       | AbstractMetaClass::Visible
                                                       | AbstractMetaClass::NotRemovedFromShell).size() > 0);

    // need to include QPainter for all widgets...
    {
        const AbstractMetaClass *qwidget = java_class;
        while (qwidget && qwidget->name() != "QWidget") {
            qwidget = qwidget->baseClass();
        }
        if (qwidget)
            s << "#include <QtGui/QPainter>" << endl << endl;
    }

    /*
    if (qtJambiDebugTools()) {
        s << "#if defined(QTJAMBI_DEBUG_TOOLS)" << endl;
        s << " #include <qtjambi/qtjambidebugtools_p.h>" << endl;
        s << "#endif / QTJAMBI_DEBUG_TOOLS /" << endl;
        s << endl;
    }
    */

    if (shellInclude)
        s << "#include \"qtc_" << java_class->name() << ".h\"" << endl;

    Include inc = java_class->typeEntry()->include();
    if (!inc.name.isEmpty()) {
        s << "#include ";
        if (inc.type == Include::IncludePath)
            s << "<";
        else
            s << "\"";
        s << inc.name;
        if (inc.type == Include::IncludePath)
            s << ">";
        else
            s << "\"";
        s << endl;
    }

    writeShellSignatures(s, java_class);

    writeDefaultConstructedValues(s, java_class);

    if (hasCustomDestructor(java_class))
        writeFinalDestructor(s, java_class);

    if (shellClass) {
        s << "// emitting (writeShellConstructor)" << endl;
        foreach(AbstractMetaFunction *function, java_class->functions()) {
            if (function->isConstructor() && !function->isPrivate())
                writeShellConstructor(s, function, SkipRemovedArguments);
        }
        s << "// emitting (writeShellDestructor)" << endl;
        writeShellDestructor(s, java_class);

        if (java_class->isQObject()) {
            s << "// emitting (writeQObjectFunctions)" << endl;
            writeQObjectFunctions(s, java_class);
        }

        // Virtual overrides
        s << "// emitting Virtual overrides (virtualFunctions)" << endl;
        AbstractMetaFunctionList virtualFunctions = java_class->virtualFunctions();
        for (int pos = 0; pos < virtualFunctions.size(); ++pos) {
            const AbstractMetaFunction *function = virtualFunctions.at(pos);
            writeShellFunction(s, function, java_class, pos);
        }

        // Functions in shell class
        s << "// emitting Functions in shell class (nonVirtualShellFunctions)" << endl;
        AbstractMetaFunctionList shellFunctions = java_class->nonVirtualShellFunctions();
        for (int i = 0; i < shellFunctions.size(); ++i) {
            const AbstractMetaFunction *function = shellFunctions.at(i);
            writeShellFunction(s, function, java_class, -1);
        }

        // Write public overrides for functions that are protected in the base class
        // so they can be accessed from the native callback
        s << "// emitting Public Override Functions (publicOverrideFunctions)" << endl;
        AbstractMetaFunctionList public_override_functions = java_class->publicOverrideFunctions();
        foreach(AbstractMetaFunction *function, public_override_functions) {
            writePublicFunctionOverride(s, function, java_class);
        }

        // Write virtual function overries used to decide on static/virtual calls
        s << "// emitting Virtual Override Functions (virtualOverrideFunctions)" << endl;
        AbstractMetaFunctionList virtual_functions = java_class->virtualOverrideFunctions();
        foreach(const AbstractMetaFunction *function, virtual_functions) {
            writeVirtualFunctionOverride(s, function, java_class);
        }

    }
    s << "// emitting (writeExtraFunctions)" << endl;
    writeExtraFunctions(s, java_class);

    s << "// emitting (writeToStringFunction)" << endl;
    writeToStringFunction(s, java_class);

    // Signals
    s << "// emitting (writeSignalFunction)" << endl;
    AbstractMetaFunctionList signal_functions = signalFunctions(java_class);
    for (int i = 0; i < signal_functions.size(); ++i)
        writeSignalFunction(s, signal_functions.at(i), java_class, i);

    // Native callbacks (all java functions require native callbacks)
    s << "// emitting  (functionsInTargetLang writeFinalFunction)" << endl;
    AbstractMetaFunctionList class_funcs = java_class->functionsInTargetLang();
    foreach(AbstractMetaFunction *function, class_funcs) {
        if (!function->isEmptyFunction())
            writeFinalFunction(s, function, java_class);
    }

    s << "// emitting (AbstractMetaClass::NormalFunctions|AbstractMetaClass::AbstractFunctions writeFinalFunction)" << endl;
    class_funcs = java_class->queryFunctions(AbstractMetaClass::NormalFunctions
                  | AbstractMetaClass::AbstractFunctions
                  | AbstractMetaClass::NotRemovedFromTargetLang);
    foreach(AbstractMetaFunction *function, class_funcs) {
        if (function->implementingClass() != java_class) {
            writeFinalFunction(s, function, java_class);
        }
    }

    // Field accessors
    s << "// emitting Field accessors (writeFieldAccessors)" << endl;
    foreach(AbstractMetaField *field, java_class->fields()) {
        if (field->wasPublic() || (field->wasProtected() && !java_class->isFinal()))
            writeFieldAccessors(s, field);
    }

    s << "// emitting (writeFromNativeFunction)" << endl;
    writeFromNativeFunction(s, java_class);

    if (java_class->isQObject()) {
        s << "// emitting (writeOriginalMetaObjectFunction)" << endl;
        writeOriginalMetaObjectFunction(s, java_class);
    }

    if (java_class->typeEntry()->isValue()) {
        s << "// emitting (writeFromArrayFunction)" << endl;
        writeFromArrayFunction(s, java_class);
    }

    // generate the __qt_cast_to_Xxx functions
    if (!java_class->isNamespace() && !java_class->isInterface()) {
        s << "// emitting (writeInterfaceCastFunction)" << endl;
        AbstractMetaClassList interfaces = java_class->interfaces();
        foreach(AbstractMetaClass *iface, interfaces)
            writeInterfaceCastFunction(s, java_class, iface);
    }

    s << "// emitting (writeSignalInitialization)" << endl;
    writeSignalInitialization(s, java_class);

    s << endl << endl;

    lidGenerator->addSource(java_class->package(), fileNameForClass(java_class));
}

void PlainCppImplGenerator::writeExtraFunctions(QTextStream &s, const AbstractMetaClass *java_class) {
    const ComplexTypeEntry *class_type = java_class->typeEntry();
    Q_ASSERT(class_type);

    CodeSnipList code_snips = class_type->codeSnips();
    foreach(const CodeSnip &snip, code_snips) {
        if (snip.language == TypeSystem::ShellCode || snip.language == TypeSystem::NativeCode) {
            snip.formattedCode(s, INDENT) << endl;
        }
    }
}

void PlainCppImplGenerator::writeToStringFunction(QTextStream &s, const AbstractMetaClass *java_class) {
/*
    FunctionModelItem fun = java_class->hasToStringCapability();
    bool core = java_class->package() == QLatin1String("com.trolltech.qt.core");
    bool qevent = false;

    const AbstractMetaClass *cls = java_class;
    while (cls) {
        if (cls->name() == "QEvent") {
            qevent = true;
            fun = cls->hasToStringCapability();
            break;
        }
        cls = cls->baseClass();
    }

    if (!java_class->hasDefaultToStringFunction() && fun && !(qevent && core)) {

        int indirections = fun->arguments().at(1)->type().indirections();
        QString deref = QLatin1String(indirections == 0 ? "*" : "");

        s << endl;
        s << "#include <QtCore/QDebug>" << endl;
        s << c_function_signature(java_class->package(), java_class->name(), "__qt_toString", "jstring")
        << "(JNIEnv *__jni_env, jobject, jlong __this_nativeId)" << endl
        << INDENT << "{" << endl;
        {
            Indentation indent(INDENT);
            s << INDENT << java_class->qualifiedCppName() << " *__qt_this = ("
            << java_class->qualifiedCppName() << " *) qtjambi_from_jlong(__this_nativeId);" << endl
            //<< INDENT << "QTJAMBI_EXCEPTION_CHECK(__jni_env);" << endl
            << INDENT << "Q_ASSERT(__qt_this);" << endl

            << INDENT << "QString res;" << endl
            << INDENT << "QDebug d(&res);" << endl
            << INDENT << "d << " << deref << "__qt_this;" << endl;
            s << INDENT << "return qtjambi_from_qstring(__jni_env, res);" << endl;
        }
        s << INDENT << "}" << endl << endl;
    }
*/
}

void PlainCppImplGenerator::writeShellSignatures(QTextStream &s, const AbstractMetaClass *java_class) {
    bool has_constructors = java_class->hasConstructors();

    // Write the function names...
    if (has_constructors && java_class->hasVirtualFunctions()) {
        // FIXME This is sometimes emitted when it does not need to be and results in
        //  compiler warning (gcc linux - at least) about unused static data.
        // I suspect the test above is not good enough.
        AbstractMetaFunctionList virtual_functions = java_class->virtualFunctions();
        {
            Indentation indent(INDENT);

            int pos = -1;
            foreach(AbstractMetaFunction *function, virtual_functions) {
                ++pos;

                if (pos == 0)
                    s << "static const char *qtjambi_method_names[] = {";
                else
                    s << ",";
                s << endl
                //<< "/* " << QString("%1").arg(QString::number(pos), 3) << " */ "
                << "\"" << function->name() << "\"";
            }
            if (pos >= 0)
                s << endl << "};" << endl << endl;
            else
                s << "static const char **qtjambi_method_names = 0;" << endl;
        }

        // Write the function signatures
        {
            Indentation indent(INDENT);

            int pos = -1;
            foreach(AbstractMetaFunction *function, virtual_functions) {
                ++pos;

                if (pos == 0)
                    s << "static const char *qtjambi_method_signatures[] = {";
                else
                    s << ",";
                s << endl
                //<< "/* " << QString("%1").arg(QString::number(pos), 3) << " */ "
                << "\""
                << c_signature(function, SlashesAndStuff)
                << "\"";
            }
            if (pos >= 0)
                s << endl << "};" << endl;
            else
                s << "static const char **qtjambi_method_signatures = 0;" << endl;
            s << "static const int qtjambi_method_count = " << QString::number(pos + 1) << ";" << endl
            << endl;
        }
    }

    if (has_constructors && java_class->hasInconsistentFunctions()) {
        AbstractMetaFunctionList inconsistents = java_class->cppInconsistentFunctions();
        // Write the inconsistent function names...
        {
            Indentation indent(INDENT);
            s << "static const char *qtjambi_inconsistent_names[] = {";
            for (int i = 0; i < inconsistents.size(); ++i) {
                if (i != 0)
                    s << ",";
                s << endl << INDENT << "\"" << inconsistents.at(i)->name() << "\"";
            }
            s << endl << "};" << endl << endl;
        }

        // Write the function signatures
        {
            Indentation indent(INDENT);
            s << "static const char *qtjambi_inconsistent_signatures[] = {";
            for (int i = 0; i < inconsistents.size(); ++i) {
                const AbstractMetaFunction *function = inconsistents.at(i);

                if (i != 0)
                    s << ",";
                s << endl << INDENT << "\""
                << c_signature(function, SlashesAndStuff)
                << "\"";
            }
            s << endl << "};" << endl << endl;
        }
        s << "static const int qtjambi_inconsistent_count = " << inconsistents.size() << ";" << endl
        << endl;
    }


    AbstractMetaFunctionList signal_functions = java_class->cppSignalFunctions();
    if (signal_functions.size()) {
        Indentation indent(INDENT);
        s << "static const char *qtjambi_signal_names[] = {";
        for (int i = 0; i < signal_functions.size(); ++i) {
            if (i != 0)
                s << ",";

            const AbstractMetaFunction *f = signal_functions.at(i);

            QString signalName = f->name();

            s << endl << INDENT << "\"" << signalName << "\"";
        }
        s << endl << "};" << endl << endl;

        s << "static const int qtjambi_signal_argumentcounts[] = {";
        for (int i = 0; i < signal_functions.size(); ++i) {
            if (i != 0)
                s << ",";
            s << endl << INDENT << signal_functions.at(i)->arguments().count();
        }
        s << endl << "};" << endl << endl;
        s << "static const int qtjambi_signal_count = " << signal_functions.size() << ";" << endl
        << endl;
    }
}

void PlainCppImplGenerator::writeQObjectFunctions(QTextStream &s, const AbstractMetaClass *java_class) {
/*
    s << "const QMetaObject *" << shellClassName(java_class) << "::initMetaObject(JNIEnv *env, jobject java_object)" << endl
    << "{" << endl
    << "  const QMetaObject *mo = QtDynamicMetaObject::build(env, java_object, " << java_class->qualifiedCppName() << "::metaObject()" << ");" << endl;
    s << "  Q_ASSERT(mo);" << endl;

    AbstractMetaFunctionList virtualFunctions = java_class->virtualFunctions();
    for (int pos = 0; pos < virtualFunctions.size(); ++pos) {
        const AbstractMetaFunction *virtualFunction = virtualFunctions.at(pos);
        if (virtualFunction->isVirtualSlot()) {
            QStringList introspectionCompatibleSignatures = virtualFunction->introspectionCompatibleSignatures();
            foreach(QString introspectionCompatibleSignature, introspectionCompatibleSignatures) {
                s << "    {" << endl
                << "        int idx = mo->indexOfMethod(\""
                << introspectionCompatibleSignature << "\");" << endl;

                s << "        if (idx >= 0) m_map.insert(idx, " << pos << ");" << endl
                << "    }" << endl;
            }
        }
    }

    s << "  return mo;" << endl
    << "}" << endl;

    // QObject::metaObject()
    s << "const QMetaObject *" << shellClassName(java_class) << "::metaObject() const" << endl
    << "{" << endl
    << "  Q_ASSERT(m_meta_object);" << endl
    << "  return m_meta_object;" << endl
    << "}" << endl << endl;

    // QObject::qt_metacast()
    s << "void *" << shellClassName(java_class) << "::qt_metacast(const char *_clname)" << endl
    << "{" << endl
    << "  void *rv;" << endl
    //<< "  QTJAMBI_DEBUG_TRACE(\"(shell) entering: " << shellClassName(java_class) << "::qt_metacast(const char *_clname)\");" << endl
    << "  rv = qtjambishell_qt_metacast(_clname, \"" << shellClassName(java_class) << "\", m_link, static_cast<void*>(const_cast<" << shellClassName(java_class) << "*>(this)));" << endl
    << "  if (rv == (void *)-1)" << endl
    << "    rv = " << java_class->qualifiedCppName() << "::qt_metacast(_clname);" << endl
    //<< "  QTJAMBI_DEBUG_TRACE(\"(shell)  leaving: " << shellClassName(java_class) << "::qt_metacast(const char *_clname)\");" << endl
    << "  return rv;" << endl
    << "}" << endl << endl;

    // QObject::qt_metacall()
    s << "int " << shellClassName(java_class) << "::qt_metacall(QMetaObject::Call _c, int _id, void **_a)" << endl
    << "{" << endl;

    //s << "  QTJAMBI_DEBUG_TRACE(\"(shell) entering: " << shellClassName(java_class) << "::qt_metacall(QMetaObject::Call _c, int _id, void **_a)\");" << endl;

    bool need_label_done = false;
    if (java_class->hasVirtualSlots()) {
        s << "  int virtual_idx = m_map.value(_id, -1);" << endl
        << "  if (virtual_idx >= 0) {" << endl
        << "      switch (virtual_idx) {";

        AbstractMetaFunctionList virtualFunctions = java_class->virtualFunctions();
        for (int pos = 0; pos < virtualFunctions.size(); ++pos) {
            const AbstractMetaFunction *virtualFunction = virtualFunctions.at(pos);
            if (virtualFunction->isVirtualSlot()) {
                s << endl << "      case " << pos << ": {" << endl;

                // Set up variable names so the function call works
                AbstractMetaArgumentList arguments = virtualFunction->arguments();
                for (int i = 1; i <= arguments.size(); ++i) {
                    AbstractMetaArgument *argument = arguments.at(i - 1);

                    s << "          ";
                    writeTypeInfo(s, argument->type());
                    s << " __qt_" << argument->indexedName() << " = *reinterpret_cast<";
                    writeTypeInfo(s, argument->type());
                    s << " *>(_a[" << i << "]);" << endl;
                }

                // Function call
                s << "          ";
                AbstractMetaType *function_type = virtualFunction->type();
                if (function_type) {
                    writeTypeInfo(s, virtualFunction->type());
                    s << " _r = ";
                }

                writeFunctionCall(s, "this", virtualFunction);
                if (function_type) {
                    s << "          if (_a[0] != 0) "
                    << "*reinterpret_cast<";
                    writeTypeInfo(s, virtualFunction->type());
                    s << " *>(_a[0]) = _r;" << endl
                    << "          _id = -1; goto done;" << endl;
                    need_label_done = true;
                }
                s << "      }";
            }
        }

        s << "};" << endl
        << "  }" << endl;
    }

    s << "  _id = " << java_class->qualifiedCppName() << "::qt_metacall(_c, _id, _a);" << endl
    << "  if (_id >= 0)" << endl
    << "    _id = QtDynamicMetaObject::dispatch_qt_metacall(m_link, metaObject(), _c, _id, _a);" << endl;
    if (need_label_done)  // quash: warning: label 'done' defined but not used
        s << "done:" << endl;
    //s << "  QTJAMBI_DEBUG_TRACE(\"(shell)  leaving: " << shellClassName(java_class) << "::qt_metacall(QMetaObject::Call _c, int _id, void **_a)\");" << endl
    s << "  return _id;" << endl
    << "}" << endl << endl;
*/
}

void PlainCppImplGenerator::writeShellConstructor(QTextStream &s, const AbstractMetaFunction *java_function, int options) {
    if (java_function->isModifiedRemoved(TypeSystem::ShellCode))
        return;

    const AbstractMetaClass *cls = java_function->ownerClass();
    AbstractMetaArgumentList arguments = java_function->arguments();

    writeFunctionSignature(s, java_function, cls, /*default-arg*/QString(), Option(options));

    s << endl;
    s << "    : " << cls->qualifiedCppName() << "(";
    bool needComma = false;
    for (int i = 0; i < arguments.size(); ++i) {
        const AbstractMetaArgument *argument = arguments.at(i);
        if ((options & SkipRemovedArguments) == SkipRemovedArguments
                && java_function->argumentRemoved(i + 1)) {
            continue;
        }
        if(needComma)
            s << ", ";
        else
            needComma = true;
        s << arguments.at(i)->indexedName();
    }
/*
    s << ")," << endl;
    if (cls->isQObject())
        s << "    m_meta_object(0)," << endl;
    s << "      m_vtable(0)," << endl
    << "      m_link(0)" << endl;
*/
    s << ")" << endl;

    s << "{" << endl;
    {
        Indentation indent(INDENT);

        /*
        s << INDENT << "QTJAMBI_DEBUG_TRACE(\"(shell) entering: " << shellClassName(cls) << "::";
        writeFunctionSignature(s, java_function, cls);
        s << "\");" << endl;
        */

        writeCodeInjections(s, java_function, cls, CodeSnip::Beginning, TypeSystem::ShellCode);
        writeCodeInjections(s, java_function, cls, CodeSnip::End, TypeSystem::ShellCode);

        /*
        s << INDENT << "QTJAMBI_DEBUG_TRACE(\"(shell) leaving: " << shellClassName(cls) << "::";
        writeFunctionSignature(s, java_function, cls);
        s << "\");" << endl;
        */
    }
    s << "}" << endl << endl;
}

void PlainCppImplGenerator::writeShellDestructor(QTextStream &s, const AbstractMetaClass *java_class) {
  /*
    s << shellClassName(java_class) << "::~"
    << shellClassName(java_class) << "()" << endl
    << "{" << endl;
    {
        Indentation indent(INDENT);

        s << INDENT << "QTJAMBI_DEBUG_TRACE(\"(shell) entering: " << shellClassName(java_class) << "::~"
        << shellClassName(java_class) << "()\");" << endl;

        s << "#ifdef QT_DEBUG" << endl
        << INDENT << "if (m_vtable)" << endl
        << INDENT << "    m_vtable->deref();" << endl
        << "#endif" << endl;
        if (java_class->isQObject())
            s << INDENT << "QtDynamicMetaObject::check_dynamic_deref(m_meta_object);" << endl;
        s << INDENT << "if (m_link) {" << endl;
        {   // Signal reference/memory cleanup
            Indentation indent(INDENT);
            AbstractMetaFunctionList signal_functions = signalFunctions(java_class);
            if (signal_functions.size() > 0) {
                s << INDENT << "QtJambi_SignalWrapper_" << java_class->name() << " *qt_wrapper = " << "reinterpret_cast<" << "QtJambi_SignalWrapper_" << java_class->name() << " *" << ">(m_link->signalWrapper());" << endl;
                s << INDENT << "if (qt_wrapper)" << endl;
                s << INDENT << "    qtjambishell_signals_cleanup(qt_wrapper->m_signals, qtjambi_signal_count);" << endl;
            }
        }

        AbstractMetaClassList interfaces = java_class->interfaces();
        int totalRegisterSubObject = interfaces.size() + (java_class->baseClass() != 0 ? 1 : 0);
        if (totalRegisterSubObject > 1) {
            Indentation indent(INDENT);
            s << INDENT << "{" << endl;
            {
                Indentation indent(INDENT);
                s << INDENT << "void *unregPtrs[" << totalRegisterSubObject << "];" << endl;
                s << "#if defined(QTJAMBI_DEBUG_TOOLS)" << endl;
                s << INDENT << "const char *unregNamePtrs[" << totalRegisterSubObject << "];" << endl;
                s << "#else" << endl;
                s << INDENT << "const char **unregNamePtrs = 0;" << endl;
                s << "#endif / QTJAMBI_DEBUG_TOOLS /" << endl;
                if (java_class->baseClass() != 0)
                    interfaces += java_class->baseClass();
                int i = 0;
                foreach(AbstractMetaClass *iface, interfaces) {
                    AbstractMetaClass *impl = iface->isInterface() ? iface->primaryInterfaceImplementor() : iface;
                    //s << INDENT << "m_link->unregisterSubObject((" << impl->qualifiedCppName() << " *) this);" << endl;
                    s << INDENT << "unregPtrs[" << i << "] = (" <<  impl->qualifiedCppName() << " *) this;" << endl;
                    s << "#if defined(QTJAMBI_DEBUG_TOOLS)" << endl;
                    s << INDENT << "unregNamePtrs[" << i << "] = \"" <<  impl->qualifiedCppName() << "\";" << endl;
                    s << "#endif / QTJAMBI_DEBUG_TOOLS /" << endl;
                    i++;
                }
                s << INDENT << "m_link->unregisterSubObjects(this, " << i << ", unregPtrs, unregNamePtrs);" << endl;
            }
            s << INDENT << "}" << endl;
        }

        bool emit_check_qtjambi_vm_shutdown = false;
        bool emit_nativeShellObjectDestroyed = false;
        if (!java_class->isQObject())
            emit_nativeShellObjectDestroyed = true;
        // Has to be a non-QObject and no special cplusplus ownership rules
#if 0
        // FIXME: Lookup any special ownership details for the constructor in typesystem.
        //  If cplusplus owns this object we never emit it here, for example QTextCodec
        //  is fully managed by Qt
        // Lookup the constructor "QTextCodec()" of the "QTextCodec" class, get the method
        //  signature's ownership setting.
        shellClassName(java_class);	// QtCShell_QTextCodec
        java_class->name();		// QTextCodec
        const AbstractMetaClass *cls_ctor = 0;			// FIXME
        TypeSystem::Language language = TypeSystem::TargetLangCode;	// java
        int idx = -1;
        TypeSystem::Ownership ownership = ownership(cls_ctor, language, idx);	// FIXME
        if (TypeSystem::CppOwnership == ownership) {
            emit_nativeShellObjectDestroyed = false;
        }
#else
        // HACK method ownership of constructor return value, there is just one class with this rule
        //  QTextCodec is part of Qt API.
        //  TextCodecSubclass is part of the autotests suite, that subclasses QTextCodec.
        if (java_class->name().compare("QTextCodec") == 0 || java_class->name().compare("TextCodecSubclass") == 0) {
            emit_nativeShellObjectDestroyed = false;
            emit_check_qtjambi_vm_shutdown = true;
        }
#endif
        if (emit_nativeShellObjectDestroyed) {
            s << INDENT << "    JNIEnv *__jni_env = qtjambi_current_environment();" << endl
            << INDENT << "    if (__jni_env != 0) m_link->nativeShellObjectDestroyed(__jni_env);" << endl;
        } else if(!java_class->isQObject()) {
            // We only inhibited when we started out with emit_nativeShellObjectDestroyed==true
            s << INDENT << "    // inhibited: m_link->nativeShellObjectDestroyed(__jni_env); cplusplus object ownership" << endl;
        }

        if (qtJambiDebugTools()) {
            s << "#if defined(QTJAMBI_DEBUG_TOOLS)" << endl;
            // Check this global, this stops Qt GlobalStaticDeleter causing crashes on shutdown
            if(emit_check_qtjambi_vm_shutdown)
                s << INDENT << "    if(qtjambi_vm_shutdown_get() == 0) {"  << endl;
            s << INDENT << "    qtjambi_increase_shellDestructorCalledCount(QString::fromLatin1(\"" << java_class->name() << "\"));" << endl;
            if(emit_check_qtjambi_vm_shutdown)
                s << INDENT << "    }"  << endl;
            s << "#endif / QTJAMBI_DEBUG_TOOLS /" << endl;
        }

        s << INDENT << "}" << endl;

        s << INDENT << "QTJAMBI_DEBUG_TRACE(\"(shell) leaving: "  << shellClassName(java_class) << "::~"
        << shellClassName(java_class) << "()\");" << endl;
    }
    s << "}" << endl << endl;
    */
}

void PlainCppImplGenerator::writeCodeInjections(QTextStream &s, const AbstractMetaFunction *java_function,
        const AbstractMetaClass *implementor, CodeSnip::Position position,
        TypeSystem::Language language) {

    FunctionModificationList mods;
    const AbstractMetaClass *cls = implementor;
    while (cls != 0) {
        mods += java_function->modifications(cls);

        if (cls == cls->baseClass())
            break;
        cls = cls->baseClass();
    }

    foreach(FunctionModification mod, mods) {
        if (mod.snips.count() <= 0)
            continue ;

        foreach(CodeSnip snip, mod.snips) {
            if (snip.position != position)
                continue ;

            if ((snip.language & language) == false)
                continue ;

            if (position == CodeSnip::End)
                s << endl;

            QString code;
            QTextStream tmpStream(&code);
            snip.formattedCode(tmpStream, INDENT);
            ArgumentMap map = snip.argumentMap;
            ArgumentMap::iterator it = map.begin();
            for (;it != map.end();++it) {
                int pos = it.key() - 1;
                QString meta_name = it.value();

                if (pos >= 0 && pos < java_function->arguments().count()) {
                    code = code.replace(meta_name, java_function->arguments().at(pos)->indexedName());
                } else {
                    QString debug = QString("argument map specifies invalid argument index %1"
                                            "for function '%2'")
                                    .arg(pos + 1).arg(java_function->name());
                    ReportHandler::warning(debug);
                }

            }
            s << code;
            if (position == CodeSnip::Beginning)
                s << endl;
        }
    }
}

static QString function_call_for_ownership(TypeSystem::Ownership owner, const QString &var_name) {
    if (owner == TypeSystem::CppOwnership) {
        return "setCppOwnership(__jni_env, " + var_name + ")";
    } else if (owner == TypeSystem::TargetLangOwnership) {
        return "setJavaOwnership(__jni_env, " + var_name + ")";
    } else if (owner == TypeSystem::DefaultOwnership) {
        return "setDefaultOwnership(__jni_env, " + var_name + ")";
    } else {
        Q_ASSERT(false);
        return "bogus()";
    }
}

void PlainCppImplGenerator::writeOwnership(QTextStream &s,
                                      const AbstractMetaFunction *java_function,
                                      const QString &var_name,
                                      int var_index,
                                      const AbstractMetaClass *implementor) {
    TypeSystem::Ownership owner = TypeSystem::InvalidOwnership;
    const AbstractMetaClass *cls = implementor;
    while (cls != 0 && owner == TypeSystem::InvalidOwnership) {
        owner = java_function->ownership(cls, TypeSystem::ShellCode, var_index);
        cls = cls->baseClass();
    }

    if (owner == TypeSystem::InvalidOwnership)
        return;

    /*
    if (var_index != -1) {
        s << INDENT << "if (" << var_name << " != 0) {" << endl;
        {
            Indentation indent(INDENT);
            s << INDENT << "QtJambiLink *__link = QtJambiLink::findLink(__jni_env, "
            << var_name << ");" << endl
            << INDENT << "Q_ASSERT(__link != 0);" << endl;

            s << INDENT << "__link->" << function_call_for_ownership(owner, var_name) << ";" << endl;
        }
        s << INDENT << "}" << endl;
    } else {
        s << INDENT << "if (m_link) {" << endl;
        {
            Indentation indent(INDENT);
            s << INDENT << "m_link->" << function_call_for_ownership(owner, "m_link->javaObject(__jni_env)") << ";" << endl;
        }
        s << INDENT << "}" << endl;
    }
    */

}

void PlainCppImplGenerator::writeShellFunction(QTextStream &s, const AbstractMetaFunction *java_function,
        const AbstractMetaClass *implementor, int id) {
/*
    writeFunctionSignature(s, java_function, implementor, QString(), OriginalName);

    s << endl
    << "{" << endl;
*/

    Indentation indent(INDENT);

    QString java_function_signature = java_function->signature();
    /*
    s << INDENT << "QTJAMBI_DEBUG_TRACE(\"(shell) entering: " << implementor->name() << "::"
    << java_function_signature << "\");" << endl;
    */

    writeCodeInjections(s, java_function, implementor, CodeSnip::Beginning, TypeSystem::ShellCode);

    //     s << "    printf(\"%s : %s\\n\", \"" << java_function->enclosingClass()->name() << "\""
    //       << ", \"" << java_function->name() << "\");" << endl;

/*
    if (!java_function->isFinalInCpp() || java_function->isVirtualSlot()) {
        s << INDENT << "jmethodID method_id = m_vtable->method(" << id << ");" << endl;
        s << INDENT << "if (method_id) {" << endl;

        {
            Indentation indent(INDENT);

            AbstractMetaType *function_type = java_function->type();
            QString new_return_type = java_function->typeReplaced(0);
            bool has_function_type = ((function_type != 0
                                       || !new_return_type.isEmpty())
                                      && new_return_type != "void");

            s << INDENT << "JNIEnv *__jni_env = qtjambi_current_environment();" << endl;

            // This nasty case comes up when we're shutting down while receiving virtual
            // calls.. With these checks we safely abort...
            s << INDENT << "if (!__jni_env) {" << endl;
            {
                Indentation indent(INDENT);
                bool emittedReturnKeyword = writeBaseClassFunctionCall(s, java_function, implementor);
                if (!emittedReturnKeyword)
                    s << INDENT << "return;" << endl;
            }
            s << INDENT << "}" << endl;

            // otherwise, continue with the function call...
            s << INDENT << "__jni_env->PushLocalFrame(100);" << endl;

            if (function_type) {
                s << INDENT;
                writeTypeInfo(s, function_type);
                s << " " << "__qt_return_value;" << endl;	// declaration only
            }

            // It is possible for QEvent to be attempted for delivery to eventFilter() when the GC is deleted
            //  the object.  This makes m_link->javaObject(__jni_env)==0 and we are not allows to call any
            //  __jni_env->CallXxxMethod() with this null.
            s << INDENT << "jobject __java_return_value_object = m_link->javaObject(__jni_env);" << endl;
            // This may also be a WeakGlobalRef so we need to check it is valid (before calling any
            //  invocation methods CallXxxMethod).
            s << INDENT << "if(__jni_env->IsSameObject(__java_return_value_object, 0) == JNI_FALSE) {" << endl;
            {
                Indentation indent(INDENT);

                AbstractMetaArgumentList arguments = java_function->arguments();
                AbstractMetaArgumentList argumentsToReset;
                foreach(AbstractMetaArgument *argument, arguments) {
                    if (!java_function->argumentRemoved(argument->argumentIndex() + 1)) {
                        if (!argument->type()->isPrimitive()
                                || !java_function->conversionRule(TypeSystem::NativeCode, argument->argumentIndex() + 1).isEmpty()) {
                            writeQtToJava(s,
                                          argument->type(),
                                          argument->indexedName(),
                                          "__java_" + argument->indexedName(),
                                          java_function,
                                          argument->argumentIndex() + 1);
                        }

                        if (java_function->resetObjectAfterUse(argument->argumentIndex() + 1))
                            argumentsToReset.append(argument);
                    }
                }

                for (int i = 0; i < arguments.size(); ++i)
                    writeOwnership(s, java_function, "__java_" + arguments.at(i)->indexedName(), i + 1, implementor);

                s << INDENT;
                if (has_function_type) {
                    if (new_return_type.isEmpty()) {
                        s << translateType(function_type);
                    } else {
                        s << cName(new_return_type);
                    }
                    s << " " << "__java_return_value = ";	// declaration only
                }

                s << "__jni_env->";
#if 0 // JNI -> C TODO
                if (new_return_type.isEmpty()) {
                    s << callXxxMethod(java_function->type());
                } else if (!has_function_type) {
                    s << "CallVoidMethod";
                } else {
                    s << callXxxMethod(new_return_type);
                }
#endif

                s << "(__java_return_value_object, method_id";
                if (arguments.size() > 0)
                    s << ", ";
                writeFunctionCallArguments(s, java_function, "__java_", Option(NoCasts | SkipRemovedArguments));
                s << ");" << endl;
                // We'd like to not emit this line below, if the one just outside the
                //  generated brace section is next.
                s << INDENT << "qtjambi_exception_check(__jni_env);" << endl;

                if (has_function_type) {
                    writeJavaToQt(s, function_type, "__qt_return_value_tmp", "__java_return_value",
                                  java_function, 0, GlobalRefJObject);
                    // This line below i sonly needed because we can't instruction writeJavaToQt
                    //  to omit the emitting the function type, so then we could assign
                    //  __qt_return_value directly.
                    // This is not very efficient for complex types as it may cause extra copying.
                    if(function_type)
                        s << INDENT << "__qt_return_value = __qt_return_value_tmp;" << endl;

                    if (java_function->nullPointersDisabled()) {
                        s << INDENT << "if (__java_return_value == 0) {" << endl;
                        {
                            Indentation indent(INDENT);
                            s << INDENT << "fprintf(stderr, \"QtJambi: Unexpected null pointer returned from override of '" << java_function->name() << "' in class '%s'\\n\"," << endl
                            << INDENT << "        qPrintable(qtjambi_object_class_name(__jni_env, m_link->javaObject(__jni_env))));" << endl;
                            s << INDENT << "__qt_return_value = ";
                            QString defaultValue = java_function->nullPointerDefaultValue();
                            if (!defaultValue.isEmpty())
                                s << defaultValue << ";";
                            else
                                writeBaseClassFunctionCall(s, java_function, implementor, NoReturnStatement);
                            s << endl;
                        }

                        s << INDENT << "}" << endl;
                    }
                } else if (!java_function->conversionRule(TypeSystem::ShellCode, 0).isEmpty()) {
                    writeConversionRule(s, TypeSystem::ShellCode, java_function, 0, "<invalid>", "<invalid>");
                }

                writeOwnership(s, java_function, "this", -1, implementor);
                writeOwnership(s, java_function, "__java_return_value", 0, implementor);

                foreach(AbstractMetaArgument *argumentToReset, argumentsToReset) {

                    QString argumentName = "__java_" + argumentToReset->indexedName();

                    s << INDENT;
                    if (argumentToReset->type()->isContainer())
                        s << "qtjambi_invalidate_collection(";
                    else
                        s << "qtjambi_invalidate_object(";

                    s << "__jni_env, " << argumentName << ");" << endl;
                }
            }
            s << INDENT << "} else {" << endl;  // if(__java_return_value_object)
            {
                Indentation indent(INDENT);

                // FIXME: Use helper method
                // Raise exception ?  Maybe this should be QtJambiNoNativeException
                s << INDENT << "qtjambishell_throw_nullpointerexception(__jni_env, " << "\"" << implementor->name() << "::" << java_function_signature << "\"" << ");" << endl;
                if(function_type)
                    s << INDENT << "__qt_return_value = " << default_return_statement_qt(function_type, Generator::NoReturnStatement) << ";" << endl;
            }
            s << INDENT << "}" << endl;         // if(__java_return_value_object)

            s << INDENT << "qtjambi_exception_check(__jni_env);" << endl;
            s << INDENT << "__jni_env->PopLocalFrame(0);" << endl;

            s << INDENT << "QTJAMBI_DEBUG_TRACE(\"(shell) -> leaving: "  << implementor->name()
            << "::" << java_function_signature << "\");" << endl;

            if (function_type)
                s << INDENT << "return __qt_return_value;" << endl;

        }

        s << INDENT << "} else {" << endl;

        {
            Indentation indent(INDENT);
            s << INDENT << "QTJAMBI_DEBUG_TRACE(\"(shell) -> super() and leaving: "
            << implementor->name() << "::" << java_function_signature << "\");" << endl;
            writeBaseClassFunctionCall(s, java_function, implementor);
        }


        s << INDENT << "}" << endl;

        writeCodeInjections(s, java_function, implementor, CodeSnip::End, TypeSystem::ShellCode);

        // A little trick to close open painters on a widget
        if (java_function->name() == "paintEvent") {
            s << INDENT << "JNIEnv *env = qtjambi_current_environment();" << endl
            << INDENT << "qtjambi_end_paint(env, m_link->javaObject(env));" << endl;
        }

    } else {
        if (java_function->isRemovedFrom(implementor, TypeSystem::TargetLangCode)) {
            // Avoid compiler warnings for unused parameters
            AbstractMetaArgumentList arguments = java_function->arguments();

            foreach(const AbstractMetaArgument *argument, arguments) {
                s << INDENT << "Q_UNUSED(" << argument->indexedName() << ")" << endl;
            }
        }
        writeBaseClassFunctionCall(s, java_function, implementor);
        writeCodeInjections(s, java_function, implementor, CodeSnip::End, TypeSystem::ShellCode);
    }

    s << "}" << endl << endl;
*/

}

// ### kill implementor

void PlainCppImplGenerator::writePublicFunctionOverride(QTextStream &s,
        const AbstractMetaFunction *java_function,
        const AbstractMetaClass *implementor) {
    Q_ASSERT(java_function->originalAttributes()
             & (AbstractMetaAttributes::Protected
                | AbstractMetaAttributes::Final));

    // The write a public override version of this function to be used by native functions
    writeFunctionSignature(s, java_function, implementor, "__public_",
                           Option(EnumAsInts | UnderscoreSpaces
                                  | (java_function->isAbstract() ? SkipName : NoOption)));
    s << endl
    << "{" << endl;
    Indentation indent(INDENT);
    writeBaseClassFunctionCall(s, java_function, implementor);
    s << "}" << endl << endl;
}


void PlainCppImplGenerator::writeVirtualFunctionOverride(QTextStream &s,
        const AbstractMetaFunction *java_function,
        const AbstractMetaClass *implementor) {
    Q_ASSERT(!java_function->isFinalInCpp());

    Option options = Option(EnumAsInts | UnderscoreSpaces);

    // The write a public override version of this function to be used by native functions
    writeFunctionSignature(s, java_function, implementor, "__override_",
                           options,
                           QString(), // the class prefix
                           QStringList() << "bool static_call");
    s << endl
    << "{" << endl;
    Indentation indent(INDENT);
    s << INDENT << "if (static_call) {" << endl;
    {
        Indentation indent(INDENT);
        writeBaseClassFunctionCall(s, java_function, implementor);
    }
    s << INDENT << "} else {" << endl;
    {
        Indentation indent(INDENT);
        writeBaseClassFunctionCall(s, java_function, implementor, VirtualCall);
    }

    s << INDENT << "}" << endl
    << "}" << endl << endl;
}


bool PlainCppImplGenerator::writeBaseClassFunctionCall(QTextStream &s,
        const AbstractMetaFunction *java_function,
        const AbstractMetaClass *,
        Option options) {
    bool rv = false;    // did we emit a "return"
    bool static_call = !(options & VirtualCall);
    if ((options & NoReturnStatement) == 0)
        s << INDENT;
    if (java_function->isAbstract() && static_call) {
        s << default_return_statement_qt(java_function->type(), options) << ";" << endl;
        if ((options & NoReturnStatement) == 0)
            rv = true;
    } else {
        if (java_function->type() && (options & NoReturnStatement) == 0) {
            s << "return ";
            rv = true;
        }
        if (static_call) {
            const AbstractMetaClass *implementor = java_function->implementingClass();
            if (java_function->isInterfaceFunction())
                implementor = java_function->interfaceClass()->primaryInterfaceImplementor();
            s << implementor->qualifiedCppName() << "::";
        }
        s << java_function->originalName() << "(";
        writeFunctionCallArguments(s, java_function, QString(), Option(options | ForceEnumCast));
        s << ");" << endl;
    }
    return rv;
}


void PlainCppImplGenerator::writeFunctionName(QTextStream &s,
        const AbstractMetaFunction *java_function,
        const AbstractMetaClass *java_class,
        uint options) {
    const AbstractMetaClass *cls = java_class ? java_class : java_function->ownerClass();
    AbstractMetaArgumentList arguments = java_function->arguments();

    // Function signature
    QString return_type = jniReturnName(java_function);
    QString function_name = java_function->marshalledName();

    bool callThrough = java_function->needsCallThrough();
    QString args = "__";

    if (callThrough && !java_function->isStatic() && !java_function->isConstructor())
        args += "J";

    if (!arguments.isEmpty()) {
        foreach(const AbstractMetaArgument *argument, arguments) {
            if (!java_function->argumentRemoved(argument->argumentIndex() + 1)) {
                if (!argument->type()->hasNativeId()) {
                    QString modified_type = java_function->typeReplaced(argument->argumentIndex() + 1);
                    if (modified_type.isEmpty())
                        args += c_signature(argument->type(), Underscores);
                    else
                        args += c_signature(modified_type, Underscores);
                } else {
                    args += "J";
                }
            }
        }
    }

    s << c_function_signature(cls->package(), cls->name(), function_name,
                              return_type, args, options);

}

void PlainCppImplGenerator::writeFinalFunctionArguments(QTextStream &s, const AbstractMetaFunction *java_function, const QString &java_object_name) {
    s << "(";

    uint nativeArgCount = 0;
    const AbstractMetaClass *cls = java_function->ownerClass();
    if (java_function->isConstructor() &&
        ( cls->hasVirtualFunctions()
        || cls->typeEntry()->isObject() ) )
    {
        s << "void *c_ptr";
        nativeArgCount++;
    }

    // passing pointer to C++ object
    bool callThrough = java_function->needsCallThrough();
    bool hasNativeId = callThrough && !java_function->isStatic() && !java_function->isConstructor();
    if (hasNativeId) {
        if (nativeArgCount > 0)
            s << "," << endl << " ";
        s << "void* __this_nativeId";
        nativeArgCount++;
    }

    // the function arguments
    AbstractMetaArgumentList arguments = java_function->arguments();
    foreach(const AbstractMetaArgument *argument, arguments) {
        if (!java_function->argumentRemoved(argument->argumentIndex() + 1)) {
            if (nativeArgCount > 0)
                s << "," << endl << " ";
            if (!argument->type()->hasNativeId())
                s << translateType(argument->type(), EnumAsInts);
            else
                s << "long ";
            s << " " << argument->indexedName();
            nativeArgCount++;
        }
    }
    s << ")" << endl;
}


/*!
    Generates type conversion from Java -> Qt for all the arguments
    that are to be to be passed to the function
*/
void PlainCppImplGenerator::writeFinalFunctionSetup(QTextStream &s, const AbstractMetaFunction *java_function,
        const QString &qt_object_name,
        const AbstractMetaClass *cls) {
    // Translate each of the function arguments into qt types
    AbstractMetaArgumentList arguments = java_function->arguments();
    foreach(const AbstractMetaArgument *argument, arguments) {
        if (!argument->type()->isPrimitive()
                || !java_function->conversionRule(TypeSystem::NativeCode, argument->argumentIndex() + 1).isEmpty()) {
            writeJavaToQt(s,
                          argument->type(),
                          "__qt_" + argument->indexedName(),
                          argument->indexedName(),
                          java_function,
                          argument->argumentIndex() + 1,
                          Option(UseNativeIds | EnumAsInts));
        }
    }

    // Extract the qt equivalent to the this pointer and name it "qt_object_name"
    if (!java_function->isStatic() && !java_function->isConstructor()) {
        QString className = java_function->isFinalOverload() ? cls->name() : shellClassName(cls);
        s << INDENT
        << className << " *" << qt_object_name
        << " = (" << className << " *) __this_nativeId;"
        << endl
        << INDENT << "// Q_ASSERT(" << qt_object_name << ");" << endl;
    }
}


void PlainCppImplGenerator::writeFinalFunction(QTextStream &s, const AbstractMetaFunction *java_function,
        const AbstractMetaClass *java_class) {
    Q_ASSERT(java_class);

    if (java_function->isModifiedRemoved(TypeSystem::NativeCode))
        return;

    if (java_function->isSignal() && java_function->isPrivate())
        return;

    const AbstractMetaClass *cls = java_class ? java_class : java_function->ownerClass();

    QString java_function_signature = cls->name() + "::" + java_function->signature();

    s << "// " << java_function_signature << endl;

    const AbstractMetaType *function_type = java_function->type();
    QString new_return_type = java_function->typeReplaced(0);
    bool has_function_type = new_return_type != "void"
    && (!new_return_type.isEmpty() || function_type != 0)
    && java_function->argumentReplaced(0).isEmpty();

    const QString qt_object_name = java_function->isStatic() ? shellClassName(cls) : "__qt_this";
    const QString java_object_name = java_function->isStatic() ? "__jni_class" : "__jni_object";

    // function signature...
    bool callThrough = java_function->needsCallThrough();
    uint options = m_native_jump_table ? ReturnType | ExternC : StandardJNISignature;
    writeFunctionName(s, java_function, cls, options);
    s << endl;
    writeFinalFunctionArguments(s, java_function, java_object_name);
    s << "{" << endl;
    Indentation indent(INDENT);

    s << INDENT << "// QTJAMBI_DEBUG_TRACE(\"(native) entering: " << java_function_signature << "\");" << endl;

    // Avoid compiler warnings when the variables are unused
    {
        if (java_function->isConstructor())
            s << INDENT << "Q_UNUSED(" << java_object_name << ")" << endl;

        bool hasNativeId = (callThrough && !java_function->isStatic() && !java_function->isConstructor());
        if (hasNativeId)
            s << INDENT << "Q_UNUSED(__this_nativeId)" << endl;
    }

    if (cls->isFinal() && (!java_function->isAbstract() || !java_function->isFinalInTargetLang()) && !java_function->wasPublic()) {
        QString debug = QString("protected function '%1' in final class '%2'")
                        .arg(java_function->signature()).arg(java_class->name());
        ReportHandler::warning(debug);
        // Avoid compiler warnings for unused parameters
        AbstractMetaArgumentList arguments = java_function->arguments();

        foreach(const AbstractMetaArgument *argument, arguments) {
            s << INDENT << "Q_UNUSED(" << argument->indexedName() << ")" << endl;
        }
        s << INDENT << default_return_statement_qt(java_function->type()) << ";";

    } else {
        writeFinalFunctionSetup(s, java_function, qt_object_name, cls);

        writeCodeInjections(s, java_function, java_function->implementingClass(), CodeSnip::Beginning, TypeSystem::NativeCode);

        if (java_function->isConstructor()) {
            writeFinalConstructor(s, java_function, qt_object_name, java_object_name);

            s << INDENT << "// QTJAMBI_DEBUG_TRACE(\"(native) -> leaving: "
            << java_function_signature << "\");" << endl;
        } else {

            QString function_prefix = "";
            QStringList extra_param;
            Option option = NoOption;

            bool hasShell = cls->generateShellClass();

            if (java_function->isFinalOverload()) {
                // no prefix
            } else if (java_function->isFinalInCpp() && !java_function->wasPublic() && hasShell) {
                function_prefix = "__public_";
            } else if (!java_function->isFinalInCpp() && !java_function->isStatic() && hasShell) {
                function_prefix = "__override_";
                extra_param.append("__do_static_call");
                s << INDENT
                << "// bool __do_static_call = __this_nativeId ? ((QtJambiLink *) "
                << "__this_nativeId)->createdByJava() : false;" << endl;
                s << INDENT
                << "bool __do_static_call = false;" << endl;
            } else {
                option = OriginalName;
            }

            // Call the Qt function on the java object
            s << "    ";
            if (has_function_type) {
                const QString qt_return_value = "__qt_return_value";
                if (function_type) {
                    writeTypeInfo(s, function_type, EnumAsInts);
                    s << " " << qt_return_value
                    << " = ";
                }

                writeFunctionCall(s, qt_object_name, java_function, function_prefix, option,
                                  extra_param);
                s << endl;

                s << INDENT << "// QTJAMBI_DEBUG_TRACE(\"(native) -> leaving: "
                << java_function_signature << "\");" << endl;

                s << INDENT << "return " << qt_return_value << ";";

            } else {
                writeFunctionCall(s, qt_object_name, java_function, function_prefix, option,
                                  extra_param);

                s << INDENT << "// QTJAMBI_DEBUG_TRACE(\"(native) -> leaving: "
                << java_function_signature << "\");" << endl;
            }
        }
    }
    if (!java_function->argumentReplaced(0).isEmpty()) {
        s << INDENT << "return 0;" << endl;
    }

    s << endl << "}";
    s << endl << endl;
}

void PlainCppImplGenerator::writeAssignment(QTextStream &s, const QString &destName, const QString &srcName,
                                       const AbstractMetaType *java_type) {
    if (java_type->isArray()) {
        for (int i = 0; i < java_type->arrayElementCount(); ++i) {
            writeAssignment(s, destName + "[" + QString::number(i) + "]",
                            srcName + "[" + QString::number(i) + "]", java_type->arrayElementType());
        }
    } else {
        s << INDENT << destName << " = " << srcName << ";" << endl;
    }
}

void PlainCppImplGenerator::writeFieldAccessors(QTextStream &s, const AbstractMetaField *java_field) {
/*
    Q_ASSERT(java_field);
    Q_ASSERT(java_field->isPublic() || java_field->isProtected());

    const AbstractMetaFunction *setter = java_field->setter();
    const AbstractMetaFunction *getter = java_field->getter();

    const AbstractMetaClass *cls = java_field->enclosingClass();
    FieldModification mod = cls->typeEntry()->fieldModification(java_field->name());

    // Setter
    if (mod.isWritable() && !java_field->type()->isConstant()) {
        // Write public override for protected fields
        if (setter->wasProtected()) {
            writeFunctionSignature(s, setter, setter->ownerClass());
            s << endl
            << "{" << endl;
            {
                Indentation indent(INDENT);

                Q_ASSERT(setter->arguments().count() > 0);
                const AbstractMetaArgument *argument = setter->arguments().at(0);

                QString thisRef = java_field->isStatic()
                                  ? setter->ownerClass()->qualifiedCppName() + QString("::")
                                  : QString("this->");
                writeAssignment(s, thisRef + java_field->name(), argument->indexedName(), argument->type());
            }
            s << "}" << endl << endl;
        }

        writeFunctionName(s, setter, setter->ownerClass());
        s << endl;
        writeFinalFunctionArguments(s, setter, "__java_object");
        s  << "{" << endl;

        {
            Indentation indent(INDENT);

            writeFinalFunctionSetup(s, setter, "__qt_object", setter->ownerClass());

            Q_ASSERT(setter->arguments().count() == 1);
            const AbstractMetaArgument *argument = setter->arguments().at(0);

            QString dest;
            if (setter->isStatic())
                dest = shellClassName(setter->ownerClass()) + "::";
            else
                dest = "__qt_object->";

            QString src;
            if (!argument->type()->isPrimitive())
                src = "__qt_" + argument->indexedName();
            else
                src = argument->indexedName();

            if (setter->wasPublic())
                writeAssignment(s, dest + java_field->name(), src, argument->type());
            else
                s << INDENT << dest << setter->name() << "_setter(" << src << ");" << endl;
        }
        s << "}" << endl << endl;
    }

    if (mod.isReadable()) {
        // Getter
        if (getter->wasProtected()) {
            writeFunctionSignature(s, getter, getter->ownerClass());
            s << endl
            << "{" << endl;
            {
                Indentation indent(INDENT);
                s << INDENT << "return " << java_field->name() << ";" << endl;
            }
            s << "}" << endl << endl;
        }


        writeFunctionName(s, getter, getter->ownerClass());
        s << endl;
        writeFinalFunctionArguments(s, getter, "__java_object");
        s << "{" << endl;
        {
            Indentation indent(INDENT);

            writeFinalFunctionSetup(s, getter, "__qt_object", getter->ownerClass());

            const QString java_return_value = "__java_return_value";
            QString qt_return_value;
            if (setter->isStatic())
                qt_return_value = shellClassName(setter->ownerClass()) + "::";
            else
                qt_return_value = "__qt_object->";


            // To avoid "taking address of tmp"
            s << INDENT;
            writeTypeInfo(s, getter->type(), Option(ArrayAsPointer));
            QString tmp_name = "__tmp_" + getter->name();
            s << tmp_name << " = ";

            if (getter->wasPublic())
                qt_return_value += java_field->name();
            else
                qt_return_value += getter->name() + "_getter()";
            s << qt_return_value << ";" << endl;

            writeQtToJava(s, getter->type(), tmp_name, java_return_value, 0, -1, EnumAsInts);
            s << INDENT << "return " << java_return_value << ";" << endl;
        }
        s << "}" << endl << endl;
    }
*/
}

void PlainCppImplGenerator::writeFinalDestructor(QTextStream &s, const AbstractMetaClass *cls) {
    if (cls->hasConstructors()) {
      /*
        s << INDENT << "static void qtjambi_destructor(void *ptr)" << endl
        << INDENT << "{" << endl;
        {
            Indentation indent(INDENT);
            if (!cls->isQObject() && !cls->generateShellClass()) {
                s << INDENT << "QtJambiLink *link = QtJambiLink::findLinkForUserObject(ptr);" << endl
                << INDENT << "if (link) link->resetObject(qtjambi_current_environment());" << endl;
            }

            // Code injectsions...
            foreach(const CodeSnip &snip, cls->typeEntry()->codeSnips()) {
                if (snip.language == TypeSystem::DestructorFunction) {
                    s << snip.code();
                }
            }

            s << INDENT << "delete (" << shellClassName(cls) << " *)ptr;" << endl;

            if (qtJambiDebugTools()) {
                s << "#if defined(QTJAMBI_DEBUG_TOOLS)" << endl;
                s << INDENT << "qtjambi_increase_destructorFunctionCalledCount(QString::fromLatin1(\"" << cls->name() << "\"));" << endl;
                s << "#endif / QTJAMBI_DEBUG_TOOLS /" << endl;
            }
        }

        s << INDENT << "}" << endl << endl;
        */
    }
}

void PlainCppImplGenerator::writeFinalConstructor(QTextStream &s,
        const AbstractMetaFunction *java_function,
        const QString &qt_object_name,
        const QString &java_object_name) {
/*
    const AbstractMetaClass *cls = java_function->ownerClass();
    AbstractMetaArgumentList arguments = java_function->arguments();
    QString className = cls->typeEntry()->qualifiedCppName();

    bool hasShellClass = cls->generateShellClass();

    s << INDENT << shellClassName(cls) << " *" << qt_object_name
    << " = new " << shellClassName(cls)
    << "(";
    writeFunctionCallArguments(s, java_function, "__qt_", SkipRemovedArguments);
    s << ");" << endl;

    if (cls->isQObject())
        s << INDENT << qt_object_name << "->m_meta_object = " << qt_object_name << "->initMetaObject(__jni_env, " << java_object_name << ");" << endl;

    s << INDENT << "QtJambiLink *__qt_java_link = ";
    if (cls->isQObject()) {
        s << "qtjambi_construct_qobject(__jni_env, " << java_object_name << ", "
        << qt_object_name << ", " << qt_object_name << "->m_meta_object)";
    } else {
        s << "qtjambi_construct_object(__jni_env, " << java_object_name << ", " << qt_object_name;
        if (cls->typeEntry()->isValue())
            s << ", \"" << className << "\")";
        else // non-QObject, object type
            s << ", QMetaType::Void, QLatin1String(\"" << cls->fullName().replace(".", "/") << "\"), true)";
    }
    s << ";" << endl
    << INDENT << "if (!__qt_java_link) {" << endl;
    {
        Indentation indent(INDENT);
        if (cls->isQObject())
*/
        //s << INDENT << "/* QtDynamicMetaObject::check_dynamic_deref(" << qt_object_name << "->m_meta_object) done in delete */" << endl;
/*
        s << INDENT << "delete " << qt_object_name << ";" << endl;
        s << INDENT << "qtjambishell_throw_runtimeexception(__jni_env, \"object construction failed for type: " << className << "\");" << endl;
        s << INDENT << "return;" << endl;
    }
    s << INDENT << "}" << endl;


    if (cls->isQObject()) {
        // Make sure all qobjects created by Java are owned by java only if
        // parent object has not been set.
        // All other objects will default to split ownership.
        s << INDENT << "if(!__qt_this->QObject::parent()){" << endl;
        s << INDENT << "    __qt_java_link->setJavaOwnership(__jni_env, " << java_object_name << ");" << endl;
        s << INDENT << "}" << endl;
    } else {
        // All non-qobjects created by Java are owned by java
        s << INDENT << "__qt_java_link->setJavaOwnership(__jni_env, " << java_object_name << ");" << endl;
    }

    if (hasCustomDestructor(cls)) {
        s << INDENT << "__qt_java_link->setDestructorFunction(qtjambi_destructor);" << endl;
    }

    if (cls->typeEntry()->typeFlags() & ComplexTypeEntry::DeleteInMainThread)
        s << INDENT << "__qt_java_link->setDeleteInMainThread(true);" << endl;

    if (!cls->hasVirtualFunctions() && !cls->hasInconsistentFunctions() && !cls->typeEntry()->isObject())
        return;

    if (hasShellClass) {
        // Set up the link object
        s << INDENT << qt_object_name << "->m_link = __qt_java_link;" << endl;

        s << INDENT << qt_object_name << "->m_link->setCreatedByJava(true);" << endl;



        AbstractMetaClassList interfaces = cls->interfaces();
        int totalRegisterSubObject = interfaces.size() + (cls->baseClass() != 0 ? 1 : 0);
        if (totalRegisterSubObject > 1)  {
            s << INDENT << "{" << endl;
            {
                Indentation indent(INDENT);
                s << INDENT << "void *regPtrs[" << totalRegisterSubObject << "];" << endl;
                s << "#if defined(QTJAMBI_DEBUG_TOOLS)" << endl;
                s << INDENT << "const char *regNamePtrs[" << totalRegisterSubObject << "];" << endl;
                s << "#else" << endl;
                s << INDENT << "const char **regNamePtrs = 0;" << endl;
*/
                //s << "#endif /* QTJAMBI_DEBUG_TOOLS */" << endl;
/*
                if (cls->baseClass() != 0)
                    interfaces += cls->baseClass();
                int i = 0;
                foreach(AbstractMetaClass *iface, interfaces) {
                    AbstractMetaClass *impl = iface->isInterface() ? iface->primaryInterfaceImplementor() : iface;
                    //s << INDENT << qt_object_name << "->m_link->registerSubObject((" << impl->qualifiedCppName() << " *) " << qt_object_name << ");" << endl;
                    s << INDENT << "regPtrs[" << i << "] = (" <<  impl->qualifiedCppName() << " *) " << qt_object_name << ";" << endl;
                    s << "#if defined(QTJAMBI_DEBUG_TOOLS)" << endl;
                    s << INDENT << "regNamePtrs[" << i << "] = \"" <<  impl->qualifiedCppName() << "\";" << endl;
*/
                    //s << "#endif /* QTJAMBI_DEBUG_TOOLS */" << endl;
/*
                    i++;
                }
                s << INDENT << qt_object_name << "->m_link->registerSubObjects(" << qt_object_name << ", " << i << ", regPtrs, regNamePtrs);" << endl;
            }
            s << INDENT << "}" << endl;
        }
    }

    if (!cls->hasVirtualFunctions() && !cls->hasInconsistentFunctions())
        return;

    // Set up the vtable
    s << INDENT;
    QString space(24, ' ');
    if (hasShellClass)
        s << qt_object_name << "->m_vtable = ";
    s << "qtjambi_setup_vtable(__jni_env, " << endl << space << "__jni_object, " << endl;

    if (cls->hasInconsistentFunctions()) {
        s << space << "qtjambi_inconsistent_count, " << endl
        << space << "qtjambi_inconsistent_names, " << endl
        << space << "qtjambi_inconsistent_signatures, " << endl;
    } else {
        s << space << "0, 0, 0, // no inconsistent functions" << endl;
    }

    if (cls->hasVirtualFunctions()) {
        s << space << "qtjambi_method_count, " << endl
        << space << "qtjambi_method_names, " << endl
        << space << "qtjambi_method_signatures" << endl;
    } else {
        s << space << "0, 0, 0 // no virtual functions" << endl;
    }

    s << space << ");" << endl;
*/
}

void PlainCppImplGenerator::writeSignalInitialization(QTextStream &s, const AbstractMetaClass *java_class) {
    if (!java_class->isQObject()
            || java_class->cppSignalFunctions().size() == 0) {
        return ;
    }

    /*
    s << c_function_signature(java_class->package(), java_class->name(), "__qt_signalInitialization", "jboolean")
    << endl << "(JNIEnv *__jni_env, jobject java_object, jlong ptr, jstring java_signal_name)" << endl
    << "{" << endl
    << "   QtJambiLink *link = (QtJambiLink *) ptr;" << endl
    << "   if (link == 0)" << endl
    << "       return true;" << endl << endl
    << "   QObject *qt_this = link->qobject();" << endl
    << "   Q_ASSERT(qt_this);" << endl << endl
    << "   QtJambi_SignalWrapper_" << java_class->name() << " *qt_wrapper = "
    << "   (QtJambi_SignalWrapper_" << java_class->name() << " *) link->signalWrapper();" << endl
    << "   QString signal_name = qtjambi_to_qstring(__jni_env, java_signal_name);" << endl
    << "   if (qt_wrapper == 0) {" << endl
    << "       qt_wrapper = new QtJambi_SignalWrapper_" << java_class->name() << ";" << endl
    << "       link->setSignalWrapper(qt_wrapper);" << endl
    << "       qt_wrapper->link = link;" << endl << endl
    << "       qtjambi_resolve_signals(__jni_env," << endl
    << "                               java_object," << endl
    << "                               qt_wrapper->m_signals," << endl
    << "                               qtjambi_signal_count," << endl
    << "                               qtjambi_signal_names," << endl
    << "                               qtjambi_signal_argumentcounts" << endl
    << "#if defined(QTJAMBI_DEBUG_TOOLS)" << endl
    << "                               ," << endl
    << "                               \"" << java_class->fullName() << "\"," << endl
    << "                               signal_name.toLatin1().constData()" << endl
    << "#endif / QTJAMBI_DEBUG_TOOLS /" << endl
    << "                               );" << endl
    << "   }" << endl
    << "   return qtjambi_connect_cpp_to_java(__jni_env," << endl
    << "                                      signal_name," << endl
    << "                                      qt_this," << endl
    << "                                      qt_wrapper," << endl
    << "                                      QLatin1String(\"" << java_class->fullName() << "\")," << endl
    << "                                      QLatin1String(\"" << signalWrapperPrefix() << "\"));" << endl
    << "}" << endl;
    */
}

QString PlainCppImplGenerator::fromObject(const TypeEntry *entry,
                                     const QString &var_name) {
    QString returned;
/*
    QString package = entry->javaPackage();
    const ComplexTypeEntry *centry = entry->isComplex()
                                     ? static_cast<const ComplexTypeEntry *>(entry)
                                     : 0;

    if (centry == 0 || centry->polymorphicIdValue().isEmpty()) {
        returned = "qtjambi_from_object(__jni_env, " + var_name + ", \""
                   + entry->lookupName()
                   + "\", \"" + QString(package).replace(".", "/") + "/\", true);";
    } else {
        AbstractMetaClass *cls = classes().findClass(centry->qualifiedCppName());
        if (!cls) {
            qFatal("PlainCppImplGenerator::fromObject(): class '%s' could not be resolved...",
                   qPrintable(centry->qualifiedCppName()));
        }

        while (cls != 0 && !cls->typeEntry()->isPolymorphicBase())
            cls = cls->baseClass();

        QString full_name;
        if (cls != 0) {
            full_name = cls->fullName();
        } else {
            ReportHandler::warning(QString("class '%1' has polymorphic id but does not inherit a polymorphic class")
                                   .arg(centry->qualifiedCppName()));
        }

        returned = "qtjambi_from_object(__jni_env, " + var_name + ", \""
                   + centry->lookupName()
                   + "\", \"" + QString(package).replace(".", "/") + "/\","
                   + "\"" + c_signature(full_name, Underscores) + "\", true);";
    }

*/
    return returned;
}

void PlainCppImplGenerator::writeOriginalMetaObjectFunction(QTextStream &s, const AbstractMetaClass *java_class) {
    Q_ASSERT(java_class->isQObject());
/*

    s << c_function_signature(java_class->package(),
                              java_class->name(),
                              "originalMetaObject",
                              "jlong");

    s << endl
    << "(JNIEnv *," << endl
    << " jclass)" << endl
    << "{" << endl;
    {
        Indentation indent(INDENT);
        s << INDENT << "return reinterpret_cast<jlong>(&" << java_class->qualifiedCppName() << "::staticMetaObject);" << endl;
    }
    s << "}" << endl << endl;
*/
}

void PlainCppImplGenerator::writeFromNativeFunction(QTextStream &s, const AbstractMetaClass *java_class) {
/*
    s << c_function_signature(java_class->package(),
                              java_class->name(),
                              "fromNativePointer",
                              "jobject");
    s << endl
    << "(JNIEnv *__jni_env," << endl
    << " jclass," << endl
    << " jobject nativePointer)" << endl
    << "{" << endl;
    {
        Indentation indent(INDENT);
        s << INDENT << "void *ptr = qtjambi_to_cpointer(__jni_env, nativePointer, 1);" << endl
        << INDENT << "return " << fromObject(java_class->typeEntry(), "ptr") << endl
        << "}" << endl;
    }
*/
}

void PlainCppImplGenerator::writeFromArrayFunction(QTextStream &s, const AbstractMetaClass *java_class) {
/*
    s << c_function_signature(java_class->package(),
                              java_class->name(),
                              "nativePointerArray",
                              "jobject");
    s << endl
    << "(JNIEnv *__jni_env," << endl
    << " jclass," << endl
    << " jobjectArray array)" << endl
    << "{" << endl;
    {
        Indentation indent(INDENT);
        s << INDENT << "return qtjambi_array_to_nativepointer(__jni_env, " << endl
        << INDENT << "                                     array, " << endl
        << INDENT << "                                     sizeof("
        << java_class->qualifiedCppName() << "));" << endl;
    }
    s << "}" << endl;
*/
}


void PlainCppImplGenerator::writeInterfaceCastFunction(QTextStream &s,
        const AbstractMetaClass *java_class,
        const AbstractMetaClass *interface) {
/*
    Q_ASSERT(interface->isInterface());
    const InterfaceTypeEntry *ie = static_cast<const InterfaceTypeEntry *>(interface->typeEntry());
    QString interface_name = ie->origin()->targetLangName();

    s << endl
    << c_function_signature(java_class->package(),
                            java_class->name(),
                            QString("__qt_cast_to_%1").arg(interface_name),
                            "jlong",
                              "__J");

    s << endl
    << "(JNIEnv *," << endl
    << " jobject," << endl
    << " jlong ptr)" << endl
    << "{" << endl
    << "    return (jlong) (" << interface->primaryInterfaceImplementor()->qualifiedCppName() << " *) "
                << "(" << java_class->qualifiedCppName() << " *) ptr;" << endl
                << "}" << endl;
*/
}

bool PlainCppImplGenerator::writeConversionRule(QTextStream &s,
        TypeSystem::Language target_language,
        const AbstractMetaFunction *java_function,
        int argument_index,
        const QString &qt_name,
        const QString &java_name) {
    if (argument_index < 0 || java_function == 0)
        return false;

    QString conversion_rule = java_function->conversionRule(target_language, argument_index);

    if (!conversion_rule.isEmpty()) {
        QString qt_name_var;
        QString java_name_var;

        if ((argument_index == 0 && target_language == TypeSystem::NativeCode)
                || (argument_index != 0 && target_language == TypeSystem::ShellCode)) {
            qt_name_var = "%in";
            java_name_var = "%out";
        } else {
            qt_name_var = "%out";
            java_name_var = "%in";
        }

        conversion_rule  = conversion_rule.replace(qt_name_var, qt_name)
                           .replace(java_name_var, java_name);

        AbstractMetaArgumentList arguments = java_function->arguments();
        for (int i = 0; i < arguments.size(); ++i) {
            conversion_rule = conversion_rule.replace("%" + QString::number(i + 1),
                              arguments.at(i)->indexedName());
        }

        QStringList lines = conversion_rule.split("\n");
        foreach(QString line, lines) {
            s << INDENT << line.trimmed() << endl;
        }

        return true;
    } else {
        return false;
    }
}


void PlainCppImplGenerator::writeJavaToQt(QTextStream &s,
                                     const AbstractMetaClass *java_class,
                                     const AbstractMetaType *function_return_type,
                                     const QString &qt_name,
                                     const QString &java_name,
                                     const AbstractMetaFunction *java_function,
                                     int argument_index) {
    // Conversion to C++: Shell code for return values, native code for arguments
/*
    TypeSystem::Language lang = argument_index == 0 ? TypeSystem::ShellCode : TypeSystem::NativeCode;
    if (writeConversionRule(s, lang, java_function, argument_index, qt_name, java_name))
        return;

    s << INDENT << shellClassName(java_class) << " *" << qt_name << " = ("
    << shellClassName(java_class) << " *) ";
    if (java_class->isQObject())
        s << "qtjambi_to_qobject";
    else
        s << "qtjambi_to_object";
    s << "(__jni_env, " << java_name << ");" << endl;
    if (java_class->isQObject()) {
        // ### throw exceptions when objects are null...
        s << INDENT << "if (!" << qt_name << ") "
        << default_return_statement_java(function_return_type) << ";"  << endl << endl;
    }
*/
}


void PlainCppImplGenerator::writeJavaToQt(QTextStream &s,
                                     const AbstractMetaType *java_type,
                                     const QString &qt_name,
                                     const QString &java_name,
                                     const AbstractMetaFunction *java_function,
                                     int argument_index,
                                     Option options) {
    // Conversion to C++: Shell code for return values, native code for arguments
/*
    TypeSystem::Language lang = argument_index == 0 ? TypeSystem::ShellCode : TypeSystem::NativeCode;
    if (java_function && writeConversionRule(s, lang, java_function, argument_index, qt_name, java_name))
        return;

    if (java_type == 0) {
        QString warn = QString("no conversion possible for argument '%1' in function '%2::%3' for "
                               "language '%4'")
                       .arg(argument_index)
                       .arg(java_function->implementingClass()->name())
                       .arg(java_function->name())
                       .arg(int(lang));
        ReportHandler::warning(warn);
        return;
    }

    if (java_type->isJObjectWrapper()) {
        s << INDENT << "JObjectWrapper " << qt_name
        << " = qtjambi_to_jobjectwrapper(__jni_env, " << java_name << ");" << endl;
    } else if (java_type->isVariant()) {
        s << INDENT << "QVariant " << qt_name
        << " = qtjambi_to_qvariant(__jni_env, " << java_name << ");" << endl;
    } else if (java_type->isArray() && java_type->arrayElementType()->isPrimitive()) {
        AbstractMetaType *elementType = java_type->arrayElementType();

        // ### Don't assert on wrong array lengths
        s << INDENT << "Q_ASSERT(__jni_env->GetArrayLength((jarray) " << java_name << ") == " << java_type->arrayElementCount() << ");" << endl;
        s << INDENT;
        writeTypeInfo(s, elementType);
        s << " " << qt_name << "[" << java_type->arrayElementCount() << "];" << endl;

#if 0 // JNI -> C TODO
        s << INDENT << "__jni_env->" << getXxxArrayRegion(elementType) << "( (" << translateType(java_type, options)
        << ")" << java_name << ", 0, " << java_type->arrayElementCount() << ", "
        << "(" << translateType(elementType, options) << " *" << ")"
        << qt_name << ");" << endl;
#endif

    } else if (java_type->isArray()) {
        AbstractMetaType *elementType = java_type->arrayElementType();

        s << INDENT << "Q_ASSERT(__jni_env->GetArrayLength((jarray) " << java_name << ") == " << java_type->arrayElementCount() << ");" << endl;
        writeTypeInfo(s, elementType);
        s << "[" << java_type->arrayElementCount() << "]" << qt_name << ";" << endl;

        for (int i = 0; i < java_type->arrayElementCount(); ++i) {
            writeJavaToQt(s, elementType, qt_name + "[" + QString::number(i) + "]",
                          "__jni_env->GetObjectArrayElement(" + java_name + ", " + QString::number(i) + ")", 0, -1, options);
        }

    } else if (java_type->isTargetLangString()) {
        s << INDENT << "QString " << qt_name
        << " =  qtjambi_to_qstring(__jni_env, (jstring) " << java_name << ");" << endl;

    } else if (java_type->isTargetLangStringRef()) {
        s << INDENT << "QString " << qt_name << "_s"
        << " =  qtjambi_to_qstring(__jni_env, (jstring) " << java_name << ");" << endl;
        s << INDENT << "QStringRef " << qt_name
        << " =  QStringRef(&" << qt_name << "_s);" << endl;

    } else if (java_type->isTargetLangChar()) {
        s << INDENT << "QChar " << qt_name
        << " = (ushort)" << java_name << ";" << endl;

    } else if (java_type->isEnum() || java_type->isFlags()) {

        bool written = false;
        if (java_type->isEnum()) {
            AbstractMetaEnum *java_enum =
                m_classes.findEnum(static_cast<const EnumTypeEntry *>(java_type->typeEntry()));
            if (java_enum && !java_enum->isPublic()) {

                s << INDENT << "int " << qt_name << " = ";
                written = true;
            }
        }

        if (!written) {
            QString qualified_name = java_type->typeEntry()->qualifiedCppName();
            s << INDENT << qualified_name << " " << qt_name
            << " = (" << qualified_name << ") ";
        }

        if ((options & EnumAsInts) == 0 && (java_type->isTargetLangEnum() || java_type->isTargetLangFlags())) {
            s << "qtjambi_to_enumerator(__jni_env, " << java_name << ");" << endl;

        } else if (options & BoxedPrimitive) {
            const PrimitiveTypeEntry *pentry = TypeDatabase::instance()->findTargetLangPrimitiveType("int");
            Q_ASSERT(pentry);

            s << "qtjambi_to_" << pentry->targetLangName() << "(__jni_env, " << java_name << ");" << endl;

        } else {
            s << java_name << ';' << endl;
        }

    } else if (java_type->isContainer()) {
        writeJavaToQtContainer(s, java_type, qt_name, java_name, 0, -1);

    } else if (java_type->isThread()) {
        s << INDENT << "QThread *" << qt_name << " = qtjambi_to_thread(__jni_env, " << java_name
        << ");" << endl;

    } else if (java_type->typeEntry()->isCustom()) {
        const CustomTypeEntry *custom_type =
            static_cast<const CustomTypeEntry *>(java_type->typeEntry());
        s << INDENT;
        custom_type->generateCppJavaToQt(s, java_type, "__jni_env", qt_name, java_name);
        s << ";" << endl;

    } else {

        const TypeEntry *type = java_type->typeEntry();
        QString class_name = type->name();
        QString qualified_class_name = fixCppTypeName(type->qualifiedCppName());

        // Declaration and the c-cast
        s << INDENT;
        writeTypeInfo(s, java_type);
        s << ' ' << qt_name << " = (";
        writeTypeInfo(s, java_type);
        s << ") ";

        if (java_type->isPrimitive()) {
            if (options & BoxedPrimitive) {
                const PrimitiveTypeEntry *pentry = static_cast<const PrimitiveTypeEntry *>(type);

                if (!pentry->preferredConversion())
                    pentry = TypeDatabase::instance()->findTargetLangPrimitiveType(pentry->targetLangName());
                Q_ASSERT(pentry);

                s << "qtjambi_to_" << pentry->targetLangName() << "(__jni_env, " << java_name << ");" << endl;

            } else if ((options & GlobalRefJObject) && type->jniName() == QLatin1String("jobject")) {
                s << "__jni_env->NewGlobalRef(" << java_name << ");" << endl;
            } else {
                s << java_name << ';' << endl;
            }

#if 0
        } else if (java_type->isEnum()) {
            s << "qtjambi_to_enum(__jni_env, " << java_name << ");" << endl;
#endif

        } else if ((java_type->isQObject() || java_type->isObject())
                   && static_cast<const ObjectTypeEntry *>(type)->designatedInterface()) {
            const InterfaceTypeEntry *ie =
                static_cast<const ObjectTypeEntry *>(type)->designatedInterface();
            s << "qtjambi_to_interface(__jni_env, ";

            // This cast is only valid if we're dealing with a native id
            if ((options & UseNativeIds) == UseNativeIds)
                s << "(QtJambiLink *)";

            s << java_name << ", ";

            s << "\"" << ie->targetLangName() << "\", \""
            << ie->javaPackage().replace(".", "/") << "/\", "
            << "\"__qt_cast_to_" << type->targetLangName() << "\");" << endl;

        } else if (java_type->isObject() || java_type->isQObject() || java_type->isNativePointer()) {
            if (java_type->isReference()) {
                s << "* (" << qualified_class_name << " "
                << QString(java_type->actualIndirections(), '*') << ") ";
            }

            if (java_type->isNativePointer()) {
                s << "qtjambi_to_cpointer("
                << "__jni_env, "
                << java_name << ", "
                << java_type->actualIndirections() << ");" << endl;
            } else if (java_type->isQObject()) {
                if ((options & UseNativeIds) == 0)
                    s << "qtjambi_to_qobject(__jni_env, ";
                else
                    s << "qtjambi_from_jlong(";
                s << java_name;
                s << ");" << endl;
            } else {
                if ((options & UseNativeIds) == 0)
                    s << "qtjambi_to_object(__jni_env, ";
                else
                    s << "qtjambi_from_jlong(";
                s << java_name;
                s << ");" << endl;
            }

        } else {
            // Return values...
            if (argument_index == 0) {
                s << "(" << java_name << " != 0 ? *(" << qualified_class_name << " *)";
                if ((options & UseNativeIds) == 0)
                    s << "qtjambi_to_object(__jni_env, ";
                else
                    s << "qtjambi_from_jlong(";
                s << java_name;
                s << ") : " << qualified_class_name << "());" << endl;
            } else {
                s << "*"
                << "(" << qualified_class_name << " *)";
                bool null_check = false;
                if ((options & UseNativeIds) == 0) {
                    s << "qtjambi_to_object(__jni_env, ";
                } else if (hasDefaultConstructor(java_type)) {
                    null_check = true;
                    s << "(" << java_name << " != 0 ? qtjambi_from_jlong(";
                } else {
                    s << "qtjambi_from_jlong(";
                }
                s << java_name;
                s << ")";

                if (null_check)
                    s << " : default_" << QString(qualified_class_name).replace("::", "_") << "())";
                s << ";" << endl;
            }

        }
    }
    s << INDENT << "QTJAMBI_EXCEPTION_CHECK(__jni_env);" << endl;
*/
}

static int nativePointerType(const AbstractMetaType *java_type) {
    Q_ASSERT(java_type);
    Q_ASSERT(java_type->isNativePointer());

    if (!java_type->typeEntry()->isPrimitive())
        return PointerType;

    if (java_type->indirections() > 1)
        return PointerType;

    static QHash<QString, int> types;
    if (types.isEmpty()) {
        types["boolean"] = BooleanType;
        types["byte"] = ByteType;
        types["char"] = CharType;
        types["short"] = ShortType;
        types["int"] = IntType;
        types["long"] = LongType;
        types["float"] = FloatType;
        types["double"] = DoubleType;
    }

    QString targetLangName = java_type->typeEntry()->targetLangName();
    if (!types.contains(targetLangName))
        return PointerType;

    return types[targetLangName];
}

static const QString nativePointerTypeString(const AbstractMetaType *java_type) {
    int type = nativePointerType(java_type);

    static QHash<int, QString> macrodef;
    if (macrodef.isEmpty()) {
        macrodef[BooleanType] = QString("QTJAMBI_NATIVEPOINTER_TYPEID__BOOLEAN_0");
        macrodef[ByteType]    = QString("QTJAMBI_NATIVEPOINTER_TYPEID__BYTE_1");
        macrodef[CharType]    = QString("QTJAMBI_NATIVEPOINTER_TYPEID__CHAR_2");
        macrodef[ShortType]   = QString("QTJAMBI_NATIVEPOINTER_TYPEID__SHORT_3");
        macrodef[IntType]     = QString("QTJAMBI_NATIVEPOINTER_TYPEID__INT_4");
        macrodef[LongType]    = QString("QTJAMBI_NATIVEPOINTER_TYPEID__LONG_5");
        macrodef[FloatType]   = QString("QTJAMBI_NATIVEPOINTER_TYPEID__FLOAT_6");
        macrodef[DoubleType]  = QString("QTJAMBI_NATIVEPOINTER_TYPEID__DOUBLE_7");
        macrodef[PointerType] = QString("QTJAMBI_NATIVEPOINTER_TYPEID__POINTER_8");
        macrodef[StringType]  = QString("QTJAMBI_NATIVEPOINTER_TYPEID__STRING_9");
    }

    if(macrodef.contains(type))
        return macrodef[type];

    return QString("%1").arg(type);
}

void PlainCppImplGenerator::writeQtToJava(QTextStream &s,
                                     const AbstractMetaType *java_type,
                                     const QString &qt_name,
                                     const QString &java_name,
                                     const AbstractMetaFunction *java_function,
                                     int argument_index,
                                     Option option) {
/*
    // Conversion to Java: Native code for return values, shell code for arguments
    TypeSystem::Language lang = argument_index == 0 ? TypeSystem::NativeCode : TypeSystem::ShellCode;
    if (java_function && writeConversionRule(s, lang, java_function, argument_index, qt_name, java_name))
        return;

    if (java_type == 0) {
        QString warn = QString("no conversion possible for argument '%1' in function '%2::%3' for "
                               "language '%4'")
                       .arg(argument_index)
                       .arg(java_function->implementingClass()->name())
                       .arg(java_function->name())
                       .arg(int(lang));
        ReportHandler::warning(warn);
        return;
    }


    if (java_type->isArray() && java_type->arrayElementType()->isPrimitive()) {
        AbstractMetaType *elementType = java_type->arrayElementType();

#if 0 // JNI -> C TODO
        s << INDENT << translateType(java_type, option) << " " << java_name << " = __jni_env->" << newXxxArray(elementType)
        << "(" << java_type->arrayElementCount() << ");" << endl;

        s << INDENT << "__jni_env->" << setXxxArrayRegion(elementType) << "("
        << "(" << translateType(java_type, option) << ")" << java_name
        << ", 0, " << java_type->arrayElementCount() << ", "
        << "(" << translateType(elementType, option) << " *" << ")"
        << qt_name << ");" << endl;
#endif

    } else if (java_type->isArray()) {
        AbstractMetaType *elementType = java_type->arrayElementType();

        s << INDENT << "jobject " << java_name << " = __jni_env->NewObjectArray("
        << java_type->arrayElementCount() << ");" << endl;

        s << "jobject __qt_element = 0;";

        for (int i = 0; i < java_type->arrayElementCount(); ++i) {
            writeQtToJava(s, elementType, qt_name + "[" + QString::number(i) + "]",
                          "__qt_element", 0, -1, option);
            s << "__jni_env->SetObjectArrayElement((jobjectArray) " << java_name << ", "
            << i << ", __qt_element);" << endl;
        }

    } else if (java_type->isPrimitive()) {
        const PrimitiveTypeEntry *type =
            static_cast<const PrimitiveTypeEntry *>(java_type->typeEntry());
        Q_ASSERT(type);
        if (option & BoxedPrimitive) {
            s << INDENT << "jobject " << java_name << " = qtjambi_from_" << type->targetLangName()
            << "(__jni_env, " << qt_name << ");" << endl;
        } else {
            s << INDENT << type->jniName() <<  " " << java_name << " = (" << type->jniName() << ") "
            << qt_name << ";" << endl;
        }
    } else if (java_type->isJObjectWrapper()) {
        s << INDENT << "jobject " << java_name << " = qtjambi_from_jobjectwrapper(__jni_env, "
        << qt_name << ");" << endl;
    } else if (java_type->isVariant()) {
        s << INDENT << "jobject " << java_name << " = qtjambi_from_qvariant(__jni_env, "
        << qt_name << ");" << endl;

    } else if (java_type->isTargetLangString()) {
        s << INDENT << "jstring " << java_name << " = qtjambi_from_qstring(__jni_env, "
        << qt_name << ");" << endl;

    } else if (java_type->isTargetLangStringRef()) {
        s << INDENT << "jstring " << java_name << " = qtjambi_from_qstringref(__jni_env, "
        << qt_name << ");" << endl;

    } else if (java_type->isTargetLangChar()) {
        s << INDENT << "jchar " << java_name << " = " << qt_name << ".unicode();" << endl;

    } else if (java_type->isIntegerEnum() || java_type->isIntegerFlags()
               || ((option & EnumAsInts) && (java_type->isEnum() || java_type->isFlags()))) {
//     } else if (java_type->isEnum() || java_type->isFlags()) {

//         if (option & EnumAsInts) {
//             qDebug() << java_type->name() << "should be int...";
//         }

        if (option & BoxedPrimitive) {
            s << INDENT << "jobject " << java_name << " = qtjambi_from_int(__jni_env, "
            << qt_name << ");" << endl;
        } else {
            s << INDENT << "int " << java_name << " = " << qt_name << ";" << endl;
        }

    } else if (java_type->isTargetLangEnum()) {
        Q_ASSERT((option & EnumAsInts) == 0);
        const EnumTypeEntry *et = static_cast<const EnumTypeEntry *>(java_type->typeEntry());
        s << INDENT << "jobject " << java_name << " = qtjambi_from_enum(__jni_env, "
        << qt_name << ", \"" << et->javaPackage().replace('.', '/') << '/'
        << et->javaQualifier() << '$' << et->targetLangName() << "\");" << endl;

    } else if (java_type->isTargetLangFlags()) {
        Q_ASSERT((option & EnumAsInts) == 0);
        const FlagsTypeEntry *ft = static_cast<const FlagsTypeEntry *>(java_type->typeEntry());
        s << INDENT << "jobject " << java_name << " = qtjambi_from_flags(__jni_env, "
        << qt_name << ", \"" << ft->javaPackage().replace('.', '/') << '/'
        << ft->originator()->javaQualifier() << '$' << ft->targetLangName() << "\");" << endl;

    } else if (java_type->isContainer()) {
        writeQtToJavaContainer(s, java_type, qt_name, java_name, 0, -1);

    } else if (java_type->isThread()) {
        s << INDENT << "jobject " << java_name << " = qtjambi_from_thread(__jni_env, " << qt_name
        << ");" << endl;

    } else if (!java_type->isNativePointer() && java_type->typeEntry()->isCustom()) {
        s << INDENT;
        static_cast<const CustomTypeEntry *>(java_type->typeEntry())
        ->generateCppQtToJava(s, java_type, "__jni_env", qt_name, java_name);
        s << ";" << endl;

    } else {
        s << INDENT << "jobject " << java_name << " = ";

        if (java_type->isQObject()) {
            s << "qtjambi_from_qobject(__jni_env, " << "(QObject *) ";

            if (java_type->isReference() && java_type->indirections() == 0)
                s << "&";

            s << qt_name
            << ", \"" << java_type->typeEntry()->lookupName() << "\""
            << ", \"" << java_type->package().replace(".", "/") << "/\""
            << ");" << endl;

#if 0
        } else if (java_type->isEnum()) {

            const EnumTypeEntry *et = static_cast<const EnumTypeEntry *>(java_type->typeEntry());
            s << "qtjambi_from_enum(__jni_env, " << qt_name << ", \""
            << et->javaQualifier() << "$" << et->targetLangName() << "\");" << endl;
#endif
        } else if (java_type->isNativePointer()) {
            s << "qtjambi_from_cpointer(__jni_env, ";
            if (java_type->isReference())
                s << "&";
            s << qt_name << ", " << nativePointerTypeString(java_type) << ", "
            << java_type->actualIndirections() << ");" << endl;
        } else if (java_type->isValue()) {
            s << fromObject(java_type->typeEntry(), "&" + qt_name) << endl;
        } else {
            s << fromObject(java_type->typeEntry(),
                            (java_type->isReference() ? "&" : "") + qt_name) << endl;
        }
    }

    s << INDENT << "QTJAMBI_EXCEPTION_CHECK(__jni_env);" << endl;
*/
}


void PlainCppImplGenerator::writeQtToJavaContainer(QTextStream &s,
        const AbstractMetaType *java_type,
        const QString &qt_name,
        const QString &java_name,
        const AbstractMetaFunction *java_function,
        int argument_index) {
    // Language for conversion to Java: Native code for return values and Shell code for arguments
/*
    TypeSystem::Language lang = argument_index == 0 ? TypeSystem::NativeCode : TypeSystem::ShellCode;
    if (java_function && writeConversionRule(s, lang, java_function, argument_index, qt_name, java_name))
        return;

    if (java_type == 0) {
        QString warn = QString("no conversion possible for argument '%1' in function '%2::%3' for "
                               "language '%4'")
                       .arg(argument_index)
                       .arg(java_function->implementingClass()->name())
                       .arg(java_function->name())
                       .arg(int(lang));
        ReportHandler::warning(warn);
        return;
    }

    Q_ASSERT(java_type->isContainer());
    const ContainerTypeEntry *type =
        static_cast<const ContainerTypeEntry *>(java_type->typeEntry());

    if (type->type() == ContainerTypeEntry::ListContainer
            || type->type() == ContainerTypeEntry::VectorContainer
            || type->type() == ContainerTypeEntry::StringListContainer
            || type->type() == ContainerTypeEntry::LinkedListContainer
            || type->type() == ContainerTypeEntry::StackContainer
            || type->type() == ContainerTypeEntry::SetContainer
            || type->type() == ContainerTypeEntry::QueueContainer) {
        Q_ASSERT(java_type->instantiations().size() == 1);
        AbstractMetaType *targ = java_type->instantiations().first();

        s << endl
        << INDENT << "jobject " << java_name << " = ";

        switch (type->type()) {
            case ContainerTypeEntry::LinkedListContainer:
            case ContainerTypeEntry::QueueContainer:
                s << "qtjambi_linkedlist_new(__jni_env)";
                break;
            case ContainerTypeEntry::StackContainer:
                s << "qtjambi_stack_new(__jni_env)";
                break;
            case ContainerTypeEntry::SetContainer:
                s << "qtjambi_hashset_new(__jni_env)";
                break;
            default:
                s << "qtjambi_arraylist_new(__jni_env, " << qt_name << ".size())";
                break;
        }

        s << ";" << endl
        << INDENT;


        writeTypeInfo(s, java_type, ForceValueType);
        QString iteratorEndName = "__qt_" + QString(qt_name).replace('.', '_') + "_end_it";
        QString iteratorName = "__qt_" + QString(qt_name).replace('.', '_') + "_it";
        s << "::const_iterator " << iteratorEndName << " = " << qt_name << ".constEnd();" << endl
        << INDENT;
        s << "for (";
        writeTypeInfo(s, java_type, ForceValueType);
        s << "::const_iterator " << iteratorName << " = " << qt_name << ".constBegin(); "
        << iteratorName << " != " << iteratorEndName << "; ++" << iteratorName << ") {" << endl;
        {
            Indentation indent(INDENT);
            s << INDENT;
            writeTypeInfo(s, targ);
            s << " __qt_tmp = *" << iteratorName << ";" << endl;
            writeQtToJava(s, targ, "__qt_tmp", "__java_tmp", 0, -1, BoxedPrimitive);
            s << INDENT << "qtjambi_collection_add(__jni_env, " << java_name << ", __java_tmp);"
            << endl;
        }
        s << INDENT << "}" << endl;

    } else if (type->type() == ContainerTypeEntry::PairContainer) {
        QList<AbstractMetaType *> args = java_type->instantiations();
        Q_ASSERT(args.size() == 2);

        s << INDENT << "jobject " << java_name << ";" << endl
        << INDENT << "{" << endl;
        {
            Indentation indent(INDENT);
            writeQtToJava(s, args.at(0), qt_name + ".first", "__java_tmp_first", 0, -1, BoxedPrimitive);
            writeQtToJava(s, args.at(1), qt_name + ".second", "__java_tmp_second", 0, -1, BoxedPrimitive);
            s << INDENT << java_name << " = qtjambi_pair_new(__jni_env, "
            << "__java_tmp_first, __java_tmp_second);" << endl;
        }

        s << INDENT << "}" << endl;

    } else if (type->type() == ContainerTypeEntry::MultiMapContainer) {

        Q_ASSERT(java_type->instantiations().size() == 2);
        AbstractMetaType *targ_key = java_type->instantiations().at(0);
        AbstractMetaType *targ_val = java_type->instantiations().at(1);

        s << endl
        << INDENT << "jobject " << java_name << " = qtjambi_treemap_new(__jni_env, " << qt_name << ".keys().size());" << endl
        << INDENT << "QList<";
        writeTypeInfo(s, targ_key);
        s << "> __qt_keys = " << qt_name << ".keys();" << endl
        << INDENT << "for (int i=0; i<__qt_keys.size(); ++i) {" << endl;
        {
            Indentation indent(INDENT);

            s << INDENT;
            writeTypeInfo(s, targ_key);
            s << " __qt_tmp_key = __qt_keys.at(i);" << endl;
            writeQtToJava(s, targ_key, "__qt_tmp_key", "__java_tmp_key", 0, -1, BoxedPrimitive);

            s << INDENT << "QList<";
            writeTypeInfo(s, targ_val);
            s << "> __qt_values = " << qt_name << ".values(__qt_tmp_key);" << endl
            << INDENT << "jobject __java_value_list = qtjambi_arraylist_new(__jni_env, __qt_values.size());" << endl
            << INDENT << "for (int j=0; j<__qt_values.size(); ++j) {" << endl;
            {
                Indentation indent(INDENT);

                s << INDENT;
                writeTypeInfo(s, targ_val);
                s << " __qt_tmp_val = __qt_values.at(j);" << endl;
                writeQtToJava(s, targ_val, "__qt_tmp_val", "__java_tmp_val", 0, -1, BoxedPrimitive);

                s << INDENT << "qtjambi_collection_add(__jni_env, __java_value_list, __java_tmp_val);" << endl;
            }
            s << INDENT << "}" << endl
            << INDENT << "qtjambi_map_put(__jni_env, " << java_name << ", __java_tmp_key, __java_value_list);" << endl;
        }
        s << INDENT << "}" << endl;

    } else if (type->type() == ContainerTypeEntry::MapContainer
               || type->type() == ContainerTypeEntry::HashContainer) {
        QString constructor = type->type() == ContainerTypeEntry::MapContainer
                              ? "qtjambi_treemap_new"
                              : "qtjambi_hashmap_new";

        Q_ASSERT(java_type->instantiations().size() == 2);
        AbstractMetaType *targ_key = java_type->instantiations().at(0);
        AbstractMetaType *targ_val = java_type->instantiations().at(1);

        s << endl
        << INDENT << "jobject " << java_name << " = " << constructor << "(__jni_env, " << qt_name
        << ".size());" << endl
        << INDENT;
        QString iteratorName = "__qt_" + qt_name + "_it";
        writeTypeInfo(s, java_type, Option(ExcludeReference | ExcludeConst));
        s << "::const_iterator " << iteratorName << ";" << endl
        << INDENT << "for (" << iteratorName << "=" << qt_name << ".constBegin(); "
        << iteratorName << "!=" << qt_name << ".constEnd(); ++" << iteratorName << ") {" << endl;
        {
            Indentation indent(INDENT);
            s << INDENT;
            writeTypeInfo(s, targ_key);
            s << " __qt_tmp_key = " << iteratorName << ".key();" << endl
            << INDENT;
            writeTypeInfo(s, targ_val);
            s << " __qt_tmp_val = " << iteratorName << ".value();" << endl;
            writeQtToJava(s, targ_key, "__qt_tmp_key", "__java_tmp_key", 0, -1, BoxedPrimitive);
            writeQtToJava(s, targ_val, "__qt_tmp_val", "__java_tmp_val", 0, -1, BoxedPrimitive);
            s << INDENT << "qtjambi_map_put(__jni_env, " << java_name
            << ", __java_tmp_key, __java_tmp_val);" << endl;
        }
        s << INDENT << "}" << endl;

    } else {
        ReportHandler::warning(QString("unable to generate container type %1, type=%2")
                               .arg(java_type->name()).arg(type->type()));
    }

    s << INDENT << "QTJAMBI_EXCEPTION_CHECK(__jni_env);" << endl;
*/
}


void PlainCppImplGenerator::writeJavaToQtContainer(QTextStream &s,
        const AbstractMetaType *java_type,
        const QString &qt_name,
        const QString &java_name,
        const AbstractMetaFunction *java_function,
        int argument_index) {
    // Conversion to C++: Shell code for return value, native code for arguments
/*
    TypeSystem::Language lang = argument_index == 0 ? TypeSystem::ShellCode : TypeSystem::NativeCode;
    if (java_function && writeConversionRule(s, lang, java_function, argument_index, qt_name, java_name))
        return;

    if (java_type == 0) {
        QString warn = QString("no conversion possible for argument '%1' in function '%2::%3' for "
                               "language '%4'")
                       .arg(argument_index)
                       .arg(java_function->implementingClass()->name())
                       .arg(java_function->name())
                       .arg(int(lang));
        ReportHandler::warning(warn);
        return;
    }


    Q_ASSERT(java_type->isContainer());
    const ContainerTypeEntry *type =
        static_cast<const ContainerTypeEntry *>(java_type->typeEntry());

    if (type->type() == ContainerTypeEntry::ListContainer
            || type->type() == ContainerTypeEntry::VectorContainer
            || type->type() == ContainerTypeEntry::StringListContainer
            || type->type() == ContainerTypeEntry::LinkedListContainer
            || type->type() == ContainerTypeEntry::StackContainer
            || type->type() == ContainerTypeEntry::SetContainer
            || type->type() == ContainerTypeEntry::QueueContainer) {
        Q_ASSERT(java_type->instantiations().size() == 1);
        AbstractMetaType *targ = java_type->instantiations().first();

        s << INDENT;
        writeTypeInfo(s, java_type, ForceValueType);
        s << qt_name << ";" << endl;

        s << INDENT << "if (" << java_name << " != 0) {" << endl;
        {
            Indentation indent(INDENT);
            s << INDENT << "jobjectArray __qt__array = qtjambi_collection_toArray(__jni_env, "
            << java_name << ");" << endl
            << INDENT << "jsize __qt__size = __jni_env->GetArrayLength(__qt__array);" << endl;

            if (type->type() == ContainerTypeEntry::VectorContainer
                    || type->type() == ContainerTypeEntry::StackContainer)
                s << INDENT << qt_name << ".reserve(__qt__size);" << endl;

            s << INDENT << "for (int i=0; i<__qt__size; ++i) {" << endl;
            {
                Indentation indent(INDENT);
                s << INDENT <<
                "jobject __java_element = " <<
                "__jni_env->GetObjectArrayElement(__qt__array, i);" <<
                endl;
                QString element_name = (qt_name == "__qt_element") ? "__qt_element2" : "__qt_element";
                writeJavaToQt(s, targ, element_name, "__java_element", 0, -1, BoxedPrimitive);
                s << INDENT << qt_name << " << " << element_name << ";" << endl;
            }
            s << INDENT << "}" << endl;
        }
        s << INDENT << "}" << endl;
    } else if (type->type() == ContainerTypeEntry::PairContainer) {
        QList<AbstractMetaType *> targs = java_type->instantiations();
        Q_ASSERT(targs.size() == 2);

        s << INDENT;
        writeTypeInfo(s, java_type, ForceValueType);
        s << " " << qt_name << ";" << endl
        << INDENT << "if (" << java_name << " != 0) {" << endl;
        {
            // separate scope required just in case function takes two QPair's.
            Indentation indent(INDENT);
            s << INDENT << "jobject __java_first = qtjambi_pair_get(__jni_env, "
            << java_name << ", 0);" << endl;
            writeJavaToQt(s, targs.at(0), "__qt_first", "__java_first", 0, -1, BoxedPrimitive);

            s << INDENT << "jobject __java_second = qtjambi_pair_get(__jni_env, "
            << java_name << ", 1);" << endl;
            writeJavaToQt(s, targs.at(1), "__qt_second", "__java_second", 0, -1, BoxedPrimitive);

            s << INDENT << qt_name << ".first = __qt_first;" << endl
            << INDENT << qt_name << ".second = __qt_second;" << endl;
        }
        s << INDENT << "}" << endl;
    } else if (type->type() == ContainerTypeEntry::MapContainer
               || type->type() == ContainerTypeEntry::HashContainer) {
        Q_ASSERT(java_type->instantiations().size() == 2);
        AbstractMetaType *targ_key = java_type->instantiations().at(0);
        AbstractMetaType *targ_val = java_type->instantiations().at(1);

        s << INDENT;
        writeTypeInfo(s, java_type, ForceValueType);
        s << qt_name << ";" << endl;
        s << INDENT << "if (" << java_name << " != 0) {" << endl;
        {
            Indentation indent(INDENT);
            s << INDENT << "int __qt_list_size = qtjambi_map_size(__jni_env, " << java_name
            << ");" << endl
            << INDENT
            << "jobjectArray __java_entry_set = qtjambi_map_entryset_array(__jni_env, " << java_name
            << ");" << endl;

            s << INDENT << "for (int i=0; i<__qt_list_size; ++i) {" << endl;
            {
                Indentation indent(INDENT);
                s << INDENT
                << "QPair<jobject, jobject> __java_entry = "
                << "qtjambi_entryset_array_get(__jni_env, __java_entry_set, i);"
                << endl
                << INDENT << "jobject __java_key = __java_entry.first;" << endl
                << INDENT << "jobject __java_val = __java_entry.second;" << endl;
                writeJavaToQt(s, targ_key, "__qt_key", "__java_key", 0, -1, BoxedPrimitive);
                writeJavaToQt(s, targ_val, "__qt_val", "__java_val", 0, -1, BoxedPrimitive);
                s << INDENT << qt_name << ".insert(__qt_key, __qt_val);" << endl;
            }
            s << INDENT << "}" << endl;
        }
        s << INDENT << "}" << endl;

    } else {
        ReportHandler::warning(QString("unable to generate container type %1, %2")
                               .arg(java_type->name()).arg(type->type()));
    }

    s << INDENT << "QTJAMBI_EXCEPTION_CHECK(__jni_env);" << endl;
*/
}


void PlainCppImplGenerator::writeFunctionCall(QTextStream &s, const QString &object_name,
        const AbstractMetaFunction *java_function,
        const QString &prefix,
        Option option,
        const QStringList &extra_arguments) {
    QString function_name = option & OriginalName ? java_function->originalName() : java_function->name();

    AbstractMetaClassList interfaces = java_function->implementingClass()->interfaces();

    QString classPrefix;
    if (prefix.isEmpty()
            && !java_function->implementingClass()->interfaces().isEmpty()
            && !java_function->implementingClass()->inheritsFrom(java_function->declaringClass())) {
        classPrefix = java_function->declaringClass()->qualifiedCppName() + "::";
    }

    if (java_function->isInGlobalScope()) {

        // Global scope stream operators need the arguments to be reordered (this ref at end)
        // so we special case them in order to simplify this code
        bool stream_operator = java_function->originalName() == "operator<<"
        || java_function->originalName() == "operator>>";

        if (java_function->type() == 0)
            s << "if (" << object_name << " != 0) ";
        else
            s << "(" << object_name << " != 0) ? ";
        s << "::" << prefix << function_name << "(";
        if (!stream_operator)
            s << "*" << object_name << ", ";
        writeFunctionCallArguments(s, java_function, "__qt_");
        if (stream_operator)
            s << ", *" << object_name;
        s << ")";
        if (java_function->type() != 0)
            s << " : " << default_return_statement_qt(java_function->type(), Generator::Option(option | Generator::NoReturnStatement));
        s << ";";
    } else {
        s << object_name << (java_function->isStatic() ? QLatin1String("::") : QLatin1String("->") + classPrefix)
        << prefix << function_name << "(";
        writeFunctionCallArguments(s, java_function, "__qt_");

        // The extra arguments...
        for (int i = 0; i < extra_arguments.size(); ++i) {
            if (i > 0 || java_function->arguments().size() != 0)
                s << ", ";
            s << extra_arguments.at(i);
        }

        s << ");";
    }

    s << endl;

}


void PlainCppImplGenerator::writeFunctionCallArguments(QTextStream &s,
        const AbstractMetaFunction *java_function,
        const QString &prefix,
        Option options) {
    AbstractMetaArgumentList arguments = java_function->arguments();

    int written_arguments = 0;
    for (int i = 0; i < arguments.size(); ++i) {
        const AbstractMetaArgument *argument = arguments.at(i);

        if ((options & SkipRemovedArguments) == SkipRemovedArguments
                && java_function->argumentRemoved(i + 1)) {
            continue;
        }

        if (written_arguments++ > 0) {
            s << ", ";
        }

        bool enum_as_int = (options & EnumAsInts) && (argument->type()->typeEntry()->isEnum()
                           || argument->type()->typeEntry()->isFlags());
        if (argument->type()->isEnum()) {
            AbstractMetaEnum *java_enum =
                m_classes.findEnum(static_cast<const EnumTypeEntry *>(argument->type()->typeEntry()));
            if (java_enum == 0) {
                ReportHandler::warning(QString("enum not found: '%1'")
                                       .arg(argument->type()->typeEntry()->qualifiedCppName()));
            } else {
                enum_as_int |= !java_enum->isPublic();
            }
        }

        if ((!(options & NoCasts) && !enum_as_int) || ((options & ForceEnumCast) && argument->type()->isEnum())) {

            // If the type in the signature is specified without template instantiation, but the
            // class is actually a template class, then we have troubles.
            AbstractMetaClass *cls = classes().findClass(argument->type()->typeEntry()->qualifiedCppName());
            if (cls == 0 || cls->templateArguments().size() == argument->type()->instantiations().size()) {
                s << "(";
                writeTypeInfo(s, argument->type());
                s << ")";
            }
        }

        if (!argument->type()->isPrimitive()
                || !java_function->conversionRule(TypeSystem::NativeCode, argument->argumentIndex() + 1).isEmpty()) {
            s << prefix;
        }
        s << argument->indexedName();
    }
}


QString PlainCppImplGenerator::translateType(const AbstractMetaType *java_type, Option option) {
    if (!java_type)
        return "void";

    if (java_type->isPrimitive()
            || java_type->isTargetLangString()
            || java_type->isTargetLangStringRef()
            || java_type->isVariant()
            || java_type->isJObjectWrapper()
            || java_type->isTargetLangChar()
            || java_type->isArray()) {
        return java_type->typeEntry()->jniName();
    } else if (java_type->isIntegerEnum() || java_type->isIntegerFlags()
               || ((option & EnumAsInts) && (java_type->isEnum() || java_type->isFlags()))) {
        return "int";
    } else {
        return "void *";
    }
}

void PlainCppImplGenerator::writeExtraIncludes(QTextStream &s, const AbstractMetaClass *java_class) {
    IncludeList includes = java_class->typeEntry()->extraIncludes();
    qSort(includes.begin(), includes.end());

    int used = 0;
    foreach(const Include &i, includes) {
        if (i.type != Include::TargetLangImport) {
            s << i.toString() << endl;
            ++used;
        }
    }


    if (used)
        s << endl;

}


void PlainCppImplGenerator::writeDefaultConstructedValues_helper(QSet<QString> &values,
        const AbstractMetaFunction *func) {
    foreach(AbstractMetaArgument *arg, func->arguments()) {
        AbstractMetaType *type = arg->type();
        if (func->typeReplaced(arg->argumentIndex() + 1).isEmpty() && type->isValue() && hasDefaultConstructor(type))
            values << type->typeEntry()->qualifiedCppName();
    }
}


void PlainCppImplGenerator::writeDefaultConstructedValues(QTextStream &s, const AbstractMetaClass *java_class) {

    QSet<QString> values;

    // Class functions, more or less copied from the logic in write(Class) above...
    AbstractMetaFunctionList class_funcs;

    // Add normal final functions
    foreach(AbstractMetaFunction *function, java_class->functionsInTargetLang()) {
        if (!function->isEmptyFunction())
            class_funcs << function;
    }

    // Add abstract functions, I think...
    foreach(AbstractMetaFunction *function, java_class->queryFunctions(AbstractMetaClass::NormalFunctions
            | AbstractMetaClass::AbstractFunctions
            | AbstractMetaClass::NotRemovedFromTargetLang)) {
        if (function->implementingClass() != java_class)
            class_funcs << function;
    }

    // Signals (their c++ wrapper calls actually...)
    class_funcs += java_class->queryFunctions(AbstractMetaClass::Signals);

    //
    foreach(AbstractMetaFunction *f, class_funcs) {
        writeDefaultConstructedValues_helper(values, f);
    }

    foreach(AbstractMetaField *field, java_class->fields()) {
        writeDefaultConstructedValues_helper(values, field->setter());
    }

/*
    if (!values.isEmpty()) {
        // FIXME This is sometimes emitted when it does not need to be and results in
        //  compiler warning (gcc linux - at least) about unused static data.
        // I suspect the test above is not good enough.
        s << endl << endl
        << "// Default constructed values used throughout final functions..." << endl;
        for (QSet<QString>::const_iterator it = values.constBegin(); it != values.constEnd(); ++it) {
            s << "Q_GLOBAL_STATIC(" << *it << ", default_" << QString(*it).replace("::", "_")
            << ");" << endl;
        }
        s << endl << endl;
    }
*/
}
