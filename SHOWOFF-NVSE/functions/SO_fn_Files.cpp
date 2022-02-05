﻿#include "SO_fn_Files.h"
#include "nvse/Utilities.h"

#include "config.hpp"
#include <tao/json.hpp>

#include <filesystem>
#include <fstream>

#define SI_SUPPORT_IOSTREAMS
#include "SimpleIni.h"

#define TEST_JSON_READ_PERFORMANCE false


//Made by anhatthezoo, requested by Trooper.
bool Cmd_CreateFolder_Execute(COMMAND_ARGS)
{
	*result = false;
	char folderPath[MAX_PATH]; // relative to "Fallout New Vegas" folder.
	folderPath[0] = 0;
	if (ExtractArgsEx(EXTRACT_ARGS_EX, &folderPath) && folderPath[0])
	{
		*result = std::filesystem::create_directories(GetFalloutDirectory() + folderPath);
	}
	return true;
}



namespace JsonToNVSE
{
	using valType = tao::json::type;
	using DefaultJsonValue = tao::json::value;
	using ConfigJsonValue = tao::config::value;

	template< template< typename... > class Traits >
	ArrayElementL BasicJsonValueToArrayElem(tao::json::basic_value<Traits> const& val)
	{
		ArrayElementL elem;
		if (val.is_string())
		{
			elem = val.get_string().c_str();
		}
		else if (val.is_null())
		{
			//NULL isn't representable in Obscript, so it'll be an invalid array.
			elem = static_cast<NVSEArrayVar*>(nullptr);
		}
		else if (val.is_number())
		{
			elem = val.as<double>();
		}
		else if (val.is_boolean())
		{
			elem = val.get_boolean();
		}
		else {
			throw std::logic_error("SHOWOFF - GetArrayFromJSON: Non-Exhausive if checks.");
		}
		return elem;
	}

	//Case 1: json val is 1-dimensional -> create 1D array
	//Case 2: json val is multidimensional -> create array with sub-arrays (use recursion)
	template< template< typename... > class Traits >
	NVSEArrayVar* SubElementsToNVSEArray(tao::json::basic_value<Traits> const& val, Script* scriptObj)
	{
		NVSEArrayVar* resArr;
		if (auto const type = val.type();
			valType::ARRAY == type)
		{
			auto const& arr = val.get_array();
			Vector<ArrayElementL> arrData(arr.size());
			for (auto const& value : arr)
			{
				if (auto const type = value.type();
					valType::ARRAY == type || valType::OBJECT == type)
				{
					arrData.Append(ArrayElementL{ SubElementsToNVSEArray(value, scriptObj) });
				}
				else
				{
					arrData.Append(BasicJsonValueToArrayElem(value));
				}
			}
			resArr = g_arrInterface->CreateArray(arrData.Data(), arr.size(), scriptObj);
		}
		else if (valType::OBJECT == type)
		{
			auto const& obj = val.get_object();
			resArr = g_arrInterface->CreateStringMap(nullptr, nullptr, 0, scriptObj);
			for (auto const& [key, value] : obj)
			{
				if (auto const type = value.type();
					valType::ARRAY == type || valType::OBJECT == type)
				{
					g_arrInterface->SetElement(resArr, ArrayElementL(key.c_str()), ArrayElementL(SubElementsToNVSEArray(value, scriptObj)));
				}
				else
				{
					g_arrInterface->SetElement(resArr, ArrayElementL(key.c_str()), BasicJsonValueToArrayElem(value));
				}
			}
		}
		else
		{
			throw std::logic_error("SHOWOFF - SubElementsToNVSEArray: Function called with invalid json object type.");
		}
		return resArr;
	}

	//Single-element JSON is valid
	//Single array/object is also valid
	//Otherwise, for multiple arrays/objects, you need to wrap in an array/object.
	template< template< typename... > class Traits >
	ArrayElementL GetNVSEFromJSON(tao::json::basic_value<Traits> const& val, Script* scriptObj)
	{
		if (auto const type = val.type();
			valType::ARRAY == type || valType::OBJECT == type)
		{
			return SubElementsToNVSEArray(val, scriptObj);
		}
		
		//return single element (NOT in an array).
		return BasicJsonValueToArrayElem(val);
	}

	enum Parser : UInt32
	{
		kParser_JSON = 0, kParser_JAXN, kParser_TaoConfig,
		kParser_Invalid
	};

