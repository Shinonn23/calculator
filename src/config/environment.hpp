#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>

namespace math_solver {

    using json = nlohmann::json;

    // Represents a named environment mapping string keys to string values.
    // Invariant: `name` must be unique within the context where environments
    // are managed. The `variables` map is assumed to be small and is not
    // optimized for large-scale lookups. Serialization/deserialization is lossy
    // if variable names or values are not valid UTF-8.
    struct Environment {
        std::string                                  name;
        std::unordered_map<std::string, std::string> variables;

        // Serializes the environment to JSON.
        // Assumes all variable names and values are valid for JSON encoding.
        json                                         to_json() const;

        // Constructs an Environment from JSON, associating it with the given
        // name. Precondition: `j` must conform to the expected schema;
        // otherwise, behavior is undefined.
        static Environment from_json(const std::string& name, const json& j);
    };

} // namespace math_solver