#include "ailo_schema_internal.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <json-schema-to-grammar.h>
#include <nlohmann/json.hpp>
#include <set>

namespace ailo {
namespace client {
namespace {
using json = nlohmann::ordered_json;
[[noreturn]] void invalid(const char *message) {
  throw failure(AILO_INVALID_ARGUMENT, message);
}
[[noreturn]] void unsupported() {
  throw failure(AILO_UNSUPPORTED,
                "JSON schema uses an unsupported constraint or combination");
}

bool annotation(const std::string &key) {
  return key == "title" || key == "description" || key == "default" ||
         key == "examples" || key == "$defs" || key == "definitions" ||
         key == "$schema";
}
bool type_matches(const json &value, const std::string &type) {
  if (type == "object")
    return value.is_object();
  if (type == "array")
    return value.is_array();
  if (type == "string")
    return value.is_string();
  if (type == "number")
    return value.is_number();
  if (type == "integer")
    return value.is_number_integer() ||
           (value.is_number_float() &&
            std::floor(value.get<double>()) == value.get<double>());
  if (type == "boolean")
    return value.is_boolean();
  if (type == "null")
    return value.is_null();
  return false;
}

class normalizer {
  const json &root;
  size_t nodes = 0;
  std::set<const json *> active;

  static int64_t count(const json &value) {
    if (!value.is_number_integer() || value.get<double>() < 0 ||
        value.get<double>() > 4096)
      invalid("JSON schema array bounds must be integers in [0, 4096]");
    return value.get<int64_t>();
  }

public:
  explicit normalizer(const json &input) : root(input) {}