	using JsonValueVariant = std::variant<DefaultJsonValue, ConfigJsonValue>;
	using JsonValueVariant_ContainsRef = std::variant<std::reference_wrapper<DefaultJsonValue>, std::reference_wrapper<ConfigJsonValue>>;
	using JsonValueVariantRef = std::reference_wrapper<JsonValueVariant>;

	using NewJsonValueVariant_OrRef = std::variant <JsonValueVariantRef, JsonValueVariant>;
	constexpr JsonValueVariant& GetRef(NewJsonValueVariant_OrRef &jsonArg)
	{
		return *std::visit(overloaded{
			[](JsonValueVariantRef& json) -> JsonValueVariant* {
				return &json.get();
			},
			[](JsonValueVariant& json) -> JsonValueVariant* {
				return &json;
			} 
		}, jsonArg);
	}

	Map<const char*, JsonValueVariant> g_CachedJSONFiles;
	ICriticalSection g_JsonMapLock;

	JsonValueVariant ReadJson_Unsafe(const Parser parser, const std::string& JSON_Path)
	{
		JsonValueVariant retn;
		switch (parser)
		{
		case kParser_JSON:
			retn = tao::json::from_file(JSON_Path);
			break;
		case kParser_JAXN:
			retn = tao::json::jaxn::from_file(JSON_Path);
			break;
		case kParser_TaoConfig:
			retn = tao::config::from_file(JSON_Path);
			break;
		case kParser_Invalid:
		default:
			throw std::logic_error("SHOWOFF - ReadJSONWithParser >> somehow reached invalid case in Switch statement.");
		}
		return retn;
	}

	//MUST be called with valid Parser type.
	std::optional<NewJsonValueVariant_OrRef> ReadJSONWithParser(
		const Parser parser, 
		const std::string &JSON_FullPath, 
		const std::string_view relativePath, 
		const std::string_view& funcName, 
		const bool cache)
	{
		if (auto const cachedRef = g_CachedJSONFiles.GetPtr(relativePath.data()))
		{
			return std::ref(*cachedRef);
		}
		
		try
		{
			auto parsedJson = ReadJson_Unsafe(parser, JSON_FullPath);
			if (!cache)
				return parsedJson;
			ScopedLock lock(g_JsonMapLock);
			return std::ref(g_CachedJSONFiles[relativePath.data()] = std::move(parsedJson) );
		}
		catch (tao::pegtl::parse_error& e)
		{
			if (IsConsoleMode() || g_ShowFuncDebug)
				Console_Print("%s >> Could not parse JSON file, likely due to invalid formatting.", funcName.data());
			_MESSAGE("ReadArrayFromJSONFile >> PARSE ERROR (%s)", e.what());
		}
		catch (std::filesystem::filesystem_error& e)
		{
			if (IsConsoleMode() || g_ShowFuncDebug)
				Console_Print("%s >> Could not find JSON file.", funcName.data());
			_MESSAGE("ReadArrayFromJSONFile >> FILE NOT FOUND (%s)", e.what());
		}
		return {};
	}

	std::optional<JsonValueVariant_ContainsRef> GetJSONValueAtJSONPointer(const JsonValueVariant &value, const std::string& jsonPointer, const std::string_view& funcName)
	{
		try
		{
			return std::visit([&](auto&& val) -> JsonValueVariant_ContainsRef {
				return val.at(tao::json::pointer(jsonPointer));
				//todo: ensure this always returns a valid value, or a caught exception!
				//TODO: also ensure it isn't making a copy!
				}, const_cast<JsonValueVariant&>(value));
		}
		catch (const std::runtime_error& e)
		{
			if (IsConsoleMode() || g_ShowFuncDebug)
				Console_Print("%s >> Invalid JSON pointer arg.", funcName.data());
			_MESSAGE("%s >> JSON POINTER ERROR (%s)", funcName.data(), e.what());
		}
		catch (const std::out_of_range& e)
		{
			if (IsConsoleMode() || g_ShowFuncDebug)
				Console_Print("%s >> Invalid JSON pointer arg.", funcName.data());
			_MESSAGE("%s >> JSON POINTER ERROR (OUT OF RANGE) (%s)", funcName.data(), e.what());
		}
		catch (const std::invalid_argument& e)
		{
			if (IsConsoleMode() || g_ShowFuncDebug)
				Console_Print("%s >> Invalid JSON pointer arg.", funcName.data());
			_MESSAGE("%s >> JSON POINTER ERROR (Invalid Arg) (%s)", funcName.data(), e.what());
		}
		return {};
	}

