#ifndef DEFINITIONS02_HPP
#define DEFINITIONS02_HPP

#include "StringTools.hpp"
#include "symtabs.h"

#include <string>
#include <vector>
#include <limits.h>
using namespace std;
using namespace patch;
class Range;
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
    static vector<Type *> types;
    static Type *findType(TypeNo &no) {
        for(int i = 0; i < types.size(); i++)
            if(types[i]->no.equals(no)) {
                return types[i];
            }
        return 0;
    }
    static void addType(Type *type) {
        types.push_back(type);
    }
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
        T_ConformantArray
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
        R_Integer,
        R_Float32,
        R_Float64,
        R_Float128,
        R_Complex,
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
            //if(!(newNo == no)) panic;
            /*if(str.peek() == '=') {
                TypeNo temp(str); //swallow
            }*/
            str.peekSkip(';');
            lower = str.getInt();
            str.peekSkip(';');
            upper = str.getInt();
            str.peekSkip(';');
            if(lower == 0 && upper == -1) {
                rangeType = R_Integer;
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
                    rangeType = R_Complex;
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
            if(bytes == 16 && rangeType == R_Complex)
                rangeType = R_Complex16;
            if(bytes == 32 && rangeType == R_Complex)
                rangeType = R_Complex32;
        }
    }
    uint32_t byteSize() {
        switch(rangeType) {
            case R_Integer:
                return sizeof(int);
            case R_Float32:
                return 4;
            case R_Float64:
                return 8;
            case R_Float128:
                return 16;
            case R_Complex:
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
            default:
                return 0;
        }
    }
    string toString() {
        string result("r" + no.toString() + " : ");
        switch(rangeType) {
            case R_Integer:
                result += "<Integer>";
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
            case R_Complex:
                result += "<Complex>";
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
Type *InterpretType(Type::TypeNo no, astream &str);
class Array : public Type {
public:
    Range *range;
    uint64_t lower, upper;
    Type *type;
public:
    Array(TypeNo no, astream &str)
    : Type(T_Array, no)
    {
        str.peekSkip('a');
        if(str.peek() == 'r') {
            str.skip();
            TypeNo rNo(str);
            range = dynamic_cast<Range *>(Type::findType(rNo));
            if(!range) {
                range = new Range(rNo, str);
                Type::addType(range);
            }
        } else range = 0;
        str.peekSkip(';');
        lower = str.getInt();
        str.peekSkip(';');
        upper = str.getInt();
        str.peekSkip(';');
        if(str.peek() == '(') {
            TypeNo tNo(str);
            type = findType(tNo);
            if(!type)
                type = InterpretType(tNo, str);
        } else type = 0;
    }
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
    Struct(TypeNo no, astream &str)
    : Type(T_Struct, no)
    {
        str.peekSkip('s');
        str.peekSkip('u');
        size = str.getInt();
        while(1) {
            string name = str.get(':');
            if(str.eof()) {
                addEntry(name, 0, 0, 0);
                break;
            }
            Type::TypeNo no(str);
            Type *type = Type::findType(no);
            if(!type)
                type = InterpretType(no, str);
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
    Pointer(Type::TypeNo no, astream &str)
    : Type(T_Pointer, no),
    pointsTo(0)
    {
        str.skip();
        Type::TypeNo pNo(str);
        pointsTo = Type::findType(pNo);
        if(!pointsTo)
            pointsTo = InterpretType(no, str);
    }
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
class Sym {
public:
    typedef enum {
        L_Typedef,
        L_Local,
        L_Param,
        L_Global
    } SymType;
    SymType symType;
    string name;
    uint64_t address; //location, depends on symType
    Sym(SymType symType, string name, uint64_t address = 0) {
        this->symType = symType;
        this->name = name;
        this->address = address;
    }
    virtual string toString() {
        return (symType == L_Typedef ? "<Typedef>" : "<0x" + patch::toString((void *)address) + ">");
    }
};
class TSym : public Sym {
public:
    Type *type;
    TSym(string name, Type *type, uint64_t address)
    : Sym(L_Typedef, name, address)
    {
        this->type = type;
    }
    string toString() {
        return "TSYM: " + name + " : " + type->toString();
    }

};
class LSym : public Sym {
public:
    Type *type;
    LSym(string name, Type *type, uint64_t address)
    : Sym(L_Local, name, address)
    {
        this->type = type;
    }
    string toString() {
        return "LSYM: " + name + " : " + type->toString();
    }
};
class GSym : public Sym {
public:
    Type *type;
    GSym(string name, Type *type)
    : Sym(L_Global, name)
    {
        this->type = type;
    }
    string toString() {
        return "" + name + "(GSYM) : " + type->toString();
    }
};
class PSym : public Sym {
public:
    Type *type;
    PSym(string name, Type *type, uint64_t address)
    : Sym(L_Param, name, address)
    {
        this->type = type;
    }
    string toString() {
        return "" + name + "(PSYM) : " + type->toString();
    }
};
Sym *InterpretLSym(astream &str, uint64_t address);
GSym *InterpretGSym(astream &str);
PSym *InterpretPSym(astream &str, uint64_t address);
void readSyms(SymtabEntry *stab, const char *stabstr, unsigned int stabsize);
void printSyms();

#endif //DEFINITIONS_HPP