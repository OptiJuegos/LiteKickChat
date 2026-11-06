#ifndef KICKCHAT_CHANNELS_HPP
#define KICKCHAT_CHANNELS_HPP

#include <string>
#include <vector>

std::string normalizeChannel(const std::string &value);
bool loadChannels(std::vector<std::string> &channels);
bool saveChannels(const std::vector<std::string> &channels);

#endif