	//assume numeric or string element (from a Map/StringMap's keys).
	std::string ConvertKeyElemToStr(NVSEArrayElement& elem)
	{
		if (auto const type = elem.GetType();
			type == NVSEArrayVarInterface::kType_Numeric)
		{
			return std::to_string(elem.num);
		}
		else if (type == NVSEArrayVarInterface::kType_String)
		{
			return elem.str;
		}
		throw std::logic_error("SHOWOFF - ConvertKeyElemToStr >> gave wrongly typed elem");
	}

	template< template< typename... > class Traits >
	tao::json::basic_value<Traits> BasicArrayElemToJsonValue(NVSEArrayElement& elem)
	{
		if (auto const type = elem.GetType();
			type == NVSEArrayVarInterface::kType_Numeric)
		{
			return tao::json::basic_value<Traits>(elem.num);
		}
		else if (type == NVSEArrayVarInterface::kType_Form)
		{
			if (elem.form)
				return tao::json::basic_value<Traits>(elem.form->refID);
			return tao::json::basic_value<Traits>(0u);
		}
		else if (type == NVSEArrayVarInterface::kType_String)
		{
			return tao::json::basic_value<Traits>(std::string(elem.str));
		}
		throw std::logic_error("SHOWOFF - BasicArrayElemToJsonValue >> Received wrongly typed elem.");
	}
	
	template< template< typename... > class Traits >
	tao::json::basic_value<Traits> GetJSONFromNVSE_Helper(NVSEArrayElement& elem)
	{
		tao::json::basic_value<Traits> value;
		if (elem.GetType() == NVSEArrayVarInterface::kType_Array)
		{
			if (!elem.arr)
			{
				value = tao::json::null;	//invalid array = json null
			}
			else
			{
				if (auto const arrType = g_arrInterface->GetContainerType(elem.arr);
					arrType == NVSEArrayVarInterface::ContainerTypes::kArrType_Array)
				{
					value = tao::json::empty_array;
					ArrayData const data = { elem.arr, true };
					for (auto i = 0; i < data.size; i++)
					{
						if (auto const type = data.vals[i].GetType();
							type == NVSEArrayVarInterface::kType_Array)
						{
							value.emplace_back(GetJSONFromNVSE_Helper<Traits>(data.vals[i]));	//recursion
						}
						else
						{
							value.emplace_back(BasicArrayElemToJsonValue<Traits>(data.vals[i]));
						}
					}

				}
				else if (arrType == NVSEArrayVarInterface::ContainerTypes::kArrType_StringMap
					|| arrType == NVSEArrayVarInterface::ContainerTypes::kArrType_Map)		//if Map-type, convert numeric keys to string.
				{
					value = tao::json::empty_object;
					ArrayData const data = { elem.arr, false };
					for (auto i = 0; i < data.size; i++)
					{
						if (auto const type = data.vals[i].GetType();
							type == NVSEArrayVarInterface::kType_Array)
						{
							value[ConvertKeyElemToStr(data.keys[i])] = GetJSONFromNVSE_Helper<Traits>(data.vals[i]);	//recursion
						}
						else
						{
							value[ConvertKeyElemToStr(data.keys[i])] = BasicArrayElemToJsonValue<Traits>(data.vals[i]);
						}
					}
				}
				else //invalid
				{
					value = tao::json::null;
				}
			}
		}
		else
		{
			value = BasicArrayElemToJsonValue<Traits>(elem);
		}
		return value;
	}
	
	//Parser arg determines the type of variant to return.
	//Assume elem and parser are valid.
	JsonValueVariant GetJSONFromNVSE(NVSEArrayElement &elem, Parser parser)
	{
		switch (parser)
		{
		case kParser_JSON:
		case kParser_JAXN:
			return GetJSONFromNVSE_Helper<tao::json::traits>(elem);
		case kParser_TaoConfig:
			return GetJSONFromNVSE_Helper<tao::config::traits>(elem);
		default:
			throw std::logic_error("SHOWOFF - GetJSONFromNVSE >> Reached invalid default case");
		}
	}


