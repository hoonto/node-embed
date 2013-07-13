node-embed
===
# The Node Embedders Guide
### A guide for embedding Node.js within other applications.

Until this process gets some community blessing, please note that it represents one potential way of embedding Node.js, which may or may not be the "Right Way."  This readme will be updated as more information becomes available.

This guide is based on experience around the following projects.

* [plnode](https://github.com/hoonto/plnode) (based on v0.10.12).

(if you have embedded Node.js and would like to contribute your tips, tricks, or details, please submit pull request and I'll udpate this guide as well as add links to your projects here).

**Note**  This step is currently targeted at embedding Node.js using tools available on a Linux platform.  Alternative steps are not yet (but should be) provided for Visual Studio or mingw environments as well.

#### Step 1: Turn Node.js into a shared library:

First, fork and clone [Node.js](https://github.com/joyent/node)

in the local root of the cloned Node.js git repository, in common.gypi, add '-fPIC' argument in the cflags array. 

```
'cflags': [ '-fPIC', '-Wall', '-Wextra', '-Wno-unused-parameter', '-pthread', ],
```

and in node.gyp modify target type from executable to shared_library:

```
'type': 'shared_library',
```

**See** [How To Write Shared Libraries](http://www.akkadia.org/drepper/dsohowto.pdf) for more details.

Don't run make yet, there's more work yet to do!

#### Step 3: Modify Node.js to cooperate as a shared library 

Node.js has a src/node.cc file which provides a node::Start method which takes command-line arguments, builds the V8 context, and invokes libuv's uv_run.
However, your application may need to create and receive a V8 context first prior to uv_run invocation.  So the following code snippets show how to break 
out node::Start into two separate functions:

In src/node.h add the following:

```
NODE_EXTERN void buildContext(int argc, char *argv[], v8::Handle<v8::ObjectTemplate> globaltemplate, v8::Persistent<v8::Context> &contextref,  v8::Handle<v8::Object> &processref, char** &argvcopyref);
NODE_EXTERN int runContext(v8::Persistent<v8::Context> contextref, v8::Handle<v8::Object> processref, char** argvcopyref);
```

In src/node.cc add the following:

```
// Provides V8 context, process, and a copy of the argv's via the reference parameters contextref, processref, and argvcopyref respectively.
// The only reason processref and argvcopyref are used is to be able to clean them up when uv_run exits, there is probably a better mechanism for doing this.

void buildContext(int argc, char *argv[], Handle<ObjectTemplate> globaltemplate, Persistent<Context> &contextref, Handle<Object> &processref, char** &argvcopyref){

  // Hack aroung with the argv pointer. Used for process.title = "blah".
  argv = uv_setup_args(argc, argv);

  // Logic to duplicate argv as Init() modifies arguments
  // that are passed into it.
  char **argv_copy = copy_argv(argc, argv);

  // This needs to run *before* V8::Initialize()
  // Use copy here as to not modify the original argv:
  Init(argc, argv_copy);

  V8::Initialize();
  {

    //Locker locker;
    HandleScope handle_scope;

    // Create the one and only Context.
    Persistent<Context> context = Context::New(NULL, globaltemplate);
    Context::Scope context_scope(context);

    // Use original argv, as we're just copying values out of it.
    Handle<Object> process_l = SetupProcessObject(argc, argv);
    v8_typed_array::AttachBindings(context->Global());

    // Create all the objects, load modules, do everything.
    // so your next reading stop should be node::Load()!
    Load(process_l);
    contextref = context;
    processref = process_l;
    argvcopyref = argv_copy;
  }
}

// Invokes uv_run, must call buildContext first
// Takes the context, process, arnd argvcopy provided by a previous call to buildContext

int runContext(Persistent<Context> context, Handle<Object> process, char** argvcopy){
  // All our arguments are loaded. We've evaluated all of the scripts. We
  // might even have created TCP servers. Now we enter the main eventloop. If
  // there are no watchers on the loop (except for the ones that were
  // uv_unref'd) then this function exits. As long as there are active
  // watchers, it blocks.
  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  // Unfortunately I ran into problems with this section of code,
  // and have found I need to return0 to prevent the embedding application from terminating strangely
  // So for the time being:
  return 0;

  EmitExit(process);
  RunAtExit();

#ifndef NDEBUG
  context.Dispose();
#endif

#ifndef NDEBUG
  // Clean up. Not strictly necessary.
  V8::Dispose();
#endif  // NDEBUG

  // Clean up the copy:
  free(argvcopy);

  return 0;
}
```

Now you can:

```
./configure
make
```

And find a shared library here for Node:  ./out/Release/lib.target/libnode.so

#### Step 3: Invoke Node from your application

Modify the wrapping application to build the context and do anything it wants with V8 before libuv's uv_run is invoked. 

In your cloned Node repository, you'll find the Node.js includes in ./src and the V8 includes in ./deps/v8/include (./deps/v8/include/v8.h and ./src/node.h are likely the ones you will want to include in your application's src).

For example, you might have something like this in your application source code that uses Node to build the V8 context, provide it to you for future use, as well as invoke libuv's uv_run:
```
// From the root of the cloned node repository:
#include "v8.h"    // ./deps/v8/include/v8.h
#include "node.h"  // ./src/node.h


GetGlobalObjectTemplate(){
  static v8::Persistent<v8::ObjectTemplate>	global;

	if (global.IsEmpty())
	{
		v8::HandleScope				handle_scope;

		global = v8::Persistent<v8::ObjectTemplate>::New(v8::ObjectTemplate::New());
		// ERROR levels for elog
		global->Set(String::NewSymbol("DEBUG5"), Int32::New(DEBUG5));
		global->Set(String::NewSymbol("DEBUG4"), Int32::New(DEBUG4));
		global->Set(String::NewSymbol("DEBUG3"), Int32::New(DEBUG3));
		global->Set(String::NewSymbol("DEBUG2"), Int32::New(DEBUG2));
		global->Set(String::NewSymbol("DEBUG1"), Int32::New(DEBUG1));
		global->Set(String::NewSymbol("DEBUG"), Int32::New(DEBUG5));
		global->Set(String::NewSymbol("LOG"), Int32::New(LOG));
		global->Set(String::NewSymbol("INFO"), Int32::New(INFO));
		global->Set(String::NewSymbol("NOTICE"), Int32::New(NOTICE));
		global->Set(String::NewSymbol("WARNING"), Int32::New(WARNING));
		global->Set(String::NewSymbol("ERROR"), Int32::New(ERROR));

		v8::Handle<v8::ObjectTemplate>	plnode = ObjectTemplate::New();

		SetupPlv8Functions(plnode);
		plnode->Set(String::NewSymbol("version"), String::New(PLV8_VERSION));

		global->Set(String::NewSymbol("plnode"), plnode);
	}

	return global;
}


static v8::Persistent<v8::Context> GetNodeContext()
{
    v8::Persistent<v8::Context>	global_context;
    
    // As it is right now, we split the node::Start into node::buildContext and node::runContext
    // and so we throw in some random arguments so that we don't have to modify more in Node.js
    // source.  You may have a variable number of arguments some of which need to be passed to
    // Node.js, so you can create your argv here with arguments and initialize argc as shown below:
    
    char *argv[] = {"myapp", NULL};
    int argc = sizeof(argv) / sizeof(char*) - 1;

    // I'm not sure if this is all that necessary, but in order for Node to tear down successfully
    // without leakage, you'll need to hand processref and argvcopyref to runContext 
    // (yes this is hacky, submit pull request I'll clean this up, thanks!)
    
    v8::Handle<v8::Object> processref;
    char** argvcopyref;

		v8::HandleScope				handle_scope;
    
    // Create a template for the global object:
    static v8::Persistent<v8::ObjectTemplate>  globaltemplate;
  	globaltemplate = v8::Persistent<v8::ObjectTemplate>::New(v8::ObjectTemplate::New());
    
    // Add anything you want to the global template, which will be used when creating the V8 context
    // and you will have access to in javascript executed by V8.  This could be special C++ functions
    // defined in this file.
    // ...
    // Or do so in another function elsewhere, as plnode does:
    // v8::Handle<v8::ObjectTemplate>  globaltemplate = GetGlobalObjectTemplate();

    // Invoke node::buildContext with our argc, argv, globaltemplate, and references to our
    // context, process, and argvcopy variables that will be set by buildContext within Node:
    node::buildContext(argc, argv, globaltemplate, global_context, processref, argvcopyref);

    // Now, using v8 context provided through node::buildContext, you can run some functions:
    
		if (plnode_start_proc != NULL)
		{
			v8::Local<v8::Function>		func;

			v8::HandleScope			handle_scope;
			v8::Context::Scope		context_scope(global_context);
			TryCatch			try_catch;
			MemoryContext		ctx = CurrentMemoryContext;

			PG_TRY();
			{
				func = find_js_function_by_name(plnode_start_proc);
			}
			PG_CATCH();
			{
				ErrorData	   *edata;

				MemoryContextSwitchTo(ctx);
				edata = CopyErrorData();
				elog(WARNING, "failed to find js function %s", edata->message);
				FlushErrorState();
				FreeErrorData(edata);
			}
			PG_END_TRY();

			if (!func.IsEmpty())
			{
				Handle<v8::Value>	result =
					DoCall(func, global_context->Global(), 0, NULL);
				if (result.IsEmpty())
					throw js_error(try_catch);
			}
		}

#ifdef ENABLE_DEBUGGER_SUPPORT
		debug_message_context = v8::Persistent<v8::Context>::New(global_context);

		v8::Locker locker;

		v8::Debug::SetDebugMessageDispatchHandler(DispatchDebugMessages, true);

		v8::Debug::EnableAgent("plnode", plnode_debugger_port, false);
#endif  // ENABLE_DEBUGGER_SUPPORT
	}

    // And now invoke libuv's uv_run:

    node::runContext(global_context, processref, argvcopyref);

  	return global_context;
}

```


References:
===
* [Node itself](https://github.com/joyent/node)
* [PLV8 itself](https://code.google.com/p/plv8js/wiki/PLV8)
* [V8 Embedders Guide](https://developers.google.com/v8/embed)
* [PLV8 on PGXN](http://pgxn.org/dist/plv8/)
* [A comment that helped out in the beginning](http://comments.gmane.org/gmane.comp.lang.javascript.nodejs/48685)
* [A Node.js thread with respect to exceptions](http://logs.nodejs.org/libuv/2013-03-17)
* [Does v8 play well with native exceptions?](http://www.mail-archive.com/v8-users@googlegroups.com/msg00871.html)
* [To enable exceptions in gyp for Node](https://github.com/TooTallNate/node-gyp/issues/17)

Other interesting links:
===
* [plv8-jpath](https://github.com/adunstan/plv8-jpath)
* [PLV8 JSON Selectors](http://www.postgresonline.com/journal/archives/272-Using-PLV8-to-build-JSON-selectors.html)
* [jsonselect](http://jsonselect.org/#overview)
* [Postgres 9.3 beta 2 JSON accoutrements](http://www.postgresql.org/docs/9.3/static/functions-json.html)
* [libnode, not sure what ultimate purpose is, but interesting nonetheless](https://github.com/plenluno/libnode)
* [Jerry Sievert's PLV8 fork that sounds interesting](https://github.com/JerrySievert/plv8)

And possibly the coolest of all:
===
* [Postgres-XC 1.1 beta](http://postgres-xc.sourceforge.net/) is out and [why that is not only cool, but relevant](http://www.slideshare.net/stormdb_cloud_database/postgres-xc-askeyvaluestorevsmongodb)

