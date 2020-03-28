#include "Definitions02.hpp"
#include "symtabs.h"
#include <vector>
//vector<Sym *> syms;
vector<Type *> Type::types;
Type::~Type(){}
Type *InterpretType(Type::TypeNo no, astream &str) {
    Type *type = 0;
    str.peekSkip('='); //skip the '='
    switch(str.peek()) {
        case 'R': // range
        case 'r':
            type = new Range(no, str);
            break;
        case 'a': //array
            type = new Array(no, str);
            break;
        case 's':  //struct or
        case 'u':  //union
            type = new Struct(no, str);
            break;
        case 'e': //enum
            type = new Enum(no, str);
            break;
        case '*': //pointer
            type = new Pointer(no, str);                
            break;
        case 'x': //conformant array
            type = new ConformantArray(no, str);
            break;
        case 'f':
            type = new FunctionType(no);
            break;
        case '(': {
            Type::TypeNo iNo(str);
            if(iNo.equals(no)) {
                type = new Void(no);
            } else {
                Type *iType = InterpretType(iNo, str);
                type = new Ref(iNo, iType);
            }
            break;
        }
        default:
            break;
    }
    if(type)
        Type::addType(type);
    return type;
}
Symbol *InterpretSymbol(astream &str, uint64_t address) {
    Symbol *result = 0;
    string name = str.get(':');
    char c = str.peek();
    if(c != '(') str.skip();
    Type::TypeNo no(str);
    Type *type = Type::findType(no);
    if(!type)
        type = InterpretType(no, str);    
    switch(c) {
        case 't':
        case 'T':
            result = new Symbol(Symbol::S_Typedef, name, type, address);
            break;
        case '(':
            result = new Symbol(Symbol::S_Local, name, type, address);
            break;
        case 'G':
            result = new Symbol(Symbol::S_Global, name, type, address);
            break;
        case 'p':
            result = new Symbol(Symbol::S_Param, name, type, address);
            break;
        default:
            cout << "error " << c << "\n";
            break;
    }
    return result;
}
Function *InterpretFun(astream &str, uint64_t address) {
    Function *result = 0;
    string name = str.get(':');
    char c = str.get();
    switch(c) {
        case 'F':
        case 'f': {
            Type::TypeNo no(str);
            Type *type = Type::findType(no);
            if(!type)
                type = new FunctionType(no);
            result = new Function(name, type, address);
            break;
        }
        default:
            break;
    }
    return result;
}
SourceObject::SourceObject(SymtabEntry **_sym, SymtabEntry *stab, const char *stabstr, uint64_t stabsize) {
    astream temp("r(0,0);0;-1;");
    types.push_back((Type *)new Range(Type::TypeNo(0, 0), temp));

    SymtabEntry *sym = *_sym;
    name = string(stabstr + sym->n_strx);
    start = sym->n_value;
    sym++;

    Function *function = 0;
    Scope *scope = 0;
	while ((uint32_t)sym < (uint32_t)stab + stabsize) {
        astream str(string(stabstr + sym->n_strx));
		switch (sym->n_type) {
            case N_SO:
                end = sym->n_value;
                sym++;
                break;
			case N_LSYM: {
                Symbol *symbol = InterpretSymbol(str, sym->n_value);
                if(function)
                    scope->symbols.push_back(symbol);
                else
                    locals.push_back(symbol);
                break;
            }
			case N_GSYM: {
                Symbol *symbol = InterpretSymbol(str, sym->n_value);
                if(symbol)
                    globals.push_back(symbol);
                break;
            }
            case N_FUN: {
                function = InterpretFun(str, sym->n_value);
                if(function) {
                    function->locals.push_back(new Scope(0, function->address));
                    functions.push_back(function);
                }
                break;
            }
			case N_PSYM: {
                Symbol *symbol = InterpretSymbol(str, sym->n_value);
                function->params.push_back(symbol);
                break;
            }
            case N_SLINE:
                function->addLine(sym->n_value, sym->n_desc);
                break;
            case N_LBRAC:
                scope->children.push_back(scope = new Scope(scope, function->address + sym->n_value));
                break;
            case N_RBRAC:
                scope->end = function->address + sym->n_value;
                scope = scope->parent;
                break;
            default:
                break;
        }
        sym++;
    }
    *_sym = sym;
}
string SourceObject::toString() {
    string result = name + "<SO> : [ " + patch::toString((void *)start) + "," + patch::toString((void *)end) + " ] --- {\n";
    for(vector<Type *>::iterator it = types.begin(); it != types.end(); it++)
        result += (*it)->toString() + "\n";
    for(vector<Symbol *>::iterator it = locals.begin(); it != locals.end(); it++)
        result += (*it)->toString() + "\n";
    for(vector<Symbol *>::iterator it = globals.begin(); it != globals.end(); it++)
        result += (*it)->toString() + "\n";
    for(vector<Function *>::iterator it = functions.begin(); it != functions.end(); it++)
        result += (*it)->toString() + "\n";
    return result + "}\n";
}
Binary::Binary(string name, SymtabEntry *stab, const char *stabstr, uint64_t stabsize) {
    this->name = name;
    this->stab = stab;
    this->stabstr = stabstr;
    this->stabsize = stabsize;
	SymtabEntry *sym = stab;
	while ((uint32_t)sym < (uint32_t)stab + stabsize) {
		switch (sym->n_type) {
            case N_SO:
                objects.push_back(new SourceObject(&sym, stab, stabstr, stabsize));
                break;
            default:
                break;
        }
        sym++;
    }
}
string Binary::toString() {
    string result = "<Binary> : [ STAB: 0x" + patch::toString((void*)stab) + " STABSTR: 0x" + patch::toString((void *)stabstr) + " STABSIZE: " + patch::toString((int)stabsize) + "] -- {\n";
    for(vector<SourceObject *>::iterator it = objects.begin(); it != objects.end(); it++)
        result += (*it)->toString();
    return result + "}\n";
}