	bool InsertValueAtJSONPointer(JsonValueVariant &baseVariant, const JsonValueVariant &insertVariant, const std::string &jsonPointer, 
		const std::string_view &funcName)
	{
		return std::visit([&]<typename T0, typename T1>(T0 &&base, T1 &&insert) -> bool {
			if constexpr (decay_equiv<T0, T1>::value)
			{
				try
				{
					ScopedLock lock(g_JsonMapLock);	//in case the baseJson was retrieved from the map.
					base.insert(tao::json::pointer(jsonPointer), insert);
					return true;
				}
				catch(std::exception &e)
				{
					if (IsConsoleMode() || g_ShowFuncDebug)
						Console_Print("%s >> Invalid JSON pointer arg.", funcName.data());
					_MESSAGE("%s >> JSON POINTER ERROR (%s)", funcName.data(), e.what());
					return false;
				}
			}
			else
				throw std::logic_error("SHOWOFF - InsertValueAtJSONPointer >> both visitors should be of the same type.");
		}, baseVariant, insertVariant);
	}
	

	//return false in case of failure, for error reporting.
	bool TryReadFromJSONFile(PluginExpressionEvaluator& eval, Script* scriptObj)
	{
		std::string json_path_relative = eval.GetNthArg(0)->GetString();
		if (json_path_relative.empty())
			return false;
		std::string jsonPointer = "";	// the path in the JSON hierarchy, pass "" to get the root value.
		Parser parser = kParser_JSON;
		bool cache = false;
		if (auto const numArgs = eval.NumArgs();
			numArgs >= 2)
		{
			jsonPointer = eval.GetNthArg(1)->GetString();
			if (numArgs >= 3)
			{
				parser = static_cast<Parser>(eval.GetNthArg(2)->GetInt());
				if (parser >= kParser_Invalid)
					return false;
				if (numArgs >= 4)
				{
					cache = eval.GetNthArg(3)->GetBool();
				}
			}
		}

		auto [JSON_FullPath, relPath] = GetFullPath(std::move(json_path_relative));

		constexpr std::string_view funcName = { "ReadFromJSONFile" };
		bool success = false;
		if (auto jsonValOpt = ReadJSONWithParser(parser, JSON_FullPath, relPath, funcName, cache))
		{
			auto const jsonVal = GetRef(*jsonValOpt);
			if (auto const JsonRef = GetJSONValueAtJSONPointer(jsonVal, jsonPointer, funcName))
			{
				std::visit([scriptObj, &eval](auto&& val) {
					auto res = JsonToNVSE::GetNVSEFromJSON(val.get(), scriptObj);
					eval.AssignCommandResult(res);
					}, JsonRef.value());
				success = true;
			}
		}
		
		return success;
	}
}




bool Cmd_ReadFromJSONFile_Execute(COMMAND_ARGS)
{
#if TEST_JSON_READ_PERFORMANCE
	auto const start = std::chrono::high_resolution_clock::now();
#endif
	
	using namespace JsonToNVSE;
	*result = 0;
	if (PluginExpressionEvaluator eval(PASS_COMMAND_ARGS);
		eval.ExtractArgs())
	{
		if (!TryReadFromJSONFile(eval, scriptObj))
		{
			ArrayElementL invalidRetn;
			eval.AssignCommandResult(invalidRetn);	//report error, since return value is too ambiguous.
			//will inform script. If in console mode OR in ShowOff debug mode, will inform the nature of the error.
			//todo: report better error instead!
		}
	}
#if TEST_JSON_READ_PERFORMANCE
	auto const end = std::chrono::high_resolution_clock::now();
	auto const duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
	std::ofstream out("ReadFromJSONFile_Performance.txt", std::ios::app);
	out << duration.count() << " (ms)\n";
#endif

	return true;
}

