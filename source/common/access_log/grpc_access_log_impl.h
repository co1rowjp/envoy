#pragma once

#include "envoy/access_log/access_log.h"
#include "envoy/grpc/async_client.h"
#include "envoy/singleton/instance.h"
#include "envoy/thread_local/thread_local.h"
#include "envoy/upstream/cluster_manager.h"

#include "api/filter/accesslog/accesslog.pb.h"

namespace Envoy {
namespace AccessLog {

/**
 * fixfix
 */
class GrpcAccessLogStreamer : public Singleton::Instance {
public:
  GrpcAccessLogStreamer(Upstream::ClusterManager& cluster_manager, ThreadLocal::SlotAllocator& tls,
                        const std::string& cluster_name);

  void send(envoy::api::v2::filter::accesslog::StreamAccessLogsMessage& message,
            const std::string& log_name) {
    tls_slot_->getTyped<ThreadLocalStreamer>().send(message, log_name);
  }

private:
  struct ThreadLocalStreamer : public ThreadLocal::ThreadLocalObject,
                               public Grpc::AsyncStreamCallbacks<
                                   envoy::api::v2::filter::accesslog::StreamAccessLogsResponse> {
    ThreadLocalStreamer(Upstream::ClusterManager& cluster_manager, const std::string& cluster_name);
    void send(envoy::api::v2::filter::accesslog::StreamAccessLogsMessage& message,
              const std::string& log_name);

    // Grpc::AsyncStreamCallbacks
    void onCreateInitialMetadata(Http::HeaderMap&) override {}
    void onReceiveInitialMetadata(Http::HeaderMapPtr&&) override {}
    void onReceiveMessage(
        std::unique_ptr<envoy::api::v2::filter::accesslog::StreamAccessLogsResponse>&&) override {}
    void onReceiveTrailingMetadata(Http::HeaderMapPtr&&) override {}
    void onRemoteClose(Grpc::Status::GrpcStatus status, const std::string& message) override;

    std::unique_ptr<Grpc::AsyncClient<envoy::api::v2::filter::accesslog::StreamAccessLogsMessage,
                                      envoy::api::v2::filter::accesslog::StreamAccessLogsResponse>>
        client_;
    std::unordered_map<
        std::string, Grpc::AsyncStream<envoy::api::v2::filter::accesslog::StreamAccessLogsMessage>*>
        stream_map_;
  };

  ThreadLocal::SlotPtr tls_slot_;
};

typedef std::shared_ptr<GrpcAccessLogStreamer> GrpcAccessLogStreamerSharedPtr;

/**
 * Access log Instance that streams HTTP logs over gRPC.
 */
class HttpGrpcAccessLog : public Instance {
public:
  HttpGrpcAccessLog(FilterPtr&& filter,
                    const envoy::api::v2::filter::accesslog::HttpGrpcAccessLogConfig& config,
                    GrpcAccessLogStreamerSharedPtr grpc_access_log_streamer);

  // AccessLog::Instance
  void log(const Http::HeaderMap* request_headers, const Http::HeaderMap* response_headers,
           const RequestInfo& request_info) override;

private:
  FilterPtr filter_;
  const envoy::api::v2::filter::accesslog::HttpGrpcAccessLogConfig config_;
  GrpcAccessLogStreamerSharedPtr grpc_access_log_streamer_;
};

} // namespace AccessLog
} // namespace Envoy
