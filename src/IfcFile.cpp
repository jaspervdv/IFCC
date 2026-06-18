#include "IfcClass.hpp"
#include "IfcFile.hpp"

#include <string>
#include <map>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>
#include <cmath>

#include "mz.h"
#include "mz_strm.h"
#include "mz_strm_os.h"
#include "mz_zip.h"


bool IfcFile::pathIsZip(const std::filesystem::path& filePath)
{
	std::string pathExtension = filePath.extension().string();
	std::transform(pathExtension.begin(), pathExtension.end(), pathExtension.begin(), ::toupper);
	if (pathExtension == ".IFCZIP") { return true; }
	return false;
}

bool IfcFile::pathIsFrag(const std::filesystem::path& filePath)
{
	std::string pathExtension = filePath.extension().string();
	std::transform(pathExtension.begin(), pathExtension.end(), pathExtension.begin(), ::toupper);
	if (pathExtension == ".FRAG") { return true; }
	return false;
}

std::string IfcFile::unZip(const std::string& filepath)
{
	void* file_stream = mz_stream_os_create();
	mz_stream_open(file_stream, filepath.c_str(), MZ_OPEN_MODE_READ);

	void* zip_handle = mz_zip_create();
	mz_zip_open(zip_handle, file_stream, MZ_OPEN_MODE_READ);

	// Go to first entry (or locate by name)
	mz_zip_goto_first_entry(zip_handle);

	mz_zip_file* file_info = nullptr;
	mz_zip_entry_get_info(zip_handle, &file_info);

	if (mz_zip_entry_read_open(zip_handle, 0, nullptr) != MZ_OK) {
		return {};
	}

	std::filesystem::path inputPath = file_info->filename;
	std::string pathExtension = inputPath.extension().string();
	std::transform(pathExtension.begin(), pathExtension.end(), pathExtension.begin(), ::toupper);
	if (pathExtension != ".IFC") { return {}; }

	std::vector<char> buffer(file_info->uncompressed_size);

	int32_t bytesRead = mz_zip_entry_read(zip_handle, buffer.data(), buffer.size());

	mz_zip_entry_close(zip_handle);
	mz_zip_close(zip_handle);
	mz_zip_delete(&zip_handle);

	mz_stream_close(file_stream);
	mz_stream_delete(&file_stream);

	std::string content(buffer.begin(), buffer.end());
	return content;
}

void IfcFile::storeFrag(const std::filesystem::path& outputPath)
{
#ifdef _WIN32
	std::filesystem::path tempPath = std::filesystem::current_path().string() + std::string("\\TempFile.ifc");
#elif __linux__  || __EMSCRIPTEN__
	std::filesystem::path tempPath = std::filesystem::current_path().string() + std::string("/TempFile.ifc");
#endif

	storeFileIFC(tempPath);

#ifdef _WIN32
	std::string cmd = "IfcSwap.exe \"" + tempPath.string() + "\" \"" + outputPath.string() + "\"";
#elif __linux__  || __EMSCRIPTEN__
	std::string cmd = "./IfcSwap \"" + tempPath.string() + "\" \"" + outputPath.string() + "\"";
	std::cout << cmd << std::endl;
#endif
	int result = system(cmd.c_str());
	std::remove(tempPath.string().c_str());

	return;
}

void IfcFile::storeFileZip(const std::filesystem::path& outputPath)
{
	std::string fileName = outputPath.stem().string() + std::string(".ifc");
	if (storeFile7Zip(outputPath, fileName)) { return; }

	// if not use this
	void* file_stream = mz_stream_os_create();
	if (mz_stream_open(file_stream, outputPath.string().c_str(), MZ_OPEN_MODE_CREATE) != MZ_OK) {
		std::cerr << "Failed to open zip file\n";
		return;
	}

	void* zip_handle = mz_zip_create();
	mz_zip_open(zip_handle, file_stream, MZ_OPEN_MODE_WRITE);

	std::string fileContent = dumptoString();
	int64_t fileSize = static_cast<int64_t>(fileContent.size());

	mz_zip_file file_info = {};
	file_info.filename = fileName.c_str();
	file_info.compression_method = MZ_COMPRESS_METHOD_DEFLATE;

	int32_t err = mz_zip_entry_write_open(zip_handle, &file_info, MZ_COMPRESS_LEVEL_BEST, 0, nullptr);
	if (err != MZ_OK) {
		std::cerr << "Failed to open zip entry: " << err << std::endl;
		return;
	}

	err = mz_zip_entry_write(zip_handle, fileContent.data(), fileSize);
	if (err != fileSize) {
		std::cerr << "Failed to write zip entry: " << err << std::endl;
		return;
	}

	mz_zip_entry_close(zip_handle);
	mz_zip_close(zip_handle);
	mz_zip_delete(&zip_handle);
	mz_stream_close(file_stream);
	mz_stream_delete(&file_stream);

	return;
}

