// Copyright 2010 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License"); you
// may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
// implied. See the License for the specific language governing
// permissions and limitations under the License.

#include "protobuf_v8.h"

#include <map>
#include <string>
#include <iostream>
#include <sstream>

#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/descriptor.pb.h>

#include "logging.h"
#include "util.h"

#include "node_buffer.h"
#include "node_object_wrap.h"

#include "node_util.h"

//#define PROTOBUF_V8_DEBUG
#ifdef  PROTOBUF_V8_DEBUG

#define DBG(...) LOGD(__VA_ARGS__)

#else

#define DBG(...)

#endif

using google::protobuf::Descriptor;
using google::protobuf::DescriptorPool;
using google::protobuf::DynamicMessageFactory;
using google::protobuf::FieldDescriptor;
using google::protobuf::FileDescriptorSet;
using google::protobuf::Message;
using google::protobuf::Reflection;

//using ObjectWrap;
//using Buffer;

using std::map;
using std::string;

using v8::Array;
using v8::AccessorInfo;
using v8::Arguments;
using v8::Boolean;
using v8::Context;
using v8::External;
using v8::Function;
using v8::FunctionTemplate;
using v8::Integer;
using v8::Handle;
using v8::HandleScope;
using v8::InvocationCallback;
using v8::Local;
using v8::NamedPropertyGetter;
using v8::Number;
using v8::Object;
using v8::ObjectTemplate;
using v8::Persistent;
using v8::Script;
using v8::String;
using v8::Value;
using v8::V8;

namespace protobuf_v8 {

  template <typename T>
  static T* UnwrapThis(const Arguments& args) {
    return ObjectWrap::Unwrap<T>(args.This());
  }

  template <typename T>
  static T* UnwrapThis(const AccessorInfo& args) {
    return ObjectWrap::Unwrap<T>(args.This());
  }

  Persistent<FunctionTemplate> SchemaTemplate;
  Persistent<FunctionTemplate> TypeTemplate;
  Persistent<FunctionTemplate> ParseTemplate;
  Persistent<FunctionTemplate> SerializeTemplate;

  class Schema : public ObjectWrap {
  public:
    Schema(Handle<Object> self, const DescriptorPool* pool)
        : pool_(pool) {
      DBG("Schema::Schema E:");
      factory_.SetDelegateToGeneratedFactory(true);
      self->SetInternalField(1, Array::New());
      Wrap(self);
      DBG("Schema::Schema X:");
    }

    virtual ~Schema() {
      DBG("~Schema::Schema E:");
      if (pool_ != DescriptorPool::generated_pool())
        delete pool_;
      DBG("~Schema::Schema X:");
    }

    class Type : public ObjectWrap {
    public:
      Schema* schema_;
      const Descriptor* descriptor_;

      Message* NewMessage() const {
        DBG("Type::NewMessage() EX:");
        return schema_->NewMessage(descriptor_);
      }

      Handle<Function> Constructor() const {
        DBG("Type::Constrocutor() EX:");
        return handle_->GetInternalField(2).As<Function>();
      }

      Local<Object> NewObject(Handle<Value> properties) const {
        DBG("Type::NewObjext(properties) EX:");
        return Constructor()->NewInstance(1, &properties);
      }

