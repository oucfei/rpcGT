# GRPC and Distributed Systems

## Forward

In this project, you will design and implement a simple distributed file system (DFS).  First, you will develop several file transfer protocols using gRPC and Protocol Buffers. Next, you will incorporate a weakly consistent synchronization system to manage cache consistency between multiple clients and a single server. The system should be able to handle both binary and text-based files.

Your source code will use a combination of C++14, gRPC, and Protocol Buffers to complete the implementation.

## Setup

You can clone the code in the Project 4 repository with the command:

```
git clone https://github.gatech.edu/gios-sum-19/pr4.git
```

## Submission Instructions

Submit all code through the submit.py script given at the top level of the repository. For instructions on how to submit individual components of the assignment, see the instructions below.

For this assignment, you may submit your code up to 10 times in 24 hours. After the deadline, we download your last submission before the deadline, review your submission, and assign a grade. There is no limit to the number of times you may submit your readme-student.md file.

After submitting, you may double-check the results of your submission by visiting the Udacity/GT autograding website and going to the student portal.

## Readme

Throughout the project, we encourage you to keep notes on what you have done, how you have approached each part of the project, and what resources you used in preparing your work. We have provided you with a prototype file, readme-student.md that you should use throughout the project.

You may submit your readme-student.md file with the command:

```
python submit.py readme
```

At the prompt, please provide your GT username and password.

If this is your first time submitting, the program will then ask you if you want to save the JWT. If you reply yes, it will save a token on your filesystem so that you don't have to provide your username and password each time that you submit.

The Udacity site will store your readme-student.md file in a database, where it will be used during grading. The submit script will acknowledge receipt of your README file. For this project, like in Project 3, you only need to submit one README file for both parts of the project.

Note: you may choose to submit a PDF version of this file (readme-student.pdf) in place of the markdown version. The submission script will automatically detect and submit this file if it is present in the project root directory. If you submit both files, we will give the PDF version preference.

## Directions

### Part 1 - Building the RPC protocol service