bool Cmd_WriteToJSONFile_Execute(COMMAND_ARGS)
{
	using namespace JsonToNVSE;
	*result = false;	//bSuccess
	if (PluginExpressionEvaluator eval(PASS_COMMAND_ARGS);
		eval.ExtractArgs())
	{
		ArrayElementR elem; 
		eval.GetNthArg(0)->GetElement(elem);
		if (!elem.IsValid())
			return true;
		std::string json_path_rel = eval.GetNthArg(1)->GetString();
		if (json_path_rel.empty())
			return true;
		std::string jsonPointer = "";	// the path in the JSON hierarchy, pass "" to get the root value.
		Parser parser = kParser_JSON;
		bool cache = false;
		bool saveFile = true;
		switch (auto const numArgs = eval.NumArgs();
			numArgs)
		{
		case 6:
			saveFile = eval.GetNthArg(5)->GetBool();
			[[fallthrough]];
		case 5:
			cache = eval.GetNthArg(4)->GetBool();
			[[fallthrough]];
		case 4:
			parser = static_cast<Parser>(eval.GetNthArg(3)->GetInt());
			if (parser >= kParser_Invalid)
				return true;
			[[fallthrough]];
		case 3:
			jsonPointer = eval.GetNthArg(2)->GetString();
			break;
		default:
			throw std::logic_error("Invalid case for switch!");
		}

		auto [JSON_FullPath, relPath] = GetFullPath(std::move(json_path_rel));
		auto const fileExisted = std::filesystem::exists(JSON_FullPath);
		NewJsonValueVariant_OrRef elemAsJSON = JsonToNVSE::GetJSONFromNVSE(elem, parser);

		//if jsonPointer is empty, then the entire file will be replaced; no point in reading it.
		bool const readFileAtJPointer = !jsonPointer.empty() && fileExisted;
		if (readFileAtJPointer)
		{
			constexpr std::string_view funcName = { "WriteToJSONFile" };
			if (auto jsonValOpt = ReadJSONWithParser(parser, JSON_FullPath, relPath, funcName, cache))
			{
				JsonValueVariant* jsonVal = std::get_if<JsonValueVariant>(&jsonValOpt.value());
				bool jsonCached = false; // assume new value was created (uncached)
				if (jsonVal)	//true only if no caching is involved.
				{
					if (!saveFile)	//it's all for nothing if changes aren't being cached and won't be saved to file.
						return true;
					//The reason this isn't checked earlier, with "cached" and "savefile", 
					// is because the user might've cached the file earlier (which will be retrieved even if "cached" = false).
				}
				else
				{
					jsonVal = &std::get_if<JsonValueVariantRef>(&jsonValOpt.value())->get();	//I hate myself
					jsonCached = true;
				}

				if (InsertValueAtJSONPointer(*jsonVal, GetRef(elemAsJSON), jsonPointer, funcName))
				{
					if (!jsonCached)
						elemAsJSON = std::move(*jsonVal);	//Move jsonVal if it's not owned by Map global
					else
					{
						elemAsJSON = std::ref(*jsonVal); //elemAsJSON points to global map value
						*result = true;	//success if cached data was modified
					}
				}
				else
					return true;
			}
			else //catch parsing errors
			{
				return true;
			}
		}

		if (saveFile)
		{
			if (std::ofstream output(JSON_FullPath);	//erase previous contents, potentially create new file.
				output.is_open())
			{
				std::visit([&output](auto&& val) {
					output << std::setw(4) << val;	//pretty-printed with tab indents
					}, GetRef(elemAsJSON));
				*result = true;	//success if data was written to file
			}
		}			

		if (!readFileAtJPointer && cache)
		{
			// File was either created, or was completely replaced; cache that new data.
			// Also possible that file didn't exist and wasn't created, but we can still cache that data.
			ScopedLock lock(g_JsonMapLock);
			g_CachedJSONFiles[relPath.data()] = std::move(GetRef(elemAsJSON));
			*result = true;	//success if cached data is changed
		}
	}
	return true;
}








namespace IniToNVSE
{
	//Convert passed partialPath to a fullPath.
	void GetFullINIPath(std::string &partialPath)
	{		
		//Make string end with ".ini" if it didn't have a file extension.
		if (partialPath.find( '.') == std::string::npos)
		{
			partialPath += ".ini";
		}
		//otherwise, unlike in JIP's GetINIPath, we accept extensions other than ".ini"

		partialPath.insert(0, "data\\config\\");
	}

	//If iniRelPath is empty, will point it to the mod's name.
	//Returns an empty string if an error occured.
	//Copied from JIP's GetINIPath(), converted to use the STL.
	std::string GetINIPath(const char* &iniRelPath, Script* const scriptObj)
	{
		std::string fullIniPath;
		
		if (!iniRelPath[0])
		{
			UInt8 const modIdx = scriptObj->GetOverridingModIdx();
			if (modIdx == 0xFF)
				return fullIniPath;	//empty str
			iniRelPath = g_dataHandler->GetNthModName(modIdx);
			fullIniPath = iniRelPath;
			std::ranges::replace(fullIniPath, '/', '\\');
		}
		
		GetFullINIPath(fullIniPath);
		return fullIniPath;
	}

