#include <map>
#include <chrono>
#include <cstdio>
#include <string>
#include <thread>
#include <errno.h>
#include <iostream>
#include <fstream>
#include <getopt.h>
#include <dirent.h>
#include <sys/stat.h>
#include <grpcpp/grpcpp.h>
#include <sys/types.h>

#include "src/dfs-utils.h"
#include "dfslib-shared-p1.h"
#include "dfslib-servernode-p1.h"
#include "proto-src/dfs-service.grpc.pb.h"

using grpc::Status;
using grpc::Server;
using grpc::StatusCode;
using grpc::ServerReader;
using grpc::ServerWriter;
using grpc::ServerContext;
using grpc::ServerBuilder;
using dfs_service::DFSService;

using std::ofstream;

//
// STUDENT INSTRUCTION:
//
// DFSServiceImpl is the implementation service for the rpc methods
// and message types you defined in the `dfs-service.proto` file.
//
// You should add your definition overrides here for the specific
// methods that you created for your GRPC service protocol. The
// gRPC tutorial described in the readme is a good place to get started
// when trying to understand how to implement this class.
//
// The method signatures generated can be found in `proto-src/dfs-service.grpc.pb.h` file.
//
// Look for the following section:
//
//      class Service : public ::grpc::Service {
//
// The methods returning grpc::Status are the methods you'll want to override.
//
// In C++, you'll want to use the `override` directive as well. For example,
// if you have a service method named MyMethod that takes a MyMessageType
// and a ServerWriter, you'll want to override it similar to the following:
//
//      Status MyMethod(ServerContext* context,
//                      const MyMessageType* request,
//                      ServerWriter<MySegmentType> *writer) override {
//
//          /** code implementation here **/
//      }
//
class DFSServiceImpl final : public DFSService::Service {

private:

    /** The mount path for the server **/
    std::string mount_path;

    /**
     * Prepend the mount path to the filename.
     *
     * @param filepath
     * @return
     */
    const std::string WrapPath(const std::string &filepath) {
        return this->mount_path + filepath;
    }

public:

    DFSServiceImpl(const std::string &mount_path): mount_path(mount_path) {
    }

    ~DFSServiceImpl() {}

  Status SayHello(ServerContext* context, const HelloRequest* request,
                  HelloReply* reply) override {
    dfs_log(LL_SYSINFO) << "DFSServerNode saying hello!";
        
    std::string prefix("Hello ");
    reply->set_message(prefix + request->name());
    return Status::OK;
  }


  Status GetStat(ServerContext* context, const GetStatRequest* request,
    GetStatResponse* response) override{
    dfs_log(LL_SYSINFO) << "DFSServerNode received get stat request!";

    std::string fileToGetStat(WrapPath(request->filename()));

    struct stat fileStat;
    stat(fileToGetStat.c_str(), &fileStat);
 
    response->set_filesize(fileStat.st_size);
    response->set_creationtime(fileStat.st_ctime);
    response->set_modifiedtime(fileStat.st_mtime);

    return Status::OK;
  }

Status ListAllFiles(ServerContext* context, const ListFilesRequest* request,
                  ListFilesResponse* reply) override
{
    DIR* dirp = opendir(mount_path.c_str());
    struct dirent * dp;
    while ((dp = readdir(dirp)) != NULL) {
        std::string file(WrapPath(dp->d_name));
        struct stat fileStat;
        stat(file.c_str(), &fileStat);

        ListFileInfo* fileInfo = reply->add_allfileinfo();
        
        fileInfo->set_filename(dp->d_name);
        fileInfo->set_modifiedtime(fileStat.st_mtime);
        dfs_log(LL_SYSINFO) << "Added file: " << dp->d_name;
    }

    return Status::OK;
}

  Status Fetch(ServerContext* context, const FetchRequest* request,
                  ServerWriter<Chunk>* writer) override {

    dfs_log(LL_SYSINFO) << "DFSServerNode received fetch request!";
        
    std::string fileToFetch(WrapPath(request->filename()));
    dfs_log(LL_SYSINFO) << "FiletoFetch "<< fileToFetch;

    std::ifstream input(fileToFetch, std::ios::binary);
    std::string contents((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    
    dfs_log(LL_SYSINFO) << "File size: "<< contents.size();

    Chunk chunk;
    chunk.set_content(contents);
    writer->Write(chunk);

    return Status::OK;
  }

  Status Store(ServerContext* context, ServerReader<Chunk>* reader, 
        StoreResponse* response) override {
    
    dfs_log(LL_SYSINFO) << "DFSServerNode received Store request!";
    std::multimap<grpc::string_ref, grpc::string_ref> metadata = context->client_metadata();

    auto iter = metadata.begin();
    char dest[256];
    strncpy(dest, iter->second.data(), iter->second.length());
    std::string filename(dest);
    dfs_log(LL_SYSINFO) << "DFSServerNode received Store request filename: " << filename;

    Chunk chunk;
    reader->Read(&chunk);

    size_t size = chunk.content().size();
    
    std::string filePath = WrapPath(filename);
    ofstream outfile(filePath, ofstream::binary);
  
    std::cout << "writing to file " << filePath << "\n";
    outfile.write(chunk.content().c_str(), size);
 
    outfile.close();

    return Status::OK;
  }

    //
    // STUDENT INSTRUCTION:
    //
    // Add your additional code here, including
    // implementations of your protocol service methods
    //


};

//
// STUDENT INSTRUCTION:
//
// The following three methods are part of the basic DFSServerNode
// structure. You may add additional methods or change these slightly,
// but be aware that the testing environment is expecting these three
// methods as-is.
//
/**
 * The main server node constructor
 *
 * @param server_address
 * @param mount_path
 */
DFSServerNode::DFSServerNode(const std::string &server_address,
        const std::string &mount_path,
        std::function<void()> callback) :
    server_address(server_address), mount_path(mount_path), grader_callback(callback) {}

/**
 * Server shutdown
 */
DFSServerNode::~DFSServerNode() noexcept {
    dfs_log(LL_SYSINFO) << "DFSServerNode shutting down";
    this->server->Shutdown();
}

/** Server start **/
void DFSServerNode::Start() {
    DFSServiceImpl service(this->mount_path);
    ServerBuilder builder;
    builder.AddListeningPort(this->server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    this->server = builder.BuildAndStart();
    dfs_log(LL_SYSINFO) << "DFSServerNode server listening on " << this->server_address;
    this->server->Wait();
}

//
// STUDENT INSTRUCTION:
//
// Add your additional DFSServerNode definitions here
//

