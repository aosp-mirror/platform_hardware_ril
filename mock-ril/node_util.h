/**
 * Contents of this file are snippets from node.h and node.cc
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
 */


#ifndef MOCK_RIL_NODE_UTIL_H_
#define MOCK_RIL_NODE_UTIL_H_

#include <v8.h>

enum encoding {ASCII, UTF8, BINARY};

enum encoding ParseEncoding(v8::Handle<v8::Value> encoding_v,
                            enum encoding _default = BINARY);

void FatalException(v8::TryCatch &try_catch);

v8::Local<v8::Value> Encode(const void *buf, size_t len,
                            enum encoding encoding = BINARY);

// returns bytes written.
ssize_t DecodeWrite(char *buf,
                    size_t buflen,
                    v8::Handle<v8::Value>,
                    enum encoding encoding = BINARY);

#define SET_PROTOTYPE_METHOD(templ, name, callback)                      \
do {                                                                     \
  v8::Local<v8::Signature> __callback##_SIG = v8::Signature::New(templ); \
  v8::Local<v8::FunctionTemplate> __callback##_TEM =                     \
    v8::FunctionTemplate::New(callback, v8::Handle<v8::Value>(),         \
                          __callback##_SIG);                             \
  templ->PrototypeTemplate()->Set(v8::String::NewSymbol(name),           \
                                  __callback##_TEM);                     \
} while (0)

#define SET_METHOD(obj, name, callback)                                  \
  obj->Set(v8::String::NewSymbol(name),                                  \
           v8::FunctionTemplate::New(callback)->GetFunction())


#endif  // MOCK_RIL_NODE_UTIL_H_
