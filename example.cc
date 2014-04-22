#include "node.h"
#include "v8.h"
//#include <new>

#include <iostream>

using namespace v8;



int main (int argc, char *argv[])
{
    v8::Handle<v8::ObjectTemplate>  globaltemplate;
    v8::Handle<v8::Object> processref;
    v8::Persistent<v8::Context> contextref;

    node::buildContext(argc, argv, globaltemplate, contextref, processref, argv);
    node::runContext(contextref,processref,argv);

    v8::Local<v8::Value> emit_v = processref->Get(String::New("emit"));
    assert(emit_v->IsFunction());
    v8::Local<v8::Function> emit = v8::Local<v8::Function>::Cast(emit_v);
    v8::Local<v8::Value> args[] = { v8::String::New("hello"), v8::Integer::New(0) };
    TryCatch try_catch;
    emit->Call(processref, 2, args);
    if (try_catch.HasCaught()) {
        node::FatalException(try_catch);
    }

    return 0;
}