	Map<const char*, CSimpleIniA> g_CachedIniFiles;

	using StringOrNumber = std::variant<const char*, double>;
		
	namespace SetINIValue
	{
		SI_Error SetIniValue(CSimpleIniA &ini, StringOrNumber& newValue, 
			const char* fullPath, const char* section, const char* key, 
			const char* comment, bool saveFile)
		{
			SI_Error result;
			if (auto const strVal = std::get_if<const char*>(&newValue))
			{
				if (auto const e = ini.SetValue(section, key, *strVal, comment);
					e >= SI_OK)	//if success
				{
					result = e;
				}
				else return SI_FAIL;
			}
			else //assume number
			{
				if (auto const e = ini.SetDoubleValue(section, key, std::get<double>(newValue), comment);
					e >= SI_OK)
				{
					result = e;
				}
				else return SI_FAIL;
			}

			if (saveFile)
			{
				if (auto const err = ini.SaveFile(fullPath, false);
					err < SI_OK)
				{
					return err;
				}
			}
			return result;
		}

		
		void Call(std::string& sectionAndKey, StringOrNumber& newValue, const char* relIniPath,
			const char* comment, Script* scriptObj, bool cache, bool saveFile, double* result)
		{
			auto [section, key] = SplitStringBySingleDelimiter(sectionAndKey, ":/\\");
			if (!section || !key)
				return;

			auto const fullPath = GetINIPath(relIniPath, scriptObj);
			if (fullPath.empty())
				return;

			if (auto const fullPathCStr = fullPath.c_str();
				auto ini = g_CachedIniFiles.GetPtr(relIniPath))
			{
				*result = SetIniValue(*ini, newValue, fullPathCStr, section, key, comment, saveFile);
			}
			else
			{
				CSimpleIniA iniLocal(true);
				bool const existed = iniLocal.LoadFile(fullPathCStr) >= SI_OK;
				bool const created = 
					SetIniValue(iniLocal, newValue, fullPathCStr, section, key, comment, saveFile)
					== SI_INSERTED;

				if (cache && (existed || created))
				{
					g_CachedIniFiles.Emplace(relIniPath, std::move(iniLocal));
				}
			}
		}
	}
	
	namespace GetINIValue
	{
		void GetOrDefaultIniValue(CSimpleIniA& ini, const char* section, 
			const char* key, auto& result)
		{
			using T = decltype(result);
			if constexpr (std::is_same_v<T, double&>)
			{
				result = ini.GetDoubleValue(section, key, result);
			}
			else if constexpr (std::is_same_v<T, const char*&>)
			{
				result = ini.GetValue(section, key, result);
			}
			else
			{
				static_assert(false, "Invalid type for result arg.");
			}
		}
		
		void Call_GetOrDefault(std::string& sectionAndKey, const char* relIniPath,
			Script* scriptObj, bool cache, auto &result)
		{
			auto [section, key] = SplitStringBySingleDelimiter(sectionAndKey, ":/\\");
			if (!section || !key)
				return;

			auto const fullPath = GetINIPath(relIniPath, scriptObj);
			if (fullPath.empty())
				return;

			//Always try to get a cached file first; so modders can be lazy and leave "cache" arg at default.
			if (auto ini = g_CachedIniFiles.GetPtr(relIniPath))
			{
				GetOrDefaultIniValue(*ini, section, key, result);
			}
			else
			{
				if (CSimpleIniA iniLocal(true);
					iniLocal.LoadFile(fullPath.c_str()) >= SI_OK)
				{
					GetOrDefaultIniValue(iniLocal, section, key, result);

					if (cache)
					{
						g_CachedIniFiles.Emplace(relIniPath, std::move(iniLocal));
					}
				}
			}

		}