In Part 1, you will build a series of remote procedure calls (RPC) and message types that will fetch, store, list, and get attributes for files on a remote server. The SunRPC implementation (ONC RPC) and XDR interface definition language (IDL) you learned about in the lectures are currently deprecated in favor of more modern implementations of the concepts behind RPC and IDL.  There are many RPC replacements, including [TI-RPC](https://docs.oracle.com/cd/E19683-01/816-1435/rpcintro-46812/), [Finagle](https://twitter.github.io/finagle/), [Thrift](https://thrift.apache.org), and [Cap'n Proto](https://capnproto.org/). However, in this assignment, we will use [gRPC](https://grpc.io/) for RPC services and [Protocol Buffers](https://developers.google.com/protocol-buffers/) as the definition language. The core gRPC library is written in C but supports multiple languages, including C++, Java, Go, and others. It is actively developed and in use at several organizations, such as Google, Square, Netflix, Juniper, Cisco, and Dropbox. In this assignment, we will use the gRPC C++ API.

#### Part 1 Goals

The goal of part 1 is to generate an RPC service that will perform the following operations on a file:

* Fetch a file from a remote server and transfer its contents via gRPC
* Store a file to a remote server and transfer its data via gRPC
* List all files on the remote server:

    * For this assignment, the server is only required to contain files in a single directory; it is not necessary to manage nested directories.
    * The file listing should include the file name and the modified time (_mtime_) of the file data in seconds from the epoch.

* Get the following attributes for a file on the remote server:

    *  Size
    *  Modified Time
    * Creation Time

The client should be able to request each of the operations described above for binary and text-based files. The server will respond to those requests using the gRPC service methods you specify in your proto buffer definition file.

* Finally, the client should recognize when the server has timed out. gRPC can signal the server using a deadline timeout as described in [this gRPC article](https://grpc.io/blog/deadlines/). You should ensure that your client recognizes this timeout signal.

##### Part 1 Sequence Diagram

A sequence diagram of the expected interactions in part 1 is available in the [docs/part1-sequence.pdf](docs/part1-sequence.pdf) file of this repository.

#### Protocol Buffers and gRPC

To begin part 1, you should first familiarize yourself with the basics of using Protocol Buffers. In particular, you should focus on the use of RPC service definitions and message type definitions for the request and response types that are used by the RPC services.

You will then create your protocol in the [dfs-service.proto](dfs-service.proto) file inside the project repository. There are several required services and message types described in the proto file that you should implement, but you may add as many additional methods and/or message types that you deem necessary. What you name those services and message types is also up to your discretion.

To autogenerate the gRPC and Protocol Buffer class and header files, we’ve provided a Makefile command that will take care of that for you. When you are ready to generate your protobuf/gRPC files, run the following command from the root of the repository:

```
make protos
```

You will find the results of that command in the [part1/proto-src](part1/proto-src) directory of the repository. You should familiarize yourself with the results in that directory, but you won’t need to, and should not, make any changes to those files. Your job will be to override the service methods in your [dfslib-servernode-p1.cpp](dfslib-servnode-p1.cpp) source file.

Once you have familiarized yourself with Protocol Buffers, you should next familiarize yourself with the [C++ API for gRPC](https://grpc.github.io/grpc/cpp/index.html). In particular, pay close attention to how the server implementation overrides methods, and the client makes calls to the RPC service for streaming message types.

> You do not need to concern yourself with asynchronous gRPC, we will only be working with the synchronous version in this project.

#### Part 1 Structure

All of the part 1 files are available in the [part1](part1) directory. You will find several source files in that directory, but you are only responsible for adjusting and submitting the `difslib-*` files inside `part1`.  The rest of the source files provide the supporting structure for the program. You may change any of the other source files for your testing purposes, but they will not be submitted as a part of your grade.

In each of the files to be modified, you will find additional instructions and hints on how you should approach the contents of that file. The following comment marker precedes each tip in the source code:

```
// STUDENT INSTRUCTION:
```

**Source code file descriptions:**

* `src/dfs-client-p1.[cpp,h]` - the CLI executable for the client side.

* `src/dfs-server-p1.[cpp,h]` - the CLI executable for the server side.

* `src/dfs-clientnode.[cpp,h]` - the parent class for the client node library file that you will override. All of the methods you will override are documented in the `dfslib-clientnode-p1.h` file you will modify.

* `src/dfs-utils.h` - A header file of utilities used by the executables. You may change this, but note that this file is not submitted to Bonnie. There is a separate `dfs-shared` file you may use for your utilities.

* `dfs-service.proto` - **TO BE MODIFIED** Add your proto buffer service and message types to this file, then run the `make protos` command to generate the source.

* `dfslib-servernode-p1.[cpp,h]` - **TO BE MODIFIED** - Override your gRPC service methods in this file by adding them to the `DFSServerImpl` class. The service method signatures can be found in the `proto-src/dfs-service.grpc.pb.h` file generated by the `make protos` command you ran earlier.

* `dfslib-clientnode-p1.[cpp,h]` - **TO BE MODIFIED** -  Add your client-side calls to the gRPC service in this file. We’ve provided the basic structure and method calls expected by the client executable. However, you may add any additional declarations and definitions that you deem necessary.

* `dfslib-shared-p1.[cpp,h]` - **TO BE MODIFIED** - Add any shared code or utilities that you need in this file. The shared header is available on both the client and server side.

#### Part 1 Compiling and Running

To compile the source code in Part 1, you may use the Makefile in the root of the repository and run:

```
make part1
```

Or, you may change to the `part1` directory and run `make`.

> For a list of all make commands available, run `make` in the root of the repository.

To run the executables, see the usage instructions in their respective files.

In most cases, you'll start the server with:

```
./bin/dfs-server-p1
```

The client is then used to fetch, store, list, and stat files. For example:

```
./bin/dfs-client-p1 fetch gt-campanile.jpg
```

#### Part 1 Submitting

To submit part 1, run the following from the root of the repository:

```
python submit.py part1
```

### Part 2 - Completing the DFS

Now that you have a working gRPC service, we will turn our focus towards completing a rudimentary DFS. For this assignment, we’ll apply a weakly consistent cache strategy to the RPC calls you already created in Part 1. This is similar to the approach used by the [Andrew File System (AFS)](https://en.wikipedia.org/wiki/Andrew_File_System). To keep things simple, we’ll focus on whole-file caching for the client-side and a simple lock strategy on the server-side to ensure that only one client may write to the server at any given time.

#### Part 2 Goals

For this assignment, your consistency model should adhere to the following expectations:

* **Whole-file caching**. The client should cache whole files on the client-side (i.e., do not concern yourself with partial file caches). Read and write operations should be applied to local files and only update the server when a file has been modified or created on the client.

* **One Creator/Writer per File (i.e., writer locks)**. Clients should request a file write lock from the server before pushing. If they are not able to obtain a lock, the attempt should fail. The server should keep track of which client holds a lock to a particular file, then release that lock after the client has successfully stored a file to the server.

* **CRC Checksums**. To determine if a file has changed between the server and the client, you may use a CRC checksum function we have provided for you. Any differences in the checksums between the server and the client constitute a change. The CRC function requires a file path and a table that we’ve already set up for you. The return value will be a `uint32_t` value that you can use in your gRPC message types. An example of how to obtain the checksum follows:

```cpp
std::uint32_t crc = file_checksum(filepath, this->crc_table);
```

> Note that you can copy the code from part1 for your Store, Fetch, List, and Stat methods to part2. However, please note that you will most likely need to adjust those methods to meet the requirements of the DFS implementation.

* **Date based Sequences**. If a file has changed between the server and the client, then the last modified timestamp should win. In other words, if the server has a more recent timestamp, then the client should fetch it from the server. If the client has a more recent timestamp, then the client should store it on the server. If there is no change in the modified time for a file, the client should do nothing. Keep in mind, when storing data, you must first request a write lock as described earlier.

#### Part 2 Structure

The file structure for Part 2 is identical to Part 1. As with Part 1, you are only responsible for adjusting the `dfslib-*` files in the `part2` directory.

Two threads on the client side will run concurrently. You will need to synchronize these threads and their access to the server.

The **watcher thread** uses the `inotify` system commands to monitor client directories for you. We’ve already provided the structural components to manage the events. However, you will need to make the appropriate changes to ensure that file notification events coordinate with the synchronization timer described next. An event callback function is provided in the `dfslib-clientnode-p2.cpp` file to assist you with that; read the `STUDENT INSTRUCTION` comments carefully as they provide some hints on how to manage that process.

The **sync thread** uses a simple timer to connect with the server every 3 seconds, similar to how NFS connects to a server. The client should request a list of files from the server, along with their last modified time (_mtime_), then synchronize the files between the client and the server. The client should fetch, store, or do nothing based on the goals discussed earlier.

##### Part 2 Sequence Diagram

A sequence diagram of the expected interactions for part 2 is available in the [docs/part2-sequence.pdf](docs/part2-sequence.pdf) file of this repository.

#### Part 2 Compiling

To compile the source code in Part 2, you may use the Makefile in the root of the repository and run:

```
make part2
```

Or, you may change to the `part2` directory and run `make`.

> For a list of all make commands available, run `make` in the root of the repository.

To run the executables, see the usage instructions in their respective files.

In most cases, you'll start the server with something similar to the following:

```
./bin/dfs-server-p2
```

The client should then mount to the client path. For example:

```
./bin/dfs-client-p2 mount
```

The above command will mount the client to the `mnt/client` directory, then start the watcher and sync timer threads. Changes to the mount path should then synchronize to the server and any other clients that have mounted.

As in Part 1, the client should also continue to accept individual commands, such as fetch, store, list, and stat.

#### Part 2 Submitting

To submit part 2, run the following from the root of the repository:

```
python submit.py part2
```

## References

### Relevant lecture material

* [P4L1 Remote Procedure Calls](https://www.udacity.com/course/viewer#!/c-ud923/l-3450238825)

### gRPC and Protocol Buffer resources


- [gRPC C++ Reference](https://grpc.github.io/grpc/cpp/index.html)
- [Protocol Buffers 3 Language Guide](https://developers.google.com/protocol-buffers/docs/proto3)
- [gRPC C++ Examples](https://github.com/grpc/grpc/tree/master/examples/cpp)
- [gRPC C++ Tutorial](https://grpc.io/docs/tutorials/basic/c/)
- [Protobuffers Scalar types](https://developers.google.com/protocol-buffers/docs/proto3#scalar)
- [gRPC Status Codes](https://github.com/grpc/grpc/blob/master/doc/statuscodes.md)
- [gRPC Deadline](https://grpc.io/blog/deadlines/)

## Rubric

Your project will be graded at least on the following items:

- Interface specification (.proto)
- Service implementation
- gRPC initiation and binding
- Proper handling of deadline timeouts
- Proper clean up of memory and gRPC resources
- Proper communication with the server
- Proper request and management of write locks
- Proper synchronization of files between multiple clients and a single server
- Insightful observations in the Readme file and suggestions for improving the class for future semesters

#### gRPC Implementation (40 points)

Full credit requires: code compiles successfully, does not crash, files fully transmitted, basic safety checks, and proper use of gRPC  - including the ability to get, store, and list files, along with the ability to recognize a timeout. Note that the automated tests will test some of these automatically, but graders may execute additional tests of these requirements.

#### DFS Implementation (50 points)

Full credit requires: code compiles successfully, does not crash, files fully transmitted, basic safety checks, proper use of gRPC, write locks properly handled, cache properly handled, synchronization of sync and inotify threads properly handled, and synchronization of multiple clients to a single server. Note that the automated tests will test some of these automatically, but graders may execute additional tests of these requirements.

#### README (10 points + 5 point extra credit opportunity)

* Clearly demonstrates your understanding of what you did and why - we want to see your design and your explanation of the choices that you made and why you made those choices. (4 points)
* A description of the flow of control for your code; we strongly suggest that you use graphics here, but a thorough textual explanation is sufficient. (2 points)
* A brief explanation of how you implemented and tested your code. (2 points)
* References any external materials that you consulted during your development process (2 points)
* Suggestions on how you would improve the documentation, sample code, testing, or other aspects of the project (up to 5 points extra credit available for noteworthy suggestions here, e.g., actual descriptions of how you would change things, sample code, code for tests, etc.) We do not give extra credit for simply reporting an issue - we're looking for actionable suggestions on how to improve things.

### Questions

For all questions, please use the class Piazza forum or the class Slack channel so that TA's and other students can assist you.