      Type(Schema* schema, const Descriptor* descriptor, Handle<Object> self)
        : schema_(schema), descriptor_(descriptor) {
        DBG("Type::Type(schema, descriptor, self) E:");
        // Generate functions for bulk conversion between a JS object
        // and an array in descriptor order:
        //   from = function(arr) { this.f0 = arr[0]; this.f1 = arr[1]; ... }
        //   to   = function()    { return [ this.f0, this.f1, ... ] }
        // This is faster than repeatedly calling Get/Set on a v8::Object.
        std::ostringstream from, to;
        from << "(function(arr) { if(arr) {";
        to << "(function() { return [ ";

        for (int i = 0; i < descriptor->field_count(); i++) {
          from <<
            "var x = arr[" << i << "]; "
            "if(x !== undefined) this['" <<
            descriptor->field(i)->camelcase_name() <<
            "'] = x; ";

          if (i > 0) to << ", ";
          to << "this['" << descriptor->field(i)->camelcase_name() << "']";
          DBG("field name=%s", descriptor->field(i)->name().c_str());
        }

        from << " }})";
        to << " ]; })";

        // managed type->schema link
        self->SetInternalField(1, schema_->handle_);

        Handle<Function> constructor =
          Script::Compile(String::New(from.str().c_str()))->Run().As<Function>();
        constructor->SetHiddenValue(String::New("type"), self);

        Handle<Function> bind =
          Script::Compile(String::New(
              "(function(self) {"
              "  var f = this;"
              "  return function(arg) {"
              "    return f.call(self, arg);"
              "  };"
              "})"))->Run().As<Function>();
        Handle<Value> arg = self;
        constructor->Set(String::New("parse"), bind->Call(ParseTemplate->GetFunction(), 1, &arg));
        constructor->Set(String::New("serialize"), bind->Call(SerializeTemplate->GetFunction(), 1, &arg));
        self->SetInternalField(2, constructor);
        self->SetInternalField(3, Script::Compile(String::New(to.str().c_str()))->Run());

        Wrap(self);
        DBG("Type::Type(schema, descriptor, self) X:");
      }

#define GET(TYPE)                                                        \
      (index >= 0 ?                                                      \
       reflection->GetRepeated##TYPE(instance, field, index) :           \
       reflection->Get##TYPE(instance, field))

      static Handle<Value> ToJs(const Message& instance,
                                const Reflection* reflection,
                                const FieldDescriptor* field,
                                const Type* message_type,
                                int index) {
        DBG("Type::ToJs(instance, refelction, field, message_type) E:");
        switch (field->cpp_type()) {
        case FieldDescriptor::CPPTYPE_MESSAGE:
          DBG("Type::ToJs CPPTYPE_MESSAGE");
          return message_type->ToJs(GET(Message));
        case FieldDescriptor::CPPTYPE_STRING: {
          DBG("Type::ToJs CPPTYPE_STRING");
          const string& value = GET(String);
          return String::New(value.data(), value.length());
        }
        case FieldDescriptor::CPPTYPE_INT32:
          DBG("Type::ToJs CPPTYPE_INT32");
          return Integer::New(GET(Int32));
        case FieldDescriptor::CPPTYPE_UINT32:
          DBG("Type::ToJs CPPTYPE_UINT32");
          return Integer::NewFromUnsigned(GET(UInt32));
        case FieldDescriptor::CPPTYPE_INT64:
          DBG("Type::ToJs CPPTYPE_INT64");
          return Number::New(GET(Int64));
        case FieldDescriptor::CPPTYPE_UINT64:
          DBG("Type::ToJs CPPTYPE_UINT64");
          return Number::New(GET(UInt64));
        case FieldDescriptor::CPPTYPE_FLOAT:
          DBG("Type::ToJs CPPTYPE_FLOAT");
          return Number::New(GET(Float));
        case FieldDescriptor::CPPTYPE_DOUBLE:
          DBG("Type::ToJs CPPTYPE_DOUBLE");
          return Number::New(GET(Double));
        case FieldDescriptor::CPPTYPE_BOOL:
          DBG("Type::ToJs CPPTYPE_BOOL");
          return Boolean::New(GET(Bool));
        case FieldDescriptor::CPPTYPE_ENUM:
          DBG("Type::ToJs CPPTYPE_ENUM");
          return String::New(GET(Enum)->name().c_str());
        }

        return Handle<Value>();  // NOTREACHED
      }
#undef GET

      Handle<Object> ToJs(const Message& instance) const {
        DBG("Type::ToJs(Message) E:");
        const Reflection* reflection = instance.GetReflection();
        const Descriptor* descriptor = instance.GetDescriptor();

        Handle<Array> properties = Array::New(descriptor->field_count());
        for (int i = 0; i < descriptor->field_count(); i++) {
          HandleScope scope;

          const FieldDescriptor* field = descriptor->field(i);
          bool repeated = field->is_repeated();
          if (repeated && !reflection->FieldSize(instance, field)) {
            DBG("Ignore repeated field with no size in reflection data");
            continue;
          }
          if (!repeated && !reflection->HasField(instance, field)) {
            DBG("Ignore field with no field in relfection data");
            continue;
          }

          const Type* child_type =
            (field->cpp_type() == FieldDescriptor::CPPTYPE_MESSAGE) ?
            schema_->GetType(field->message_type()) : NULL;

          Handle<Value> value;
          if (field->is_repeated()) {
            int size = reflection->FieldSize(instance, field);
            Handle<Array> array = Array::New(size);
            for (int j = 0; j < size; j++) {
              array->Set(j, ToJs(instance, reflection, field, child_type, j));
            }
            value = array;
          } else {
            value = ToJs(instance, reflection, field, child_type, -1);
          }

          DBG("Type::ToJs: set property[%d]=%s", i, ToCString(value));
          properties->Set(i, value);
        }

        DBG("Type::ToJs(Message) X:");
        return NewObject(properties);
      }

      static Handle<Value> Parse(const Arguments& args) {
        DBG("Type::Parse(args) E:");
        Type* type = UnwrapThis<Type>(args);
        Buffer* buf = ObjectWrap::Unwrap<Buffer>(args[0]->ToObject());

        Message* message = type->NewMessage();
        message->ParseFromArray(buf->data(), buf->length());
        Handle<Object> result = type->ToJs(*message);
        delete message;

        DBG("Type::Parse(args) X:");
        return result;
      }

#define SET(TYPE, EXPR)                                                 \
      if (repeated) reflection->Add##TYPE(instance, field, EXPR);       \
      else reflection->Set##TYPE(instance, field, EXPR)

      static bool ToProto(Message* instance,
                          const FieldDescriptor* field,
                          Handle<Value> value,
                          const Type* type,
                          bool repeated) {
        DBG("Type::ToProto(instance, field, value, type, repeated) E:");
        bool ok = true;
        HandleScope scope;

        DBG("Type::ToProto field->name()=%s", field->name().c_str());
        const Reflection* reflection = instance->GetReflection();
        switch (field->cpp_type()) {
        case FieldDescriptor::CPPTYPE_MESSAGE:
          DBG("Type::ToProto CPPTYPE_MESSAGE");
          ok = type->ToProto(repeated ?
                                reflection->AddMessage(instance, field) :
                                reflection->MutableMessage(instance, field),
                                    value.As<Object>());
          break;
        case FieldDescriptor::CPPTYPE_STRING: {
          DBG("Type::ToProto CPPTYPE_STRING");
          String::AsciiValue ascii(value);
          SET(String, string(*ascii, ascii.length()));
          break;
        }
        case FieldDescriptor::CPPTYPE_INT32:
          DBG("Type::ToProto CPPTYPE_INT32");
          SET(Int32, value->NumberValue());
          break;
        case FieldDescriptor::CPPTYPE_UINT32:
          DBG("Type::ToProto CPPTYPE_UINT32");
          SET(UInt32, value->NumberValue());
          break;
        case FieldDescriptor::CPPTYPE_INT64:
          DBG("Type::ToProto CPPTYPE_INT64");
          SET(Int64, value->NumberValue());
          break;
        case FieldDescriptor::CPPTYPE_UINT64:
          DBG("Type::ToProto CPPTYPE_UINT64");
          SET(UInt64, value->NumberValue());
          break;
        case FieldDescriptor::CPPTYPE_FLOAT:
          DBG("Type::ToProto CPPTYPE_FLOAT");
          SET(Float, value->NumberValue());
          break;
        case FieldDescriptor::CPPTYPE_DOUBLE:
          DBG("Type::ToProto CPPTYPE_DOUBLE");
          SET(Double, value->NumberValue());
          break;
        case FieldDescriptor::CPPTYPE_BOOL:
          DBG("Type::ToProto CPPTYPE_BOOL");
          SET(Bool, value->BooleanValue());
          break;
        case FieldDescriptor::CPPTYPE_ENUM:
          DBG("Type::ToProto CPPTYPE_ENUM");

          // Don't use SET as vd can be NULL
          char error_buff[256];
          const google::protobuf::EnumValueDescriptor* vd;
          int i32_value = 0;
          const char *str_value = NULL;
          const google::protobuf::EnumDescriptor* ed = field->enum_type();

          if (value->IsNumber()) {
            i32_value = value->Int32Value();
            vd = ed->FindValueByNumber(i32_value);
            if (vd == NULL) {
              snprintf(error_buff, sizeof(error_buff),
                  "Type::ToProto Bad enum value, %d is not a member of enum %s",
                      i32_value, ed->full_name().c_str());
            }
          } else {
            str_value = ToCString(value);
            // TODO: Why can str_value be corrupted sometimes?
            LOGD("str_value=%s", str_value);
            vd = ed->FindValueByName(str_value);
            if (vd == NULL) {
              snprintf(error_buff, sizeof(error_buff),
                  "Type::ToProto Bad enum value, %s is not a member of enum %s",
                      str_value, ed->full_name().c_str());
            }
          }
          if (vd != NULL) {
            if (repeated) {
               reflection->AddEnum(instance, field, vd);
            } else {
               reflection->SetEnum(instance, field, vd);
            }
          } else {
            v8::ThrowException(String::New(error_buff));
            ok = false;
          }
          break;
        }
        DBG("Type::ToProto(instance, field, value, type, repeated) X: ok=%d", ok);
        return ok;
      }
#undef SET

      bool ToProto(Message* instance, Handle<Object> src) const {
        DBG("ToProto(Message *, Handle<Object>) E:");

        Handle<Function> to_array = handle_->GetInternalField(3).As<Function>();
        Handle<Array> properties = to_array->Call(src, 0, NULL).As<Array>();
        bool ok = true;
        for (int i = 0; ok && (i < descriptor_->field_count()); i++) {
          Handle<Value> value = properties->Get(i);
          if (value->IsUndefined()) continue;

          const FieldDescriptor* field = descriptor_->field(i);
          const Type* child_type =
            (field->cpp_type() == FieldDescriptor::CPPTYPE_MESSAGE) ?
            schema_->GetType(field->message_type()) : NULL;
          if (field->is_repeated()) {
            if(!value->IsArray()) {
              ok = ToProto(instance, field, value, child_type, true);
            } else {
              Handle<Array> array = value.As<Array>();
              int length = array->Length();
              for (int j = 0; ok && (j < length); j++) {
                ok = ToProto(instance, field, array->Get(j), child_type, true);
              }
            }
          } else {
            ok = ToProto(instance, field, value, child_type, false);
          }
        }
        DBG("ToProto(Message *, Handle<Object>) X: ok=%d", ok);
        return ok;
      }

      static Handle<Value> Serialize(const Arguments& args) {
        Handle<Value> result;
        DBG("Serialize(Arguments&) E:");
        if (!args[0]->IsObject()) {
          DBG("Serialize(Arguments&) X: not an object");
          return v8::ThrowException(args[0]);
        }

        Type* type = UnwrapThis<Type>(args);
        Message* message = type->NewMessage();
        if (type->ToProto(message, args[0].As<Object>())) {
          int length = message->ByteSize();
          Buffer* buffer = Buffer::New(length);
          message->SerializeWithCachedSizesToArray((google::protobuf::uint8*)buffer->data());
          delete message;

          result = buffer->handle_;
        } else {
          result = v8::Undefined();
        }
        DBG("Serialize(Arguments&) X");
        return result;
      }

      static Handle<Value> ToString(const Arguments& args) {
        return String::New(UnwrapThis<Type>(args)->descriptor_->full_name().c_str());
      }
    };

