#pragma once

#include <matrix/time_model/time_model.hpp>

namespace paxos {

whirl::matrix::ITimeModelPtr MakeTimeModel();

}  // namespace paxos
