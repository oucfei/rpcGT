#include <regex>
#include <vector>
#include <string>
#include <thread>
#include <cstdio>
#include <chrono>
#include <errno.h>
#include <csignal>
#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <getopt.h>
#include <unistd.h>
#include <limits.h>
#include <sys/inotify.h>
#include <grpcpp/grpcpp.h>

#include "dfslib-shared-p1.h"
#include "dfslib-clientnode-p1.h"
#include "proto-src/dfs-service.grpc.pb.h"

using grpc::Status;
using grpc::Channel;
using grpc::StatusCode;
using grpc::ClientWriter;
using grpc::ClientReader;
using grpc::ClientContext;
using dfs_service::HelloRequest;
using dfs_service::HelloReply;
using dfs_service::DFSService;
using dfs_service::FetchRequest;
using dfs_service::Chunk;
using std::ofstream;

//
// STUDENT INSTRUCTION:
//
// You may want to add aliases to your namespaced service methods here.
// All of the methods will be under the `dfs_service` namespace.
//
// For example, if you have a method named MyMethod, add
// the following:
//
//      using dfs_service::MyMethod
//
class GrpcClient {
 public:
  GrpcClient(std::shared_ptr<Channel> channel)
      : stub_(dfs_service::DFSService::NewStub(channel)) {}

  // Assembles the client's payload, sends it and presents the response back
  // from the server.
  std::string SayHello(const std::string& user) {
    // Data we are sending to the server.
    HelloRequest request;
    request.set_name(user);

    // Container for the data we expect from the server.
    HelloReply reply;

    // Context for the client. It could be used to convey extra information to
    // the server and/or tweak certain RPC behaviors.
    ClientContext context;

    // The actual RPC.
    Status status = stub_->SayHello(&context, request, &reply);

    // Act upon its status.
    if (status.ok()) {
      return reply.message();
    } else {
      std::cout << status.error_code() << ": " << status.error_message()
                << std::endl;
      return "RPC failed";
    }
  }

 private:
  std::unique_ptr<dfs_service::DFSService::Stub> stub_;
};

DFSClientNodeP1::DFSClientNodeP1() : DFSClientNode() {

}

DFSClientNodeP1::~DFSClientNodeP1() noexcept {}

StatusCode DFSClientNodeP1::Store(const std::string &filename) {
    return StatusCode::OK;
    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to store a file here. This method should
    // connect to your gRPC service implementation method
    // that can accept and store a file.
    //
    // When working with files in gRPC you'll need to stream
    // the file contents, so consider the use of gRPC's ClientWriter.
    //
    // The StatusCode response should be:
    //
    // StatusCode::OK - if all went well
    // StatusCode::DEADLINE_EXCEEDED - if the deadline timeout occurs
    // StatusCode::CANCELLED otherwise
    //
    //
}

StatusCode DFSClientNodeP1::Fetch(const std::string &filename) {
    std::cout << "begin fetch " << filename;
    ClientContext context;
    FetchRequest request;
    Chunk chunk;
    request.set_filename(filename);

    std::unique_ptr<ClientReader<Chunk>> reader(service_stub->Fetch(&context, request));
        
    /* std::string user("xiaofma!!");
    HelloRequest request2;
    request2.set_name(user);
   HelloReply reply;
    Status status2 = service_stub->SayHello(&context, request2, &reply);

    std::cout << "RPC call to fetch returned: " << reply.message();*/

    reader->Read(&chunk);
    size_t size = chunk.content().size();
    std::cout << "Received chunk size: " << size;

    std::string filePath = WrapPath(filename);
    ofstream outfile(filePath, ofstream::binary);
  
    std::cout << "Fetch finished, writing to file " << filePath << "\n";
    outfile.write(chunk.content().c_str(), size);
 
    outfile.close();

    return StatusCode::OK;

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to fetch a file here. This method should
    // connect to your gRPC service implementation method
    // that can accept a file request and return the contents
    // of a file from the service.
    //
    // As with the store function, you'll need to stream the
    // contents, so consider the use of gRPC's ClientReader.
    //
    // The StatusCode response should be:
    //
    // StatusCode::OK - if all went well
    // StatusCode::DEADLINE_EXCEEDED - if the deadline timeout occurs
    // StatusCode::NOT_FOUND - if the file cannot be found on the server
    // StatusCode::CANCELLED otherwise
    //
    //
}

StatusCode DFSClientNodeP1::List(std::map<std::string,int>* file_map, bool display) {

  GrpcClient greeter(grpc::CreateChannel(
      "localhost:42001", grpc::InsecureChannelCredentials()));
  std::string user("world!!");
  std::string reply = greeter.SayHello(user);
  std::cout << "Greeter received: " << reply << std::endl;

    return StatusCode::OK;
    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to list all files here. This method
    // should connect to your service's list method and return
    // a list of files using the message type you created.
    //
    // The file_map parameter is a simple map of files. You should fill
    // the file_map with the list of files you receive with keys as the
    // file name and values as the modified time (mtime) of the file
    // received from the server.
    //
    // The StatusCode response should be:
    //
    // StatusCode::OK - if all went well
    // StatusCode::DEADLINE_EXCEEDED - if the deadline timeout occurs
    // StatusCode::CANCELLED otherwise
    //
    //
}

StatusCode DFSClientNodeP1::Stat(const std::string &filename, void* file_status) {
    return StatusCode::OK;
    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to get the status of a file here. This method should
    // retrieve the status of a file on the server. Note that you won't be
    // tested on this method, but you will most likely find that you need
    // a way to get the status of a file in order to synchronize later.
    //
    // The status might include data such as name, size, mtime, crc, etc.
    //
    // The file_status is left as a void* so that you can use it to pass
    // a message type that you defined. For instance, may want to use that message
    // type after calling Stat from another method.
    //
    // The StatusCode response should be:
    //
    // StatusCode::OK - if all went well
    // StatusCode::DEADLINE_EXCEEDED - if the deadline timeout occurs
    // StatusCode::NOT_FOUND - if the file cannot be found on the server
    // StatusCode::CANCELLED otherwise
    //
    //
}

//
// STUDENT INSTRUCTION:
//
// Add your additional code here, including
// implementations of your client methods
//

