#ifndef DEFINITIONS02_HPP
#define DEFINITIONS02_HPP

#include "Strings.hpp"
#include "LowLevel.hpp"
#include "Symbols.hpp"
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
    virtual ~Type();
    virtual string toString() = 0;
    virtual uint32_t byteSize() = 0;
    virtual vector<string> values(uint32_t base) = 0;
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
    uint32_t byteSize() {
        return ref->byteSize();
    }
    vector<string> values(uint32_t base) {
        return ref->values(base);
    };
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
    uint32_t byteSize() {
        return 0;
    }
    vector<string> values(uint32_t base) {
        vector<string> result;
        result.push_back("<void>");
        return result;
    };
};
class Range : public Type {
public:
    typedef enum {
        R_UInt64,
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
                rangeType = R_UInt64;
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
            case R_UInt64:
                return 8;
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
            case R_UInt64:
                result += "<UInt64>";
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
    vector<string> values(uint32_t base) {
        //cout << "base: 0x" << (void *)base;
        vector<string> result;
        if(!is_readable_address(base)) {
            result.push_back("<no access>");
            return result;
        }
        switch(rangeType) {
            case R_UInt64: {
                //result.push_back(patch::toString(*(unsigned long long *)base)); //crashes
                unsigned int value = *(unsigned long long *)base;
                result.push_back(patch::toString(value));
                break;
            }
            case R_Float32:
                result.push_back(patch::toString(*(float *)base));
                break;
            case R_Float64:
                result.push_back(patch::toString(*(double *)base));
                break;
            case R_Float128:
                result.push_back(patch::toString(*(long double *)base));
                break;
            case R_Complex8:
                result.push_back("real : " + patch::toString(*(float *)base));
                result.push_back("imag : " + patch::toString(*(float *)(base + 4)));
                break;
            case R_Complex16:
                result.push_back("real : " + patch::toString(*(double *)base));
                result.push_back("imag : " + patch::toString(*(double *)(base + 8)));
                break;
            case R_Complex32:
                result.push_back("real : " + patch::toString(*(long double *)base));
                result.push_back("imag : " + patch::toString(*(long double *)(base + 16)));
                break;
            case R_Defined:
                if(lower >= 0) {
                    switch(byteSize()) {
                        case 1:
                            result.push_back(patch::toString(*(unsigned char *)base));
                            break;
                        case 2:
                            result.push_back(patch::toString(*(unsigned short *)base));
                            break;
                        case 4:
                            result.push_back(patch::toString(*(unsigned int *)base));
                            break;
                        case 8: {
                            // result.push_back(patch::toString(*(unsigned long long *)base));
                            unsigned int value = *(unsigned long long *)base;
                            result.push_back(patch::toString(value));
                            break;
                        }
                        default:
                            result.push_back("<unknown>");
                            break;
                    }
                } else {
                    switch(byteSize()) {
                        case 1:
                            result.push_back(patch::toString(*(char *)base));
                            break;
                        case 2:
                            result.push_back(patch::toString(*(short *)base));
                            break;
                        case 4:
                            cout << "==INT==\n";
                            result.push_back(patch::toString(*(int *)base));
                            break;
                        case 8: {
                            // result.push_back(patch::toString(*(long long *)base));
                            int value = *(long long *)base;
                            result.push_back(patch::toString(value));
                            break;
                        }
                        default:
                            result.push_back("<unknown>");
                            break;
                    }
                }
                break;
            default:
                result.push_back("<unknown>");
                break;
        }
        return result;
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
    uint32_t byteSize() {
        return type->byteSize() * (upper - lower);
    }
    vector<string> values(uint32_t base) {
        vector<string> result;

        uint32_t address = base;
        uint32_t place = lower;

        while(place <= upper) {
            vector<string> v = type->values(address);
            result.insert(result.end(), v.begin(), v.end());
            address += type->byteSize();
            place++;
        }
        return result;
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
    uint32_t byteSize() {
        return size;
    }
    vector<string> values(uint32_t base) {
        vector<string> result;
        for(int i = 0; i < entries.size(); i++) {
            uint32_t offset = entries[i]->bitOffset / 8;
            vector<string> v = entries[i]->type->values(base + offset);
            if(v.size() == 1)
                result.push_back(entries[i]->name + " : " + v[0]);
            else {
                result.push_back(entries[i]->name + " : {");
                result.insert(result.end(), v.begin(), v.end());
                result.push_back("}");
            }
        }
        return result;
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
    uint32_t byteSize() {
        return 4; //is this coherent?
    }
    vector<string> values(uint32_t base) {
        vector<string> result;
        if(is_readable_address(base)) {
            int value = *(int *)base;
            for(int i = 0; i < entries.size(); i++)
                if(entries[i]->value == value) {
                    result.push_back(entries[i]->name + "(" + patch::toString(value) + ")");
                    break;
                }
        } else {
            result.push_back("<no access");
        }
        return result;
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
    uint32_t byteSize() {
        return sizeof(void*);
    }
    vector<string> values(uint32_t base) {
        vector<string> result;
        uint32_t address = *(uint32_t *)base;
        vector<string> v = pointsTo->values(address);
        if(v.size() == 1)
            result.push_back("(*) " + v[0]);
        else
            result.insert(result.end(), v.begin(), v.end());
        return result;
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
    uint32_t byteSize() {
        return 0;
    }
    vector<string> values(uint32_t base) {
        vector<string> result;
        result.push_back("<unknwon array>");
        return result;
    }
};
class FunctionType : public Type {
public:
    FunctionType(Type::TypeNo no)
    : Type(T_Function, no)
    { }
    string toString() { return "f" + no.toString(); }
    uint32_t byteSize() { return sizeof(void *); }
    vector<string> values(uint32_t base) {
        vector<string> result;
        result.push_back("<function>");
        return result;
    }
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
    uint32_t address; //location, depends on symType
    Type *type;
    Symbol(SymType symType, string name, Type *type, uint32_t address = 0) {
        this->symType = symType;
        this->name = name;
        this->type = type;
        this->address = address;
    }
    string typeString() {
        switch(symType) {
            case S_Typedef:
                return "<Typedef>";
            case S_Local:
                return "<Local>";
            case S_Param:
                return "<Param>";
            case S_Global:
                return "<Global>";
            case S_Function:
                return "<Function>";
        }
        return "";
    }
    virtual string toString() {
        return name + " " + typeString() + " [addr: " + patch::toString((void *)address) + "] " + (type ? type->toString() : "");
    }
    vector<string> values(uint32_t base) {
        vector<string> result;
        vector<string> v = type->values(base + address);
        if(v.size() == 1)
            result.push_back(name + " " + typeString() + " : " + v[0]);
        else {
            result.push_back(name + " " + typeString() + " : {");
            result.insert(result.end(), v.begin(), v.end());
            result.push_back("}");
        }
        return result;
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
    Function::SLine *getLocation(uint32_t address);
    string getSourceFile(uint32_t address);
    int getSourceLine(uint32_t address);
    uint32_t getFunctionAddress(string name);
    vector<string> getContext(uint32_t ip, uint32_t sp);
    vector<string> getGlobals(ElfSymbols &symbols);
    string toString();
};
#endif //DEFINITIONS_HPP