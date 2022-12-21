#include <rpc/errors.hpp>

namespace rpc {

//////////////////////////////////////////////////////////////////////

std::error_code TransportError() {
  return std::make_error_code(std::errc::connection_reset);
}

std::error_code Cancelled() {
  return std::make_error_code(std::errc::operation_canceled);
}

//////////////////////////////////////////////////////////////////////

bool IsRetriableError(std::error_code ec) {
  return ec == std::errc::connection_reset;
}

}  // namespace rpc
