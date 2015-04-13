This file describes how to use the or-tools java binary archive.

OR-Tools is located at http://code.google.com/p/or-tools

The binary archives can be found [here](https://drive.google.com/folderview?id=0B2yUSpEp04BNdEU4QW5US1hvTzg&usp=sharing) ([mirror](https://app.box.com/s/gmgtnfqgl8ibrz5d1cm0)).

This module has been tested under:
  * ubuntu 10.04 and up (32 and 64 bit).
  * Mac OS X Lion with xcode 5.x (64 bit, Mountain Lion and up).
  * Microsoft Windows with Visual Studio 2012 and 2013 (32 and 64 bit).

Upon decompressing the archive, you will get the following structure:

```
or-tools/
  LICENSE-2.0.txt  <- Apache License
  README           <- This file
  data/            <- Data for the examples
  examples/        <- Java examples
  lib/             <- Directory containing jar files and native libraries
  objs/            <- Directory that will contain compiled classes
```

Running the examples will involve compiling them, then running them.

Let's compile and run
examples/com/google/ortools/constraintsolver/samples/RabbitsPheasants.java

```
javac -d objs -cp lib/com.google.ortools.jar examples/com/google/ortools/constraintsolver/samples/RabbitsPheasants.java
java -Djava.library.path=lib -cp objs:lib/com.google.ortools.jar com.google.ortools.constraintsolver.samples.RabbitsPheasants
```