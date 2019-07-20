#include <map>
#include <mutex>
#include <shared_mutex>
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

#include "dfslib-shared-p2.h"
#include "dfslib-servernode-p2.h"
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
extern dfs_log_level_e DFS_LOG_LEVEL;

//
// STUDENT INSTRUCTION:
//
// As with Part 1, the DFSServiceImpl is the implementation service for the rpc methods
// and message types you defined in your `dfs-service.proto` file.
//
// You may start with your Part 1 implementations of each service method.
//
// Elements to consider for Part 2:
//
// - How will you implement the write lock at the server level?
// - How will you keep track of which client has a write lock for a file?
//      - Note that we've provided a preset client_id in DFSClientNode that generates
//        a client id for you. You can pass that to the server to identify the current client.
// - How will you release the write lock?
// - How will you handle a store request for a client that doesn't have a write lock?
// - When matching files to determine similary, you can use the `file_checksum` method we've provided.
//      - Both the client and server have a premade `crc_table` variable to speed things up.
//      - Use the `file_checksum` method to compare two files, similar to the following:
//
//          std::uint32_t server_crc = dfs_file_checksum(filepath, &this->crc_table);
//
//      - Hint: as the crc checksum is a simple integer, you can pass it around inside your message types.
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

    // CRC Table kept in memory for faster calculations
    CRC::Table<std::uint32_t, 32> crc_table;

public:

    DFSServiceImpl(const std::string& mount_path): mount_path(mount_path), crc_table(CRC::CRC_32()) {

    }

    ~DFSServiceImpl() {}

    //
    // STUDENT INSTRUCTION:
    //
    // Add your additional code here, including
    // the implementations of your rpc protocol methods.
    //
Status GetStat(ServerContext* context, const GetStatRequest* request,
    GetStatResponse* response) override{
    dfs_log(LL_SYSINFO) << "DFSServerNode received get stat request!";
    if (context->IsCancelled()) {
      dfs_log(LL_SYSINFO) << "Deadline exceeded";
      return Status(StatusCode::DEADLINE_EXCEEDED, "Deadline exceeded or Client cancelled, abandoning.");
    }

    std::string fileToGetStat(WrapPath(request->filename()));
    std::ifstream input(fileToGetStat, std::ios::binary);
    if (input.fail())
    {
      dfs_log(LL_SYSINFO) << "failed to open file: not exist";
      return Status(StatusCode::NOT_FOUND, "File not exist.");
    }

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
    if (context->IsCancelled()) {
      dfs_log(LL_SYSINFO) << "Deadline exceeded";
      return Status(StatusCode::DEADLINE_EXCEEDED, "Deadline exceeded or Client cancelled, abandoning.");
    }

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
    if (input.fail())
    {
      dfs_log(LL_SYSINFO) << "failed to open file: not exist";
      return Status(StatusCode::NOT_FOUND, "File not exist.");
    }

    struct stat filestatus;
    stat(fileToFetch.c_str(), &filestatus);

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

    dfs_log(LL_SYSINFO) << "streaming file: total chunk: " << total_chunks;
    for (size_t chunk = 0; chunk < total_chunks; ++chunk)
    {
      if (context->IsCancelled()) {
        dfs_log(LL_SYSINFO) << "Deadline exceeded";
        return Status(StatusCode::DEADLINE_EXCEEDED, "Deadline exceeded or Client cancelled, abandoning.");
      }

      size_t this_chunk_size = chunk == total_chunks - 1? last_chunk_size : chunk_size;
      std::vector<char> chunk_data(this_chunk_size);

      input.read(&chunk_data[0], this_chunk_size); /* this many bytes is to be read */

      Chunk chunkToSend;
      std::string contentStr(chunk_data.begin(), chunk_data.end());
      chunkToSend.set_content(contentStr);
      writer->Write(chunkToSend);
    }

    return Status::OK;
  }

  Status Store(ServerContext* context, ServerReader<Chunk>* reader, 
        StoreResponse* response) override {

    std::multimap<grpc::string_ref, grpc::string_ref> metadata = context->client_metadata();

    auto iter = metadata.begin();
    char dest[iter->second.length() + 1];
    
    dfs_log(LL_SYSINFO) << "DFSServerNode received Store request filename!" << iter->second.data();
    dfs_log(LL_SYSINFO) << "DFSServerNode received Store request filename length!" << iter->second.length();

    strncpy(dest, iter->second.data(), iter->second.length());
    dest[iter->second.length()] = '\0';

    std::string filename(dest);
    dfs_log(LL_SYSINFO) << "DFSServerNode received Store request filename: " << filename;

    std::string filePath = WrapPath(filename);
    ofstream outfile(filePath, ofstream::binary);
    if (context->IsCancelled()) {
      dfs_log(LL_SYSINFO) << "Deadline exceeded";
      return Status(StatusCode::DEADLINE_EXCEEDED, "Deadline exceeded or Client cancelled, abandoning.");
    }

    Chunk chunk;
    while (reader->Read(&chunk))
    {
      if (context->IsCancelled()) {
        dfs_log(LL_SYSINFO) << "Deadline exceeded";
        outfile.close();
        return Status(StatusCode::DEADLINE_EXCEEDED, "Deadline exceeded or Client cancelled, abandoning.");
      }

      outfile << chunk.content();
      chunk.clear_content();
    }

    outfile.close();
    return Status::OK;
  }

};

//
// STUDENT INSTRUCTION:
//
// The following three methods are part of the basic DFSServerNode
// structure. You may add additional methods or change these slightly
// to add additional startup/shutdown routines inside, but be aware that
// the basic structure should stay the same as the testing environment
// will be expected this structure.
//
/**
 * The main server node constructor
 *
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
/**
 * Start the DFSServerNode server
 */
void DFSServerNode::Start() {
    ServerBuilder builder;
    DFSServiceImpl service(this->mount_path);
    builder.AddListeningPort(this->server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    this->server = builder.BuildAndStart();


    dfs_log(LL_SYSINFO) << "DFSServerNode server listening on " << this->server_address;
    this->server->Wait();
}

//
// STUDENT INSTRUCTION:
//
// Add your additional definitions here
//

