#include "Definitions02.hpp"
#include "symtabs.h"
#include <vector>
vector<Sym *> syms;
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
Sym *InterpretLSym(astream &str, uint64_t address) {
    Sym *result = 0;
    string name = str.get(':');
    char c = str.peek();
    switch(c) {
        case 't':
        case 'T': {
            str.skip();
            Type::TypeNo no(str);
            Type *type = Type::findType(no);
            if(!type)
                type = InterpretType(no, str);
            result = new TSym(name, type, address);
            break;
        }

        case '(': {
            Type::TypeNo no(str);
            Type *type = Type::findType(no);
            if(!type)
                type = InterpretType(no, str);
            result = new LSym(name, type, address);
            break;
        }
        
        default: //??
            cout << "error " << c << "\n";
            break;
    }
    return result;
}/*
GSym *InterpretGSym(astream &str) {
    GSym *result = 0;
    string name = str.get(':');
    char c = str.peek();
    switch(c) {
        case 'G': {
            str.skip();
            Type::TypeNo no(str);
            Type *type = Type::findType(no);
            if(!type) 
                type = InterpretType(no, str);
            result = new GSym(name, type);
            break;
        }
        default:
            cout << "error";
            break;
    }
    return result;
}
PSym *InterpretPSym(astream &str, uint64_t address) {
    PSym *result = 0;
    string name = str.get(':');
    char c = str.peek();
    switch(c) {
        case 'p': {
            str.skip();
            Type::TypeNo no(str);
            Type *type = Type::findType(no);
            if(!type) 
                type = InterpretType(no, str);
            result = new PSym(name, type, address);
            break;
        }
        default:
            cout << "error";
            break;
    }
    return result;
}*/
void readSyms(SymtabEntry *stab, const char *stabstr, unsigned int stabsize) {
    astream temp("r(0,0);0;-1;");
    Type::types.push_back((Type *)new Range(Type::TypeNo(0, 0), temp));

	SymtabEntry *sym = stab;
	while ((uint32_t)sym < (uint32_t)stab + stabsize) {
		switch (sym->n_type) {
			case N_LSYM: {
                astream str(string(stabstr + sym->n_strx));
                syms.push_back(InterpretLSym(str, sym->n_value));
            }/*
			case N_GSYM: {
                astream str(string(stabstr + sym->n_strx));
                syms.push_back(InterpretGSym(str));
            }
			case N_PSYM: {
                astream str(string(stabstr + sym->n_strx));
                syms.push_back(InterpretPSym(str, sym->n_value));
            }*/
            default:
                break;
        }
        sym++;
    }
}
void printSyms() {
    cout << "--Syms:--\n";
    for(vector<Sym *>::iterator it = syms.begin(); it != syms.end(); it++) {
        cout << (*it)->toString() << "\n";
    }
}