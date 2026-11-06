#include "kickchat_json.hpp"

static void appendUtf8(std::string &s, unsigned cp) {
    if (cp <= 0x7F) s += (char)cp;
    else if (cp <= 0x7FF) { s += (char)(0xC0 | (cp >> 6)); s += (char)(0x80 | (cp & 63)); }
    else if (cp <= 0xFFFF) { s += (char)(0xE0 | (cp >> 12)); s += (char)(0x80 | ((cp >> 6) & 63)); s += (char)(0x80 | (cp & 63)); }
    else if (cp <= 0x10FFFF) { s += (char)(0xF0 | (cp >> 18)); s += (char)(0x80 | ((cp >> 12) & 63)); s += (char)(0x80 | ((cp >> 6) & 63)); s += (char)(0x80 | (cp & 63)); }
}

// Pusher may provide the event payload as a nested JSON string.
struct JsonParser {
    const std::string &s; size_t i; bool ok;
    JsonParser(const std::string &src): s(src), i(0), ok(true) {}
    void ws() { while (i<s.size() && (s[i]==' ' || s[i]=='\n' || s[i]=='\r' || s[i]=='\t')) ++i; }
    int hex() {
        int v=0;
        for (int k=0;k<4;k++) {
            if (i>=s.size()) { ok=false; return 0; }
            char c=s[i++]; int d=(c>='0'&&c<='9')?c-'0':(c>='a'&&c<='f')?c-'a'+10:(c>='A'&&c<='F')?c-'A'+10:-1;
            if (d<0) {ok=false; return 0;} v=v*16+d;
        }
        return v;
    }
    std::string str() {
        std::string out;
        if (i>=s.size() || s[i++]!='"') { ok=false; return out; }
        bool closed=false;
        while (i<s.size() && ok) {
            unsigned char c=(unsigned char)s[i++];
            if (c=='"') { closed=true; break; }
            if (c<32) { ok=false; break; }
            if (c!='\\') { out += (char)c; continue; }
            if (i>=s.size()) {ok=false; break;}
            c=(unsigned char)s[i++];
            switch(c) {
                case '"': out+='"'; break; case '\\': out+='\\'; break;
                case '/': out+='/'; break; case 'b': out+='\b'; break;
                case 'f': out+='\f'; break; case 'n': out+='\n'; break;
                case 'r': out+='\r'; break; case 't': out+='\t'; break;
                case 'u': {
                    unsigned cp=(unsigned)hex();
                    if (cp>=0xD800 && cp<=0xDBFF && i+2<=s.size() && s[i]=='\\' && s[i+1]=='u') {
                        i+=2; unsigned lo=(unsigned)hex();
                        if (lo>=0xDC00 && lo<=0xDFFF) cp=0x10000+((cp-0xD800)<<10)+(lo-0xDC00);
                        else { ok=false; break; }
                    }
                    if (cp>=0xD800 && cp<=0xDFFF) { ok=false; break; }
                    appendUtf8(out,cp); break;
                }
                default: ok=false; break;
            }
        }
        if (!closed) ok=false;
        return out;
    }
    Json val(int depth=0) {
        Json out; ws();
        if (!ok || depth>32 || i>=s.size()) {ok=false; return out;}
        char c=s[i];
        if (c=='"') {out.type=Json::STRING; out.value=str(); return out;}
        if (c=='{') {
            out.type=Json::OBJECT; ++i; ws();
            if (i<s.size()&&s[i]=='}') {++i; return out;}
            while (ok) {
                ws(); if (i>=s.size()||s[i]!='"') {ok=false;break;}
                std::string key=str(); ws();
                if (i>=s.size()||s[i++]!=':') {ok=false;break;}
                out.obj[key]=val(depth+1); ws();
                if (i>=s.size()) {ok=false;break;}
                char delim=s[i++]; if (delim=='}') break;
                if (delim!=',') {ok=false;break;}
            }
            return out;
        }
        if (c=='[') {
            out.type=Json::ARRAY; ++i; ws();
            if (i<s.size()&&s[i]==']') {++i;return out;}
            while (ok) {
                out.arr.push_back(val(depth+1)); ws();
                if (i>=s.size()) {ok=false;break;}
                char delim=s[i++]; if (delim==']') break;
                if (delim!=',') {ok=false;break;}
            }
            return out;
        }
        if (c=='-' || (c>='0'&&c<='9') || c=='t' || c=='f' || c=='n') {
            out.type=Json::PRIMITIVE; size_t begin=i;
            while (i<s.size() && s[i]!=',' && s[i]!=']' && s[i]!='}' && s[i]!=' ' && s[i]!='\n' && s[i]!='\r') ++i;
            out.value=s.substr(begin,i-begin); if (out.value.empty()) ok=false;
            return out;
        }
        ok=false; return out;
    }
    bool parse(Json &dst) { dst=val(); ws(); return ok && i==s.size(); }
};
bool parseJson(const std::string &s, Json &out) { JsonParser p(s); return p.parse(out); }

std::string jsonEscape(const std::string &s) {
    const char *digits="0123456789abcdef"; std::string v="\"";
    for(size_t i=0;i<s.size();++i) {unsigned char c=(unsigned char)s[i];
        if(c=='"'||c=='\\'){v+='\\';v+=(char)c;}
        else if(c<32){v+="\\u00";v+=digits[c>>4];v+=digits[c&15];}
        else v+=(char)c;
    } return v+'"';
}
