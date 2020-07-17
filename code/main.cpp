#include <filesystem>
#include <cstdlib>
#include <string>
using std::string;
#include <sstream>
using std::stringstream;
using std::istringstream;
#include <vector>
using std::vector;
#include <regex>
using std::regex;
#include <cassert>
namespace fs = std::filesystem;
static const wchar_t* ASSET_IGNORE_FILE_NAME = L"assets.ignore";
static bool g_verbose;
/** @return null-ternimated c-string of the entire file's contents */
static char* readEntireFile(const fs::path::value_type* fileName, 
                            uintmax_t fileSize)
{
#if _MSC_VER
	FILE* file = _wfopen(fileName, L"rb");
#else
	FILE* file = fopen(fileName, "rb");
#endif
	if(file)
	{
		char* data = static_cast<char*>(malloc(fileSize + 1));
		if(!data)
		{
			fprintf(stderr, "Failed to alloc %lli bytes for '%ws'!\n", 
			        fileSize + 1, fileName);
			return nullptr;
		}
		const size_t bytesRead = fread(data, sizeof(char), fileSize, file);
		if(bytesRead != fileSize)
		{
			free(data);
			fprintf(stderr, "Failed to completely read '%ws'!\n", fileName);
			return nullptr;
		}
		if(fclose(file) != 0)
		{
			fprintf(stderr, "Failed to close '%ws'!\n", fileName);
		}
		data[fileSize] = '\0';
		return data;
	}
	else
	{
		fprintf(stderr, "Failed to open '%ws'!\n", fileName);
		return nullptr;
	}
}
static bool writeEntireFile(const fs::path::value_type* fileName, 
                            const char* nullTerminatedFileData, 
                            bool appendWriteMode)
{
#if _MSC_VER
	FILE* file = _wfopen(fileName, appendWriteMode ? L"ab" : L"wb");
#else
	FILE* file = fopen(fileName, appendWriteMode ? "ab" : "wb");
#endif
	if(file)
	{
		const size_t elementSize = sizeof(nullTerminatedFileData[0]);
		const size_t fileDataElementCount = 
			string(nullTerminatedFileData).size();
		const size_t elementsWritten = 
			fwrite(nullTerminatedFileData, elementSize, 
			       fileDataElementCount, file);
		if(fclose(file) != 0)
		{
			fprintf(stderr, "Failed to close '%ws'!\n", fileName);
		}
		if(elementsWritten != fileDataElementCount)
		{
			fprintf(stderr, "Failed to write '%ws'!\n", fileName);
			return false;
		}
	}
	else
	{
		fprintf(stderr, "Failed to open '%ws'!\n", fileName);
		return false;
	}
	return true;
}
static bool isAlpha(wchar_t c)
{
	const bool result = 
		(c >= 'a' && c <= 'z') ||
		(c >= 'A' && c <= 'Z') ||
		c == '_';
	return result;
}
static bool isNumeric(wchar_t c)
{
	const bool result = (c >= '0' && c <= '9');
	return result;
}
string generateKAssetsHeader(const vector<string>& assetFileNames)
{
	string result;
	result.append("#pragma once\n");
	result.append("enum class KAssetIndex : unsigned\n");
	for(size_t afn = 0; afn < assetFileNames.size(); afn++)
	{
		const string& assetFileName = assetFileNames[afn];
		result.append(afn == 0 
			? "\t{ " 
			: "\t, ");
		for(wchar_t c : assetFileName)
		{
			if(isAlpha(c) || isNumeric(c))
			{
				result.push_back(c);
			}
			else
			{
				result.push_back('_');
			}
		}
		stringstream wss;
		wss << " = "<< afn << "\n";
		result.append(wss.str());
	}
	if(assetFileNames.empty())
	{
		result.append("\t{ ENUM_SIZE\n");
	}
	else
	{
		result.append("\t, ENUM_SIZE\n");
	}
	result.append("};\n");
	result.append("static const unsigned KASSET_COUNT = \n");
	result.append("\tstatic_cast<unsigned>(KAssetIndex::ENUM_SIZE);\n");
	result.append("static const char* kAssetFileNames[] = \n");
	for(size_t afn = 0; afn < assetFileNames.size(); afn++)
	{
		const string& assetFileName = assetFileNames[afn];
		result.append(afn == 0 
			? "\t{ \"" 
			: "\t, \"");
		for(wchar_t c : assetFileName)
		{
			if(c == '\\')
			{
				result.push_back('/');
			}
			else
			{
				result.push_back(c);
			}
		}
		result.append("\"\n");
	}
	if(assetFileNames.empty())
	{
		result.append("{\"NO_KASSETS_FOUND\"};\n");
	}
	else
	{
		result.append("};\n");
	}
	return result;
}
// string whitespace trimming functions //
//	Source: https://stackoverflow.com/a/217605
// trim from start (in place)
static inline void ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) {
        return !std::isspace(ch);
    }));
}
// trim from end (in place)
static inline void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}
int main(int argc, char** argv)
{
	if(argc <= 1)
	{
		printf("Usage: kasset asset_directory output_directory [--verbose]\n");
		return EXIT_SUCCESS;
	}
	if(argc < 3)
	{
		fprintf(stderr, "Incorrect # of arguments!\n");
		return EXIT_FAILURE;
	}
	const fs::path assetPath (argv[1]);
	const fs::path outputPath(argv[2]);
	for(int a = 3; a < argc; a++)
	{
		if(strcmp(argv[a], "--verbose") == 0)
		{
			g_verbose = true;
		}
		else
		{
			fprintf(stderr, "ERROR: incorrect usage on param[%i]=='%s'\n",
			        a, argv[a]);
			return EXIT_FAILURE;
		}
	}
	const fs::directory_entry entryAssetIgnore(
		assetPath/ASSET_IGNORE_FILE_NAME);
	vector<regex> regexListIgnore;
	if(entryAssetIgnore.exists())
	// if the asset ignore file exists, load its contents so we can obey the 
	//	ignored patterns contained within... //
	{
		const uintmax_t fileSize = fs::file_size(entryAssetIgnore.path());
		char*const fileAssetIgnore = 
			readEntireFile(entryAssetIgnore.path().c_str(), fileSize);
		// @HACK: `fileAssetIgnore` can leak and that's fine, since this file 
		//        should be extremely small anyways.
		istringstream iss(fileAssetIgnore);
		string line;
		while(std::getline(iss, line))
		{
			ltrim(line);
			rtrim(line);
			if(g_verbose)
			{
				printf("Adding ignore line '%s'...\n", line.c_str());
			}
			regexListIgnore.emplace_back(line);
		}
	}
	vector<string> assetFileNames;
	for(const fs::directory_entry& entry : 
		fs::recursive_directory_iterator(assetPath))
	{
		if(entry == entryAssetIgnore)
		{
			continue;
		}
		if(entry.is_regular_file())
		{
			// Create a path to the file excluding `assetPath` //
			fs::path pathToFileRelativeAssetPath = entry.path().filename();
			{
				fs::path currPath = entry.path().parent_path();
				while(currPath != assetPath)
				{
					pathToFileRelativeAssetPath = 
						currPath.filename() / pathToFileRelativeAssetPath;
					currPath = currPath.parent_path();
				}
			}
			// check to see if the asset's filename matches an ignored regex //
			bool isIgnored = false;
			for(const regex& regexIgnore : regexListIgnore)
			{
				if(std::regex_match(
				          (char*)pathToFileRelativeAssetPath.u8string().c_str(), 
				          regexIgnore))
				{
					isIgnored = true;
				}
			}
			if(isIgnored)
			{
				if(g_verbose)
					printf("Ignoring asset '%ws'...\n", 
					       pathToFileRelativeAssetPath.c_str());
				continue;
			}
			// Add the asset! //
			if(g_verbose)
				printf("Adding asset '%ws'...\n", 
				       pathToFileRelativeAssetPath.c_str());
			assetFileNames.push_back(
				(char*)pathToFileRelativeAssetPath.u8string().c_str());
		}
	}
	// output the generated asset manifest header file //
	fs::create_directories(outputPath);
	const string genKAssetHeader = generateKAssetsHeader(assetFileNames);
	const fs::path outPath = outputPath / "gen_kassets.h";
	writeEntireFile(outPath.c_str(), genKAssetHeader.c_str(), false);
	return EXIT_SUCCESS;
}
#if 0
#include <cstdio>
#include <chrono>
namespace chrono = std::chrono;
static bool g_verbose;
static vector<string> g_kassets;
enum class KTokenType : int8_t
{
	PAREN_OPEN,
	PAREN_CLOSE,
	COLON,
	SEMICOLON,
	ASTERISK,
	BRACKET_OPEN,
	BRACKET_CLOSE,
	BRACE_OPEN,
	BRACE_CLOSE,
	HASH_TAG,
	WHITESPACE,
	COMMENT,
	STRING,
	CHARACTER,
	IDENTIFIER,
	END_OF_STREAM,
	UNKNOWN
};
struct KToken
{
	KTokenType type;
	int textLength;
	const char* text;
};
struct KTokenizer
{
	const char* at;
};
static bool isEndOfLine(char c)
{
	const bool result = 
		c == '\r' ||
		c == '\n';
	return result;
}
static bool isWhitespace(char c)
{
	const bool result = 
		c == ' ' ||
		c == '\t' ||
		isEndOfLine(c);
	return result;
}
static KToken ktokeParseWhitespace(KTokenizer& tokenizer)
{
	KToken result = {
		.type = KTokenType::WHITESPACE,
		.textLength = 0,
		.text = tokenizer.at
	};
	while(isWhitespace(tokenizer.at[0]))
	{
		tokenizer.at++;
		result.textLength++;
	}
	return result;
}
static KToken ktokeParseComment(KTokenizer& tokenizer)
{
	KToken result = {
		.type = KTokenType::COMMENT,
		.textLength = 0,
		.text = tokenizer.at
	};
	if(tokenizer.at[0] == '/' && tokenizer.at[1] == '/')
	{
		tokenizer.at += 2;
		result.textLength += 2;
		while(tokenizer.at[0] && !isEndOfLine(tokenizer.at[0]))
		{
			tokenizer.at++;
			result.textLength++;
		}
		return result;
	}
	else if(tokenizer.at[0] == '/' && tokenizer.at[1] == '*')
	{
		tokenizer.at += 2;
		result.textLength += 2;
		while(tokenizer.at[0] && 
			!(tokenizer.at[0] == '*' && tokenizer.at[1] == '/'))
		{
			tokenizer.at++;
			result.textLength++;
		}
		if(tokenizer.at[0] == '*')
		{
			tokenizer.at += 2;
			result.textLength += 2;
		}
		return result;
	}
	return result;
}
static bool isAlpha(char c)
{
	const bool result = 
		(c >= 'a' && c <= 'z') ||
		(c >= 'A' && c <= 'Z') ||
		c == '_';
	return result;
}
static bool isNumeric(char c)
{
	const bool result = (c >= '0' && c <= '9');
	return result;
}
KToken ktokeParseIdentifier(KTokenizer& tokenizer)
{
	KToken result = {
		.type = KTokenType::IDENTIFIER,
		.textLength = 1,
		.text = tokenizer.at
	};
	tokenizer.at++;
	while(isAlpha(tokenizer.at[0]) || isNumeric(tokenizer.at[0]))
	{
		tokenizer.at++;
		result.textLength++;
	}
	return result;
}
#if 0
KToken ktokeParseNumber(KTokenizer& tokenizer)
{
}
#endif // 0
static KToken ktokeNext(KTokenizer& tokenizer)
{
	if(isWhitespace(tokenizer.at[0]))
	{
		return ktokeParseWhitespace(tokenizer);
	}
	KToken result = {
		.type = KTokenType::UNKNOWN,
		.textLength = 1,
		.text = tokenizer.at
	};
	switch(tokenizer.at[0])
	{
		case '\0': result.type = KTokenType::END_OF_STREAM; tokenizer.at++; break;
		case '(':  result.type = KTokenType::PAREN_OPEN;    tokenizer.at++; break;
		case ')':  result.type = KTokenType::PAREN_CLOSE;   tokenizer.at++; break;
		case ':':  result.type = KTokenType::COLON;         tokenizer.at++; break;
		case ';':  result.type = KTokenType::SEMICOLON;     tokenizer.at++; break;
		case '*':  result.type = KTokenType::ASTERISK;      tokenizer.at++; break;
		case '[':  result.type = KTokenType::BRACKET_OPEN;  tokenizer.at++; break;
		case ']':  result.type = KTokenType::BRACKET_CLOSE; tokenizer.at++; break;
		case '{':  result.type = KTokenType::BRACE_OPEN;    tokenizer.at++; break;
		case '}':  result.type = KTokenType::BRACE_CLOSE;   tokenizer.at++; break;
		case '#':  result.type = KTokenType::HASH_TAG;      tokenizer.at++; break;
		case '/':
		{
			if(tokenizer.at[1] && 
				(tokenizer.at[1] == '/' || tokenizer.at[1] == '*'))
			{
				return ktokeParseComment(tokenizer);
			}
			// otherwise, this is something else like a division operator. Skip.
			tokenizer.at++;
		}break;
		case '"': 
		{
			result.type = KTokenType::STRING;
			result.textLength = 0;
			tokenizer.at++;
			result.text = tokenizer.at;
			while(tokenizer.at[0] && tokenizer.at[0] != '"')
			{
				if(tokenizer.at[0] == '\\' && tokenizer.at[1])
				{
					tokenizer.at++; result.textLength++;
				}
				tokenizer.at++; result.textLength++;
			}
			// consume the final '"' 
			if(tokenizer.at[0] == '"')
			{
				tokenizer.at++;
			}
		}break;
		case '\'':
		{
			result.type = KTokenType::CHARACTER;
			result.textLength = 0;
			tokenizer.at++;
			result.text = tokenizer.at;
			while(tokenizer.at[0] && tokenizer.at[0] != '\'')
			{
				if(tokenizer.at[0] == '\\' && tokenizer.at[1])
				{
					tokenizer.at++; result.textLength++;
				}
				tokenizer.at++; result.textLength++;
			}
			// consume the final '\''
			if(tokenizer.at[0] == '\'')
			{
				tokenizer.at++;
			}
		}break;
		default:
		{
			if(isAlpha(tokenizer.at[0]))
			{
				return ktokeParseIdentifier(tokenizer);
			}
#if 0
			else if(isNumeric(tokenizer.at[0]))
			{
				return ktokeParseNumber(tokenizer);
			}
#endif// 0
			else 
			{
				// skip our UNKNOWN token character
				tokenizer.at++;
			}
		}break;
	}
	return result;
}
static bool ktokeEquals(const KToken& token, const char* cStr)
{
	const char* cStrCurr = cStr;
	for(int c = 0; c < token.textLength; c++, cStrCurr++)
	{
		if(cStr[c] == '\0' || token.text[c] != cStr[c])
		{
			return false;
		}
	}
	const bool result = *cStrCurr == '\0';
	return result;
}
static KToken kcppRequireToken(KTokenizer& tokenizer, KTokenType tokenType)
{
	KToken token;
	do
	{
		token = ktokeNext(tokenizer);
#if 0
		if(token.type != tokenType &&
			(token.type == KTokenType::END_OF_STREAM ||
				!(token.type == KTokenType::WHITESPACE ||
				token.type == KTokenType::COMMENT)))
#endif// 0
		if(token.type == KTokenType::END_OF_STREAM)
		{
			return token;
		}
	} while(token.type != tokenType);
	return token;
}
static void kcppParseKAssetInclude(KTokenizer& tokenizer, string& outString)
{
	if(kcppRequireToken(tokenizer, KTokenType::PAREN_OPEN).type == 
		KTokenType::PAREN_OPEN)
	{
		outString.append("#include \"gen_kassets.h\"");
		kcppRequireToken(tokenizer, KTokenType::PAREN_CLOSE);
	}
	else
	{
		fprintf(stderr, "kcppParseKAssetInclude failed!\n"); assert(false); 
	}
}
static void kcppParseKAsset(KTokenizer& tokenizer, string& outString)
{
	if(kcppRequireToken(tokenizer, KTokenType::PAREN_OPEN).type == 
		KTokenType::PAREN_OPEN)
	{
		KToken tokenAssetString = 
			kcppRequireToken(tokenizer, KTokenType::STRING);
		if(tokenAssetString.type == KTokenType::STRING)
		{
			const string strKasset(tokenAssetString.text, 
			                       tokenAssetString.textLength);
			auto itKasset = find(g_kassets.begin(), g_kassets.end(), strKasset);
			size_t kAssetIndex = g_kassets.size();
			if(itKasset == g_kassets.end())
			{
				g_kassets.push_back(strKasset);
			}
			else
			{
				kAssetIndex = itKasset - g_kassets.begin();
			}
			stringstream ss;
			ss << "&g_kassets[" << kAssetIndex << "]";
			outString.append(ss.str());
			kcppRequireToken(tokenizer, KTokenType::PAREN_CLOSE);
		}
		else
		{
			fprintf(stderr, "kcppParseKAsset failed 2!\n"); assert(false); 
		}
	}
	else
	{
		fprintf(stderr, "kcppParseKAsset failed!\n"); assert(false); 
	}
}
static void kcppParseKAssetSearch(KTokenizer& tokenizer, string& outString)
{
	if(kcppRequireToken(tokenizer, KTokenType::PAREN_OPEN).type == 
		KTokenType::PAREN_OPEN)
	{
		KToken tokenAssetFileNamePu8 = 
			kcppRequireToken(tokenizer, KTokenType::IDENTIFIER);
		if(tokenAssetFileNamePu8.type == KTokenType::IDENTIFIER)
		{
			const string strPu8(tokenAssetFileNamePu8.text, 
			                    tokenAssetFileNamePu8.textLength);
			stringstream ss;
			ss << "findKAssetCStr(" << strPu8 << ")";
			outString.append(ss.str());
			kcppRequireToken(tokenizer, KTokenType::PAREN_CLOSE);
		}
		else
		{
			fprintf(stderr, "kcppParseKAssetSearch failed 2!\n"); assert(false); 
		}
	}
	else
	{
		fprintf(stderr, "kcppParseKAssetSearch failed!\n"); assert(false); 
	}
}
static void kcppParseKAssetCStr(KTokenizer& tokenizer, string& outString)
{
	if(kcppRequireToken(tokenizer, KTokenType::PAREN_OPEN).type == 
		KTokenType::PAREN_OPEN)
	{
		KToken tokenAssetIndex = 
			kcppRequireToken(tokenizer, KTokenType::IDENTIFIER);
		if(tokenAssetIndex.type == KTokenType::IDENTIFIER)
		{
			const string strSizeT(tokenAssetIndex.text, 
			                      tokenAssetIndex.textLength);
			stringstream ss;
			ss << "g_kassets[" << strSizeT << "]";
			outString.append(ss.str());
			kcppRequireToken(tokenizer, KTokenType::PAREN_CLOSE);
		}
		else
		{
			fprintf(stderr, "kcppParseKAssetCStr failed 2!\n"); assert(false); 
		}
	}
	else
	{
		fprintf(stderr, "kcppParseKAssetCStr failed!\n"); assert(false); 
	}
}
static void kcppParseKAssetIndex(KTokenizer& tokenizer, string& outString)
{
	if(kcppRequireToken(tokenizer, KTokenType::PAREN_OPEN).type == 
		KTokenType::PAREN_OPEN)
	{
		KToken tokenAssetString = 
			kcppRequireToken(tokenizer, KTokenType::IDENTIFIER);
		if(tokenAssetString.type == KTokenType::IDENTIFIER)
		{
			const string strKAssetCStr(tokenAssetString.text, 
			                           tokenAssetString.textLength);
			stringstream ss;
			ss << "static_cast<u32>("<< strKAssetCStr << " - g_kassets)";
			outString.append(ss.str());
			kcppRequireToken(tokenizer, KTokenType::PAREN_CLOSE);
		}
		else
		{
			fprintf(stderr, "kcppParseKAsset failed 2!\n"); assert(false); 
		}
	}
	else
	{
		fprintf(stderr, "kcppParseKAsset failed!\n"); assert(false); 
	}
}
static void kcppParseKAssetType(KTokenizer& tokenizer, string& outString)
{
	if(kcppRequireToken(tokenizer, KTokenType::PAREN_OPEN).type == 
		KTokenType::PAREN_OPEN)
	{
		KToken tokenAssetString = 
			kcppRequireToken(tokenizer, KTokenType::IDENTIFIER);
		if(tokenAssetString.type == KTokenType::IDENTIFIER)
		{
			const string strKAssetCStr(tokenAssetString.text, 
			                           tokenAssetString.textLength);
			stringstream ss;
			ss << "g_kassetFileTypes[("<< strKAssetCStr << " - g_kassets)]";
			outString.append(ss.str());
			kcppRequireToken(tokenizer, KTokenType::PAREN_CLOSE);
		}
		else
		{
			fprintf(stderr, "kcppParseKAsset failed 2!\n"); assert(false); 
		}
	}
	else
	{
		fprintf(stderr, "kcppParseKAsset failed!\n"); assert(false); 
	}
}
static void kcppParseKAssetCount(KTokenizer& tokenizer, string& outString)
{
	outString.append("(sizeof(g_kassets)/sizeof(g_kassets[0]))");
}
static void kcppParseKAssetTypePng(KTokenizer& tokenizer, string& outString)
{
	outString.append("KAssetFileType::PNG");
}
static void kcppParseKAssetTypeWav(KTokenizer& tokenizer, string& outString)
{
	outString.append("KAssetFileType::WAV");
}
static void kcppParseKAssetTypeOgg(KTokenizer& tokenizer, string& outString)
{
	outString.append("KAssetFileType::OGG");
}
static void kcppParseKAssetTypeFlipbookMeta(KTokenizer& tokenizer, 
                                            string& outString)
{
	outString.append("KAssetFileType::FLIPBOOK_META");
}
static void kcppParseKAssetTypeUnknown(KTokenizer& tokenizer, string& outString)
{
	outString.append("KAssetFileType::UNKNOWN");
}
static void kcppParseMacroDefinition(KTokenizer& tokenizer, string& outString)
{
	const char* macroDef = tokenizer.at;
	size_t macroDefSize = 0;
	while(tokenizer.at[0] && !isEndOfLine(tokenizer.at[0]))
	{
		if(tokenizer.at[0] == '\\' && isEndOfLine(tokenizer.at[1]))
		{
			do
			{
				tokenizer.at++;
				macroDefSize++;
			} while (isEndOfLine(tokenizer.at[0]));
		}
		else
		{
			tokenizer.at++;
			macroDefSize++;
		}
	}
	outString.append(macroDef, macroDefSize);
}
static string processFileData(const char* fileData)
{
	string result;
	bool parsing = true;
	KTokenizer tokenizer = {.at = fileData };
	while(parsing)
	{
		KToken token = ktokeNext(tokenizer);
#if 0
		printf("%d:'%.*s'\n", token.type, token.textLength, token.text);
		if( ktokeEquals(token, "stb_decompress") ||
			ktokeEquals(token, "proggy_clean_ttf_compressed_data_base85"))
		{
			fflush(stdout);
			printf("hello");
		}
#endif// 0
		switch(token.type)
		{
			case KTokenType::HASH_TAG:
			{
				KToken tokenNext = ktokeNext(tokenizer);
				result.append(token.text, token.textLength);
				result.append(tokenNext.text, tokenNext.textLength);
				if(tokenNext.type == KTokenType::IDENTIFIER &&
					ktokeEquals(tokenNext, "define"))
				{
					kcppParseMacroDefinition(tokenizer, result);
				}
			}break;
			case KTokenType::IDENTIFIER:
			{
				if(ktokeEquals(token, "INCLUDE_KASSET"))
				{
					kcppParseKAssetInclude(tokenizer, result);
				}
				else if(ktokeEquals(token, "KASSET"))
				{
					kcppParseKAsset(tokenizer, result);
				}
				else if(ktokeEquals(token, "KASSET_SEARCH"))
				{
					kcppParseKAssetSearch(tokenizer, result);
				}
				else if(ktokeEquals(token, "KASSET_CSTR"))
				{
					kcppParseKAssetCStr(tokenizer, result);
				}
				else if(ktokeEquals(token, "KASSET_INDEX"))
				{
					kcppParseKAssetIndex(tokenizer, result);
				}
				else if(ktokeEquals(token, "KASSET_TYPE"))
				{
					kcppParseKAssetType(tokenizer, result);
				}
				else if(ktokeEquals(token, "KASSET_COUNT"))
				{
					kcppParseKAssetCount(tokenizer, result);
				}
				else if(ktokeEquals(token, "KASSET_TYPE_PNG"))
				{
					kcppParseKAssetTypePng(tokenizer, result);
				}
				else if(ktokeEquals(token, "KASSET_TYPE_WAV"))
				{
					kcppParseKAssetTypeWav(tokenizer, result);
				}
				else if(ktokeEquals(token, "KASSET_TYPE_OGG"))
				{
					kcppParseKAssetTypeOgg(tokenizer, result);
				}
				else if(ktokeEquals(token, "KASSET_TYPE_FLIPBOOK_META"))
				{
					kcppParseKAssetTypeFlipbookMeta(tokenizer, result);
				}
				else if(ktokeEquals(token, "KASSET_TYPE_UNKNOWN"))
				{
					kcppParseKAssetTypeUnknown(tokenizer, result);
				}
				else
				{
					result.append(token.text, token.textLength);
				}
			}break;
			case KTokenType::STRING:
			{
				result.push_back('"');
				result.append(token.text, token.textLength);
				result.push_back('"');
			}break;
			case KTokenType::CHARACTER:
			{
				result.push_back('\'');
				result.append(token.text, token.textLength);
				result.push_back('\'');
			}break;
			default:
			{
				result.append(token.text, token.textLength);
			}break;
			case KTokenType::END_OF_STREAM:
			{
				parsing = false;
			}break;
		}
	}
	return result;
}
#if defined(_WIN32)
#include <Windows.h>
void cloneFileTimestamps(const std::filesystem::path& source, 
                         const std::filesystem::path& dest)
{
	const HANDLE hFileSource = 
		CreateFileW(source.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, 
		            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	assert(hFileSource != INVALID_HANDLE_VALUE);
	if(hFileSource == INVALID_HANDLE_VALUE)
	{
		fprintf(stderr, "Failed to open file '%ws'! getlasterror=%i\n", 
		        source.c_str(), GetLastError());
	}
	const HANDLE hFileDest = 
		CreateFileW(dest.c_str(), FILE_WRITE_ATTRIBUTES, 0, nullptr, 
		            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	assert(hFileDest != INVALID_HANDLE_VALUE);
	if(hFileDest == INVALID_HANDLE_VALUE)
	{
		fprintf(stderr, "Failed to open file '%ws'! getlasterror=%i\n", 
		        dest.c_str(), GetLastError());
	}
	FILETIME fileTimeCreation;
	FILETIME fileTimeLastAccess;
	FILETIME fileTimeLastWrite;
	if(!GetFileTime(hFileSource, &fileTimeCreation, &fileTimeLastAccess, 
	                &fileTimeLastWrite))
	{
		fprintf(stderr, "Failed to get file timestamps for '%ws'! "
		        "getlasterror=%i\n", source.c_str(), GetLastError());
	}
	if(!SetFileTime(hFileDest, &fileTimeCreation, &fileTimeLastAccess, 
	                &fileTimeLastWrite))
	{
		fprintf(stderr, "Failed to set file timestamps for '%ws'! "
		        "getlasterror=%i\n", dest.c_str(), GetLastError());
	}
	CloseHandle(hFileDest);
	CloseHandle(hFileSource);
}
static void setFileReadOnly(const fs::path::value_type* fileName)
{
	if(!SetFileAttributesW(fileName, FILE_ATTRIBUTE_READONLY))
	{
		fprintf(stderr, "Failed to set read-only '%ws'!\n", fileName);
	}
}
#endif// defined(_WIN32)
string generateHeaderKAssets()
{
	string result;
	result.append("#pragma once\n");
	result.append("static const char* g_kassets[] = {\n");
	if(g_kassets.empty())
	{
		result.append("\t\"NO_KASSETS_FOUND_IN_SOURCE!\"\n");
	}
	for(size_t a = 0; a < g_kassets.size(); a++)
	{
		const string& kasset = g_kassets[a];
		stringstream ss;
		ss << "\t\"" << kasset<< "\",\n";
		result.append(ss.str());
		const size_t fbmPostfixOffset = kasset.find(".fbm", kasset.size() - 4);
		if(fbmPostfixOffset != string::npos)
		{
			// Implicitly infer that there must be a `png` file of the same file
			//	name prefix in the same directory as the `fbm`! (for now...) //
			const string impliedPngAssetFileName = 
				kasset.substr(0, fbmPostfixOffset) + string(".png");
			auto itImpliedPng = find(g_kassets.begin(), g_kassets.end(), 
			                         impliedPngAssetFileName);
			if(itImpliedPng == g_kassets.end())
			{
				g_kassets.push_back(impliedPngAssetFileName);
			}
		}
	}
	result.append("};\n");
	result.append("enum class KAssetFileType : unsigned char {\n");
	result.append("\tPNG,\n");
	result.append("\tWAV,\n");
	result.append("\tOGG,\n");
	result.append("\tFLIPBOOK_META,\n");
	result.append("\tUNKNOWN,\n");
	result.append("};\n");
	result.append("static const KAssetFileType g_kassetFileTypes[] = {\n");
	if(g_kassets.empty())
	{
		result.append("\tKAssetFileType::UNKNOWN\n");
	}
	for(const string& kasset : g_kassets)
	{
		if(kasset.find(".png", kasset.size() - 4) != string::npos)
		{
			result.append("\tKAssetFileType::PNG,\n");
		}
		else if(kasset.find(".wav", kasset.size() - 4) != string::npos)
		{
			result.append("\tKAssetFileType::WAV,\n");
		}
		else if(kasset.find(".ogg", kasset.size() - 4) != string::npos)
		{
			result.append("\tKAssetFileType::OGG,\n");
		}
		else if(kasset.find(".fbm", kasset.size() - 4) != string::npos)
		{
			result.append("\tKAssetFileType::FLIPBOOK_META,\n");
		}
		else
		{
			result.append("\tKAssetFileType::UNKNOWN,\n");
		}
	}
	result.append("};\n");
	result.append("static const char*const* findKAssetCStr(const char* str)\n");
	result.append("{\n");
	result.append("\tfor(size_t a = 0; \n"
	              "\t    a < (sizeof(g_kassets)/sizeof(g_kassets[0])); a++)\n");
	result.append("\t{\n");
	result.append("\t\tif(strcmp(g_kassets[a], str) == 0)\n");
	result.append("\t\t{\n");
	result.append("\t\t\treturn &g_kassets[a];\n");
	result.append("\t\t}\n");
	result.append("\t}\n");
	result.append("\treturn nullptr;\n");
	result.append("}\n");
	return result;
}
static void printManual()
{
	printf("---KC++: An extremely lightweight language extension to C++ ---\n");
	printf("Usage: kc++ input_code_tree_directory "
	       "[--verbose]\n");
	printf("Result: Upon successful completion, all C++ code in the input \n"
	       "\tcode tree directory is transformed and saved into the desired \n"
	       "\toutput directory.\n");
	printf("Transformations: \n");
	printf("\t-Replace `KASSET(c_str)` macros with an array index unique to \n"
	       "\t    this c_str.\n");
	printf("\t-Generate a file at the root of desired output directory \n"
	       "\t    called `kasset_array.h`, which contains the accumulated \n"
	       "\t    string data of all KASSET macro invocations in an array.\n");
}
int main(int argc, char** argv)
{
	const auto timeMainStart = chrono::high_resolution_clock::now();
	if(argc < 2)
	{
		fprintf(stderr, "ERROR: incorrect usage!\n");
		printManual();
		return EXIT_FAILURE;
	}
	g_verbose = false;
	const fs::path inputCodeTreeDirectory = argv[1];
	for(int a = 2; a < argc; a++)
	{
		if(strcmp(argv[a], "--verbose") == 0)
		{
			g_verbose = true;
		}
		else
		{
			fprintf(stderr, "ERROR: incorrect usage on param[%i]=='%s'\n",
			        a, argv[a]);
			printManual();
			return EXIT_FAILURE;
		}
	}
	if(g_verbose)
	{
		printf("inputCodeTreeDirectory='%ws'\n", 
		       inputCodeTreeDirectory.c_str());
	}
	const string tempInputCodeTreeFolderName = 
		inputCodeTreeDirectory.filename().string() + "_backup";
	const fs::path inputCodeTreeParentDirectory = inputCodeTreeDirectory/"..";
	const fs::path backupInputCodeTreeDirectory = 
		inputCodeTreeParentDirectory/tempInputCodeTreeFolderName;
	fs::rename(inputCodeTreeDirectory, backupInputCodeTreeDirectory);
	for(const fs::directory_entry& p : 
		fs::recursive_directory_iterator(backupInputCodeTreeDirectory))
	{
		if(g_verbose)
		{
			printf("'%ws'\n", p.path().c_str());
		}
		if(p.is_directory())
		{
		}
		else if(p.is_regular_file())
		{
#if 0
			if(g_verbose)
			{
				printf("p.path().parent_path()='%ws' "
				       "p.path().filename()='%ws'\n",
				       p.path().parent_path().c_str(), 
				       p.path().filename().c_str());
			}
#endif
			// Create a path to the file excluding 
			//	`backupInputCodeTreeDirectory` //
			fs::path inCodeTreePath = p.path().filename();
			{
				fs::path currPath = p.path().parent_path();
				while(currPath != backupInputCodeTreeDirectory)
				{
					inCodeTreePath = currPath.filename() / inCodeTreePath;
					currPath = currPath.parent_path();
				}
			}
#if 0
			if(g_verbose)
			{
				printf("inCodeTreePath='%ws'\n", inCodeTreePath.c_str());
			}
#endif
			// Recreate the original source directory //
			const fs::path outPathOriginal = 
				inputCodeTreeDirectory / inCodeTreePath;
			fs::create_directories(outPathOriginal.parent_path());
#if 0
			if(g_verbose)
			{
				printf("file size=%lli\n", fs::file_size(p.path()));
			}
#endif
			char*const fileData = 
				readEntireFile(p.path().c_str(), fs::file_size(p.path()));
			if(fileData)
			{
				const string processedFileData = processFileData(fileData);
				free(fileData);
				// Recreate the original file, with kc++ modifications applied //
				if(!writeEntireFile(outPathOriginal.c_str(), 
				                    processedFileData.c_str()))
				{
					fprintf(stderr, "Failed to write file '%ws'!\n", 
					        outPathOriginal.c_str());
				}
				cloneFileTimestamps(p, outPathOriginal);
			}
			else
			{
				fprintf(stderr, "Failed to read file '%ws'!\n", 
				        p.path().c_str());
			}
		}
	}
	// generate the kasset string database //
	if(!g_kassets.empty())
	{
		const fs::path outPath = inputCodeTreeDirectory / "gen_kassets.h";
		const string fileData = generateHeaderKAssets();
		if(!writeEntireFile(outPath.c_str(), fileData.c_str()))
		{
			fprintf(stderr, "Failed to write file '%ws'!\n", 
					outPath.c_str());
		}
		setFileReadOnly(outPath.c_str());
	}
	// calculate the program execution time //
	const auto timeMainEnd = chrono::high_resolution_clock::now();
	const auto timeMainDuration = chrono::duration_cast<chrono::microseconds>(
		timeMainEnd - timeMainStart);
	const float secondsMainDuration = timeMainDuration.count() / 1000000.f;
	printf("kc++ complete! Seconds elapsed=%f\n", secondsMainDuration);
	return EXIT_SUCCESS;
}
#endif// 0