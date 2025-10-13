#pragma once
#include <grpcpp/grpcpp.h>

#include <memory>
#include <string>

using grpc::Server;
using grpc::ServerBuilder;

/**
 * Generic DFS Server class that encapsulates gRPC server lifecycle management.
 * Provides clean Start/Stop/Wait interface for any gRPC service.
 */
template <typename ServiceType>
class DFSServer {
public:
  /**
   * Constructor
   * @param service Pointer to the gRPC service implementation
   * @param address Server address (e.g., "0.0.0.0:50051")
   */
  DFSServer(ServiceType* service, const std::string& address)
      : service_(service), address_(address), server_(nullptr) {}

  /**
   * Start the server
   */
  virtual void Start() {
    ServerBuilder builder;
    builder.AddListeningPort(address_, grpc::InsecureServerCredentials());
    builder.RegisterService(service_);
    server_ = builder.BuildAndStart();
  }

  /**
   * Stop the server gracefully
   */
  virtual void Stop() {
    if (server_) {
      server_->Shutdown();
    }
  }

  /**
   * Force shutdown the server
   */
  void ForceShutdown() {
    if (server_) {
      server_->Shutdown(std::chrono::system_clock::now());
      server_ = nullptr;
    }
  }

  /**
   * Wait for the server to finish (blocks until Stop() is called)
   */
  void Wait() {
    if (server_) {
      server_->Wait();
    }
  }

  /**
   * Check if server is running
   */
  bool IsRunning() const { return server_ != nullptr; }

  /**
   * Virtual destructor for proper inheritance
   */
  virtual ~DFSServer() = default;

protected:
  ServiceType* service_;
  std::string address_;
  std::unique_ptr<Server> server_;
};