		// Returns true if file has been saved without errors.
		bool GetOrCreateIniValue(CSimpleIniA &ini, const char* comment, 
			const char* section, const char* key,
			const char* fullPath, bool saveFile, auto &result)
		{
			using T = decltype(result);
			static_assert(std::is_same_v<T, const char*&> || std::is_same_v<T, double&>, 
				"Invalid type for result arg.");
			
			bool hasCreatedValue;
			result = ini.GetOrCreate(section, key, result, comment, true, &hasCreatedValue);

			if (hasCreatedValue && saveFile)
			{
				return ini.SaveFile(fullPath, false) >= SI_OK;
			}
			return false;
		}

		void Call_GetOrCreate(std::string& sectionAndKey, const char* relIniPath, 
			const char* comment, Script* scriptObj, bool cache, bool saveFile, auto& result)
		{
			//sectionAndKey is split into section & key variables using null character.
			auto [section, key] = SplitStringBySingleDelimiter(sectionAndKey, ":/\\");
			if (!section || !key)
				return;

			auto const fullPath = GetINIPath(relIniPath, scriptObj);
			if (fullPath.empty())
				return;

			if (auto const fullPathCStr = fullPath.c_str(); 
				auto ini = g_CachedIniFiles.GetPtr(relIniPath))
			{
				GetOrCreateIniValue(*ini, comment, section, key, fullPathCStr, saveFile, result);
			}
			else
			{
				CSimpleIniA iniLocal(true);
				bool const existed = iniLocal.LoadFile(fullPathCStr) >= SI_OK;
				auto const created = GetOrCreateIniValue(iniLocal, comment, section, key, fullPathCStr, saveFile, result);
				
				if (cache && (existed || created))
				{
					g_CachedIniFiles.Emplace(relIniPath, std::move(iniLocal));
				}
			}
		}
	};
	
}


//Most code copied from JIP LN NVSE's GetINIString
bool Cmd_HasINISetting_Execute(COMMAND_ARGS)
{
	*result = false;
	if (PluginExpressionEvaluator eval(PASS_COMMAND_ARGS);
		eval.ExtractArgs())
	{
		std::string sectionAndKey;
		const char* relIniPath = nullptr;
		bool cache = false;
		EXTRACT_ALL_ARGS_EXP(HasINISetting, eval, std::tie(sectionAndKey),
			std::tie(relIniPath, cache));

		auto [section, key] = SplitStringBySingleDelimiter(sectionAndKey, ":/\\");
		if (!section || !key)
			return true;

		auto const fullPath = IniToNVSE::GetINIPath(relIniPath, scriptObj);
		if (fullPath.empty())
			return true;

		auto const HasIniValue = [section, key](const CSimpleIniA &ini) -> bool
		{
			auto const val = ini.GetValue(section, key, nullptr);
			return val && val[0];
		};
		
		if (auto const fullPathCStr = fullPath.c_str();
			auto ini = IniToNVSE::g_CachedIniFiles.GetPtr(relIniPath))
		{
			*result = HasIniValue(*ini);
		}
		else
		{
			if (CSimpleIniA iniLocal(true);
				iniLocal.LoadFile(fullPathCStr) >= SI_OK)
			{
				*result = HasIniValue(iniLocal);
				if (cache)
				{
					IniToNVSE::g_CachedIniFiles.Emplace(relIniPath, std::move(iniLocal));
				}
			}
		}
	}
	return true;
}



bool Cmd_SetINIValue_Execute(COMMAND_ARGS)
{
	*result = SI_Error::SI_FAIL;
	if (PluginExpressionEvaluator eval(PASS_COMMAND_ARGS);
		eval.ExtractArgs())
	{
		std::string sectionAndKey;
		IniToNVSE::StringOrNumber newVal;
		const char* comment = nullptr, * iniPath = nullptr;
		bool cache = false, saveFile = true;
		EXTRACT_ALL_ARGS_EXP(SetINIValue, eval, std::tie(sectionAndKey, newVal),
			std::tie(iniPath, comment, cache, saveFile));

		IniToNVSE::SetINIValue::Call(sectionAndKey, newVal, iniPath, comment, scriptObj, cache, saveFile, result);
	}
	return true;
}

bool Cmd_SetINIFloatAlt_Execute(COMMAND_ARGS)
{
	return Cmd_SetINIValue_Execute(PASS_COMMAND_ARGS);
}

bool Cmd_SetINIStringAlt_Execute(COMMAND_ARGS)
{
	return Cmd_SetINIValue_Execute(PASS_COMMAND_ARGS);
}

