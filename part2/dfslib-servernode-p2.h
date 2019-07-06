#ifndef PR4_DFSLIB_SERVERNODE_H
#define PR4_DFSLIB_SERVERNODE_H

#include <string>
#include <iostream>
#include <thread>
#include <grpcpp/grpcpp.h>

class DFSServerNode {

private:
    /** The mount path for the server **/
    std::string mount_path;

    /** The server address information **/
    std::string server_address;

    /** The pointer to the grpc server instance **/
    std::unique_ptr<grpc::Server> server;

    /** Server callback **/
    std::function<void()> grader_callback;


public:
    DFSServerNode(const std::string& server_address, const std::string& mount_path, std::function<void()> callback);
    ~DFSServerNode();
    void Shutdown();
    void Start();

};

#endif
