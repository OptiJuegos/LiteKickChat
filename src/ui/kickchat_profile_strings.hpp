#ifndef KICKCHAT_PROFILE_STRINGS_HPP
#define KICKCHAT_PROFILE_STRINGS_HPP

#include "kickchat_strings.hpp"

namespace kickchatProfileText {

enum Key {
    RemoteIdUnavailable, PublicIdLookup, CommandCopyFailed, CommandCopied,
    CommandCopyPrefix, CommandCopySuffix, ActionPending, ModerationUnavailable,
    TimeoutConfirm, BanConfirm, UnbanConfirm, ConfirmModeration,
    AnotherOperation, ModerationStartFailed, ModerationProcessing,
    KickIdPrefix, Unverified, MissingBadges, LastReceived, NoLocalMessages,
    PublicProfileAvailable, RecentMessagesFormat, EmptyMessages,
    ViewOnKick, TimeoutButton, BanButton, UnbanButton,
    PublicIdFound, PublicIdMissing, LookingUpUser, ProfileAvailable,
    KeyCount
};

inline const wchar_t *get(Key key, kickchatText::Language lang = kickchatText::currentLanguage()) {
    static const kickchatText::Translation strings[] = {
        {L"ID remoto no disponible", L"Remote ID unavailable"},
        {L"Consultando ID publico de Kick...", L"Looking up public Kick ID..."},
        {L"No se pudo copiar el comando.", L"Could not copy the command."},
        {L"Comando copiado. Pegalo y envialo en Kick web.", L"Command copied. Paste and send it in Kick web chat."},
        {L"Se copio: ", L"Copied: "},
        {L"\n\nPegalo y envialo en el chat oficial de Kick para aplicar la accion.",
         L"\n\nPaste and send it in Kick's official chat to apply the action."},
        {L"Accion pendiente en Kick", L"Pending action in Kick"},
        {L"No hay ID verificado o permisos para moderar a este usuario.",
         L"No verified ID or permissions to moderate this user."},
        {L"Aplicar timeout de 5 minutos a ", L"Apply a 5-minute timeout to "},
        {L"Banear a ", L"Ban "},
        {L"Desbanear a ", L"Unban "},
        {L"Confirmar moderacion", L"Confirm moderation"},
        {L"Hay otra operacion en curso", L"Another operation is in progress"},
        {L"No se pudo iniciar la moderacion", L"Could not start moderation"},
        {L"Procesando en Kick; consulta el resultado en el chat",
         L"Processing on Kick; check the result in chat"},
        {L"Kick ID: ", L"Kick ID: "},
        {L"sin verificar", L"unverified"},
        {L"Badges: no informados por Kick", L"Badges: not provided by Kick"},
        {L"Ultimo mensaje recibido: ", L"Last message received: "},
        {L"Sin mensajes locales", L"No local messages"},
        {L"Perfil publico disponible en Kick", L"Public profile available on Kick"},
        {L"Mensajes locales recientes (%u)", L"Recent local messages (%u)"},
        {L"No hay mensajes guardados de este usuario. Podes consultar su perfil en Kick.",
         L"No saved messages from this user. You can view their profile on Kick."},
        {L"Ver en Kick", L"View on Kick"},
        {L"Timeout 5m", L"Timeout 5m"},
        {L"Ban", L"Ban"},
        {L"Unban", L"Unban"},
        {L"ID obtenido del perfil publico", L"ID retrieved from public profile"},
        {L"No se pudo consultar el ID; usa Ver en Kick", L"Could not retrieve ID; use View on Kick"},
        {L"Buscando ID del usuario...", L"Looking up user ID..."},
        {L"Perfil disponible", L"Profile available"}
    };
    static_assert(sizeof(strings) / sizeof(strings[0]) == KeyCount, "Profile translation mismatch");
    return key >= KeyCount ? L"" : (lang == kickchatText::English ? strings[key].en : strings[key].es);
}

} // namespace kickchatProfileText

#endif