bool IfcFile::storeFile7Zip(const std::filesystem::path& outputPath, const std::filesystem::path& fileName)
{
#ifdef _WIN32
	// find if 7zip exists
	std::string zipperPath = "C:\\Program Files\\7-Zip\\7z.exe";
	std::cout << "[INFO] searching for 7-Zip at: " + zipperPath << "\n";

	if (std::filesystem::exists(zipperPath)) //if exists use 7z
	{
		std::cout << "Found 7-Zip" << "\n\n";
		if (std::filesystem::exists(outputPath)) { std::filesystem::remove(outputPath); }

		std::filesystem::path tempPath = std::filesystem::current_path().string() + std::string("\\") + fileName.string();
		storeFileIFC(tempPath);

		std::string zipperLine = "\"\"" + zipperPath + "\" a -tzip -mx=7  -mm=Deflate -mfb=128 -mpass=3 \"" + outputPath.string() + "\"  \"" + tempPath.string() + "\"\"";
		system(zipperLine.c_str());
		std::remove(tempPath.string().c_str());

		return true;
	}
#elif __linux__ 
	std::cout << "[INFO] searching for 7-Zip \n";
	std::string zipperPath = "7zz";
	std::string zipperCom = zipperPath + " > /dev/null 2>&1";
	if (system(zipperCom.c_str()) == 0) //if exists use 7z
	{
		std::cout << "Found 7-Zip" << "\n\n";
		if (std::filesystem::exists(outputPath)) { std::filesystem::remove(outputPath); }

		std::filesystem::path tempPath = std::filesystem::current_path().string() + std::string("/") + fileName.string();
		storeFileIFC(tempPath);
		std::string zipperLine = "\"" + zipperPath + "\" a -tzip -mx=7  -mm=Deflate -mfb=128 -mpass=3 \"" + outputPath.string() + "\"  \"" + tempPath.string() + "\"";
		system(zipperLine.c_str());
		std::remove(tempPath.string().c_str());

		return true;
	}
#endif
	std::cout << "unable to find 7-Zip, minizip-ng is used" << std::endl;
	return false;
}

void IfcFile::storeFileIFC(const std::filesystem::path& outputPath)
{
	std::ofstream storageFile;
	storageFile.open(outputPath);
	storageFile << dumptoString();
	storageFile.close();

	return;
}


std::vector<std::string_view> IfcFile::splitDataString(const std::string& dataString)
{
	std::vector<std::string_view> result;
	size_t start = 0;

	bool inQuotes = false;
	for (size_t i = 0; i < dataString.size(); ++i)
	{
		char c = dataString[i];

		if (c == '\'')
		{
			inQuotes = !inQuotes;
		}

		else if (c == ';' && !inQuotes)
		{
			result.emplace_back(dataString.data() + start, i - start + 1);
			start = i + 1;
		}

		else if (c == '\n' && !inQuotes)
		{
			result.emplace_back(dataString.data() + start, i - start);
			start = i + 1;
		}
	}


	result.emplace_back(dataString.data() + start, dataString.size() - start);
	return result;
}

IfcFile::IfcFile(const std::string& filePath) {

	std::string dataString;
	if (pathIsZip(filePath)) { dataString = unZip(filePath); }
	else 
	{
		std::ifstream file(filePath);
		dataString = std::string((std::istreambuf_iterator<char>(file)),
			std::istreambuf_iterator<char>());
	}

	if (dataString.empty())
	{
		std::cout << "unable to read date from file" << std::endl;
		return;
	}

	initFromString(dataString);
	
	return;
}


void IfcFile::removeClass(int idx) {
	privateClassList_.erase(idx);
}

bool IfcFile::initFromString(const std::string& dataString)
{
	std::vector<std::string_view> splitString = splitDataString(dataString);
	std::unordered_map<std::string, int> classIndexMap;

	bool isHeader = true;
	for (const std::string_view& currentString : splitString)
	{
		if (currentString[0] != '#') {
			if (isHeader) {
				header_ += currentString;
				if (prettyPrint_) { header_ += "\n"; }
			}
			else { footer_ += currentString; }
			continue;
		}
		isHeader = false;

		int splitIndx = currentString.find_first_of("=");

		std::string stringId = std::string(currentString.substr(0, splitIndx));
		std::string stringData = std::string(currentString.substr(splitIndx + 1, currentString.length()));
		if (stringId.length() <= 1) { continue; }
		if (stringData.length() <= 0) { continue; }
		int id = std::stoi(stringId.substr(1));

		if (stringData[0] == ' ')
		{
			stringData = stringData.substr(1);
		}

		// store the class type
		std::string classType = stringData.substr(0, stringData.find_first_of("("));
		auto [it, inserted] = classIndexMap.emplace(classType, classTypes_.size());

		if (inserted) { classTypes_.push_back(classType); }

		IfcClass currentClass(
			id,
			classIndexMap.at(classType),
			stringData.substr(stringData.find_first_of("("))
		);

		privateClassList_.emplace(id, currentClass);
	}

	isGood_ = true;
	return true;
}

