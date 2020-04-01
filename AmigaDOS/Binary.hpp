#ifndef DEFINITIONS02_HPP
#define DEFINITIONS02_HPP

#include "Strings.hpp"
#include "symtabs.h"

#include <string>
#include <vector>
#include <limits.h>
using namespace std;
using namespace patch;
class SourceObject;
class Type {
public:
    class TypeNo {
    private:
        int t1, t2;
    public:
        bool equals(TypeNo &other) {
            return (t1 == other.t1 && t2 == other.t2);
        }
        TypeNo(int t1 = 0, int t2 = 0) {
            this->t1 = t1;
            this->t2 = t2;
        }
        TypeNo(astream &str)
        : TypeNo()
        {
            if(str.peek() == '(') {
                str.skip();
                t1 = str.getInt();
                if(str.peek() == ',')
                    str.skip();
                t2 = str.getInt();
                if(str.peek() == ')')
                    str.skip();
            } else t2 = str.getInt();
        }
        string toString() {
            return "(" + patch::toString(t1) + "," + patch::toString(t2) + ")";
        }
    };
public:
    typedef enum {
        T_Void,
        T_Range,
        T_Ref,
        T_Array,
        T_Struct,
        T_Union,
        T_Enum,
        T_Pointer,
        T_ConformantArray,
        T_Function
    } TypeClass;
    TypeClass typeClass;
    TypeNo no;
public:
    Type(TypeClass typeClass, TypeNo no) {
        this->typeClass = typeClass;
        this->no = no;
    }
    //Type(TypeNo no, astream &str);
    virtual ~Type();
    virtual string toString() = 0;
};
class Ref : public Type {
public:
    Type *ref;
public:
    Ref(TypeNo no, Type *ref)
    : Type(T_Ref, no)
    {
        this->ref = ref;
    }
    string toString() {
        return ref->toString();
    }
};
class Void : public Type {
public:
    Void(Type::TypeNo no)
    : Type(T_Void, no)
    {}
public:
    string toString() {
        return "<void>";
    }
};
class Range : public Type {
public:
    typedef enum {
        R_VeryLarge,
        R_Float32,
        R_Float64,
        R_Float128,
        R_Complex8,
        R_Complex16,
        R_Complex32,
        R_Defined
    } RangeType;
    RangeType rangeType;
    uint64_t lower, upper;
    Range(Type::TypeNo no, astream &str)
    : Type(T_Range, no)
    {
        str.peekSkip('=');
        char c = str.get();
        if(c == 'r') {
            TypeNo newNo(str);
            str.peekSkip(';');
            lower = str.getInt();
            str.peekSkip(';');
            upper = str.getInt();
            str.peekSkip(';');
            if(lower == 0 && upper == -1) {
                rangeType = R_VeryLarge;
                lower = 0;
                upper = UINT_MAX;
            } else if (upper == 0) {
                if(lower == 4) {
                    rangeType = R_Float32;
                } else if (lower == 8) {
                    rangeType = R_Float64;
                } else if (lower == 16) {
                    rangeType = R_Float128;
                }
            } else {
                rangeType = R_Defined;
            }
        } else if(c == 'R') {
            uint32_t kind = str.getInt();
            str.peekSkip(';');
            uint32_t bytes = str.getInt();
            str.peekSkip(';');
            str.getInt(); //swallow
            str.peekSkip(';');
            switch(kind) {
                case 1: //NF_SINGLE
                    rangeType = R_Float32;
                    break;
                case 2: //NF_DOUBLE
                    rangeType = R_Float64;
                    break;
                case 3: //NF_COMPLEX
                    rangeType = R_Complex8;
                    break;
                case 4: //NF_COMPLEX16
                    rangeType = R_Complex16;
                    break;
                case 5: //NF_COMPLEX32
                    rangeType = R_Complex32;
                    break;
                case 6: //NF_LDOUBLE
                    rangeType = R_Float128;
                    break;
            }
            if(bytes == 16 && rangeType == R_Complex8)
                rangeType = R_Complex16;
            if(bytes == 32 && rangeType == R_Complex8)
                rangeType = R_Complex32;
        }
    }
    uint32_t byteSize() {
        switch(rangeType) {
            case R_VeryLarge:
                return sizeof(unsigned long long);
            case R_Float32:
                return 4;
            case R_Float64:
                return 8;
            case R_Float128:
                return 16;
            case R_Complex8:
                return 8;
            case R_Complex16:
                return 16;
            case R_Complex32:
                return 32;
            case R_Defined:
                if(upper <= 255)
                    return 1;
                else if (upper <= 65535)
                    return 2;
                else if (upper <= 4294967295)
                    return 4;
                else if (upper <= 0xffffffffffffffff)
                    return 8;
            default:
                return 0;
        }
    }
    string toString() {
        string result("r" + no.toString() + " : ");
        switch(rangeType) {
            case R_VeryLarge:
                result += "<VeryLarge>";
                break;
            case R_Float32:
                result += "<Float32>";
                break;
            case R_Float64:
                result += "<Float64>";
                break;
            case R_Float128:
                result += "<Float128>";
                break;
            case R_Complex8:
                result += "<Complex8>";
                break;
            case R_Complex16:
                result += "<Complex16>";
                break;
            case R_Complex32:
                result += "<Complex32>";
                break;
            case R_Defined:
                result += "Def(" + patch::toString((int)lower) + "," + patch::toString((int)upper) + ")";
                break;
        }
        return result; // + patch::toString((unsigned int)byteSize());
    }
};
class Array : public Type {
public:
    Range *range;
    uint64_t lower, upper;
    Type *type;
public:
    Array(SourceObject *object, TypeNo no, astream &str);
    string toString() {
        return "a" + no.toString() + " [over: " + (range ? range->toString() : "<n>") + ";" + patch::toString((int)lower) + "," + patch::toString((int)upper)+ "] of " + (type ? type->toString() : "<n>");
    }
};
class Struct : public Type { //applies to union
public:
    struct Entry {
        string name;
        Type *type;
        uint64_t bitOffset, bitSize;
        Entry(string name, Type *type, uint64_t bitOffset, uint64_t bitSize) {
            this->name = name;
            this->type = type;
            this->bitOffset = bitOffset;
            this->bitSize = bitSize;
        }
        string toString() {
            return name + " : [" + patch::toString((int)bitOffset) + "," + patch::toString((int)bitSize) + "] of " + type->toString();
        }
    };
    vector<Entry *> entries;
    void addEntry(string name, Type *type, uint64_t bitOffset, uint64_t bitSize) {
        entries.push_back(new Entry(name, type, bitOffset, bitSize));
    }
    uint64_t size;
public:
    Struct(SourceObject *object, Type::TypeNo no, astream &str);
    string toString() {
        string result("s" + patch::toString((int)size) + " {\n");
        for(vector<Entry *>::iterator it = entries.begin(); it != entries.end(); it++)
            result += (*it)->toString() + "\n";
        return result + "}";
    }
};
class Enum : public Type {
public:
    struct Entry {
        string name;
        uint64_t value;
        Entry(string name, uint64_t value) {
            this->name = name;
            this->value = value;
        }
        string toString() {
            return name + " : " + patch::toString((int)value);
        }
    };
    vector<Entry *> entries;
    void addEntry(string name, uint64_t value) {
        entries.push_back(new Entry(name, value));
    }
public:
    Enum(TypeNo no, astream &str)
    : Type(T_Enum, no)
    {
        str.peekSkip('e');
        while(1) {
            string name = str.get(':');
            uint64_t value = str.getInt();
            str.peekSkip(',');
            addEntry(name, value);
            if(str.peek() == ';') {
                str.skip();
                break;
            }
        }
    }
    string toString() {
        string result("e {\n");
        for(vector<Entry *>::iterator it = entries.begin(); it != entries.end(); it++)
            result += (*it)->toString() + "\n";
        return result + "}";
    }
};
class Pointer : public Type {
public:
    Type *pointsTo;
public:
    Pointer(SourceObject *object, Type::TypeNo no, astream &str);
    string toString() {
        return "(*) " + (pointsTo ? pointsTo->toString() : "");
    }
};
class ConformantArray : public Type {
public:
    ConformantArray(Type::TypeNo no, astream &str)
    : Type(T_ConformantArray, no)
    {
        str.peekSkip('x');
        char c = str.peek();
        switch(c) {
            case 's': // ??
                break;
            default: break;
        }
    }
    string toString() {
        return "<conformant>: "; // + dummy->toString();
    }
};
class FunctionType : public Type {
public:
    FunctionType(Type::TypeNo no)
    : Type(T_Function, no)
    { }
    string toString() { return "f" + no.toString(); }
};
class Symbol {
public:
    typedef enum {
        S_Typedef,
        S_Local,
        S_Param,
        S_Global,
        S_Function,
        S_Bracket
    } SymType;
    SymType symType;
    string name;
    uint64_t address; //location, depends on symType
    Type *type;
    Symbol(SymType symType, string name, Type *type, uint64_t address = 0) {
        this->symType = symType;
        this->name = name;
        this->type = type;
        this->address = address;
    }
    virtual string toString() {
        string result(name);
        switch(symType) {
            case S_Typedef:
                result += ": LSYM<typedef> ";
                break;
            case S_Local:
                result += ": LSYM<var> ";
                break;
            case S_Param:
                result += ": PSYM ";
                break;
            case S_Global:
                result += ": GSYM ";
                break;
            case S_Function:
                result += ": FUN ";
        }
        return result + "[addr: " + patch::toString((void *)address) + "] " + (type ? type->toString() : "");
    }
};
class Scope {
public:
    Scope *parent;
    uint64_t begin, end;
    vector<Symbol *> symbols;
    vector<Scope *> children;
    Scope(Scope *parent, uint64_t begin) { this->parent = parent; this->begin = begin; }
    string toString() {
        string result = "LBRAC [0x" + patch::toString((void *)begin) + "] -- {\n";
        for(vector<Symbol *>::iterator it = symbols.begin(); it != symbols.end(); it++)
            result += (*it)->toString() + "\n";
        for(vector<Scope *>::iterator it = children.begin(); it != children.end(); it++)
            result += (*it)->toString();
        return result + "} RBRAC [0x" + patch::toString((void *)end) + "] --\n";
    }
    Scope *getScope(uint32_t address) {
        for(int i = 0; i < children.size(); i++) {
            Scope *s = children[i];
            if(s->begin <= address && s->end >= address)
                return s->getScope(address);
        }
        return this;
    }
};
class Function : public Symbol {
public:
    struct SLine {
        uint64_t address;
        int line;
        string source;
        SLine(uint64_t address, int line, string source) {
            this->address = address;
            this->line = line;
            this->source = source;
        }
        string toString() {
            return "SLINE [" + patch::toString(line) + "] 0x" + patch::toString((void *)address);
        }
    };
    vector<SLine *> lines;
    void addLine(uint64_t address, int line, string source) {
        lines.push_back(new SLine(address, line, source));
    }
    vector<Symbol *> params;
    vector<Scope *> locals;
    Function(string name, Type *type, uint64_t address)
    : Symbol(S_Function, name, type, address)
    { }
    string toString() {
        string result = name + ": FUN [" + patch::toString((void *)address) + " ] of " + (type ? type->toString() : "<n>") + "\n";
        for(vector<SLine *>::iterator it = lines.begin(); it != lines.end(); it++)
            result += (*it)->toString() + "\n";
        for(vector<Symbol *>::iterator it = params.begin(); it != params.end(); it++)
            result += "PARAM: " + (*it)->toString() + "\n";
        for(vector<Scope *>::iterator it = locals.begin(); it != locals.end(); it++)
            result += (*it)->toString() + "\n";
        return result + "}\n";
    }
};
class Symbol;
class Function;
class SourceObject {
public:
    string name;
    uint64_t start, end;
    vector<Type *> types;
    vector<Symbol *> locals;
    vector<Symbol *> globals;
    vector<Function *> functions;
public:
    Type *findType(Type::TypeNo &no) {
        for(int i = 0; i < types.size(); i++)
            if(types[i]->no.equals(no)) {
                return types[i];
            }
        return 0;
    }
    void addType(Type *type) {
        types.push_back(type);
    }
public:
    SourceObject(SymtabEntry **sym, SymtabEntry *stab, const char *stabstr, uint64_t stabsize);
    Type *interpretType(Type::TypeNo no, astream &str);
    Symbol *interpretSymbol(astream &str, uint64_t address);
    Function *interpretFun(astream &str, uint64_t address);
    //vector<string> getSourceNames(); 
    string toString();
};
class Binary {
public:
    string name;
    SymtabEntry *stab;
    const char *stabstr;
    uint64_t stabsize;
    vector<SourceObject *> objects;
public:
    Binary(string name, SymtabEntry *stab, const char *stabstr, uint64_t stabsize);
    vector<string> getSourceNames();
    uint32_t getLineAddress(string file, int line);
    Function *getFunction(uint32_t address);
    uint32_t getFunctionAddress(string name);
    string toString();
};
#endif //DEFINITIONS_HPP