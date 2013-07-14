#include "node.h"
#include "v8.h"
//#include <new>

#include <iostream>

using namespace v8;

int
main (void)
{   int a = 3;
    std::cout << "++a is: " << ++a << " and a++ is: " << a++ << std::endl;
 
    return 0; 
}

static v8::Local<v8::Context>
GetNodeContext()
{
    char *argv[] = {"plnode", NULL};
    int argc = sizeof(argv) / sizeof(char*) - 1;


    v8::Handle<v8::ObjectTemplate>  globaltemplate;
    v8::Handle<v8::Object> processref;
    v8::Local<v8::Context> contextref;
    char** argvcopyref;

    node::buildContext(argc, argv, globaltemplate, contextref, processref, argvcopyref);

	return contextref;
}
