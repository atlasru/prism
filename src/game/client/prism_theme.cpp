// Prism additions, distributed under the zlib license in license.txt.
#include "prism_theme.h"

#include <base/system.h>
#include <engine/shared/config.h>
#include <engine/shared/json.h>
#include <engine/storage.h>

#include <algorithm>
#include <cstring>
#include <limits>
#include <memory>
#include <vector>

namespace PrismTheme
{
std::string Serialize(const CConfig &Config)
{
	std::string Result = "{\n  \"version\": 1";
	auto Add = [&](const char *pName, unsigned Value) {
		if(std::strcmp(pName, "prism_theme_favorites") != 0)
			Result += std::string(",\n  \"") + pName + "\": " + std::to_string(Value);
	};
#define MACRO_CONFIG_INT(Name, ScriptName, Def, Min, Max, Flags, Desc) Add(#ScriptName, std::clamp(Config.m_##Name, Min, Max));
#define MACRO_CONFIG_COL(Name, ScriptName, Def, Flags, Desc) Add(#ScriptName, Config.m_##Name);
#include <engine/shared/prism_theme_variables.h>
#undef MACRO_CONFIG_INT
#undef MACRO_CONFIG_COL
	Add("prism_menu_scale", std::clamp(Config.m_PrismMenuScale, 80, 120));
	Add("prism_panel_opacity", std::clamp(Config.m_PrismPanelOpacity, 50, 100));
	Add("prism_reduced_motion", std::clamp(Config.m_PrismReducedMotion, 0, 1));
	return Result + "\n}\n";
}

bool Parse(const char *pData, size_t Length, CConfig &Config)
{
	if(!pData || Length == 0 || Length > MAX_FILE_SIZE)
		return false;
	std::unique_ptr<json_value, decltype(&json_value_free)> Root(JsonParse(pData, Length), json_value_free);
	if(!Root || Root->type != json_object || Root->u.object.length > 32)
		return false;
	const auto *pVersion = json_object_get(Root.get(), "version");
	if(pVersion->type != json_integer || pVersion->u.integer != 1)
		return false;
	CConfig Candidate = Config;
	for(unsigned i = 0; i < Root->u.object.length; ++i)
	{
		const auto &Member = Root->u.object.values[i];
		for(unsigned j = 0; j < i; ++j)
			if(std::strcmp(Member.name, Root->u.object.values[j].name) == 0)
				return false;
		if(Member.value->type != json_integer)
			return false;
		const auto Value = Member.value->u.integer;
		bool Known = std::strcmp(Member.name, "version") == 0;
		auto Int = [&](const char *pName, int &Target, int Min, int Max) {
			if(std::strcmp(pName, "prism_theme_favorites") != 0 && std::strcmp(Member.name, pName) == 0)
			{
				Target = static_cast<int>(std::clamp<json_int_t>(Value, Min, Max));
				Known = true;
			}
		};
		auto Color = [&](const char *pName, unsigned &Target) {
			if(std::strcmp(Member.name, pName) == 0 && Value >= 0 && Value <= std::numeric_limits<unsigned>::max())
			{
				Target = static_cast<unsigned>(Value);
				Known = true;
			}
		};
#define MACRO_CONFIG_INT(Name, ScriptName, Def, Min, Max, Flags, Desc) Int(#ScriptName, Candidate.m_##Name, Min, Max);
#define MACRO_CONFIG_COL(Name, ScriptName, Def, Flags, Desc) Color(#ScriptName, Candidate.m_##Name);
#include <engine/shared/prism_theme_variables.h>
#undef MACRO_CONFIG_INT
#undef MACRO_CONFIG_COL
		Int("prism_menu_scale", Candidate.m_PrismMenuScale, 80, 120);
		Int("prism_panel_opacity", Candidate.m_PrismPanelOpacity, 50, 100);
		Int("prism_reduced_motion", Candidate.m_PrismReducedMotion, 0, 1);
		if(!Known)
			return false;
	}
	Config = Candidate;
	return true;
}

bool Save(IStorage *pStorage, const CConfig &Config)
{
	if(!pStorage)
		return false;
	pStorage->CreateFolder("prism", IStorage::TYPE_SAVE);
	char aTemp[IO_MAX_PATH_LENGTH];
	IOHANDLE File = pStorage->OpenFile(IStorage::FormatTmpPath(aTemp, sizeof(aTemp), FILE_PATH), IOFLAG_WRITE, IStorage::TYPE_SAVE);
	if(!File)
		return false;
	const std::string Data = Serialize(Config);
	bool Success = io_write(File, Data.data(), Data.size()) == Data.size();
	Success = io_sync(File) == 0 && Success;
	Success = io_close(File) == 0 && Success;
	if(Success)
		Success = pStorage->RenameFile(aTemp, FILE_PATH, IStorage::TYPE_SAVE);
	if(!Success)
		pStorage->RemoveFile(aTemp, IStorage::TYPE_SAVE);
	return Success;
}

bool Load(IStorage *pStorage, CConfig &Config)
{
	if(!pStorage)
		return false;
	IOHANDLE File = pStorage->OpenFile(FILE_PATH, IOFLAG_READ, IStorage::TYPE_SAVE);
	if(!File)
		return false;
	const auto Length = io_length(File);
	if(Length <= 0 || Length > static_cast<int64_t>(MAX_FILE_SIZE))
	{
		io_close(File);
		return false;
	}
	std::vector<char> Data(static_cast<size_t>(Length));
	const bool Read = io_read(File, Data.data(), Data.size()) == Data.size();
	const bool Closed = io_close(File) == 0;
	return Read && Closed && Parse(Data.data(), Data.size(), Config);
}
}
