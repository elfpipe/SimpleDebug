#include "Binary.hpp"
#include "symtabs.h"
#include <vector>
Type::~Type(){}
Array::Array(SourceObject *object, TypeNo no, astream &str)
: Type(T_Array, no)
{
    str.peekSkip('a');
    if(str.peek() == 'r') {
        str.skip();
        TypeNo rNo(str);
        range = dynamic_cast<Range *>(object->findType(rNo));
        if(!range) {
            range = new Range(rNo, str);
            object->addType(range);
        }
    } else range = 0;
    str.peekSkip(';');
    lower = str.getInt();
    str.peekSkip(';');
    upper = str.getInt();
    str.peekSkip(';');
    if(str.peek() == '(') {
        TypeNo tNo(str);
        type = object->findType(tNo);
        if(!type)
            type = object->interpretType(tNo, str);
    } else type = 0;
}
Struct::Struct(SourceObject *object, Type::TypeNo no, astream &str)
: Type(T_Struct, no)
{
    str.peekSkip('s');
    str.peekSkip('u');
    size = str.getInt();
    while(!str.eof()) {
        string name = str.get(':');
        Type::TypeNo no(str);
        Type *type = object->findType(no);
        if(!type)
            type = object->interpretType(no, str);
        str.peekSkip(',');
        uint64_t bitOffset = str.getInt();
        str.peekSkip(',');
        uint64_t bitSize = str.getInt();
        str.peekSkip(';');
        addEntry(name, type, bitOffset, bitSize);
        if(str.peek() == ';') {
            str.skip();
            break;
        }
    }
}
Pointer::Pointer(SourceObject *object, Type::TypeNo no, astream &str)
: Type(T_Pointer, no),
pointsTo(0)
{
    str.skip();
    Type::TypeNo pNo(str);
    pointsTo = object->findType(pNo);
    if(!pointsTo)
        pointsTo = object->interpretType(no, str);
}
Type *SourceObject::interpretType(Type::TypeNo no, astream &str) {
    Type *type = 0;
    str.peekSkip('='); //skip the '='
    switch(str.peek()) {
        case 'R': // range
        case 'r':
            type = new Range(no, str);
            break;
        case 'a': //array
            type = new Array(this, no, str);
            break;
        case 's':  //struct or
        case 'u':  //union
            type = new Struct(this, no, str);
            break;
        case 'e': //enum
            type = new Enum(no, str);
            break;
        case '*': //pointer
            type = new Pointer(this, no, str);                
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
                Type *iType = findType(iNo);
                if(!iType)
                    iType = interpretType(iNo, str);
                type = new Ref(no, iType);
            }
            break;
        }
        default:
            break;
    }
    if(type)
        addType(type);
    return type;
}
Symbol *SourceObject::interpretSymbol(astream &str, uint64_t address) {
    Symbol *result = 0;
    string name = str.get(':');
    char c = str.peek();
    if(c != '(') str.skip();
    Type::TypeNo no(str);
    Type *type = findType(no);
    if(!type)
        type = interpretType(no, str);    
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
Function *SourceObject::interpretFun(astream &str, uint64_t address) {
    Function *result = 0;
    string name = str.get(':');
    char c = str.get();
    switch(c) {
        case 'F':
        case 'f': {
            Type::TypeNo no(str);
            Type *type = findType(no);
            if(!type) {
                type = new FunctionType(no);
                if(type) addType(type);
            }
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
    addType(new Range(Type::TypeNo(0, 0), temp));

    SymtabEntry *sym = *_sym;
    name = string(stabstr + sym->n_strx);
    start = sym->n_value;
    sym++;

    string source = name;
    Function *function = 0;
    Scope *scope = 0;
	while ((uint32_t)sym < (uint32_t)stab + stabsize) {
        astream str(string(stabstr + sym->n_strx));
		switch (sym->n_type) {
            case N_SO:
                end = sym->n_value;
                *_sym = ++sym;
                return;
            case N_SOL:
                source = string(stabstr + sym->n_strx);
                break;
			case N_LSYM: {
                Symbol *symbol = interpretSymbol(str, sym->n_value);
                if(function)
                    scope->symbols.push_back(symbol);
                else
                    locals.push_back(symbol);
                break;
            }
			case N_GSYM: {
                Symbol *symbol = interpretSymbol(str, sym->n_value);
                if(symbol)
                    globals.push_back(symbol);
                break;
            }
            case N_FUN: {
                function = interpretFun(str, sym->n_value);
                if(function) {
                    function->locals.push_back(scope = new Scope(0, function->address)); //there has to be a scope
                    functions.push_back(function);
                }
                break;
            }
			case N_PSYM: {
                Symbol *symbol = interpretSymbol(str, sym->n_value);
                function->params.push_back(symbol);
                break;
            }
            case N_SLINE:
                function->addLine(sym->n_value, sym->n_desc, source);
                break;
            case N_LBRAC: {
                if(scope)
                    scope->children.push_back(scope = new Scope(scope, function->address + sym->n_value));
                break;
            }
            case N_RBRAC:
                if(scope) {
                    scope->end = function->address + sym->n_value;
                    scope = scope->parent;
                }
                if(scope && scope->parent == 0) //hackaround
                    scope->end = scope->children.size() ? scope->children[0]->end : function->lines.size() ? function->lines[function->lines.size()-1]->address : function->address;
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