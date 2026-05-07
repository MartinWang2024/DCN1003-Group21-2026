#pragma once

#include <iosfwd>
#include <string>
#include <vector>

namespace dcn_protocol {

struct Course {
    std::string code;
    std::string title;
    std::string section;
    std::string instructor;
    std::string time;
    std::string classroom;
};

enum class CommandType {
    Invalid,
    Help,
    Insert,
    Update,
    Delete,
    Query,
};

enum class ResponseType {
    Ok,
    Error,
    Data,
    Row,
    End,
};

struct ParsedRequest {
    CommandType type = CommandType::Invalid;
    std::vector<std::string> parts;
};

std::vector<std::string> split(const std::string& s);

std::string to_upper(std::string s);

ParsedRequest parse_request(const std::string& line);

std::string command_help();

std::string usage_for(CommandType type);

std::string make_response(ResponseType type, const std::vector<std::string>& fields = {});

void print_client_response_line(const std::string& line, std::ostream& out);

}  // namespace dcn_protocol
