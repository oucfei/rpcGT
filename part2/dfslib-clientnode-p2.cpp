#include <regex>
#include <mutex>
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
#include <utime.h>
#include <dirent.h>

#include "src/dfs-utils.h"
#include "src/dfs-clientnode-p2.h"
#include "dfslib-shared-p2.h"
#include "dfslib-clientnode-p2.h"
#include "proto-src/dfs-service.grpc.pb.h"

using grpc::Status;
using grpc::Channel;
using grpc::StatusCode;
using grpc::ClientWriter;
using grpc::ClientReader;
using grpc::ClientContext;
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
using dfs_service::WriteLockResponse;
using dfs_service::WriteLockRequest;

extern dfs_log_level_e DFS_LOG_LEVEL;

DFSClientNodeP2::DFSClientNodeP2() : DFSClientNode() {
    dfs_log(LL_SYSINFO) << "Client started, id: " << client_id;
}

DFSClientNodeP2::~DFSClientNodeP2() {}

grpc::StatusCode DFSClientNodeP2::RequestWriteAccess(const std::string &filename) {
    dfs_log(LL_SYSINFO) << "client requesting write access to file " << filename;
    WriteLockRequest request;
    request.set_filename(filename);
    request.set_clientid(client_id);

    WriteLockResponse response;
    ClientContext context;
    //context.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(deadline_timeout+5000));
    Status status = service_stub->RequestWriteLock(&context, request, &response);
    
    if (status.ok())
    {
      return StatusCode::OK;
    }

    dfs_log(LL_SYSINFO) << "failed to RequestWriteAccess: " << status.error_message();
    return status.error_code();
    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to obtain a write lock here when trying to store a file.
    // This method should request a write lock for the given file at the server,
    // so that the current client becomes the sole creator/writer. If the server
    // responds with a RESOURCE_EXHAUSTED response, the client should cancel
    // the current file storage
    //
    // The StatusCode response should be:
    //
    // OK - if all went well
    // StatusCode::DEADLINE_EXCEEDED - if the deadline timeout occurs
    // StatusCode::RESOURCE_EXHAUSTED - if a write lock cannot be obtained
    // StatusCode::CANCELLED otherwise
    //
    //
}

grpc::StatusCode DFSClientNodeP2::Store(const std::string &filename) {
    dfs_log(LL_SYSINFO) << "begin store " << filename;

    grpc::StatusCode requestWriteLock = RequestWriteAccess(filename);
    if (requestWriteLock != StatusCode::OK)
    {
        dfs_log(LL_SYSINFO) << "unable to store, no lock " << requestWriteLock;
        return requestWriteLock;
    }

    dfs_log(LL_SYSINFO) << "acquired lock, begin to store " << filename;

    ClientContext context;
    //context.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(deadline_timeout + 5000));
    context.AddMetadata("filename", filename);
    context.AddMetadata("clientid", client_id);

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
    // Add your request to store a file here. Refer to the Part 1
    // student instruction for details on the basics.
    //
    // You can start with your Part 1 implementation. However, you will
    // need to adjust this method to recognize when a file trying to be
    // stored is the same on the server (i.e. the ALREADY_EXISTS gRPC response).
    //
    // You will also need to add a request for a write lock before attempting to store.
    //
    // If the write lock request fails, you should return a status of RESOURCE_EXHAUSTED
    // and cancel the current operation.
    //
    // The StatusCode response should be:
    //
    // StatusCode::OK - if all went well
    // StatusCode::DEADLINE_EXCEEDED - if the deadline timeout occurs
    // StatusCode::ALREADY_EXISTS - if the local cached file has not changed from the server version
    // StatusCode::RESOURCE_EXHAUSTED - if a write lock cannot be obtained
    // StatusCode::CANCELLED otherwise
    //
    //

}


grpc::StatusCode DFSClientNodeP2::Fetch(const std::string &filename) {
    dfs_log(LL_SYSINFO) << "begin fetch " << filename;
    ClientContext context;
   //context.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(deadline_timeout));
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
    // Add your request to fetch a file here. Refer to the Part 1
    // student instruction for details on the basics.
    //
    // You can start with your Part 1 implementation. However, you will
    // need to adjust this method to recognize when a file trying to be
    // fetched is the same on the client (i.e. the files do not differ
    // between the client and server and a fetch would be unnecessary.
    //
    // The StatusCode response should be:
    //
    // OK - if all went well
    // DEADLINE_EXCEEDED - if the deadline timeout occurs
    // NOT_FOUND - if the file cannot be found on the server
    // ALREADY_EXISTS - if the local cached file has not changed from the server version
    // CANCELLED otherwise
    //
    // Hint: You may want to match the mtime on local files to the server's mtime
    //
}

grpc::StatusCode DFSClientNodeP2::List(std::map<std::string,int>* file_map, bool display) {
    dfs_log(LL_SYSINFO) << "listing file: ";
    ListFilesRequest request;
    ClientContext context;
    //context.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(deadline_timeout + 5000));
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
    // Add your request to list files here. Refer to the Part 1
    // student instruction for details on the basics.
    //
    // You can start with your Part 1 implementation and add any additional
    // listing details that would be useful to your solution to the list response.
    //
    // The StatusCode response should be:
    //
    // StatusCode::OK - if all went well
    // StatusCode::DEADLINE_EXCEEDED - if the deadline timeout occurs
    // StatusCode::CANCELLED otherwise
    //
    //
}

