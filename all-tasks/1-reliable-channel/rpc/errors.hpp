#pragma once

#include <wheels/support/result.hpp>

#include <system_error>

namespace rpc {

//////////////////////////////////////////////////////////////////////

std::error_code TransportError();
std::error_code Cancelled();

//////////////////////////////////////////////////////////////////////

bool IsRetriableError(std::error_code ec);

}  // namespace rpc