  json visit(const json &input, size_t depth = 0) {
    if (++nodes > 4096 || depth > 32)
      throw failure(AILO_OUT_OF_RANGE,
                    "Expanded JSON schema exceeds its node/depth budget");
    if (!input.is_object())
      invalid("JSON schemas must be objects");
    if (!active.insert(&input).second)
      throw failure(AILO_UNSUPPORTED,
                    "Recursive JSON schema references are unsupported");
    struct leave {
      std::set<const json *> &active;
      const json *node;
      ~leave() { active.erase(node); }
    } guard{active, &input};
    json out = input;
    for (const auto &entry : input.items()) {
      const auto &key = entry.key();
      if (!annotation(key) && key != "type" && key != "$ref" &&
          key != "anyOf" && key != "const" && key != "enum" &&
          key != "properties" && key != "required" &&
          key != "additionalProperties" && key != "items" &&
          key != "minItems" && key != "maxItems" && key != "minimum" &&
          key != "maximum")
        unsupported();
      if (annotation(key))
        out.erase(key);
    }
    if (input.contains("$schema") &&
        input["$schema"] != "https://json-schema.org/draft/2020-12/schema" &&
        input["$schema"] != "http://json-schema.org/draft-07/schema#")
      unsupported();
    for (const char *key : {"title", "description"})
      if (input.contains(key) && !input[key].is_string())
        invalid("JSON schema title and description must be strings");
    if (input.contains("examples") && !input["examples"].is_array())
      invalid("JSON schema examples must be an array");
    for (const char *key : {"$defs", "definitions"})
      if (input.contains(key) && !input[key].is_object())
        invalid("JSON schema definitions must be an object");
    if (out.contains("$ref")) {
      if (out.size() != 1 || !out["$ref"].is_string())
        unsupported();
      const auto ref = out["$ref"].get<std::string>();
      if (ref.empty() || ref[0] != '#')
        unsupported();
      try {
        return visit(root.at(json::json_pointer(ref.substr(1))), depth + 1);
      } catch (const json::exception &) {
        invalid("JSON schema reference does not resolve in this document");
      }
    }
    if (out.contains("anyOf")) {
      if (out.size() != 1)
        unsupported();
      const auto &alts = out["anyOf"];
      if (!alts.is_array() || alts.empty() || alts.size() > 32)
        invalid("JSON schema anyOf needs 1 to 32 alternatives");
      for (auto &alt : out["anyOf"])
        alt = visit(alt, depth + 1);
      return out;
    }
    if (out.contains("type") && out["type"].is_array()) {
      if (out.contains("const") || out.contains("enum"))
        unsupported();
      const auto types = out["type"];
      if (types.empty() || types.size() > 7)
        invalid("Invalid JSON schema type array");
      json alts = json::array();
      std::set<std::string> unique;
      for (const auto &type : types) {
        if (!type.is_string() || !unique.insert(type.get<std::string>()).second)
          invalid("JSON schema types must be unique strings");
        json alt = out;
        alt["type"] = type;
        // Structural constraints apply to their corresponding type only.
        const std::string name = type.get<std::string>();
        if (name != "object")
          for (const char *key :
               {"properties", "required", "additionalProperties"})
            alt.erase(key);
        if (name != "array")
          for (const char *key : {"items", "minItems", "maxItems"})
            alt.erase(key);
        if (name != "integer" && name != "number") {
          alt.erase("minimum");
          alt.erase("maximum");
        }
        alts.push_back(visit(alt, depth + 1));
      }
      return json{{"anyOf", std::move(alts)}};
    }
    const std::string type = out.value("type", std::string{});
    if (!type.empty() && type != "object" && type != "array" &&
        type != "string" && type != "number" && type != "integer" &&
        type != "boolean" && type != "null")
      invalid("Unknown JSON schema type");
    if (out.contains("const") || out.contains("enum")) {
      const bool constant = out.contains("const");
      if (out.size() != (type.empty() ? 1u : 2u))
        unsupported();
      json values = constant ? json::array({out["const"]}) : out["enum"];
      if (!values.is_array() || values.empty() || values.size() > 128)
        invalid("JSON schema enum needs 1 to 128 values");
      json valid = json::array();
      for (const auto &value : values)
        if (type.empty() || type_matches(value, type))
          valid.push_back(value);
      if (valid.empty())
        invalid("JSON schema const/enum contradicts its type");
      return constant ? json{{"const", valid.front()}}
                      : json{{"enum", std::move(valid)}};
    }
    if (out.contains("properties") || out.contains("required") ||
        out.contains("additionalProperties")) {
      if (type != "object")
        unsupported();
    }
    if (type == "object") {
      if (!out.contains("properties"))
        out["properties"] = json::object();
      if (!out["properties"].is_object() || out["properties"].size() > 256)
        invalid(
            "JSON schema properties must be an object of at most 256 members");
      for (auto &entry : out["properties"].items()) {
        if (entry.key().size() > 1024)
          invalid("JSON schema property name exceeds 1024 bytes");
        entry.value() = visit(entry.value(), depth + 1);
      }
      if (out.contains("required")) {
        const auto &required = out["required"];
        if (!required.is_array() || required.size() > 256)
          invalid("Invalid required property array");
        std::set<std::string> names;
        for (const auto &value : required) {
          if (!value.is_string() ||
              !names.insert(value.get<std::string>()).second)
            invalid("Required properties must be unique strings");
          if (!out["properties"].contains(value.get<std::string>()))
            unsupported();
        }
      }
      // The pinned helper otherwise makes additionalProperties implicitly false
      // when properties is present. JSON Schema defaults it to true.
      if (!out.contains("additionalProperties"))
        out["additionalProperties"] = true;
      auto &extra = out["additionalProperties"];
      if (extra.is_object())
        extra = visit(extra, depth + 1);
      else if (!extra.is_boolean())
        invalid("Invalid additionalProperties schema");
    }
    if (out.contains("items") || out.contains("minItems") ||
        out.contains("maxItems")) {
      if (type != "array")
        unsupported();
    }
    if (type == "array") {
      if (out.contains("items"))
        out["items"] = visit(out["items"], depth + 1);
      const auto low = out.contains("minItems") ? count(out["minItems"]) : 0;
      const auto high =
          out.contains("maxItems") ? count(out["maxItems"]) : 4096;
      if (low > high)
        invalid("JSON schema minItems exceeds maxItems");
      if (!out.contains("items"))
        out["items"] = json::object();
    }
    if (out.contains("minimum") || out.contains("maximum")) {
      if (type != "integer")
        unsupported(); // Native number bounds are ignored.
      for (const char *key : {"minimum", "maximum"}) {
        if (!out.contains(key))
          continue;
        if (!out[key].is_number_integer() ||
            std::abs(out[key].get<double>()) > 1000000000000000.0)
          invalid("Integer schema bounds must be integers within +/-10^15");
      }
      if (out.contains("minimum") && out.contains("maximum") &&
          out["minimum"].get<int64_t>() > out["maximum"].get<int64_t>())
        invalid("JSON schema minimum exceeds maximum");
    }
    return out;
  }
};

lonejson_status discard(void *, const void *, size_t, lonejson_error *) {
  return LONEJSON_STATUS_OK;
}
void checked(lonejson_status status) {
  if (status != LONEJSON_STATUS_OK)
    throw failure(
        status == LONEJSON_STATUS_ALLOCATION_FAILED ? AILO_NO_MEMORY
                                                    : AILO_MODEL_FAILED,
        "Constrained output is invalid, incomplete or exceeds its JSON limits");
}
} // namespace

std::string schema_document(const char *source, bool normalize) {
  try {
    json schema = {{"type", "object"}};
    if (source) {
      const size_t size = strnlen(source, 65537);
      if (size > 65536)
        throw failure(AILO_OUT_OF_RANGE, "JSON schema exceeds 64 KiB");
      require_utf8(source, size);
      auto config = lonejson_default_config();
      config.max_depth = 64;
      std::unique_ptr<lonejson, decltype(&lonejson_free)> runtime(
          lonejson_new(&config, nullptr), lonejson_free);
      if (!runtime)
        throw std::bad_alloc();
      if (lonejson_validate_buffer(runtime.get(), source, size, nullptr) !=
          LONEJSON_STATUS_OK)
        invalid("JSON schema is malformed or exceeds JSON limits");
      std::vector<std::set<std::string>> objects;
      schema = json::parse(
          source, source + size,
          [&](int, json::parse_event_t event, json &value) {
            if (event == json::parse_event_t::object_start)
              objects.emplace_back();
            if (event == json::parse_event_t::key &&
                !objects.back().insert(value.get<std::string>()).second)
              invalid("JSON schema contains duplicate keys");
            if (event == json::parse_event_t::object_end)
              objects.pop_back();
            return true;
          });
    }
    if (!schema.is_object())
      invalid("JSON schemas must be objects");
    return (normalize ? normalizer(schema).visit(schema) : schema).dump();
  } catch (const failure &) {
    throw;
  } catch (const std::bad_alloc &) {
    throw;
  } catch (...) {
    invalid("JSON schema cannot be compiled by the pinned grammar helper");
  }
}

std::string openai_tool_parameters(const char *source, int strict,
                                   bool responses, bool &effective_strict) {
  const auto original = schema_document(source, false);
  effective_strict = false;
  if (strict == 0 || (strict < 0 && !responses))
    return original;
  try {
    auto schema = json::parse(original);
    if (!schema.contains("type") || schema["type"] != "object")
      invalid("Strict API function parameters require an object root");
    const bool normalize = strict < 0;
    size_t nodes = 0;
    std::function<void(json &, size_t)> visit = [&](json &node, size_t depth) {
      if (++nodes > 4096 || depth > 32)
        throw failure(AILO_OUT_OF_RANGE,
                      "API tool schema exceeds its node/depth budget");
      if (!node.is_object())
        invalid("API tool schemas must be objects");
      const bool object = node.contains("type") &&
                          (node["type"] == "object" ||
                           (node["type"].is_array() &&
                            std::find(node["type"].begin(), node["type"].end(),
                                      "object") != node["type"].end()));
      if (object) {
        if (node.contains("additionalProperties") &&
            node["additionalProperties"] != false)
          invalid(
              "Strict API object schemas require additionalProperties=false");
        if (!node.contains("additionalProperties")) {
          if (!normalize)
            invalid(
                "Strict API object schemas require additionalProperties=false");
          node["additionalProperties"] = false;
        }
        json required = json::array();
        if (node.contains("properties")) {
          if (!node["properties"].is_object())
            invalid("Invalid API tool properties");
          for (const auto &property : node["properties"].items())
            required.push_back(property.key());
        }
        if (normalize)
          node["required"] = required;
        else {
          auto actual = node.value("required", json::array());
          if (!actual.is_array() || actual.size() != required.size())
            invalid("Strict API object schemas require every property");
          for (const auto &key : required)
            if (std::find(actual.begin(), actual.end(), key) == actual.end())
              invalid("Strict API object schemas require every property");
        }
      }
      for (const char *group : {"properties", "$defs", "definitions"})
        if (node.contains(group)) {
          if (!node[group].is_object())
            invalid("Invalid API tool schema map");
          for (auto &entry : node[group].items())
            visit(entry.value(), depth + 1);
        }
      if (node.contains("items"))
        visit(node["items"], depth + 1);
      if (node.contains("anyOf")) {
        if (!node["anyOf"].is_array())
          invalid("Invalid API tool schema alternatives");
        for (auto &child : node["anyOf"])
          visit(child, depth + 1);
      }
    };
    visit(schema, 0);
    auto serialized = schema.dump();
    (void)schema_document(serialized.c_str(), true);
    effective_strict = true;
    return serialized;
  } catch (const failure &) {
    if (strict > 0)
      throw;
    // Responses' documented omitted-strict fallback preserves the original
    // schema and advertises strict=false. Explicit true never falls back.
    return original;
  } catch (const json::exception &) {
    if (strict > 0)
      invalid("Strict API function schema is invalid");
    return original;
  }
}

std::string output_grammar(const ailo_request &request) {
  if (request.output_format < AILO_OUTPUT_TEXT ||
      request.output_format > AILO_OUTPUT_JSON_SCHEMA)
    invalid("Unknown output format");
  if (request.output_format != AILO_OUTPUT_JSON_SCHEMA && request.json_schema)
    invalid("JSON schema requires JSON_SCHEMA output format");
  if (request.output_format == AILO_OUTPUT_TEXT)
    return {};
  if (request.output_format == AILO_OUTPUT_JSON_SCHEMA && !request.json_schema)
    invalid("JSON_SCHEMA output requires a schema");
  auto grammar = json_schema_to_grammar(
      common_json::parse(schema_document(request.json_schema, true)), true);
  if (grammar.size() > 4 * 1024 * 1024)
    throw failure(AILO_OUT_OF_RANGE, "Compiled JSON grammar exceeds 4 MiB");
  return grammar;
}

json_validation::json_validation(size_t limit) {
  auto config = lonejson_default_config();
  config.max_alloc_bytes = 4 * 1024 * 1024;
  config.max_depth = config.json_value_max_depth = 64;
  config.json_value_max_total_bytes = limit;
  config.json_value_max_string_bytes = limit;
  config.json_value_max_key_bytes = 1024;
  config.json_value_max_number_bytes = 1024;
  config.write_max_output_bytes = limit;
  runtime.reset(lonejson_new(&config, nullptr));
  if (!runtime)
    throw std::bad_alloc();
  lonejson_writer_value_stream_init(&stream);
  try {
    checked(lonejson_writer_init_sink(runtime.get(), &writer, discard, nullptr,
                                      nullptr));
    checked(stream.open(&stream, &writer, nullptr));
  } catch (...) {
    lonejson_writer_value_stream_cleanup(&stream);
    lonejson_writer_cleanup(&writer);
    throw;
  }
}
json_validation::~json_validation() {
  lonejson_writer_value_stream_cleanup(&stream);
  lonejson_writer_cleanup(&writer);
}
void json_validation::push(const char *data, size_t size) {
  checked(stream.push(&stream, data, size, nullptr));
}
void json_validation::finish() {
  checked(stream.close(&stream, nullptr));
  checked(writer.finish(&writer, nullptr));
}
} // namespace client
} // namespace ailo
