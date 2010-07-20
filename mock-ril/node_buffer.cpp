/**
 * Copied from node_buffer.cc
 * see http://www.nodejs.org/
 *
 * Node's license follows:
 *
 * Copyright 2009, 2010 Ryan Lienhart Dahl. All rights reserved.
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "node_buffer.h"

#include <assert.h>
#include <stdlib.h> // malloc, free
#include <v8.h>

#include <string.h> // memcpy

#include <arpa/inet.h>  // htons, htonl

#include "logging.h"
#include "node_util.h"
#include "util.h"

//#define BUFFER_DEBUG
#ifdef  BUFFER_DEBUG

#define DBG(...) LOGD(__VA_ARGS__)

#else

#define DBG(...)

#endif

#define MIN(a,b) ((a) < (b) ? (a) : (b))

using namespace v8;

#define SLICE_ARGS(start_arg, end_arg)                                   \
  if (!start_arg->IsInt32() || !end_arg->IsInt32()) {                    \
    return ThrowException(Exception::TypeError(                          \
          v8::String::New("Bad argument.")));                            \
  }                                                                      \
  int32_t start = start_arg->Int32Value();                               \
  int32_t end = end_arg->Int32Value();                                   \
  if (start < 0 || end < 0) {                                            \
    return ThrowException(Exception::TypeError(                          \
          v8::String::New("Bad argument.")));                            \
  }                                                                      \
  if (!(start <= end)) {                                                 \
    return ThrowException(Exception::Error(                              \
          v8::String::New("Must have start <= end")));                   \
  }                                                                      \
  if ((size_t)end > parent->length_) {                                   \
    return ThrowException(Exception::Error(                              \
          v8::String::New("end cannot be longer than parent.length")));  \
  }

static Persistent<String> length_symbol;
static Persistent<String> chars_written_sym;
static Persistent<String> write_sym;
Persistent<FunctionTemplate> Buffer::constructor_template;


// Each javascript Buffer object is backed by a Blob object.
// the Blob is just a C-level chunk of bytes.
// It has a reference count.
struct Blob_ {
  unsigned int refs;
  size_t length;
  char *data;
};
typedef struct Blob_ Blob;


static inline Blob * blob_new(size_t length) {
  DBG("blob_new E");
  Blob * blob  = (Blob*) malloc(sizeof(Blob));
  if (!blob) return NULL;

  blob->data = (char*) malloc(length);
  if (!blob->data) {
    DBG("blob_new X no memory for data");
    free(blob);
    return NULL;
  }

  V8::AdjustAmountOfExternalAllocatedMemory(sizeof(Blob) + length);
  blob->length = length;
  blob->refs = 0;
  DBG("blob_new X");
  return blob;
}


static inline void blob_ref(Blob *blob) {
  blob->refs++;
}


static inline void blob_unref(Blob *blob) {
  assert(blob->refs > 0);
  if (--blob->refs == 0) {
    DBG("blob_unref == 0");
    //fprintf(stderr, "free %d bytes\n", blob->length);
    V8::AdjustAmountOfExternalAllocatedMemory(-(sizeof(Blob) + blob->length));
    free(blob->data);
    free(blob);
    DBG("blob_unref blob and its data freed");
  }
}

#if 0
// When someone calls buffer.asciiSlice, data is not copied. Instead V8
// references in the underlying Blob with this ExternalAsciiStringResource.
class AsciiSliceExt: public String::ExternalAsciiStringResource {
 friend class Buffer;
 public:
  AsciiSliceExt(Buffer *parent, size_t start, size_t end) {
    blob_ = parent->blob();
    blob_ref(blob_);

    assert(start <= end);
    length_ = end - start;
    assert(start + length_ <= parent->length());
    data_ = parent->data() + start;
  }


  ~AsciiSliceExt() {
    //fprintf(stderr, "free ascii slice (%d refs left)\n", blob_->refs);
    blob_unref(blob_);
  }


  const char* data() const { return data_; }
  size_t length() const { return length_; }

 private:
  const char *data_;
  size_t length_;
  Blob *blob_;
};
#endif

Buffer* Buffer::New(size_t size) {
  DBG("Buffer::New(size) E");
  HandleScope scope;

  Local<Value> arg = Integer::NewFromUnsigned(size);
  Local<Object> b = constructor_template->GetFunction()->NewInstance(1, &arg);

  DBG("Buffer::New(size) X");
  return ObjectWrap::Unwrap<Buffer>(b);
}


Handle<Value> Buffer::New(const Arguments &args) {
  DBG("Buffer::New(args) E");
  HandleScope scope;

  Buffer *buffer;
  if ((args.Length() == 0) || args[0]->IsInt32()) {
    size_t length = 0;
    if (args[0]->IsInt32()) {
      length = args[0]->Uint32Value();
    }
    buffer = new Buffer(length);
  } else if (args[0]->IsArray()) {
    Local<Array> a = Local<Array>::Cast(args[0]);
    buffer = new Buffer(a->Length());
    char *p = buffer->data();
    for (unsigned int i = 0; i < a->Length(); i++) {
      p[i] = a->Get(i)->Uint32Value();
    }
  } else if (args[0]->IsString()) {
    Local<String> s = args[0]->ToString();
    enum encoding e = ParseEncoding(args[1], UTF8);
    int length = e == UTF8 ? s->Utf8Length() : s->Length();
    buffer = new Buffer(length);
  } else if (Buffer::HasInstance(args[0]) && args.Length() > 2) {
    // var slice = new Buffer(buffer, 123, 130);
    // args: parent, start, end
    Buffer *parent = ObjectWrap::Unwrap<Buffer>(args[0]->ToObject());
    SLICE_ARGS(args[1], args[2])
    buffer = new Buffer(parent, start, end);
  } else {
    DBG("Buffer::New(args) X Bad argument");
    return ThrowException(Exception::TypeError(String::New("Bad argument")));
  }

  buffer->Wrap(args.This());
  args.This()->SetIndexedPropertiesToExternalArrayData(buffer->data(),
                                                       kExternalUnsignedByteArray,
                                                       buffer->length());
  args.This()->Set(length_symbol, Integer::New(buffer->length_));

  if (args[0]->IsString()) {
    if (write_sym.IsEmpty()) {
      write_sym = Persistent<String>::New(String::NewSymbol("write"));
    }

    Local<Value> write_v = args.This()->Get(write_sym);
    assert(write_v->IsFunction());
    Local<Function> write = Local<Function>::Cast(write_v);

    Local<Value> argv[2] = { args[0], args[1] };

    TryCatch try_catch;

    write->Call(args.This(), 2, argv);

    if (try_catch.HasCaught()) {
      ReportException(&try_catch);
    }
  }

  DBG("Buffer::New(args) X");
  return args.This();
}


Buffer::Buffer(size_t length) : ObjectWrap() {
  DBG("Buffer::Buffer(length) E");
  blob_ = blob_new(length);
  off_ = 0;
  length_ = length;

  blob_ref(blob_);

  V8::AdjustAmountOfExternalAllocatedMemory(sizeof(Buffer));
  DBG("Buffer::Buffer(length) X");
}


Buffer::Buffer(Buffer *parent, size_t start, size_t end) : ObjectWrap() {
  DBG("Buffer::Buffer(parent, start, end) E");
  blob_ = parent->blob_;
  assert(blob_->refs > 0);
  blob_ref(blob_);

  assert(start <= end);
  off_ = parent->off_ + start;
  length_ = end - start;
  assert(length_ <= parent->length_);

  V8::AdjustAmountOfExternalAllocatedMemory(sizeof(Buffer));
  DBG("Buffer::Buffer(parent, start, end) X");
}


Buffer::~Buffer() {
  DBG("Buffer::~Buffer() E");
  assert(blob_->refs > 0);
  //fprintf(stderr, "free buffer (%d refs left)\n", blob_->refs);
  blob_unref(blob_);
  V8::AdjustAmountOfExternalAllocatedMemory(-static_cast<long int>(sizeof(Buffer)));
  DBG("Buffer::~Buffer() X");
}


char* Buffer::data() {
  char *p = blob_->data + off_;
  DBG("Buffer::data() EX p=%p", p);
  return p;
}

void Buffer::NewBlob(size_t length) {
  DBG("Buffer::NewBlob(length) E");
  blob_unref(blob_);
  blob_ = blob_new(length);
  off_ = 0;
  length_ = length;

  blob_ref(blob_);

  V8::AdjustAmountOfExternalAllocatedMemory(sizeof(Buffer));
  DBG("Buffer::NewBlob(length) X");
}


Handle<Value> Buffer::BinarySlice(const Arguments &args) {
  DBG("Buffer::BinarySlice(args) E");
  HandleScope scope;
  Buffer *parent = ObjectWrap::Unwrap<Buffer>(args.This());
  SLICE_ARGS(args[0], args[1])

  const char *data = const_cast<char*>(parent->data() + start);
  //Local<String> string = String::New(data, end - start);

  Local<Value> b =  Encode(data, end - start, BINARY);

  DBG("Buffer::BinarySlice(args) X");
  return scope.Close(b);
}


Handle<Value> Buffer::AsciiSlice(const Arguments &args) {
  DBG("Buffer::AsciiSlice(args) E");
  HandleScope scope;
  Buffer *parent = ObjectWrap::Unwrap<Buffer>(args.This());
  SLICE_ARGS(args[0], args[1])

#if 0
  AsciiSliceExt *ext = new AsciiSliceExt(parent, start, end);
  Local<String> string = String::NewExternal(ext);
  // There should be at least two references to the blob now - the parent
  // and the slice.
  assert(parent->blob_->refs >= 2);
#endif

  const char *data = const_cast<char*>(parent->data() + start);
  Local<String> string = String::New(data, end - start);


  DBG("Buffer::AsciiSlice(args) X");
  return scope.Close(string);
}


Handle<Value> Buffer::Utf8Slice(const Arguments &args) {
  DBG("Buffer::Utf8Slice(args) E");
  HandleScope scope;
  Buffer *parent = ObjectWrap::Unwrap<Buffer>(args.This());
  SLICE_ARGS(args[0], args[1])
  const char *data = const_cast<char*>(parent->data() + start);
  Local<String> string = String::New(data, end - start);
  DBG("Buffer::Utf8Slice(args) X");
  return scope.Close(string);
}


Handle<Value> Buffer::Slice(const Arguments &args) {
  DBG("Buffer::Slice(args) E");
  HandleScope scope;
  Local<Value> argv[3] = { args.This(), args[0], args[1] };
  Local<Object> slice =
    constructor_template->GetFunction()->NewInstance(3, argv);
  DBG("Buffer::Slice(args) X");
  return scope.Close(slice);
}


// var bytesCopied = buffer.copy(target, targetStart, sourceStart, sourceEnd);
Handle<Value> Buffer::Copy(const Arguments &args) {
  DBG("Buffer::Copy(args) E");
  HandleScope scope;

  Buffer *source = ObjectWrap::Unwrap<Buffer>(args.This());

  if (!Buffer::HasInstance(args[0])) {
    DBG("Buffer::Copy(args) X arg[0] not buffer");
    return ThrowException(Exception::TypeError(String::New(
            "First arg should be a Buffer")));
  }

  Buffer *target = ObjectWrap::Unwrap<Buffer>(args[0]->ToObject());

  ssize_t target_start = args[1]->Int32Value();
  ssize_t source_start = args[2]->Int32Value();
  ssize_t source_end = args[3]->IsInt32() ? args[3]->Int32Value()
                                          : source->length();

  if (source_end < source_start) {
    DBG("Buffer::Copy(args) X end < start");
    return ThrowException(Exception::Error(String::New(
            "sourceEnd < sourceStart")));
  }

  if (target_start < 0 || ((size_t)target_start) > target->length()) {
    DBG("Buffer::Copy(args) X targetStart bad");
    return ThrowException(Exception::Error(String::New(
            "targetStart out of bounds")));
  }

  if (source_start < 0 || ((size_t)source_start) > source->length()) {
    DBG("Buffer::Copy(args) X base source start");
    return ThrowException(Exception::Error(String::New(
            "sourceStart out of bounds")));
  }

  if (source_end < 0 || ((size_t)source_end) > source->length()) {
    DBG("Buffer::Copy(args) X bad source");
    return ThrowException(Exception::Error(String::New(
            "sourceEnd out of bounds")));
  }

  ssize_t to_copy = MIN(source_end - source_start,
                        target->length() - target_start);

  memcpy((void*)(target->data() + target_start),
         (const void*)(source->data() + source_start),
         to_copy);

  DBG("Buffer::Copy(args) X");
  return scope.Close(Integer::New(to_copy));
}


// var charsWritten = buffer.utf8Write(string, offset);
Handle<Value> Buffer::Utf8Write(const Arguments &args) {
  DBG("Buffer::Utf8Write(args) X");
  HandleScope scope;
  Buffer *buffer = ObjectWrap::Unwrap<Buffer>(args.This());

  if (!args[0]->IsString()) {
  DBG("Buffer::Utf8Write(args) X arg[0] not string");
    return ThrowException(Exception::TypeError(String::New(
            "Argument must be a string")));
  }

  Local<String> s = args[0]->ToString();

  size_t offset = args[1]->Int32Value();

  if (offset >= buffer->length_) {
    DBG("Buffer::Utf8Write(args) X offset bad");
    return ThrowException(Exception::TypeError(String::New(
            "Offset is out of bounds")));
  }

  const char *p = buffer->data() + offset;

  int char_written;

  int written = s->WriteUtf8((char*)p,
                             buffer->length_ - offset,
                             &char_written,
                             String::HINT_MANY_WRITES_EXPECTED);

  constructor_template->GetFunction()->Set(chars_written_sym,
                                           Integer::New(char_written));

  if (written > 0 && p[written-1] == '\0') written--;

  DBG("Buffer::Utf8Write(args) X");
  return scope.Close(Integer::New(written));
}


// var charsWritten = buffer.asciiWrite(string, offset);
Handle<Value> Buffer::AsciiWrite(const Arguments &args) {
  DBG("Buffer::AsciiWrite(args) E");
  HandleScope scope;

  Buffer *buffer = ObjectWrap::Unwrap<Buffer>(args.This());

  if (!args[0]->IsString()) {
    DBG("Buffer::AsciiWrite(args) X arg[0] not string");
    return ThrowException(Exception::TypeError(String::New(
            "Argument must be a string")));
  }

  Local<String> s = args[0]->ToString();

  size_t offset = args[1]->Int32Value();

  if (offset >= buffer->length_) {
    DBG("Buffer::AsciiWrite(args) X bad offset");
    return ThrowException(Exception::TypeError(String::New(
            "Offset is out of bounds")));
  }

  const char *p = buffer->data() + offset;

  size_t towrite = MIN((unsigned long) s->Length(), buffer->length_ - offset);

  int written = s->WriteAscii((char*)p, 0, towrite, String::HINT_MANY_WRITES_EXPECTED);
  DBG("Buffer::AsciiWrite(args) X");
  return scope.Close(Integer::New(written));
}


Handle<Value> Buffer::BinaryWrite(const Arguments &args) {
  DBG("Buffer::BinaryWrite(args) E");
  HandleScope scope;

  Buffer *buffer = ObjectWrap::Unwrap<Buffer>(args.This());

  if (!args[0]->IsString()) {
    DBG("Buffer::BinaryWrite(args) X arg[0] not string");
    return ThrowException(Exception::TypeError(String::New(
            "Argument must be a string")));
  }

  Local<String> s = args[0]->ToString();

  size_t offset = args[1]->Int32Value();

  if (offset >= buffer->length_) {
    DBG("Buffer::BinaryWrite(args) X offset bad");
    return ThrowException(Exception::TypeError(String::New(
            "Offset is out of bounds")));
  }

  char *p = (char*)buffer->data() + offset;

  size_t towrite = MIN((unsigned long) s->Length(), buffer->length_ - offset);

  int written = DecodeWrite(p, towrite, s, BINARY);
  DBG("Buffer::BinaryWrite(args) X");
  return scope.Close(Integer::New(written));
}


// buffer.unpack(format, index);
// Starting at 'index', unpacks binary from the buffer into an array.
// 'format' is a string
//
//  FORMAT  RETURNS
//    N     uint32_t   a 32bit unsigned integer in network byte order
//    n     uint16_t   a 16bit unsigned integer in network byte order
//    o     uint8_t    a 8bit unsigned integer
Handle<Value> Buffer::Unpack(const Arguments &args) {
  DBG("Buffer::Unpack(args) E");
  HandleScope scope;
  Buffer *buffer = ObjectWrap::Unwrap<Buffer>(args.This());

  if (!args[0]->IsString()) {
    DBG("Buffer::Unpack(args) X arg[0] not string");
    return ThrowException(Exception::TypeError(String::New(
            "Argument must be a string")));
  }

  String::AsciiValue format(args[0]->ToString());
  uint32_t index = args[1]->Uint32Value();

#define OUT_OF_BOUNDS ThrowException(Exception::Error(String::New("Out of bounds")))

  Local<Array> array = Array::New(format.length());

  uint8_t  uint8;
  uint16_t uint16;
  uint32_t uint32;

  for (int i = 0; i < format.length(); i++) {
    switch ((*format)[i]) {
      // 32bit unsigned integer in network byte order
      case 'N':
        if (index + 3 >= buffer->length_) return OUT_OF_BOUNDS;
        uint32 = htonl(*(uint32_t*)(buffer->data() + index));
        array->Set(Integer::New(i), Integer::NewFromUnsigned(uint32));
        index += 4;
        break;

      // 16bit unsigned integer in network byte order
      case 'n':
        if (index + 1 >= buffer->length_) return OUT_OF_BOUNDS;
        uint16 = htons(*(uint16_t*)(buffer->data() + index));
        array->Set(Integer::New(i), Integer::NewFromUnsigned(uint16));
        index += 2;
        break;

      // a single octet, unsigned.
      case 'o':
        if (index >= buffer->length_) return OUT_OF_BOUNDS;
        uint8 = (uint8_t)buffer->data()[index];
        array->Set(Integer::New(i), Integer::NewFromUnsigned(uint8));
        index += 1;
        break;

      default:
        DBG("Buffer::Unpack(args) X unknown format character");
        return ThrowException(Exception::Error(
              String::New("Unknown format character")));
    }
  }

  DBG("Buffer::Unpack(args) X");
  return scope.Close(array);
}


// var nbytes = Buffer.byteLength("string", "utf8")
Handle<Value> Buffer::ByteLength(const Arguments &args) {
  DBG("Buffer::ByteLength(args) E");
  HandleScope scope;

  if (!args[0]->IsString()) {
    DBG("Buffer::ByteLength(args) X arg[0] not a string");
    return ThrowException(Exception::TypeError(String::New(
            "Argument must be a string")));
  }

  Local<String> s = args[0]->ToString();
  enum encoding e = ParseEncoding(args[1], UTF8);

  Local<Integer> length =
    Integer::New(e == UTF8 ? s->Utf8Length() : s->Length());

  DBG("Buffer::ByteLength(args) X");
  return scope.Close(length);
}

void Buffer::InitializeObjectTemplate(Handle<ObjectTemplate> target) {
  DBG("InitializeObjectTemplate(target) E:");
  HandleScope scope;

  length_symbol = Persistent<String>::New(String::NewSymbol("length"));
  chars_written_sym = Persistent<String>::New(String::NewSymbol("_charsWritten"));

  Local<FunctionTemplate> t = FunctionTemplate::New(Buffer::New);
  constructor_template = Persistent<FunctionTemplate>::New(t);
  constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
  constructor_template->SetClassName(String::NewSymbol("Buffer"));

  // copy free
  SET_PROTOTYPE_METHOD(constructor_template, "binarySlice", Buffer::BinarySlice);
  SET_PROTOTYPE_METHOD(constructor_template, "asciiSlice", Buffer::AsciiSlice);
  SET_PROTOTYPE_METHOD(constructor_template, "slice", Buffer::Slice);
  // TODO SET_PROTOTYPE_METHOD(t, "utf16Slice", Utf16Slice);
  // copy
  SET_PROTOTYPE_METHOD(constructor_template, "utf8Slice", Buffer::Utf8Slice);

  SET_PROTOTYPE_METHOD(constructor_template, "utf8Write", Buffer::Utf8Write);
  SET_PROTOTYPE_METHOD(constructor_template, "asciiWrite", Buffer::AsciiWrite);
  SET_PROTOTYPE_METHOD(constructor_template, "binaryWrite", Buffer::BinaryWrite);
  SET_PROTOTYPE_METHOD(constructor_template, "unpack", Buffer::Unpack);
  SET_PROTOTYPE_METHOD(constructor_template, "copy", Buffer::Copy);

  SET_PROTOTYPE_METHOD(constructor_template, "byteLength", Buffer::ByteLength);

  target->Set(String::NewSymbol("Buffer"), constructor_template);
  DBG("InitializeObjectTemplate(target) X:");
}
