#include "common/access_log/grpc_access_log_impl.h"

#include "common/grpc/async_client_impl.h"

namespace Envoy {
namespace AccessLog {

GrpcAccessLogStreamer::GrpcAccessLogStreamer(Upstream::ClusterManager& cluster_manager,
                                             ThreadLocal::SlotAllocator& tls,
                                             const std::string& cluster_name)
    : tls_slot_(tls.allocateSlot()) {

  tls_slot_->set([&cluster_manager, cluster_name](Event::Dispatcher&) {
    return ThreadLocal::ThreadLocalObjectSharedPtr{
        new ThreadLocalStreamer(cluster_manager, cluster_name)};
  });
}

GrpcAccessLogStreamer::ThreadLocalStreamer::ThreadLocalStreamer(
    Upstream::ClusterManager& cluster_manager, const std::string& cluster_name)
    : client_(
          new Grpc::AsyncClientImpl<envoy::api::v2::filter::accesslog::StreamAccessLogsMessage,
                                    envoy::api::v2::filter::accesslog::StreamAccessLogsResponse>(
              cluster_manager, cluster_name)) {}

void GrpcAccessLogStreamer::ThreadLocalStreamer::send(
    envoy::api::v2::filter::accesslog::StreamAccessLogsMessage& message,
    const std::string& log_name) {
  auto& stream_entry = stream_map_[log_name];
  if (stream_entry == nullptr) {
    stream_entry =
        client_->start(*Protobuf::DescriptorPool::generated_pool()->FindMethodByName(
                           "envoy.api.v2.filter.accesslog.AccessLogService.StreamAccessLogs"),
                       *this);

    // fixfix
    message.mutable_identifier()->set_log_name("fixfix");
  }

  if (stream_entry) {
    stream_entry->sendMessage(message, false);
  }
}

void GrpcAccessLogStreamer::ThreadLocalStreamer::onRemoteClose(Grpc::Status::GrpcStatus,
                                                               const std::string&) {
  // fixfix
}

HttpGrpcAccessLog::HttpGrpcAccessLog(
    FilterPtr&& filter, const envoy::api::v2::filter::accesslog::HttpGrpcAccessLogConfig& config,
    GrpcAccessLogStreamerSharedPtr grpc_access_log_streamer)
    : filter_(std::move(filter)), config_(config),
      grpc_access_log_streamer_(grpc_access_log_streamer) {}

void HttpGrpcAccessLog::log(const Http::HeaderMap* request_headers,
                            const Http::HeaderMap* response_headers,
                            const RequestInfo& request_info) {
  static Http::HeaderMapImpl empty_headers;
  if (!request_headers) {
    request_headers = &empty_headers;
  }
  if (!response_headers) {
    response_headers = &empty_headers;
  }

  if (filter_) {
    if (!filter_->evaluate(request_info, *request_headers)) {
      return;
    }
  }

  envoy::api::v2::filter::accesslog::StreamAccessLogsMessage message;
  grpc_access_log_streamer_->send(message, config_.common_config().log_name());
}

} // namespace AccessLog
} // namespace Envoy
