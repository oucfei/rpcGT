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
using dfs_service::StoreResponse;
using dfs_service::GetStatRequest;
using dfs_service::GetStatResponse;
using dfs_service::ListFilesRequest;
using dfs_service::ListFilesResponse;
using dfs_service::ListFileInfo;

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
/*class GrpcClient {
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
};*/

DFSClientNodeP1::DFSClientNodeP1() : DFSClientNode() {

}

DFSClientNodeP1::~DFSClientNodeP1() noexcept {}

StatusCode DFSClientNodeP1::Store(const std::string &filename) {
    dfs_log(LL_SYSINFO) << "begin store " << filename;
    ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(deadline_timeout + 5000));
    context.AddMetadata("filename", filename);

    StoreResponse response;
    std::unique_ptr<ClientWriter<Chunk>> writer(service_stub->Store(&context, &response));

    std::string fileToStore(WrapPath(filename));
    dfs_log(LL_SYSINFO) << "FiletoStore "<< fileToStore;

    std::ifstream input(fileToStore, std::ios::binary);
    struct stat filestatus;
    stat(fileToStore.c_str(), &filestatus);

    size_t total_size = filestatus.st_size;
    size_t chunk_size = 1024;
    size_t total_chunks = total_size / chunk_size;
    size_t last_chunk_size = total_size % chunk_size;
    if (last_chunk_size != 0) 
    {
      ++total_chunks;
    }
    else
    {
      last_chunk_size = chunk_size;
    }

    dfs_log(LL_SYSINFO) << "stream uploading file: total chunk: " << total_chunks;
    for (size_t chunk = 0; chunk < total_chunks; ++chunk)
    {
      size_t this_chunk_size = chunk == total_chunks - 1? last_chunk_size : chunk_size;
      std::vector<char> chunk_data(this_chunk_size);

      input.read(&chunk_data[0], this_chunk_size); /* this many bytes is to be read */

      Chunk chunkToSend;
      std::string contentStr(chunk_data.begin(), chunk_data.end());
      chunkToSend.set_content(contentStr);
      writer->Write(chunkToSend);
    }

    writer->WritesDone();
    Status status = writer->Finish();

    if (status.ok())
    {
      return StatusCode::OK;
    }

    dfs_log(LL_SYSINFO) << "failed to store in server: " << status.error_message();
    return status.error_code();
    
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
    dfs_log(LL_SYSINFO) << "begin fetch " << filename;
    ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(deadline_timeout + 10000));
    FetchRequest request;
    request.set_filename(filename);

    std::unique_ptr<ClientReader<Chunk>> reader(service_stub->Fetch(&context, request));

    std::string filePath = WrapPath(filename);
    ofstream outfile(filePath, ofstream::binary);

    Chunk chunk;
    while (reader->Read(&chunk))
    {
      outfile << chunk.content();
      chunk.clear_content();
    }

    outfile.close();
    Status status = reader->Finish();
    if (status.ok())
    {
      return StatusCode::OK;
    }

    dfs_log(LL_SYSINFO) << "failed to fetch in server: " << status.error_message();
    return status.error_code();
    
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
    dfs_log(LL_SYSINFO) << "listing file: ";
    ListFilesRequest request;
    ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(deadline_timeout + 5000));
    ListFilesResponse response;

    Status status = service_stub->ListAllFiles(&context, request, &response);
    
    for (int i = 0; i < response.allfileinfo_size(); i++)
    {
        std::string filename = response.allfileinfo(i).filename();
        int mtime = response.allfileinfo(i).modifiedtime();
        file_map->insert(std::make_pair(filename, mtime));

        dfs_log(LL_SYSINFO) << "listing file: " << filename << ", mtime: " << mtime;
    }

    if (status.ok())
    {
      return StatusCode::OK;
    }

    dfs_log(LL_SYSINFO) << "failed to list: " << status.error_message();
    return status.error_code();

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
    GetStatRequest request;
    request.set_filename(filename);
    GetStatResponse response;
    ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(deadline_timeout+5000));
    Status status = service_stub->GetStat(&context, request, &response);
    dfs_log(LL_SYSINFO) << "Get stat finished,  file size" << response.filesize() << "\n";
    dfs_log(LL_SYSINFO) << "file creat time: " << response.creationtime() << "\n";
    dfs_log(LL_SYSINFO) << "file modify time: " << response.modifiedtime() << "\n";

    if (status.ok())
    {
      return StatusCode::OK;
    }

    dfs_log(LL_SYSINFO) << "failed to get stat: " << status.error_message();
    return status.error_code();

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

