/*
    c2ffi
    Copyright (C) 2013  Ryan Pavlik

    This file is part of c2ffi.

    c2ffi is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    c2ffi is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with c2ffi.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <iostream>

#include <clang/AST/PrettyPrinter.h>
#include "c2ffi.h"
#include "c2ffi/ast.h"

using namespace c2ffi;

Type::Type(const clang::CompilerInstance &ci, const clang::Type *t)
    : _ci(ci), _type(t) { }

std::string Type::metatype() const {
    return std::string("<") + _type->getTypeClassName() + ">";
}

static std::string make_builtin_name(const clang::BuiltinType *bt) {
    clang::PrintingPolicy pp = clang::PrintingPolicy(clang::LangOptions());
    std::string name = std::string(":") + bt->getNameAsCString(pp);

    for(int i = 0; i < name.size(); i++)
        if(name[i] == ' ')
            name[i] = '-';

    return name;
}

Type* Type::make_type(C2FFIASTConsumer *ast, const clang::Type *t) {
    const clang::CompilerInstance &ci = ast->ci();

    if(t->isVoidType())
        return new SimpleType(ci, t, ":void");

    if_const_cast(td, clang::TypedefType, t) {
        const clang::TypedefNameDecl *tdd = td->getDecl();
        return new SimpleType(ci, td, tdd->getDeclName().getAsString());
    }

    if(t->isBuiltinType()) {
        const clang::BuiltinType *bt = llvm::dyn_cast<clang::BuiltinType>(t);
        if(!bt) return new SimpleType(ci, t, "<unknown-type>");

        return new SimpleType(ci, t, make_builtin_name(bt));
    }

    if_const_cast(e, clang::ElaboratedType, t)
        return make_type(ast, e->getNamedType().getTypePtr());

    if(t->isFunctionPointerType())
        return new SimpleType(ci, t, ":function-pointer");

    if(t->isFunctionType())
        return new SimpleType(ci, t, ":function");

    if(t->isPointerType())
        return new PointerType(ci, t, make_type(ast, t->getPointeeType().getTypePtr()));

    if(t->isRecordType()) {
        const clang::RecordDecl *rd = NULL;

        if(t->isStructureType())
            rd = t->getAsStructureType()->getDecl();
        else if(t->isUnionType())
            rd = t->getAsUnionType()->getDecl();
        else
            goto error;

        std::string name = rd->getDeclName().getAsString();

        if(name == "")
            return new DeclType(ci, t, ast->make_decl(rd, false));
        else
            return new RecordType(ci, t, name, rd->isUnion());
    }


    if_const_cast(ca, clang::ConstantArrayType, t)
        return new ArrayType(ci, ca,
                             make_type(ast, ca->getElementType().getTypePtr()),
                             ca->getSize().getLimitedValue());

    if_const_cast(ca, clang::IncompleteArrayType, t)
        return new PointerType(ci, ca,
                               make_type(ast, ca->getElementType().getTypePtr()));

 error:
    return new SimpleType(ci, t, "<unknown-type>");
}

void DeclType::write(OutputDriver &od) const {
    if(_d)
        _d->write(od);
}
