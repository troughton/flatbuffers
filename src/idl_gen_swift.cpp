/*
 * Copyright 2014 Google Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// independent from idl_parser, since this code is not needed for most clients

#include <string>
#include <sstream>

#include "flatbuffers/flatbuffers.h"
#include "flatbuffers/idl.h"
#include "flatbuffers/util.h"
#include "flatbuffers/code_generators.h"

#ifdef _WIN32
#include <direct.h>
#define PATH_SEPARATOR "\\"
#define mkdir(n, m) _mkdir(n)
#else
#include <sys/stat.h>
#define PATH_SEPARATOR "/"
#endif

namespace flatbuffers {
    namespace swift {
        class SwiftGenerator : public BaseGenerator {
            
            
            const CommentConfig commentConfig = { "/**", "*", "*/" };
            
        public:
            SwiftGenerator(const Parser &parser, const std::string &path,
                             const std::string &file_name)
            : BaseGenerator(parser, path, file_name, "", ".") {
            }
            
            SwiftGenerator &operator=(const SwiftGenerator &);
            bool generate() {
                std::string one_file_code;
                
                for (auto it = parser_.enums_.vec.begin(); it != parser_.enums_.vec.end();
                     ++it) {
                    std::string enumcode;
                    auto &enum_def = **it;
                    GenEnum(enum_def, &enumcode);
                    if (parser_.opts.one_file) {
                        one_file_code += enumcode;
                    } else {
                        if (!SaveType(enum_def.name, *enum_def.defined_namespace,
                                      enumcode, false)) return false;
                    }
                }
                
                for (auto it = parser_.structs_.vec.begin();
                     it != parser_.structs_.vec.end(); ++it) {
                    std::string declcode;
                    auto &struct_def = **it;
                    GenStruct(struct_def, &declcode);
                    if (parser_.opts.one_file) {
                        one_file_code += declcode;
                    } else {
                        if (!SaveType(struct_def.name, *struct_def.defined_namespace,
                                      declcode, true)) return false;
                    }
                }
                
                if (parser_.opts.one_file) {
                    return SaveType(file_name_, *parser_.namespaces_.back(),
                                    one_file_code, true);
                }
                return true;
            }
            
            // Save out the generated code for a single class while adding
            // declaration boilerplate.
            bool SaveType(const std::string &defname, const Namespace &ns,
                          const std::string &classcode, bool needs_includes) {
                if (!classcode.length()) return true;
                
                std::string code;
                code = code + "// " + FlatBuffersGeneratedWarning();
                if (needs_includes) code += "import Foundation\nimport FlatBuffers\n\n";
                code += classcode;
                auto filename = NamespaceDir(ns) + defname + ".swift";
                return SaveFile(filename.c_str(), code, false);
            }
            
            std::string FunctionStart(char upper) {
                return std::string() + (static_cast<char>(tolower(upper)));
            }
            
            static bool IsEnum(const Type& type) {
                return type.enum_def != nullptr && IsInteger(type.base_type);
            }
            
            std::string GenTypeBasic(const Type &type, const bool enableLangOverrides) {
            
                static const char *swift_typename[] = {
#define FLATBUFFERS_TD(ENUM, IDLTYPE, CTYPE, JTYPE, GTYPE, NTYPE, PTYPE, STYPE) \
#STYPE,
                    FLATBUFFERS_GEN_TYPES(FLATBUFFERS_TD)
#undef FLATBUFFERS_TD
                };
                
                if (enableLangOverrides) {
                    if (IsEnum(type)) return type.enum_def->name;
                    if (type.base_type == BASE_TYPE_STRUCT) {
                        return "Offset<" + type.struct_def->name + ">";
                    }
                }
                return swift_typename[type.base_type];
            }
            
            std::string GenTypeBasic(const Type &type) {
                return GenTypeBasic(type, true);
            }
            
            std::string GenTypePointer(const Type &type) {
                switch (type.base_type) {
                    case BASE_TYPE_STRING:
                        return "String";
                    case BASE_TYPE_VECTOR:
                        return GenTypeGet(type.VectorType());
                    case BASE_TYPE_STRUCT:
                        return type.struct_def->name;
                    case BASE_TYPE_UNION:
                        // Unions in C# use a generic Table-derived type for better type safety
                        return "TTable";
                        // fall through
                    default:
                        return "Table";
                }
            }
            
            std::string GenTypeGet(const Type &type) {
                return IsScalar(type.base_type)
                ? GenTypeBasic(type)
                : GenTypePointer(type);
            }
            
            // Find the destination type the user wants to receive the value in (e.g.
            // one size higher signed types for unsigned serialized values in Java).
            Type DestinationType(const Type &type) {
                return type;
            }
            
            std::string GenOffsetType(const StructDef &struct_def) {
                    return "Offset<" + struct_def.name + ">";
            }
            
            std::string GenOffsetConstruct(const StructDef &struct_def,
                                           const std::string &variable_name)
            {
                return "Offset<" + struct_def.name + ">(" + variable_name +
                    ")";
            }
            
            std::string GenVectorOffsetType() {
                return "VectorOffset";
            }
            
            // Generate destination type name
            std::string GenTypeNameDest(const Type &type)
            {
                return GenTypeGet(DestinationType(type));
            }
            
            // Mask to turn serialized value into destination type value.
            std::string DestinationMask(const Type &type) {
                if (type.base_type == BASE_TYPE_VECTOR) {
                    return DestinationCast(type.VectorType());
                } else {
                    // Cast from raw integral types to enum.
                    if (IsEnum(type)) {
                        return ")!";
                    }
                }
                return "";
            }
            
            // Casts necessary to correctly read serialized data
            std::string DestinationCast(const Type &type) {
                if (type.base_type == BASE_TYPE_VECTOR) {
                    return DestinationCast(type.VectorType());
                } else {
                    // Cast from raw integral types to enum.
                    if (IsEnum(type)) {
                       return type.enum_def->name + "(rawValue: ";
                    }
                }
                return "";
            }
            
            // Cast statements for mutator method parameters.
            // In Java, parameters representing unsigned numbers need to be cast down to
            // their respective type. For example, a long holding an unsigned int value
            // would be cast down to int before being put onto the buffer. In C#, one cast
            // directly cast an Enum to its underlying type, which is essential before
            // putting it onto the buffer.
            std::string SourceCast(const Type &type, bool castFromDest) {
                if (type.base_type == BASE_TYPE_VECTOR) {
                    return SourceCast(type.VectorType(), castFromDest);
                } else {
//                    if (IsEnum(type)) return type.enum_def->name + ".rawValue";
                }
                return "";
            }
            
            std::string SourceCast(const Type &type) {
                return SourceCast(type, true);
            }
            
            std::string SourceCastBasic(const Type &type, bool castFromDest) {
                return IsScalar(type.base_type) ? SourceCast(type, castFromDest) : "";
            }
            
            std::string SourceCastBasic(const Type &type) {
                return SourceCastBasic(type, true);
            }
            
            
            std::string GenEnumDefaultValue(const Value &value) {
                auto enum_def = value.type.enum_def;
                auto vec = enum_def->vals.vec;
                auto default_value = StringToInt(value.constant.c_str());
                
                auto result = value.constant;
                for (auto it = vec.begin(); it != vec.end(); ++it) {
                    auto enum_val = **it;
                    if (enum_val.value == default_value) {
                        result = enum_def->name + "." + ((enum_val.name == "NONE") ? "none" : enum_val.name);
                        break;
                    }
                }
                
                return result;
            }
            
            std::string GenDefaultValue(const Value &value, bool enableLangOverrides) {
                if (enableLangOverrides) {
                    // handles both enum case and vector of enum case
                    if (value.type.enum_def != nullptr &&
                        value.type.base_type != BASE_TYPE_UNION) {
                        return GenEnumDefaultValue(value);
                    }
                }
                
                auto longSuffix = "";
                switch (value.type.base_type) {
                    case BASE_TYPE_FLOAT: return value.constant + "";
                    case BASE_TYPE_BOOL: return value.constant == "0" ? "false" : "true";
                    case BASE_TYPE_ULONG:
                    {
                        return value.constant;
                    }
                    case BASE_TYPE_UINT:
                    case BASE_TYPE_LONG: return value.constant + longSuffix;
                    default: return value.constant;
                }
            }
            
            std::string GenDefaultValue(const Value &value) {
                return GenDefaultValue(value, true);
            }
            
            std::string GenDefaultValueBasic(const Value &value, bool enableLangOverrides) {
                if (!IsScalar(value.type.base_type)) {
                    if (enableLangOverrides) {
                            switch (value.type.base_type) {
                                case BASE_TYPE_STRING:
                                    return "StringOffset(0)";
                                case BASE_TYPE_STRUCT:
                                    return "Offset<" + value.type.struct_def->name +
                                    ">(0)";
                                case BASE_TYPE_VECTOR:
                                    return "VectorOffset(0)";
                                default:
                                    break;
                            }
                    }
                    return "0";
                }
                return GenDefaultValue(value, enableLangOverrides);
            }
            
            std::string GenDefaultValueBasic(const Value &value) {
                return GenDefaultValueBasic(value, true);
            }
            
            void GenEnum(EnumDef &enum_def, std::string *code_ptr) {
                std::string &code = *code_ptr;
                if (enum_def.generated) return;
                
                
                GenComment(enum_def.doc_comment, code_ptr, &commentConfig);
                code += std::string("public enum ") + enum_def.name;
                    code += " : " +
                    GenTypeBasic(enum_def.underlying_type, false);
                code += " {\n";
                for (auto it = enum_def.vals.vec.begin();
                     it != enum_def.vals.vec.end();
                     ++it) {
                    auto &ev = **it;
                    GenComment(ev.doc_comment, code_ptr, &commentConfig, "  ");
                    code += "    case " + (ev.name == "NONE" ? "none" : MakeCamel(ev.name, false)) + " = ";
                    code += NumToString(ev.value);
                    code += "\n";
                }
                
                // Generate a generate string table for enum values.
                    // Problem is, if values are very sparse that could generate really big
                    // tables. Ideally in that case we generate a map lookup instead, but for
                    // the moment we simply don't output a table at all.
                    auto range = enum_def.vals.vec.back()->value -
                    enum_def.vals.vec.front()->value + 1;
                    // Average distance between values above which we consider a table
                    // "too sparse". Change at will.
                    static const int kMaxSparseness = 5;
                    if (range / static_cast<int64_t>(enum_def.vals.vec.size()) < kMaxSparseness) {
                        code += "\n  public static ";
                        code += "let names : [String] = [ ";
                        auto val = enum_def.vals.vec.front()->value;
                        for (auto it = enum_def.vals.vec.begin();
                             it != enum_def.vals.vec.end();
                             ++it) {
                            while (val++ != (*it)->value) code += "\"\", ";
                            auto name = (*it)->name;
                            code += "\"" + (name == "NONE" ? "none" : MakeCamel(name, false)) + "\", ";
                        }
                        code += "]\n\n";
                        code += "  public static func";
                        code += " " + MakeCamel("name", false);
                        code += "(_ e: Int) -> String { return names[e";
                        if (enum_def.vals.vec.front()->value)
                            code += " - " + enum_def.vals.vec.front()->name;
                        code += "]; }\n";
                }
                
                // Close the class
                code += "}";
                code += "\n\n";
            }
            
            // Returns the function name that is able to read a value of the given type.
            std::string GenGetter(const Type &type) {
                switch (type.base_type) {
                    case BASE_TYPE_STRING: return "__p.__string";
                    case BASE_TYPE_STRUCT: return "__p.__struct";
                    case BASE_TYPE_UNION:  return "__p.__union";
                    case BASE_TYPE_VECTOR: return GenGetter(type.VectorType());
                    default: {
                        std::string getter =
                        "__p.bb." + FunctionStart('G') + "et";
                        if (type.base_type == BASE_TYPE_BOOL) {
                            getter = "0!=" + getter;
                        } else if (GenTypeBasic(type, false) != "UInt8") {
                            getter += MakeCamel(GenTypeBasic(type, false));
                        }
                        return getter;
                    }
                }
            }
            
            // Returns the function name that is able to read a value of the given type.
            std::string GenGetterForLookupByKey(flatbuffers::FieldDef *key_field,
                                                const std::string &data_buffer,
                                                const char *num = nullptr) {
                auto type = key_field->value.type;
                auto dest_mask = DestinationMask(type);
                auto dest_cast = DestinationCast(type);
                auto getter = data_buffer + "." + FunctionStart('G') + "et";
                if (GenTypeBasic(type, false) != "UInt8") {
                    getter += MakeCamel(GenTypeBasic(type, false));
                }
                getter = dest_cast + getter + "(at: " + GenOffsetGetter(key_field, num) + ")"
                + dest_mask;
                return getter;
            }
            
            // Direct mutation is only allowed for scalar fields.
            // Hence a setter method will only be generated for such fields.
            std::string GenSetter(const Type &type) {
                if (IsScalar(type.base_type)) {
                    std::string setter =
                    "__p.bb." + FunctionStart('P') + "ut";
                    if (GenTypeBasic(type, false) != "byte" &&
                        type.base_type != BASE_TYPE_BOOL) {
                        setter += MakeCamel(GenTypeBasic(type, false));
                    }
                    return setter;
                } else {
                    return "";
                }
            }
            
            // Returns the method name for use with add/put calls.
            std::string GenMethod(const Type &type) {
                return IsScalar(type.base_type)
                ? MakeCamel(GenTypeBasic(type, false))
                : (IsStruct(type) ? "Struct" : "Offset");
            }
            
            // Recursively generate arguments for a constructor, to deal with nested
            // structs.
            void GenStructArgs(const StructDef &struct_def, std::string *code_ptr,
                               const char *nameprefix) {
                std::string &code = *code_ptr;
                for (auto it = struct_def.fields.vec.begin();
                     it != struct_def.fields.vec.end();
                     ++it) {
                    auto &field = **it;
                    if (IsStruct(field.value.type)) {
                        // Generate arguments for a struct inside a struct. To ensure names
                        // don't clash, and to make it obvious these arguments are constructing
                        // a nested struct, prefix the name with the field name.
                        GenStructArgs(*field.value.type.struct_def, code_ptr,
                                      (nameprefix + (field.name + "_")).c_str());
                    } else {
                        code += ", ";
                        code += nameprefix;
                        code += MakeCamel(field.name, false);
                        code += ": ";
                        code += GenTypeBasic(DestinationType(field.value.type));
                    }
                }
            }
            
            // Recusively generate struct construction statements of the form:
            // builder.putType(name);
            // and insert manual padding.
            void GenStructBody(const StructDef &struct_def, std::string *code_ptr,
                               const char *nameprefix) {
                std::string &code = *code_ptr;
                code += "    builder." + FunctionStart('P') + "rep(size:";
                code += NumToString(struct_def.minalign) + ", additionalBytes: ";
                code += NumToString(struct_def.bytesize) + ");\n";
                for (auto it = struct_def.fields.vec.rbegin();
                     it != struct_def.fields.vec.rend(); ++it) {
                    auto &field = **it;
                    if (field.padding) {
                        code += "    builder." + FunctionStart('P') + "ad(size: ";
                        code += NumToString(field.padding) + ");\n";
                    }
                    if (IsStruct(field.value.type)) {
                        GenStructBody(*field.value.type.struct_def, code_ptr,
                                      (nameprefix + (field.name + "_")).c_str());
                    } else {
                        code += "    builder." + FunctionStart('P') + "ut";
                        code += GenMethod(field.value.type) + "(";
                        code += SourceCast(field.value.type);
                        auto argname = nameprefix + MakeCamel(field.name, false);
                        code += argname;
                        if (IsEnum(field.value.type)) {
                            code += ".rawValue";
                        }
                        code += ");\n";
                    }
                }
            }
            
            std::string GenByteBufferLength(const char *bb_name) {
                std::string bb_len = bb_name;
                bb_len += ".length";
                return bb_len;
            }
            
            std::string GenOffsetGetter(flatbuffers::FieldDef *key_field,
                                        const char *num = nullptr) {
                std::string key_offset = "";
                key_offset += "Table.__offset(" +
                NumToString(key_field->value.offset) + ", ";
                if (num) {
                    key_offset += num;
                    key_offset += ".value, builder.dataBuffer)";
                } else {
                    key_offset += GenByteBufferLength("bb");
                    key_offset += " - tableOffset, bb)";
                }
                return key_offset;
            }
            
            std::string GenLookupKeyGetter(flatbuffers::FieldDef *key_field) {
                std::string key_getter = "      ";
                key_getter += "let tableOffset = Table.";
                key_getter += "__indirect(vectorLocation + 4 * (start + middle)";
                key_getter += ", bb);\n      ";
                if (key_field->value.type.base_type == BASE_TYPE_STRING) {
                    key_getter += "let comp = Table.";
                    key_getter += FunctionStart('C') + "ompareStrings(";
                    key_getter += GenOffsetGetter(key_field);
                    key_getter += ", byteKey, bb);\n";
                } else {
                    auto get_val = GenGetterForLookupByKey(key_field, "bb");
                        key_getter += GenTypeNameDest(key_field->value.type) + " val = ";
                        key_getter += get_val + ";\n";
                        key_getter += "      let comp = val > key ? 1 : val < key ? -1 : 0;\n";
                }
                return key_getter;
            }
            
            
            std::string GenKeyGetter(flatbuffers::FieldDef *key_field) {
                std::string key_getter = "";
                auto data_buffer = "builder.dataBuffer";
                if (key_field->value.type.base_type == BASE_TYPE_STRING) {
                    key_getter += "Table.";
                    key_getter += FunctionStart('C') + "ompareStrings(";
                    key_getter += GenOffsetGetter(key_field, "o1") + ", ";
                    key_getter += GenOffsetGetter(key_field, "o2") + ", " + data_buffer + ")";
                }
                else {
                    auto field_getter = GenGetterForLookupByKey(key_field, data_buffer, "o1");
                        key_getter += "\n    " + GenTypeNameDest(key_field->value.type) + " val_1 = ";
                        key_getter += field_getter + ";\n    " + GenTypeNameDest(key_field->value.type);
                        key_getter += " val_2 = ";
                        field_getter = GenGetterForLookupByKey(key_field, data_buffer, "o2");
                        key_getter += field_getter + ";\n";
                        key_getter += "    return val_1 > val_2 ? 1 : val_1 < val_2 ? -1 : 0;\n ";
                }
                return key_getter;
            }
            
            void GenStruct(StructDef &struct_def, std::string *code_ptr) {
                if (struct_def.generated) return;
                std::string &code = *code_ptr;
                
                // Generate a struct accessor class, with methods of the form:
                // public type name() { return bb.getType(i + offset); }
                // or for tables of the form:
                // public type name() {
                //   int o = __offset(offset); return o != 0 ? bb.getType(o + i) : default;
                // }
                GenComment(struct_def.doc_comment, code_ptr, &commentConfig);
                code += "public ";
                code += "struct " + struct_def.name;
                    code += " : FlatbufferObject";
                    code += " {\n";
                    code += "  private var";
                code += " __p = ";
                    code += struct_def.fixed ? "Struct()" : "Table()";
                code += " \n\n";
                code += "  public init() {  }\n\n";
                
                        code += "  public var byteBuffer : ByteBuffer { get { return __p.bb; } }\n";
                
            
                if (!struct_def.fixed) {
                    // Generate a special accessor for the table that when used as the root
                    // of a FlatBuffer
                    std::string method_name = FunctionStart('G') + "etRootAs" + struct_def.name;
                    std::string method_signature = "  public static func " +
                    method_name;
                    
                    // create convenience method that doesn't require an existing object
                    code += method_signature + "(_ _bb: ByteBuffer) -> " + struct_def.name;
                    code += " { var obj = " + struct_def.name + "(); return " + method_name + "(_bb, &obj); }\n";
                    
                    // create method that allows object reuse
                    code += method_signature + "(_ _bb: ByteBuffer, _ obj: inout " + struct_def.name + ") -> " + struct_def.name + " { ";
                    code += "return (obj.__assign(Int(_bb." + FunctionStart('G') + "etInt32(at: _bb.";
                    code += "position";
                    code += ")) + _bb.";
                    code += "position";
                    code += ", _bb)); }\n";
                    if (parser_.root_struct_def_ == &struct_def) {
                        if (parser_.file_identifier_.length()) {
                            // Check if a buffer has the identifier.
                            code += "  public static ";
                            code += "func " + struct_def.name;
                            code += "BufferHasIdentifier(_ _bb: ByteBuffer) -> Bool { return ";
                            code += "Table.__has_identifier(_bb, \"";
                            code += parser_.file_identifier_;
                            code += "\"); }\n";
                        }
                    }
                }
                // Generate the __init method that sets the field in a pre-existing
                // accessor object. This is to allow object reuse.
                code += "  public mutating func __init(_ _i : Int, _ _bb: ByteBuffer) ";
                code += "{ __p.bb_pos = _i; ";
                code += "__p.bb = _bb; }\n";
                code += "  public mutating func __assign(_ _i: Int, _ _bb: ByteBuffer) -> " + struct_def.name;
                code += " { __init(_i, _bb); return self; }\n\n";
                for (auto it = struct_def.fields.vec.begin();
                     it != struct_def.fields.vec.end();
                     ++it) {
                    auto &field = **it;
                    if (field.deprecated) continue;
                    GenComment(field.doc_comment, code_ptr, &commentConfig, "  ");
                    std::string type_name = GenTypeGet(field.value.type);
                    std::string type_name_dest = GenTypeNameDest(field.value.type);
                    std::string conditional_cast = "";
                    std::string optional = "";
                    if (!struct_def.fixed &&
                        (field.value.type.base_type == BASE_TYPE_STRUCT ||
                         field.value.type.base_type == BASE_TYPE_UNION ||
                         (field.value.type.base_type == BASE_TYPE_VECTOR &&
                          field.value.type.element == BASE_TYPE_STRUCT))) {
                             optional = "?";
                             conditional_cast = type_name_dest + optional;
                         }
                    std::string dest_mask = DestinationMask(field.value.type);
                    std::string dest_cast = DestinationCast(field.value.type);
                    std::string src_cast = SourceCast(field.value.type);
                    std::string method_start = "  public var " + MakeCamel(field.name, false) + " : " + type_name_dest + optional + " ";
                    if (field.value.type.base_type == BASE_TYPE_UNION) {
                        method_start = "    public func " + MakeCamel(field.name, false) + "<TTable : FlatbufferObject>() -> TTable? ";
                    } else if (field.value.type.base_type == BASE_TYPE_VECTOR) {
                        method_start = "    public func " + MakeCamel(field.name, false) + "";
                    }
                    std::string obj = type_name + "()";
                    
                    // Most field accessors need to retrieve and test the field offset first,
                    // this is the prefix code for that:
                    auto offset_prefix = " { let o = __p.__offset(" +
                    NumToString(field.value.offset) +
                    "); if o != 0 { ";
                    // Generate the accessors that don't do object reuse.
                    if (field.value.type.base_type == BASE_TYPE_STRUCT) {
                        // Calls the accessor that takes an accessor object with a new object.
//                        if (lang_.language != IDLOptions::kCSharp) {
//                            code += method_start + "() { return ";
//                            code += MakeCamel(field.name, false);
//                            code += "(new ";
//                            code += type_name + "()); }\n";
//                        }
                    } else if (field.value.type.base_type == BASE_TYPE_VECTOR &&
                               field.value.type.element == BASE_TYPE_STRUCT) {
                        // Accessors for vectors of structs also take accessor objects, this
                        // generates a variant without that argument.
                    } else if (field.value.type.base_type == BASE_TYPE_UNION) {
                            // Union types in C# use generic Table-derived type for better type
                            // safety.
                            type_name = type_name_dest;
                    }
                    std::string getter = dest_cast + GenGetter(field.value.type);
                    code += method_start;
                    std::string default_cast = "(";
                    // only create default casts for c# scalars or vectors of scalars
                    if ((IsScalar(field.value.type.base_type) ||
                         (field.value.type.base_type == BASE_TYPE_VECTOR &&
                          IsScalar(field.value.type.element)))) {
                             // For scalars, default value will be returned by GetDefaultValue().
                             // If the scalar is an enum, GetDefaultValue() returns an actual c# enum
                             // that doesn't need to be casted. However, default values for enum
                             // elements of vectors are integer literals ("0") and are still casted
                             // for clarity.
                             if (field.value.type.enum_def == nullptr ||
                                 field.value.type.base_type == BASE_TYPE_VECTOR) {
                                 default_cast = type_name_dest + "(";
                             }
                         }
                    std::string member_suffix = "; ";
                    if (IsScalar(field.value.type.base_type)) {
                        code += " { get";
                        member_suffix += "} ";
                        if (struct_def.fixed) {
                            code += " { return " + getter;
                            code += "(at: __p.bb_pos + ";
                            code += NumToString(field.value.offset) + ")";
                            code += dest_mask;
                        } else {
                            code += offset_prefix + "return " + getter;
                            code += "(at: o + __p.bb_pos)" + dest_mask;
                            code += " } else { return " + default_cast;
                            code += GenDefaultValue(field.value);
                            code += ") }";
                        }
                    } else {
                        switch (field.value.type.base_type) {
                            case BASE_TYPE_STRUCT:
                                code += " { get";
                                member_suffix += "} ";
                                if (struct_def.fixed) {
                                    code += " { var obj = " + obj + "; return obj.__assign(__p.";
                                    code += "bb_pos + " + NumToString(field.value.offset) + ", ";
                                    code += "__p.bb)";
                                } else {
                                    code += offset_prefix;
                                    code += "var obj = " + obj + "; return obj.__assign(";
                                    code += field.value.type.struct_def->fixed
                                    ? "o + __p.bb_pos"
                                    : "__p.__indirect(o + __p.bb_pos)";
                                    code += ", __p.bb) } else { return nil }";
                                }
                                break;
                            case BASE_TYPE_STRING:
                                code += " { get";
                                member_suffix += "} ";
                                code += offset_prefix + "return " + getter + "(at: o + __p.";
                                code += "bb_pos) } else {";
                                if (field.required) {
                                    code += " fatalError() }";
                                } else {
                                    code += " return nil }";
                                }
                                break;
                            case BASE_TYPE_VECTOR: {
                                auto vectortype = field.value.type.VectorType();
                                code += "(at j: Int) -> ";
                                if (vectortype.base_type == BASE_TYPE_STRUCT) {
                                    getter = "var obj = " + obj + "; ";
                                    code += conditional_cast + offset_prefix + getter + "return obj.__assign(";
                                } else {
                                    code += conditional_cast + offset_prefix + "return " + getter +"(at: ";
                                }
                                auto index = "__p.__vector(o) + j * " +
                                NumToString(InlineSize(vectortype));
                                if (vectortype.base_type == BASE_TYPE_STRUCT) {
                                    code += vectortype.struct_def->fixed
                                    ? index
                                    : "__p.__indirect(" + index + ")";
                                    code += ", __p.bb";
                                } else {
                                    code += index;
                                }
                                code += ")" + dest_mask + " } else { return ";
                                
                                code += field.value.type.element == BASE_TYPE_BOOL ? "false" :
                                (IsScalar(field.value.type.element) ? default_cast + "0" : "nil");
                                code += " } ";
                                break;
                            }
                            case BASE_TYPE_UNION:
                                    code += offset_prefix + "return " + getter;
                                    code += "(o) } else { return nil }";
                                break;
                            default:
                                assert(0);
                        }
                    }
                    code += member_suffix;
                    code += "}\n";
                    if (field.value.type.base_type == BASE_TYPE_VECTOR) {
                        code += "  public var " + MakeCamel(field.name, false);
                        code += "Length : Int";
                        code += " { get";
                        code += offset_prefix;
                        code += "return __p.__vector_len(o) } else { return 0 }; ";
                        code += "} ";
                        code += "}\n";
                    }
                    // Generate a ByteBuffer accessor for strings & vectors of scalars.
                    if ((field.value.type.base_type == BASE_TYPE_VECTOR &&
                         IsScalar(field.value.type.VectorType().base_type)) ||
                        field.value.type.base_type == BASE_TYPE_STRING) {
                                code += "  public func get";
                                code += MakeCamel(field.name, true);
                                code += "Bytes() -> UnsafeMutableRawBufferPointer? { return ";
                                code += "__p.__vector_as_arraysegment(";
                                code += NumToString(field.value.offset);
                                code += "); }\n";
                    }
                    // generate object accessors if is nested_flatbuffer
                    auto nested = field.attributes.Lookup("nested_flatbuffer");
                    if (nested) {
                        auto nested_qualified_name =
                        parser_.namespaces_.back()->GetFullyQualifiedName(nested->constant);
                        auto nested_type = parser_.structs_.Lookup(nested_qualified_name);
                        auto nested_type_name = WrapInNameSpace(*nested_type);
                        auto nestedMethodName = MakeCamel(field.name, false)
                        + "As" + nested_type_name;
                        auto getNestedMethodName = nestedMethodName;
                            getNestedMethodName = "Get" + nestedMethodName;
                            conditional_cast = nested_type_name + "?";

                            obj = "(" + nested_type_name + "())";
                        code += "  public " + nested_type_name + "? ";
                        code += getNestedMethodName + "(";
                        code += ") { let o = __p.__offset(";
                        code += NumToString(field.value.offset) +"); ";
                        code += "    if o != 0 {\n    var obj = " + obj + "; return obj.__assign(";
                        code += "__p.";
                        code += "__indirect(__p.__vector(o)), ";
                        code += "__p.bb) } else { return nil } }\n";
                    }
                    // Generate mutators for scalar fields or vectors of scalars.
                    if (parser_.opts.mutable_buffer) {
                        auto underlying_type = field.value.type.base_type == BASE_TYPE_VECTOR
                        ? field.value.type.VectorType()
                        : field.value.type;
                        // Boolean parameters have to be explicitly converted to byte
                        // representation.
                        auto setter_parameter = underlying_type.base_type == BASE_TYPE_BOOL
                        ? "(byte)(" + field.name + " ? 1 : 0)"
                        : field.name;
                        auto mutator_prefix = MakeCamel("mutate", false);
                        // A vector mutator also needs the index of the vector element it should
                        // mutate.
                        auto mutator_params = (field.value.type.base_type == BASE_TYPE_VECTOR
                                               ? "(int j, "
                                               : "(") + GenTypeNameDest(underlying_type) + " " + field.name + ") { ";
                        auto setter_index = field.value.type.base_type == BASE_TYPE_VECTOR
                        ? "__p.__vector(o) + j * " +
                        NumToString(InlineSize(underlying_type))
                        : (struct_def.fixed
                           ? "__p.bb_pos + " +
                           NumToString(field.value.offset)
                           : "o + __p.bb_pos");
                        if (IsScalar(field.value.type.base_type) ||
                            (field.value.type.base_type == BASE_TYPE_VECTOR &&
                             IsScalar(field.value.type.VectorType().base_type))) {
                                code += "  public ";
                                code += struct_def.fixed ? "void " : "Bool";
                                code += mutator_prefix + MakeCamel(field.name, true);
                                code += mutator_params;
                                if (struct_def.fixed) {
                                    code += GenSetter(underlying_type) + "(" + setter_index + ", ";
                                    code += src_cast + setter_parameter + "); }\n";
                                } else {
                                    code += "let o = __p.__offset(";
                                    code += NumToString(field.value.offset) + ");";
                                    code += " if (o != 0) { " + GenSetter(underlying_type);
                                    code += "(" + setter_index + ", " + src_cast + setter_parameter +
                                    "); return true; } else { return false; } }\n";
                                }
                            }
                    }
                }
                code += "\n";
                flatbuffers::FieldDef *key_field = nullptr;
                if (struct_def.fixed) {
                    // create a struct constructor function
                    code += "  public static func ";
                    code += FunctionStart('C') + "reate";
                    code += struct_def.name + "(_ builder: FlatBufferBuilder";
                    GenStructArgs(struct_def, code_ptr, "");
                    code += ") -> " + GenOffsetType(struct_def) + " {\n";
                    GenStructBody(struct_def, code_ptr, "");
                    code += "    return ";
                    code += GenOffsetConstruct(struct_def,
                                               "builder." + std::string("offset"));
                    code += ";\n  }\n";
                } else {
                    // Generate a method that creates a table in one go. This is only possible
                    // when the table has no struct fields, since those have to be created
                    // inline, and there's no way to do so in Java.
                    bool has_no_struct_fields = true;
                    int num_fields = 0;
                    for (auto it = struct_def.fields.vec.begin();
                         it != struct_def.fields.vec.end(); ++it) {
                        auto &field = **it;
                        if (field.deprecated) continue;
                        if (IsStruct(field.value.type)) {
                            has_no_struct_fields = false;
                        } else {
                            num_fields++;
                        }
                    }
                    if (has_no_struct_fields && num_fields) {
                        // Generate a table constructor of the form:
                        // public static int createName(FlatBufferBuilder builder, args...)
                        code += "  public static func ";
                        code += FunctionStart('C') + "reate" + struct_def.name;
                        code += "(_ builder: FlatBufferBuilder";
                        for (auto it = struct_def.fields.vec.begin();
                             it != struct_def.fields.vec.end(); ++it) {
                            auto &field = **it;
                            if (field.deprecated) continue;
                            code += ",\n      ";
                            code += MakeCamel(field.name, false);
                            if (!IsScalar(field.value.type.base_type)) code += "Offset";
                            code += ": ";
                            code += GenTypeBasic(DestinationType(field.value.type));
                            
                                code += " = ";
                                code += GenDefaultValueBasic(field.value);
                        }
                        code += ") -> " + GenOffsetType(struct_def) + " {\n    builder.";
                        code += FunctionStart('S') + "tartObject(numFields: ";
                        code += NumToString(struct_def.fields.vec.size()) + ");\n";
                        for (size_t size = struct_def.sortbysize ? sizeof(largest_scalar_t) : 1;
                             size;
                             size /= 2) {
                            for (auto it = struct_def.fields.vec.rbegin();
                                 it != struct_def.fields.vec.rend(); ++it) {
                                auto &field = **it;
                                if (!field.deprecated &&
                                    (!struct_def.sortbysize ||
                                     size == SizeOf(field.value.type.base_type))) {
                                        code += "    " + struct_def.name + ".";
                                        code += FunctionStart('A') + "dd";
                                        auto name = MakeCamel(field.name, false);
                                        if (!IsScalar(field.value.type.base_type)) name += "Offset";
                                        code += MakeCamel(field.name) + "(builder, " + name + ": " + name;
                                        
                                        code += ");\n";
                                    }
                            }
                        }
                        code += "    return " + struct_def.name + ".";
                        code += FunctionStart('E') + "nd" + struct_def.name;
                        code += "(builder);\n  }\n\n";
                    }
                    // Generate a set of static methods that allow table construction,
                    // of the form:
                    // public static void addName(FlatBufferBuilder builder, short name)
                    // { builder.addShort(id, name, default); }
                    // Unlike the Create function, these always work.
                    code += "  public static func " + FunctionStart('S') + "tart";
                    code += struct_def.name;
                    code += "(_ builder: FlatBufferBuilder) { builder.";
                    code += FunctionStart('S') + "tartObject(numFields: ";
                    code += NumToString(struct_def.fields.vec.size()) + "); }\n";
                    for (auto it = struct_def.fields.vec.begin();
                         it != struct_def.fields.vec.end(); ++it) {
                        auto &field = **it;
                        if (field.deprecated) continue;
                        if (field.key) key_field = &field;
                        code += "  public static func " + FunctionStart('A') + "dd";
                        code += MakeCamel(field.name);
                        code += "(_ builder: FlatBufferBuilder, ";
                        auto argname = MakeCamel(field.name, false);
                        if (!IsScalar(field.value.type.base_type)) argname += "Offset";
                        code +=  argname + ": " + GenTypeBasic(DestinationType(field.value.type)) + ""
                        ") { builder." + FunctionStart('A') + "dd";
                        code += GenMethod(field.value.type) + "(";
                        code += NumToString(it - struct_def.fields.vec.begin()) + ", ";
                        code += SourceCastBasic(field.value.type);
                        std::string passedArg = argname;
                        if (!IsScalar(field.value.type.base_type) &&
                            field.value.type.base_type != BASE_TYPE_UNION) {
                            passedArg += ".value";
                        } else if (field.value.type.enum_def != nullptr) {
                            if (IsScalar(field.value.type.base_type)) {
                                passedArg += ".rawValue";
                            } else {
                                passedArg = "Int(" + passedArg + ")";
                            }
                        }
                        code += passedArg;
                        code += ", ";
                        code += GenDefaultValue(field.value, false);
                        code += "); }\n";
                        if (field.value.type.base_type == BASE_TYPE_VECTOR) {
                            auto vector_type = field.value.type.VectorType();
                            auto alignment = InlineAlignment(vector_type);
                            auto elem_size = InlineSize(vector_type);
                            if (!IsStruct(vector_type)) {
                                // Generate a method to create a vector from a Java array.
                                code += "  public static func ";
                                code += FunctionStart('C') + "reate";
                                code += MakeCamel(field.name);
                                code += "Vector(_ builder: FlatBufferBuilder, data: [";
                                code += GenTypeBasic(vector_type) + "]) -> " + GenVectorOffsetType();
                                code += " { builder." + FunctionStart('S') + "tartVector(elemSize: ";
                                code += NumToString(elem_size);
                                code += ", count: data.count, alignment: ";
                                code += NumToString(alignment);
                                code += "); for i in (0..<data.count).reversed() { builder.add";
                                code += GenMethod(vector_type);
                                code += "(";
                                code += SourceCastBasic(vector_type, false);
                                code += "data[i]";
                                if ((vector_type.base_type == BASE_TYPE_STRUCT ||
                                     vector_type.base_type == BASE_TYPE_STRING))
                                    code += ".value";
                                code += "); }; return ";
                                code += "builder." + FunctionStart('E') + "ndVector(); }\n";
                            }
                            // Generate a method to start a vector, data to be added manually after.
                            code += "  public static func " + FunctionStart('S') + "tart";
                            code += MakeCamel(field.name);
                            code += "Vector(_ builder: FlatBufferBuilder, numElems: Int) ";
                            code += "{ builder." + FunctionStart('S') + "tartVector(elemSize: ";
                            code += NumToString(elem_size);
                            code += ", count: numElems, alignment: " + NumToString(alignment);
                            code += "); }\n";
                        }
                    }
                    code += "  public static func ";
                    code += FunctionStart('E') + "nd" + struct_def.name;
                    code += "(_ builder: FlatBufferBuilder) -> " + GenOffsetType(struct_def) + " {\n    let o = builder.";
                    code += FunctionStart('E') + "ndObject();\n";
                    for (auto it = struct_def.fields.vec.begin();
                         it != struct_def.fields.vec.end();
                         ++it) {
                        auto &field = **it;
                        if (!field.deprecated && field.required) {
                            code += "    builder.`" + FunctionStart('R') + "equired`(o, ";
                            code += NumToString(field.value.offset);
                            code += ");  // " + field.name + "\n";
                        }
                    }
                    code += "    return " + GenOffsetConstruct(struct_def, "o") + ";\n  }\n";
                    if (parser_.root_struct_def_ == &struct_def) {
                        code += "  public static func ";
                        code += FunctionStart('F') + "inish" + struct_def.name;
                        code += "Buffer(_ builder: FlatBufferBuilder, _ offset: " + GenOffsetType(struct_def);
                        code += ") {";
                        code += " builder." + FunctionStart('F') + "inish(rootTable: offset.value";
                        if (parser_.file_identifier_.length())
                            code += ", fileIdentifier: \"" + parser_.file_identifier_ + "\"";
                        code += "); }\n";
                    }
                }
                if (struct_def.has_key) {

                        code += "\n  public static func ";
                        code += "createMySortedVectorOfTables(_ builder: FlatBufferBuilder, ";
                        code += "_ offsets: inout [Offset<" + struct_def.name + ">";
                        code += "]) -> VectorOffset {\n";
                        code += "    offsets.sort { (o1, o2) in\n " + GenKeyGetter(key_field);
                        code += " < 0; }\n";
                        code += "    return builder.createVectorOfTables(offsets);\n  }\n";
                    
                    code += "\n  public static func";
                    code += " " + FunctionStart('L') + "ookupByKey(vectorOffset: " + GenVectorOffsetType();
                    code += ", key: " + GenTypeNameDest(key_field->value.type);
                    code += ", _ bb: ByteBuffer) -> " + struct_def.name + "? {\n";
                    if (key_field->value.type.base_type == BASE_TYPE_STRING) {
                        code += "    let byteKey = key.utf8CString\n";
                    code += "    var vectorLocation = " + GenByteBufferLength("bb");
                    code += " - vectorOffset";
                    code += ".value";
                    code += ";\n    var span = ";
                    code += "Int(bb." + FunctionStart('G') + "etInt32(at: vectorLocation));\n";
                    code += "    var start = 0;\n";
                    code += "    vectorLocation += 4;\n";
                    code += "    while (span != 0) {\n";
                    code += "      var middle = span / 2;\n";
                    code += GenLookupKeyGetter(key_field);
                    code += "      if (comp > 0) {\n";
                    code += "        span = middle;\n";
                    code += "      } else if (comp < 0) {\n";
                    code += "        middle += 1;\n";
                    code += "        start += middle;\n";
                    code += "        span -= middle;\n";
                    code += "      } else {\n";
                    code += "        var obj = " + struct_def.name + "();\n";
                    code += "        return obj";
                    code += ".__assign(tableOffset, bb);\n";
                    code += "      }\n    }\n";
                    code += "    return nil;\n";
                    code += "  }\n";
                }
            }
                
                code += "}";
                code += "\n\n";
            }
        };
    }  // namespace swift
    
    
    bool GenerateSwift(const Parser &parser, const std::string &path,
                    const std::string &file_name) {
        swift::SwiftGenerator generator(parser, path, file_name);
        return generator.generate();
    }
    
}  // namespace FlatBuffers
