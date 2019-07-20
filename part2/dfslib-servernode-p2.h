#ifndef PR4_DFSLIB_SERVERNODE_H
#define PR4_DFSLIB_SERVERNODE_H

#include <string>
#include <iostream>
#include <thread>
#include <grpcpp/grpcpp.h>
#include "proto-src/dfs-service.grpc.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;
using dfs_service::FetchRequest;
using dfs_service::Chunk;
using dfs_service::StoreResponse;
using dfs_service::GetStatRequest;
using dfs_service::GetStatResponse;
using dfs_service::ListFilesRequest;
using dfs_service::ListFilesResponse;
using dfs_service::ListFileInfo;
using dfs_service::WriteLockResponse;
using dfs_service::WriteLockRequest;

class DFSServerNode {

private:
    /** The server address information **/
    std::string server_address;

    /** The mount path for the server **/
    std::string mount_path;

    /** The pointer to the grpc server instance **/
    std::unique_ptr<grpc::Server> server;

    /** Server callback **/
    std::function<void()> grader_callback;


public:
    DFSServerNode(const std::string& server_address, const std::string& mount_path, std::function<void()> callback);
    ~DFSServerNode();
    void Shutdown();
    void Start();

Status Fetch(ServerContext* context, const FetchRequest* request,
                  ServerWriter<Chunk>* writer);

Status Store(ServerContext* context, ServerReader<Chunk>* reader, 
    StoreResponse* response);

Status GetStat(ServerContext* context, const GetStatRequest* request,
    GetStatResponse* response);

Status ListAllFiles(ServerContext* context, const ListFilesRequest* request,
                  ListFilesResponse* reply);

Status RequestWriteLock(ServerContext* context, const WriteLockRequest* request,
                  WriteLockResponse* reply);               
};

#endif
