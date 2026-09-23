// Prism additions, distributed under the zlib license in license.txt.
#ifndef GAME_CLIENT_PRISM_THEME_H
#define GAME_CLIENT_PRISM_THEME_H

#include <cstddef>
#include <string>

class CConfig;
class IStorage;

namespace PrismTheme
{
constexpr size_t MAX_FILE_SIZE = 16384;
constexpr const char *FILE_PATH = "prism/theme.json";
// Data only: never passes imported content to the console or command interpreter.
std::string Serialize(const CConfig &Config);
bool Parse(const char *pData, size_t Length, CConfig &Config);
bool Save(IStorage *pStorage, const CConfig &Config);
bool Load(IStorage *pStorage, CConfig &Config);
}
#endif
