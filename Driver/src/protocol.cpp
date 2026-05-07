#include "protocol.h"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <sstream>

namespace dcn_protocol {

std::vector<std::string> split(const std::string& s) {
    std::vector<std::string> parts;
    std::stringstream ss(s);
    std::string token;
    while (std::getline(ss, token, '|')) {
        parts.push_back(token);
    }
    return parts;
}

std::string to_upper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    return s;
}

ParsedRequest parse_request(const std::string& line) {
    ParsedRequest req;
    req.parts = split(line);
    if (req.parts.empty()) {
        req.type = CommandType::Invalid;
        return req;
    }

    const std::string cmd = to_upper(req.parts[0]);
    if (cmd == "HELP") {
        req.type = CommandType::Help;
    } else if (cmd == "INSERT") {
        req.type = CommandType::Insert;
    } else if (cmd == "UPDATE") {
        req.type = CommandType::Update;
    } else if (cmd == "DELETE") {
        req.type = CommandType::Delete;
    } else if (cmd == "QUERY") {
        req.type = CommandType::Query;
    } else {
        req.type = CommandType::Invalid;
    }

    return req;
}

std::string command_help() {
    return "Commands: INSERT|code|title|section|instructor|time|classroom ; UPDATE|code|title|section|instructor|time|classroom ; DELETE|code ; QUERY|ALL ; QUERY|CODE|code";
}

std::string usage_for(CommandType type) {
    switch (type) {
    case CommandType::Insert:
        return "Usage: INSERT|code|title|section|instructor|time|classroom";
    case CommandType::Update:
        return "Usage: UPDATE|code|title|section|instructor|time|classroom";
    case CommandType::Delete:
        return "Usage: DELETE|code";
    case CommandType::Query:
        return "Usage: QUERY|ALL or QUERY|CODE|code";
    default:
        return "Usage: HELP";
    }
}

static std::string response_tag(ResponseType type) {
    switch (type) {
    case ResponseType::Ok:    
        return "OK";
    case ResponseType::Error: 
        return "ERROR";
    case ResponseType::Data:  
        return "DATA";
    case ResponseType::Row:   
        return "ROW";
    case ResponseType::End:   
        return "END";
    }
    return "ERROR";
}


std::string make_response(ResponseType type, const std::vector<std::string>& fields) {
    std::string response = response_tag(type);
    for (const auto& field : fields) {
        response += "|";
        response += field;
    }
    return response;
}

static std::string join_tail(const std::vector<std::string>& parts, size_t start) {
    if (parts.size() <= start) {
        return "";
    }

    std::string result = parts[start];
    for (size_t i = start + 1; i < parts.size(); ++i) {
        result += "|";
        result += parts[i];
    }
    return result;
}

void print_client_response_line(const std::string& line, std::ostream& out) {
    const auto parts = split(line);
    if (parts.empty()) {
        return;
    }

    if (parts[0] == "OK" && parts.size() >= 2) {
        out << "[OK] " << join_tail(parts, 1) << std::endl;
        return;
    }
    if (parts[0] == "ERROR" && parts.size() >= 2) {
        out << "[ERROR] " << join_tail(parts, 1) << std::endl;
        return;
    }
    if (parts[0] == "DATA" && parts.size() >= 2) {
        out << "[DATA] total=" << parts[1] << std::endl;
        return;
    }
    if (parts[0] == "ROW" && parts.size() >= 7) {
        out << "  - code=" << parts[1]
            << ", title=" << parts[2]
            << ", section=" << parts[3]
            << ", instructor=" << parts[4]
            << ", time=" << parts[5]
            << ", classroom=" << parts[6] << std::endl;
        return;
    }
    if (parts[0] == "END") {
        out << "[DATA] end" << std::endl;
        return;
    }

    out << "[Server] " << line << std::endl;
}

}  // namespace dcn_protocol
