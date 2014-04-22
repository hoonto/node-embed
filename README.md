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

Fork and/or clone this repository and [Node.js](https://github.com/joyent/node):

```
[node@hip1 dev]$ git clone https://github.com/hoonto/node-embed.git
[node@hip1 dev]$ cd node-embed/
[node@hip1 node-embed]$ git clone https://github.com/joyent/node.git
[node@hip1 node-embed]$ cd node
[node@hip1 node]$ git checkout v0.10.12-release
```

In the local root of the cloned Node.js git repository, in common.gypi, add '-fPIC' argument in the cflags array for the architectures for which you are building.  For example, for Linux change:

```
      [ 'OS =="linux" or OS=="freebsd" or OS=="openbsd" or OS=="solaris"', {
         'cflags': [ '-Wall', '-Wextra', '-Wno-unused-parameter', '-pthread', ],
```

to:

```
      [ 'OS =="linux" or OS=="freebsd" or OS=="openbsd" or OS=="solaris"', {
         'cflags': [ '-fPIC', '-Wall', '-Wextra', '-Wno-unused-parameter', '-pthread', ],
```

And in node.gyp modify target type from executable to shared_library:

```
  'targets': [
    {
       'target_name': 'node',
       'type': 'executable',
```

to:

```
  'targets': [
    {
       'target_name': 'node',
       'type': 'shared_library',
```

**See** [How To Write Shared Libraries](http://www.akkadia.org/drepper/dsohowto.pdf) for all of the gory details.

Don't make, there's more work yet to do!

#### Step 3: Modify Node.js to expose useful methods to your application

Node.js expects out-of-the-box to be run as an executable with command-line arguments and such.  It has a src/node.cc file which provides a node::Start method which takes command-line arguments, builds the V8 context, and invokes libuv's uv_run among other things, all in one big inconvenient-to-you Start method.

So your application will likely want to add some things to the global object template and request that Node create and pass back a reference to the V8 context first prior to uv_run invocation.  The following code snippets show how to break out node::Start into two separate functions, such that you may do this:

NOTE: in the node-embed directory are copies of node.cc and node.h modified that you may copy rather than cutting pasting all of this here, so you can simply move node.cc and node.h in the node-embed directory to node/src directory

In src/node.h add the following under the <code>NODE_EXTERN int Start(int argc, char *argv[]);</code> declaration:

```
//...


// If using v0.10.12, check out plnode for the externs, 
// otherwise if using the current master branch of joynet/node as of 13-07-2013:
NODE_EXTERN void buildContext(int argc, char *argv[], v8::Handle<v8::ObjectTemplate> globaltemplate, v8::Local<v8::Context> &contextref,  v8::Handle<v8::Object> &processref, char** &argvcopyref);
NODE_EXTERN int runContext(v8::Persistent<v8::Context> contextref, v8::Handle<v8::Object> processref, char** argvcopyref);

//...
```

In src/node.cc (maybe below the implementation of node::Start) add the following (for current master branch, as of 13-07-2013, otherwise if using v0.10.12 check out the node.cc in the github.com/hoonto/plnode for an example):

```
//...
// Provides V8 context, process, and a copy of the argv's via the 
// reference parameters contextref, processref, and argvcopyref 
// respectively.
// The only reason processref and argvcopyref are used is to be able 
// to clean them up when uv_run exits, there is probably a better 
// mechanism for doing this.

void buildContext(int argc, char *argv[], Handle<ObjectTemplate> globaltemplate, Local<Context> &contextref, Handle<Object> &processref, char** &argvcopyref){  // Hack aroung with the argv pointer. Used for process.title = "blah".
  argv = uv_setup_args(argc, argv);

  // Logic to duplicate argv as Init() modifies arguments
  // that are passed into it.
  char **argv_copy = copy_argv(argc, argv);

  // This needs to run *before* V8::Initialize()
  // Use copy here as to not modify the original argv:
  Init(argc, argv_copy);

  V8::Initialize();
  {
    // Declare locker if it is not yet declared.
    // If this is previously declared in wrapped application
    // comment out next 1 line:
    Locker locker(node_isolate);
    HandleScope handle_scope(node_isolate);

    // Create the one and only Context.
    Local<Context> context = Context::New(node_isolate, NULL, globaltemplate);
    Context::Scope context_scope(context);

    binding_cache.Reset(node_isolate, Object::New());

    // Use original argv, as we're just copying values out of it.
    Local<Object> process_l = SetupProcessObject(argc, argv);

    // Create all the objects, load modules, do everything.
    // so your next reading stop should be node::Load()!
    Load(process_l);

    contextref = context;
    processref = process_l;
    argvcopyref = argv_copy;
  }
}

int runContext(Persistent<Context> context, Handle<Object> &process, char** argvcop
y){
  // All our arguments are loaded. We've evaluated all of the scripts. We
  // might even have created TCP servers. Now we enter the main eventloop. If
  // there are no watchers on the loop (except for the ones that were
  // uv_unref'd) then this function exits. As long as there are active
  // watchers, it blocks.
  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

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

//...
```

Now you can:

```
./configure
make
```

And find a shared library here for Node:  ./out/Release/lib.target/libnode.so
Make sure libnode.so is in your library path before compiling your application in which libnode should be embedded.


#### Step 3: Invoke Node from your application

Modify the wrapping application to call node::buildContext and node::runContext.

The application will be able to do things with V8 prior to invoking runContext which calls libuv's uv_run and will not return until the reference count is zero.

In your cloned Node repository, you'll find the Node.js includes in ./src and the V8 includes in ./deps/v8/include (./deps/v8/include/v8.h and ./src/node.h are likely the ones you will want to include in your application's src).

Again, be sure you have libnode.so in your ld path!
For the example, you might compile with g++:

```
g++ -I./node/src -I./node/deps/uv/include -I./node/deps/v8/include -lnode example.cc -o example
```


#### TODO:

Provide a sample app.  In the meantime, feel free to check out [plnode](https://github.com/hoonto/plnode).  Note that plnode uses v0.10.12 so that will have slightly different integration than the latest master branch.

References:
===
* [Node itself](https://github.com/joyent/node)
* [V8 Embedders Guide](https://developers.google.com/v8/embed)
* [A comment that helped out in the beginning](http://comments.gmane.org/gmane.comp.lang.javascript.nodejs/48685)
* [A Node.js thread with respect to exceptions](http://logs.nodejs.org/libuv/2013-03-17)
* [Does v8 play well with native exceptions?](http://www.mail-archive.com/v8-users@googlegroups.com/msg00871.html)
* [To enable exceptions in gyp for Node](https://github.com/TooTallNate/node-gyp/issues/17)