bool Cmd_GetINIFloatOrCreate_Execute(COMMAND_ARGS)
{
	*result = 0.0;	//also the default creation value.
	if (PluginExpressionEvaluator eval(PASS_COMMAND_ARGS);
		eval.ExtractArgs())
	{
		std::string sectionAndKey;
		const char* comment = nullptr, * iniPath = nullptr;
		bool cache = false, saveFile = true;
		EXTRACT_ALL_ARGS_EXP(GetINIFloatOrCreate, eval, std::tie(sectionAndKey), std::tie(iniPath, *result, comment, cache, saveFile));
		
		IniToNVSE::GetINIValue::Call_GetOrCreate(sectionAndKey, iniPath, comment, scriptObj, cache, saveFile, *result);
	}
	return true;
}
bool Cmd_GetINIStringOrCreate_Execute(COMMAND_ARGS)
{
	const char* res = ""; //also the default creation value.
	if (PluginExpressionEvaluator eval(PASS_COMMAND_ARGS);
		eval.ExtractArgs())
	{
		std::string sectionAndKey;
		const char* comment = nullptr, * iniPath = nullptr;
		bool cache = false, saveFile = true;
		EXTRACT_ALL_ARGS_EXP(GetINIStringOrCreate, eval, std::tie(sectionAndKey), std::tie(iniPath, res, comment, cache, saveFile));

		IniToNVSE::GetINIValue::Call_GetOrCreate(sectionAndKey, iniPath, comment, scriptObj, cache, saveFile, res);
	}
	g_strInterface->Assign(PASS_COMMAND_ARGS, res); 
	return true;
}

bool Cmd_GetINIFloatOrDefault_Execute(COMMAND_ARGS)
{
	*result = 0.0;
	if (PluginExpressionEvaluator eval(PASS_COMMAND_ARGS);
		eval.ExtractArgs())
	{
		std::string sectionAndKey;
		const char* iniPath = nullptr;
		bool cache = false;
		EXTRACT_ALL_ARGS_EXP(GetINIFloatOrDefault, eval, std::tie(sectionAndKey), 
			std::tie(iniPath, *result, cache));

		IniToNVSE::GetINIValue::Call_GetOrDefault(sectionAndKey, iniPath, scriptObj, cache, *result);
	}
	return true;
}
bool Cmd_GetINIStringOrDefault_Execute(COMMAND_ARGS)
{
	const char* res = "";
	if (PluginExpressionEvaluator eval(PASS_COMMAND_ARGS);
		eval.ExtractArgs())
	{
		std::string sectionAndKey;
		const char* iniPath = nullptr;
		bool cache = false;
		EXTRACT_ALL_ARGS_EXP(GetINIStringOrDefault, eval, std::tie(sectionAndKey),
			std::tie(iniPath, res, cache));

		IniToNVSE::GetINIValue::Call_GetOrDefault(sectionAndKey, iniPath, scriptObj, cache, res);
	}
	g_strInterface->Assign(PASS_COMMAND_ARGS, res);
	return true;
}



bool Cmd_ClearFileCacheShowOff_Execute(COMMAND_ARGS)
{
	enum CacheToClear : UInt8
	{
		kCache_Ini = 0,
		kCache_Json
	};
	
	*result = false; //hasCleared
	if (PluginExpressionEvaluator eval(PASS_COMMAND_ARGS);
		eval.ExtractArgs())
	{
		const char* relPath = nullptr;	//relative to different folders, depending on toClearMode.
		//This is due to json/ini/etc funcs reading at different base folders, for consistency.
		CacheToClear toClearMode;
		EXTRACT_ALL_ARGS_EXP(ClearFileCacheShowOff, eval, std::tie(relPath, toClearMode), g_NoArgs);

		if (!relPath || !relPath[0])
			return true;
		
		if (toClearMode == kCache_Ini)
		{
			*result = IniToNVSE::g_CachedIniFiles.Erase(relPath);
		}
		else if (toClearMode == kCache_Json)
		{
			*result = JsonToNVSE::g_CachedJSONFiles.Erase(relPath);
		}
	}
	return true;
}








#if _DEBUG


bool Cmd_DemoTestFile_Execute(COMMAND_ARGS)
{
	*result = 0;

	std::filesystem::path path = { "test" };
	
	auto i = tao::config::from_file(path);
	_MESSAGE("%i", static_cast<int>(i.type()));

	
	
	return true;
}
#endif