void IfcFile::restructureFile(const std::unordered_map<int, int>& referenceMap) {

	// update the object IDs
	std::map<int, IfcClass> newPrivateClassList;

	for (const std::pair<int, int>& currentPair : referenceMap)
	{
		int oldId = currentPair.first;
		int newId = currentPair.second;

		IfcClass& currentClass = privateClassList_.at(oldId);
		currentClass.setId(newId);
		newPrivateClassList.emplace(newId, currentClass);
	}

	privateClassList_ = std::move(newPrivateClassList);

	// update the reference IDS
	for (auto& [currentIdx, currentclass] : privateClassList_)
	{
		currentclass.remapClassRelations(referenceMap);
	}

	return;
}

void IfcFile::roundFloats(int floatLength)
{
	std::cout << "[INFO] round floating point values to " << 1 / pow(10, floatLength) << "\n";
	// open the file as generic file and delete lines ad id
	for (auto fileIt = begin(); fileIt != end(); ++fileIt)
	{
		fileIt->second.RoundFloatingValues(floatLength);
	}
	std::cout << "success" << std::endl;
	return;
}

void IfcFile::updateReference(const std::unordered_map<int, int>& referenceMap)
{
	int counter = 0;
	int totalLineCount = getClassCount();

	for (auto fileIt = begin(); fileIt != end(); ++fileIt)
	{
		if (counter % 1000 == 0)
		{
			std::cout << "processing objects: " << counter << " (" << (counter * 100) / totalLineCount << "%)\r";
		}
		counter++;

		int referenceIdx = fileIt->first;
		if (referenceMap.find(referenceIdx) != referenceMap.end()) { continue; }
		fileIt->second.remapClassRelations(referenceMap);
	}
	std::cout << "processing objects: " << counter << " (" << "100%)\n";
	return;
}

void IfcFile::collapseClasses(int maxIt, int iteration)
{
	int counter = 0;
	int lineCount = getClassCount();

	std::cout << "\n[INFO] collapsing redundant classes, iteration: " << iteration << "\n";

	std::unordered_map<int, std::unordered_map<std::string, int>> uniqueItemMap;
	std::unordered_map<int, int> oldNewRefMap;

	for (auto fileIt = begin(); fileIt != end(); ++fileIt)
	{
		if (counter % 1000 == 0)
		{
			std::cout << "processing objects: " << counter << " (" << (counter * 100) / lineCount << "%)\r";
		}
		counter++;

		int currentId = fileIt->first;

		IfcClass& currentClass = fileIt->second;
		int classType = currentClass.getClassTypeIdx();
		std::string classData = currentClass.getData();

		if (uniqueItemMap.find(classType) == uniqueItemMap.end())
		{
			std::unordered_map<std::string, int> newMap;
			newMap.emplace(classData, currentId);
			uniqueItemMap.emplace(classType, newMap);
			continue;
		}

		auto [storedPropertyKey, inserted] = uniqueItemMap[classType].emplace(classData, currentId);
		if (inserted) { continue; }

		oldNewRefMap.emplace(currentId, storedPropertyKey->second);
	}

	std::cout << "processing objects: " << counter << " (100%)\n";
	std::cout << "[INFO] Updating References\n";
	updateReference(oldNewRefMap);

	for (auto remapPair : oldNewRefMap)
	{
		removeClass(remapPair.first);
	}

	if (!oldNewRefMap.empty() && maxIt != iteration)
	{
		std::cout << "reduced objects to from " << counter << " to " << counter - oldNewRefMap.size() << "\n";
		iteration += 1;

		//clear data
		oldNewRefMap = {};
		uniqueItemMap = {};

		collapseClasses(maxIt, iteration);
	}
	return;
}

