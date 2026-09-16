#include "common.hpp"
#include "core/hooking/DetourHook.hpp"
#include "game/hooks/Hooks.hpp"
#include "game/pointers/Pointers.hpp"

#include <filesystem>
#include <fstream>
#include <mutex>
#include <vector>

namespace YimMenu::Hooks
{
	namespace
	{
		struct JsonDocumentView
		{
			const char* m_Data;       // +0x00
			std::uint32_t m_Length;   // +0x08
			std::uint8_t m_Unknown0C[8];
			std::int32_t m_Cursor;    // +0x14
			std::int32_t m_Valid;     // +0x18
		};

		static std::mutex g_CatalogMutex;
		static std::vector<char> g_CachedCatalog;

		bool ExtractCatalogBytes(void* document, std::vector<char>& output)
		{
			if (!document)
				return false;
			auto* view = static_cast<JsonDocumentView*>(document);
			const auto length = view->m_Length;
			if (!view->m_Data || length == 0 || length > 256u * 1024u * 1024u)
				return false;
			std::uint32_t start = 0;
			if (view->m_Data[0] != '{')
			{
				if (view->m_Cursor < 0 || static_cast<std::uint32_t>(view->m_Cursor) >= length ||
					view->m_Data[view->m_Cursor] != '{')
					return false;
				start = static_cast<std::uint32_t>(view->m_Cursor);
			}
			output.assign(view->m_Data + start, view->m_Data + length);
			return true;
		}

		bool WriteCatalogBytes(const std::vector<char>& bytes, const char* output_path)
		{
			if (!output_path || bytes.empty())
				return false;
			std::ofstream file(output_path, std::ios::binary | std::ios::trunc);
			if (!file)
				return false;
			file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
			return static_cast<bool>(file);
		}
	}

	bool SerializeCatalogJson(void* document, const char* output_path)
	{
		if (!document || !output_path)
			return false;

		std::vector<char> bytes;
		if (!ExtractCatalogBytes(document, bytes))
			return false;
		return WriteCatalogBytes(bytes, output_path);
	}

	void RequestCatalogDump()
	{
		std::vector<char> snapshot;
		{
			std::lock_guard lock(g_CatalogMutex);
			snapshot = g_CachedCatalog;
		}
		if (snapshot.empty())
		{
			LOG(INFO) << "No catalog has been unpacked yet; the next catalog will be dumped automatically.";
			return;
		}
		try
		{
			auto path = std::filesystem::temp_directory_path() / "yimmenu_catalog.json";
			if (WriteCatalogBytes(snapshot, path.string().c_str()))
				LOG(INFO) << "Cached catalog JSON dumped to " << path.string();
		}
		catch (const std::exception&)
		{
			LOG(INFO) << "Unable to write the cached catalog dump.";
		}
	}

	std::int64_t Anticheat::CatalogUnpacker(std::int64_t context, void* document, std::int32_t* out_size)
	{
		std::vector<char> captured;
		const bool valid_catalog = ExtractCatalogBytes(document, captured);
		using Original = std::int64_t(__fastcall*)(std::int64_t, void*, std::int32_t*);
		auto hook = BaseHook::Get<Anticheat::CatalogUnpacker,
		                           DetourHook<decltype(&Anticheat::CatalogUnpacker)>>();
		const auto result = reinterpret_cast<Original>(hook->Original())(context, document, out_size);

		if (valid_catalog)
		{
			{
				std::lock_guard lock(g_CatalogMutex);
				g_CachedCatalog = captured;
			}
			try
			{
				auto path = std::filesystem::temp_directory_path() / "yimmenu_catalog.json";
				if (WriteCatalogBytes(captured, path.string().c_str()))
					LOG(INFO) << "Catalog JSON dumped to " << path.string();
			}
			catch (const std::exception&)
			{
				LOG(INFO) << "Unable to write the catalog dump.";
			}
		}
		return result;
	}
}
