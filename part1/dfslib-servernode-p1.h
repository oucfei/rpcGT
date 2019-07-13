#ifndef _DFSLIB_SERVERNODE_H
#define _DFSLIB_SERVERNODE_H

#include <string>
#include <iostream>
#include <thread>
#include <grpcpp/grpcpp.h>
#include "proto-src/dfs-service.grpc.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using dfs_service::HelloRequest;
using dfs_service::HelloReply;

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

Status SayHello(ServerContext* context, const HelloRequest* request,
                  HelloReply* reply);

    //
    // STUDENT INSTRUCTION:
    //
    // Add your additional declarations here
    //

};

#endif