bool IfcFile::recalculateId(bool restructure)
{
	std::cout << "\n[INFO] recalcualating Ids\n";

	std::unordered_map<int, int> remappedId;
	if (restructure)
	{
		std::string delimiters = "(),";
		std::map<int, int> pairIdOccuranceList;

		for (auto fileIt = begin(); fileIt != end(); ++fileIt)
		{
			int currentId = fileIt->first;
			pairIdOccuranceList[currentId] = 0;
		}

		for (auto fileIt = begin(); fileIt != end(); ++fileIt)
		{
			std::vector<std::string> tokenizedString = fileIt->second.tokenizeData(delimiters);

			for (const std::string& currentToken : tokenizedString)
			{
				if (currentToken.length() == 0) { continue; }
				if (currentToken[0] != '#') { continue; }
				int id = std::stoi(currentToken.substr(1));

				if (pairIdOccuranceList.find(id) != pairIdOccuranceList.end())
				{
					pairIdOccuranceList[id] = pairIdOccuranceList[id] + 1;
				}
			}
		}

		std::vector<std::pair<int, int>> sortedPairOccurance(pairIdOccuranceList.begin(), pairIdOccuranceList.end());

		std::sort(sortedPairOccurance.begin(), sortedPairOccurance.end(), [](const auto& a, const auto& b) {
			return a.second > b.second;
			});

		int currentId = 1;
		for (const auto& [originId, count] : sortedPairOccurance) {
			remappedId.emplace(originId, currentId);
			currentId++;
		}
	}
	else
	{
		int currentId = 1;
		for (auto fileIt = begin(); fileIt != end(); ++fileIt)
		{
			int originId = fileIt->first;
			remappedId.emplace(originId, currentId);
			currentId++;
		}
	}
	restructureFile(remappedId);

	std::cout << "success" << std::endl;
	return true;
}

void IfcFile::removingDangling()
{
	std::cout << "\n[INFO] remove dangling structures\n";
	// graph relationships
	std::unordered_map<int, std::vector<int>> indx2Relation;
	std::unordered_map<int, bool> evalMap;

	int IfcProjectId = -1;

	std::string delimiters = "(),";
	for (const auto& [currentIdx, currentClass] : privateClassList_)
	{
		if (IfcProjectId == -1)
		{
			std::string classType = classTypes_[currentClass.getClassTypeIdx()];
			std::transform(classType.begin(), classType.end(), classType.begin(), ::toupper);

			if (classType == "IFCPROJECT")
			{
				IfcProjectId = currentIdx;
			}
		}

		std::vector<int> relatedId;
		if (indx2Relation.find(currentIdx) != indx2Relation.end())
		{
			relatedId = indx2Relation[currentIdx];
		}

		std::vector<std::string> tokenizedString = currentClass.tokenizeData(delimiters);
		for (const std::string& currentToken : tokenizedString)
		{
			if (currentToken.length() == 0) { continue; }
			if (currentToken[0] != '#') { continue; }
			int id = std::stoi(currentToken.substr(1));

			auto it = find(relatedId.begin(), relatedId.end(), id);
			if (it == relatedId.end())
			{
				relatedId.emplace_back(id);
			}

			if (indx2Relation.find(id) == indx2Relation.end())
			{
				std::vector<int> localId;
				localId.emplace_back(currentIdx);
				indx2Relation.emplace(id, localId);
				continue;
			}

			indx2Relation[id].emplace_back(currentIdx);

		}
		indx2Relation[currentIdx] = relatedId;
		evalMap.emplace(currentIdx, false);
	}

	if (IfcProjectId == -1)
	{
		std::cout << "Unable to find IfcProject class\n";
		return;
	}

	std::vector<int> IndxPool = { IfcProjectId };
	while (!IndxPool.empty())
	{
		std::vector<int> bufferList;

		for (int currentIdx : IndxPool)
		{
			if (evalMap[currentIdx]) { continue; }
			evalMap[currentIdx] = true;

			for (int t : indx2Relation[currentIdx])
			{
				bufferList.emplace_back(t);
			}		
		}
		IndxPool = bufferList;
	}

	int removedObbCount = 0;
	for (const std::pair<int, bool>& currentOcc : evalMap)
	{
		if (!currentOcc.second)
		{
			removeClass(currentOcc.first);
			removedObbCount++;
		}
	}
	std::cout << "removed " << removedObbCount << " object(s)\n";
	std::cout << "success\n";
	return;
}


std::string IfcFile::dumptoString() const {
	std::string dumpedString = header_;
	if (prettyPrint_) { dumpedString += "\n"; }

	for (const auto& [currentIdx, currentClass] : privateClassList_)
	{
		dumpedString += "#" + std::to_string(currentIdx) + "=" + classTypes_[currentClass.getClassTypeIdx()] + currentClass.getData();
		if (prettyPrint_) { dumpedString += "\n"; }
	}
	dumpedString += footer_;

	return dumpedString;
}

void IfcFile::storeFile(const std::filesystem::path& outputPath)
{
	if (pathIsFrag(outputPath)) { storeFrag(outputPath); }
	else if (pathIsZip(outputPath)) { storeFileZip(outputPath); }
	else { storeFileIFC(outputPath.string()); }
	return;
}
