/* This file is part of RTags (http://rtags.net).

   RTags is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   RTags is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with RTags.  If not, see <http://www.gnu.org/licenses/>. */

#include "AST.h"
#include "ClangThread.h"
#include "selene.h"

struct UserData {
    List<AST::Cursor> parents;
    AST *ast;
};
CXChildVisitResult AST::visitor(CXCursor cursor, CXCursor, CXClientData u)
{
    UserData *userData = reinterpret_cast<UserData*>(u);
    assert(userData);
    Cursor::Data *p = userData->parents.isEmpty() ? 0 : userData->parents.back().data.get();
    Cursor c = userData->ast->construct(cursor, p);
    userData->parents.push_back(Cursor { c.data } );
    clang_visitChildren(cursor, visitor, u);
    if (userData->parents.size() > 1)
        userData->parents.pop_back();
    return CXChildVisit_Continue;
}

template <typename T> static void assign(sel::Selector selector, const T &t) { selector = t; }
void assign(sel::Selector selector, const String &str) { selector = str.ref(); }

template <typename T>
static void exposeArray(sel::Selector selector, const std::vector<T> &array)
{
    int i = 0;
    for (const T &t : array) {
        assign(selector[i++], t);
    }
}

static void registerClasses(sel::State &state)
{
    state["SourceLocation"].SetClass<AST::SourceLocation>("line", &AST::SourceLocation::line,
                                                          "column", &AST::SourceLocation::column,
                                                          "file", &AST::SourceLocation::file,
                                                          "offset", &AST::SourceLocation::offset,
                                                          "toString", &AST::SourceLocation::toString);
    state["SourceRange"].SetClass<AST::SourceRange>("start", &AST::SourceRange::start,
                                                    "end", &AST::SourceRange::end,
                                                    "length", &AST::SourceRange::length,
                                                    "toString", &AST::SourceRange::toString);
    state["Cursor"].SetClass<AST::Cursor>("location", &AST::Cursor::location,
                                          "usr", &AST::Cursor::usr,
                                          "kind", &AST::Cursor::kind,
                                          "linkage", &AST::Cursor::linkage,
                                          "availability", &AST::Cursor::availability,
                                          "language", &AST::Cursor::language,
                                          "spelling", &AST::Cursor::spelling,
                                          "displayName", &AST::Cursor::displayName,
                                          "rawComment", &AST::Cursor::rawComment,
                                          "briefComment", &AST::Cursor::briefComment,
                                          "mangledName", &AST::Cursor::mangledName,
                                          "templateKind", &AST::Cursor::templateKind,
                                          "range", &AST::Cursor::range,
                                          "children", &AST::Cursor::children,
                                          "overriddenCount", &AST::Cursor::overriddenCount,
                                          "overriddenCursors", &AST::Cursor::overriddenCursors,
                                          "argumentCount", &AST::Cursor::argumentCount,
                                          "arguments", &AST::Cursor::arguments,
                                          "fieldBitWidth", &AST::Cursor::fieldBitWidth,
                                          "typedefUnderlyingType", &AST::Cursor::typedefUnderlyingType,
                                          "enumIntegerType", &AST::Cursor::enumIntegerType,
                                          "enumConstantValue", &AST::Cursor::enumConstantValue,
                                          "includedFile", &AST::Cursor::includedFile,
                                          "templateArgumentCount", &AST::Cursor::templateArgumentCount,
                                          "templateArgumentType", &AST::Cursor::templateArgumentType,
                                          "templateArgumentValue", &AST::Cursor::templateArgumentValue,
                                          "templateArgumentKind", &AST::Cursor::templateArgumentKind,
                                          "referenced", &AST::Cursor::referenced,
                                          "canonical", &AST::Cursor::canonical,
                                          "lexicalParent", &AST::Cursor::lexicalParent,
                                          "semanticParent", &AST::Cursor::semanticParent,
                                          "definitionCursor", &AST::Cursor::definitionCursor,
                                          "specializedCursorTemplate", &AST::Cursor::specializedCursorTemplate,
                                          "childCount", &AST::Cursor::childCount,
                                          "child", &AST::Cursor::child,
                                          "isBitField", &AST::Cursor::isBitField,
                                          "isVirtualBase", &AST::Cursor::isVirtualBase,
                                          "isStatic", &AST::Cursor::isStatic,
                                          "isVirtual", &AST::Cursor::isVirtual,
                                          "isPureVirtual", &AST::Cursor::isPureVirtual,
                                          "isConst", &AST::Cursor::isConst,
                                          "isDefinition", &AST::Cursor::isDefinition,
                                          "isDynamicCall", &AST::Cursor::isDynamicCall);

    std::function<AST::Cursors(const AST::Cursor &, const std::string &, int)> recurse;
    recurse = [&recurse](const AST::Cursor &cursor, const std::string &kind, int depth = -1) -> AST::Cursors {
        AST::Cursors ret;
        if (cursor.kind() == kind)
            ret.append(cursor);
        if (const int childCount = cursor.childCount()) {
            if (depth > 0 || depth == -1) {
                const int childDepth = depth == -1 ? -1 : depth - 1;
                for (int i=0; i<childCount; ++i) {
                    const AST::Cursor &child = cursor.child(i);
                    ret.append(recurse(child, kind, childDepth));
                }
            }
        }

        return ret;
    };
    state["query"] = recurse;

}

std::shared_ptr<AST> AST::create(const Source &source, const String &sourceCode, CXTranslationUnit unit)
{
    std::shared_ptr<AST> ast(new AST);
    ast->mState.reset(new sel::State {true});
    sel::State &state = *ast->mState;
    registerClasses(state);
    ast->mSourceCode = sourceCode;
    state["sourceFile"] = source.sourceFile().ref();
    state["sourceCode"] = sourceCode.ref();
    state["write"] = [ast](const std::string &str) {
        ast->mReturnValues.append(str);
    };

    exposeArray(state["commandLine"], source.toCommandLine(Source::Default|Source::IncludeCompiler|Source::IncludeSourceFile));


    if (unit) {
        UserData userData;
        userData.ast = ast.get();
        visitor(clang_getTranslationUnitCursor(unit), clang_getNullCursor(), &userData);

        const Cursor root = userData.parents.front();
        state["root"] = [root]() { return root; };
        state["findByUsr"] = [ast](const std::string &usr) {

        };
    }
    return ast;
}

List<AST::Diagnostic> AST::diagnostics() const
{
    return List<Diagnostic>();
}

List<AST::SkippedRange> AST::skippedRanges() const
{
    return List<SkippedRange>();
}

List<String> AST::evaluate(const String &script)
{
    assert(mReturnValues.isEmpty());
    mState->operator()(script.constData());
    return std::move(mReturnValues);
}