    Message* NewMessage(const Descriptor* descriptor) {
      DBG("Schema::NewMessage(descriptor) EX:");
      return factory_.GetPrototype(descriptor)->New();
    }

    Type* GetType(const Descriptor* descriptor) {
      DBG("Schema::GetType(descriptor) E:");
      Type* result = types_[descriptor];
      if (result) return result;

      result = types_[descriptor] =
        new Type(this, descriptor, TypeTemplate->GetFunction()->NewInstance());

      // managed schema->[type] link
      Handle<Array> types = handle_->GetInternalField(1).As<Array>();
      types->Set(types->Length(), result->handle_);
      DBG("Schema::GetType(descriptor) X:");
      return result;
    }

    const DescriptorPool* pool_;
    map<const Descriptor*, Type*> types_;
    DynamicMessageFactory factory_;

    static Handle<Value> GetType(const Local<String> name,
                                 const AccessorInfo& args) {
      DBG("Schema::GetType(name, args) E:");
      Schema* schema = UnwrapThis<Schema>(args);
      const Descriptor* descriptor =
        schema->pool_->FindMessageTypeByName(*String::AsciiValue(name));

      DBG("Schema::GetType(name, args) X:");
      return descriptor ?
        schema->GetType(descriptor)->Constructor() :
        Handle<Function>();
    }

