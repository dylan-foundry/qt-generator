/****************************************************************************
**
** Copyright (C) 1992-$THISYEAR$ $TROLLTECH$. All rights reserved.
**
** This file is part of $PRODUCT$.
**
** $CPP_LICENSE$
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
****************************************************************************/

#ifndef JAVAGENERATOR_H
#define JAVAGENERATOR_H

#include "generator.h"
#include "metajava.h"

#include <QTextStream>

class DocParser;

class JavaGenerator : public Generator
{
    Q_OBJECT

public:
    JavaGenerator();

    QString translateType(const AbstractMetaType *java_type, Option option = NoOption);

    void writeInjectedCode(QTextStream &s,
                           const AbstractMetaFunction *java_function,
                           CodeSnip::Position position);
    void writeArgument(QTextStream &s,
                       const AbstractMetaFunction *java_function,
                       const AbstractMetaArgument *java_argument,
                       uint options = 0);
    void writeEnum(QTextStream &s, const AbstractMetaEnum *java_enum);
    void writeIntegerEnum(QTextStream &s, const AbstractMetaEnum *java_enum);
    void writeSignal(QTextStream &s, const AbstractMetaFunction *java_function);
    void writeFunction(QTextStream &s, const AbstractMetaFunction *java_function,
                       uint included_attributes = 0, uint excluded_attributes = 0);
    void writeFieldAccessors(QTextStream &s, const AbstractMetaField *field);
    void write(QTextStream &s, const AbstractMetaClass *java_class);

    void writeFunctionOverloads(QTextStream &s, const AbstractMetaFunction *java_function,
                                uint included_attributes, uint excluded_attributes);
    void writeEnumOverload(QTextStream &s, const AbstractMetaFunction *java_function,
                           uint include_attributes, uint exclude_attributes);
    void writeExtraFunctions(QTextStream &s, const AbstractMetaClass *java_class);
    void writeToStringFunction(QTextStream &s);
    void writeFunctionAttributes(QTextStream &s, const AbstractMetaFunction *java_function,
                                 uint included_attributes = 0, uint excluded_attributes = 0,
                                 uint options = 0);
    void writeConstructorContents(QTextStream &s, const AbstractMetaFunction *java_function);
    void writeFunctionArguments(QTextStream &s, const AbstractMetaFunction *java_function,
        int count = -1, uint options = 0);
    void writeJavaCallThroughContents(QTextStream &s, const AbstractMetaFunction *java_function);
    void writeOwnershipForContainer(QTextStream &s, TypeSystem::Ownership ownership, AbstractMetaArgument *arg,
                                    const QString &indent);
    void writeOwnershipForContainer(QTextStream &s, TypeSystem::Ownership ownership, AbstractMetaType *type,
                                    const QString &arg_name, const QString &indent);
    void writePrivateNativeFunction(QTextStream &s, const AbstractMetaFunction *java_function);
    void writeJavaLangObjectOverrideFunctions(QTextStream &s, const AbstractMetaClass *cls);
    void writeReferenceCount(QTextStream &s, const ReferenceCount &refCount, const QString &argumentName);
    void retrieveModifications(const AbstractMetaFunction *f, const AbstractMetaClass *java_class,
         uint *exclude_attributes, uint *include_attributes) const;
    QString functionSignature(const AbstractMetaFunction *java_function,
                              uint included_attributes,
                              uint excluded_attributes,
                              Option option = NoOption,
                              int arg_count = -1);
    void setupForFunction(const AbstractMetaFunction *java_function,
       uint *included_attributes, uint *excluded_attributes) const;

    virtual QString subDirectoryForClass(const AbstractMetaClass *java_class) const
    { return subDirectoryForPackage(java_class->package()); }

    virtual QString fileNameForClass(const AbstractMetaClass *java_class) const;

    bool isComparable(const AbstractMetaClass *cls) const;

#if 0
    void write1_dot_5_enum(QTextStream &s, const AbstractMetaEnum *java_enum);
#endif

    bool shouldGenerate(const AbstractMetaClass *java_class) const {
        return !java_class->typeEntry()->isContainer() && !java_class->typeEntry()->isVariant()
               && (java_class->typeEntry()->codeGeneration() & TypeEntry::GenerateTargetLang);
    }

    QString documentationDirectory() const { return m_doc_directory; }
    void setDocumentationDirectory(const QString &docDir) { m_doc_directory = docDir; }

    bool documentationEnabled() const { return m_docs_enabled; }
    void setDocumentationEnabled(bool e) { m_docs_enabled = e; }
    void generate();

private:
    QString subDirectoryForPackage(const QString &package) const { return QString(package).replace(".", "/"); }

protected:
    QString m_package_name;
    QString m_doc_directory;
    DocParser *m_doc_parser;
    bool m_docs_enabled;
    QList<const AbstractMetaFunction *> m_nativepointer_functions;
    QList<const AbstractMetaFunction *> m_reference_count_candidate_functions;
};

#endif // JAVAGENERATOR_H
