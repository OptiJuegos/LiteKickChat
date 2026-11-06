#include "kickchat_category_lookup.hpp"
#include "kickchat_oauth.hpp"
#include "kickchat_category.hpp"
#include "kickchat_api_http.hpp"
#include "kickchat_json.hpp"
#include "kickchat_strings.hpp"

bool lookupKickCategory(const std::wstring &requested, std::string &categoryId,
                        std::wstring &categoryName, std::wstring &error) {
    std::string name=utf8FromWide(requested);
    if(requested.size()<3 || requested.size()>100 || name.empty()) {
        error=kickchatText::get(kickchatText::SearchCategoryLength);
        return false;
    }
    std::vector<kickcategory::Candidate> candidates;
    kickcategory::Candidate selected;
    std::vector<kickcategory::Candidate> suggestions;
    const std::vector<std::wstring> searches=kickcategory::searchTerms(requested);
    bool legacyAvailable=true;
    for(size_t i=0;i<searches.size();++i) {
        const std::string query=utf8FromWide(searches[i]);
        const std::wstring path=toWide("/public/v1/categories?q="+formEncode(query));
        HttpResult r=kickAuthorized(path.c_str(),L"GET","",error);
        if(r.status==404 || r.status==410) {legacyAvailable=false;error.clear();break;}
        if(r.status!=200 || !r.error.empty()) {
            if(error.empty())error=kickchatText::get(kickchatText::SearchCategoryError)+apiError(r);
            return false;
        }
        Json response;
        if(!parseJson(r.body,response) ||
           !kickcategory::appendCandidates(response,requested,candidates)) {
            error=kickchatText::get(kickchatText::SearchCategoryInvalid);return false;
        }
        if(kickcategory::chooseCandidate(requested,candidates,selected,suggestions)==
           kickcategory::CLEAR_MATCH) {
            categoryId=selected.id;categoryName=selected.name;return true;
        }
    }

    // Keep the v2 name endpoint as a fallback if the older full-text search returns nothing.
    if(candidates.empty() || !legacyAvailable) {
        std::string path="/public/v2/categories?name="+formEncode(name)+"&limit=100";
        for(unsigned page=0;page<20;++page) {
            HttpResult r=kickAuthorized(toWide(path).c_str(),L"GET","",error);
            if(r.status!=200 || !r.error.empty()) {
                if(error.empty())error=kickchatText::get(kickchatText::SearchCategoryError)+apiError(r);
                return false;
            }
            Json response;
            if(!parseJson(r.body,response) ||
               !kickcategory::appendCandidates(response,requested,candidates)) {
                error=kickchatText::get(kickchatText::SearchCategoryInvalid);return false;
            }
            if(kickcategory::chooseCandidate(requested,candidates,selected,suggestions)==
               kickcategory::CLEAR_MATCH) {
                categoryId=selected.id;categoryName=selected.name;return true;
            }
            const Json &cursorValue=response.get("pagination").get("next_cursor");
            if(cursorValue.type==Json::NIL || cursorValue.value.empty() ||
               (cursorValue.type==Json::PRIMITIVE && cursorValue.value=="null")) break;
            if(cursorValue.type!=Json::STRING || cursorValue.value.size()>28 ||
               cursorValue.value.size()<4) {
                error=kickchatText::get(kickchatText::SearchCursorInvalid);return false;
            }
            path="/public/v2/categories?name="+formEncode(name)+
                 "&limit=100&cursor="+formEncode(cursorValue.value);
        }
    }

    const kickcategory::Choice choice=kickcategory::chooseCandidate(
        requested,candidates,selected,suggestions);
    if(choice==kickcategory::CLEAR_MATCH) {
        categoryId=selected.id;categoryName=selected.name;return true;
    }
    if(choice==kickcategory::NEEDS_CHOICE && !suggestions.empty()) {
        error=kickchatText::get(kickchatText::SearchMultiple)+requested+
              kickchatText::get(kickchatText::SearchChoose);
        for(size_t i=0;i<suggestions.size() && i<3;++i) {
            if(i)error+=L" | ";
            error+=L"/game "+suggestions[i].name;
        }
        return false;
    }
    error=kickchatText::get(kickchatText::SearchNotFound)+requested;
    return false;
}
