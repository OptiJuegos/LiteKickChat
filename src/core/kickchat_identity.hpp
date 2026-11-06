#ifndef KICKCHAT_IDENTITY_HPP
#define KICKCHAT_IDENTITY_HPP

namespace kickchatIdentity {

// DPAPI decryption does not depend on the stored description.
static const wchar_t oauthDescription[] = L"KickChat OAuth";
static const wchar_t mainWindowClass[] = L"KickChatNativeWindow";
static const wchar_t profileWindowClass[] = L"KickChatProfileWindow";

} // namespace kickchatIdentity

#endif
