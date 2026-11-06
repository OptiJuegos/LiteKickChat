#ifndef KICKCHAT_JSON_HPP
#define KICKCHAT_JSON_HPP

#include <string>
#include <map>
#include <vector>

struct Json {
    enum Type { NIL, STRING, OBJECT, ARRAY, PRIMITIVE } type;
    std::string value;
    std::map<std::string, Json> obj;
    std::vector<Json> arr;
    Json() : type(NIL) {}
    const Json &get(const std::string &key) const {
        std::map<std::string, Json>::const_iterator it = obj.find(key);
        if (it != obj.end()) return it->second;
        static const Json empty;
        return empty;
    }
    std::string str() const { return value; }
};

bool parseJson(const std::string &text, Json &output);
std::string jsonEscape(const std::string &text);

#endif
