#pragma once

#include <kv/types.hpp>

#include <muesli/serializable.hpp>
#include <muesli/empty.hpp>

namespace kv {

//////////////////////////////////////////////////////////////////////

struct Set {
  // Request

  struct Request {
    Key key;
    Value value;

    MUESLI_SERIALIZABLE(key, value);
  };

  // Response

  using Response = muesli::EmptyMessage;

  // Meta

  static std::string Type() {
    return "Set";
  }

  static bool ReadOnly() {
    return false;
  }
};

//////////////////////////////////////////////////////////////////////

struct Get {
  // Request

  struct Request {
    Key key;

    MUESLI_SERIALIZABLE(key)
  };

  // Response

  struct Response {
    Value value;

    MUESLI_SERIALIZABLE(value)
  };

  // Meta

  static std::string Type() {
    return "Get";
  }

  static bool ReadOnly() {
    return true;
  }
};

//////////////////////////////////////////////////////////////////////

struct Cas {
  // Request

  struct Request {
    Key key;
    Value expected_value;
    Value target_value;

    MUESLI_SERIALIZABLE(key, expected_value, target_value);
  };

  // Response

  struct Response {
    Value old_value;

    MUESLI_SERIALIZABLE(old_value);
  };

  // Meta

  static std::string Type() {
    return "Cas";
  }

  static bool ReadOnly() {
    return false;
  }
};

}  // namespace kv
