#ifndef KICKCHAT_COMMANDS_HPP
#define KICKCHAT_COMMANDS_HPP
// Local command parser. No Windows SDK dependency, C++11 compatible.
#include <string>
#include <cwctype>
#include "kickchat_strings.hpp"

namespace kickcmd {
enum Kind { CHAT, HELP, TITLE, BAN, TIMEOUT, UNBAN, USER, GAME,
            MOD, UNMOD, VIP, UNVIP, INVALID };
struct Command {
    Kind kind;
    std::wstring original, target, value, error;
    unsigned minutes;
    Command() : kind(CHAT), minutes(0) {}
};
inline bool space(wchar_t c) { return c==L' ' || c==L'\t' || c==L'\r' || c==L'\n'; }
inline std::wstring trim(const std::wstring& s) {
    size_t b=0,e=s.size();
    while(b<e && space(s[b])) ++b;
    while(e>b && space(s[e-1])) --e;
    return s.substr(b,e-b);
}
inline std::wstring lower(std::wstring s) {
    for(size_t i=0;i<s.size();++i) s[i]=(wchar_t)std::towlower(s[i]);
    return s;
}
inline std::wstring takeWord(std::wstring &s) {
    s=trim(s); size_t i=0;
    while(i<s.size() && !space(s[i])) ++i;
    std::wstring word=s.substr(0,i);
    s=trim(s.substr(i)); return word;
}
inline bool validUsername(const std::wstring &value) {
    if (value.empty() || value.size() > 25) return false;
    for (size_t i = 0; i < value.size(); ++i) {
        const wchar_t c = value[i];
        if (!((c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') ||
              (c >= L'0' && c <= L'9') || c == L'_')) return false;
    }
    return true;
}
inline Command parse(const std::wstring &input) {
    Command out;out.original=trim(input);
    if(out.original.empty() || out.original[0]!=L'/') return out;
    std::wstring rest=out.original;
    std::wstring name=lower(takeWord(rest));
    if(name==L"/help" || name==L"/comandos") {
        if(!rest.empty()) {out.kind=INVALID;out.error=kickchatText::get(kickchatText::HelpUsage);}
        else out.kind=HELP;
    } else if(name==L"/title") {
        out.kind=TITLE;out.value=rest;
        if(out.value.empty()) {out.kind=INVALID;out.error=kickchatText::get(kickchatText::TitleUsage);}
    } else if(name==L"/game") {
        out.kind=GAME;out.value=rest;
        if(out.value.empty()) {out.kind=INVALID;out.error=kickchatText::get(kickchatText::GameUsage);}
    } else if(name==L"/ban" || name==L"/timeout" || name==L"/unban" || name==L"/user" ||
              name==L"/mod" || name==L"/unmod" || name==L"/vip" || name==L"/unvip") {
        out.kind=name==L"/ban"?BAN:name==L"/timeout"?TIMEOUT:name==L"/unban"?UNBAN:
                 name==L"/user"?USER:name==L"/mod"?MOD:name==L"/unmod"?UNMOD:
                 name==L"/vip"?VIP:UNVIP;
        out.target=takeWord(rest);
        if(!out.target.empty() && out.target[0]==L'@') out.target.erase(0,1);
        if(out.target.empty()) {
            out.kind=INVALID;out.error=kickchatText::get(kickchatText::MissingUser);return out;
        }
        if(out.kind==BAN) out.value=rest;
        if(out.kind==UNBAN || out.kind==USER || out.kind==MOD || out.kind==UNMOD ||
           out.kind==VIP || out.kind==UNVIP) {
            if(!rest.empty()) {out.kind=INVALID;out.error=kickchatText::get(kickchatText::OnlyUser);}
        } else if(out.kind==TIMEOUT) {
            std::wstring duration=takeWord(rest);
            if(duration.empty()) {out.kind=INVALID;out.error=kickchatText::get(kickchatText::TimeoutUsage);return out;}
            unsigned n=0;
            for(size_t i=0;i<duration.size();++i){
                if(duration[i]<L'0'||duration[i]>L'9'||n>10080u) {
                    out.kind=INVALID;out.error=kickchatText::get(kickchatText::TimeoutRangeLong);return out;
                }
                n=n*10+(unsigned)(duration[i]-L'0');
            }
            if(n<1||n>10080) {out.kind=INVALID;out.error=kickchatText::get(kickchatText::TimeoutRange);return out;}
            out.minutes=n;out.value=rest;
        }
    } else {
        // Unknown slash commands are NOT sent to Kick as ordinary chat messages.
        out.kind=INVALID;out.error=kickchatText::get(kickchatText::UnknownCommand);
    }
    return out;
}
} // namespace kickcmd
#endif