    static Handle<Value> NewSchema(const Arguments& args) {
      DBG("Schema::NewSchema E: args.Length()=%d", args.Length());
      if (!args.Length()) {
        return (new Schema(args.This(),
                           DescriptorPool::generated_pool()))->handle_;
      }

      Buffer* buf = ObjectWrap::Unwrap<Buffer>(args[0]->ToObject());

      FileDescriptorSet descriptors;
      if (!descriptors.ParseFromArray(buf->data(), buf->length())) {
        DBG("Schema::NewSchema X: bad descriptor");
        return v8::ThrowException(String::New("Malformed descriptor"));
      }

      DescriptorPool* pool = new DescriptorPool;
      for (int i = 0; i < descriptors.file_size(); i++) {
        pool->BuildFile(descriptors.file(i));
      }

      DBG("Schema::NewSchema X");
      return (new Schema(args.This(), pool))->handle_;
    }
  };

  void Init() {
    DBG("Init E:");
    HandleScope handle_scope;

    TypeTemplate = Persistent<FunctionTemplate>::New(FunctionTemplate::New());
    TypeTemplate->SetClassName(String::New("Type"));
    // native self
    // owning schema (so GC can manage our lifecyle)
    // constructor
    // toArray
    TypeTemplate->InstanceTemplate()->SetInternalFieldCount(4);

    SchemaTemplate = Persistent<FunctionTemplate>::New(FunctionTemplate::New(Schema::NewSchema));
    SchemaTemplate->SetClassName(String::New("Schema"));
    // native self
    // array of types (so GC can manage our lifecyle)
    SchemaTemplate->InstanceTemplate()->SetInternalFieldCount(2);
    SchemaTemplate->InstanceTemplate()->SetNamedPropertyHandler(Schema::GetType);

    ParseTemplate = Persistent<FunctionTemplate>::New(FunctionTemplate::New(Schema::Type::Parse));
    SerializeTemplate = Persistent<FunctionTemplate>::New(FunctionTemplate::New(Schema::Type::Serialize));

    DBG("Init X:");
  }

}  // namespace protobuf_v8

extern "C" void SchemaObjectTemplateInit(Handle<ObjectTemplate> target) {
  DBG("SchemaObjectTemplateInit(target) EX:");
  target->Set(String::New("Schema"), protobuf_v8::SchemaTemplate);
}