grpc::StatusCode DFSClientNodeP2::Stat(const std::string &filename, void* file_status) {
    GetStatRequest request;
    request.set_filename(filename);
    GetStatResponse response;
    ClientContext context;
    //context.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(deadline_timeout+5000));
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
    // Add your request to get the status of a file here. Refer to the Part 1
    // student instruction for details on the basics.
    //
    // You can start with your Part 1 implementation and add any additional
    // status details that would be useful to your solution.
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

uint32_t DFSClientNodeP2::GetFileChecksum(const std::string &filename)
{
    GetStatRequest request;
    request.set_filename(filename);
    GetStatResponse response;
    ClientContext context;
    Status status = service_stub->GetStat(&context, request, &response);
    return response.checksum();
}

void DFSClientNodeP2::Sync() {
    dfs_log(LL_SYSINFO) << "client syncing. id: " << client_id << " mount path: " << mount_path;
    std::map<std::string,int> server_files;
    List(&server_files, false);

    dfs_log(LL_SYSINFO) << "syncing ---------------------------------------------------------- ";
    DIR* dirp = opendir(mount_path.c_str());
    struct dirent * dp;
    while ((dp = readdir(dirp)) != NULL) {
        std::string temp(dp->d_name);
        if (temp.compare(".") == 0 || temp.compare("..") == 0 || dp->d_name[0] == '.')
        {
            continue;
        }

        std::string file(WrapPath(dp->d_name));
        struct stat fileStat;
        stat(file.c_str(), &fileStat);

        if (server_files.find(dp->d_name) != server_files.end())
        {
            dfs_log(LL_SYSINFO) << "comparing files " <<  dp->d_name << " client mtime: " << fileStat.st_mtime;

            int server_mtime = server_files[dp->d_name];
            if (server_mtime > fileStat.st_mtime)
            {
                uint32_t server_crc = GetFileChecksum(dp->d_name);
                uint32_t client_crc = dfs_file_checksum(file, &this->crc_table);
                if (server_crc != client_crc)
                {
                    dfs_log(LL_SYSINFO) << "server is newer, fetching " << dp->d_name;
                    //server has newer version, fetch new file from server
                    Fetch(dp->d_name);
                }
                else
                {
                    dfs_log(LL_SYSINFO) << "checksum same for file " << dp->d_name;
                }
                
            }
            else if (server_mtime < fileStat.st_mtime)
            {
                uint32_t server_crc = GetFileChecksum(dp->d_name);
                uint32_t client_crc = dfs_file_checksum(file, &this->crc_table);
                if (server_crc != client_crc)
                {
                    dfs_log(LL_SYSINFO) << "client is newer, storing " << dp->d_name;

                    //client has newer version, store to server.
                    Store(dp->d_name);
                }
                else
                {
                    dfs_log(LL_SYSINFO) << "checksum same for file " << dp->d_name;
                }
            }

            server_files.erase(dp->d_name);
        }
        else
        {
            dfs_log(LL_SYSINFO) << "file " << dp->d_name << " not exist in server, storing";
            Store(dp->d_name);
        }
    }

    dfs_log(LL_SYSINFO) << "------------------processing client missing files.-----------------";
    std::map<std::string,int>::iterator it;
    for (it = server_files.begin(); it != server_files.end(); it++ )
    {
        dfs_log(LL_SYSINFO) << "client missing file, fetching " << it->first;

        Fetch(it->first);
    }

    //
    // STUDENT INSTRUCTION:
    //
    // Add your synchronization code here. Note that synchronization
    // of the file system occurs every 3000 milliseconds on a timer by default.
    // This structure has already been handled for you in the client executable.
    //
    // When the Sync method is called, you should synchronize the
    // files between the server and the client based on the goals
    // described in the readme.
    //
    // In addition to synchronizing the files, you'll also need to ensure
    // that the sync thread and the file watcher thread are cooperating. These
    // two threads could easily get into a race condition where both are trying
    // to write or fetch over top of each other. So, you'll need to determine
    // what type of locking/guarding is necessary to ensure the threads are
    // properly coordinated.
    //
}

void DFSClientNodeP2::InotifyWatcherCallback(std::function<void()> callback) {

    //
    // STUDENT INSTRUCTION:
    //
    // This method gets called each time inotify signals a change
    // to a file on the file system. That is every time a file is
    // modified or created.
    //
    // You may want to consider how this section will affect
    // concurrent actions between the inotify watcher and the
    // server cache checks in the SyncTimerCallback to prevent a race condition.
    //
    // The callback method shown must be called here, but you may surround it with
    // whatever structures you feel are necessary to ensure proper coordination
    // between the sync and watcher threads.
    //
    // Hint: how can you prevent race conditions between this thread and
    // the synchronization timer when a file event has been signaled?
    //


    callback();


}

//
// STUDENT INSTRUCTION:
//
// Add any additional code you need to here
//


