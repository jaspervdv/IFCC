#include <IfcClass.hpp>
#include <IfcFile.hpp>
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

std::istringstream  IfcFile::unZip(const std::string& filepath)
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
	std::istringstream stream(content);
	return stream;
}

void IfcFile::storeFrag(const std::filesystem::path& outputPath)
{
	std::filesystem::path tempPath = std::filesystem::current_path().string() + std::string("\\TempFile.ifc");
	storeFileIFC(tempPath);
#ifdef _WIN32
	std::string cmd = "IfcSwap.exe \"" + tempPath.string() + "\" \"" + outputPath.string() + "\"";
#elif __linux__
	std::string cmd = "IfcSwap \"" + tempPath.string() + "\" \"" + outputPath.string() + "\"";
#endif
	int result = system(cmd.c_str());
	std::remove(tempPath.string().c_str());

	return;
}

void IfcFile::storeFileZip(const std::filesystem::path& outputPath)
{
	void* file_stream = mz_stream_os_create();
	if (mz_stream_open(file_stream, outputPath.string().c_str(), MZ_OPEN_MODE_CREATE) != MZ_OK) {
		std::cerr << "Failed to open zip file\n";
		return;
	}

	void* zip_handle = mz_zip_create();
	mz_zip_open(zip_handle, file_stream, MZ_OPEN_MODE_WRITE);

	std::string fileName = outputPath.stem().string() + std::string(".ifc");
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

void IfcFile::storeFileIFC(const std::filesystem::path& outputPath)
{
	std::ofstream storageFile;
	storageFile.open(outputPath);
	storageFile << dumptoString();
	storageFile.close();

	return;
}


IfcFile::IfcFile(const std::string& filePath) {

	std::istringstream streamFile;
	if (pathIsZip(filePath)) { streamFile = unZip(filePath); }
	else 
	{
		std::ifstream file(filePath);
		std::string content((std::istreambuf_iterator<char>(file)),
			std::istreambuf_iterator<char>());
		streamFile = std::istringstream(content);
	}

	if (streamFile.peek() == std::char_traits<char>::eof())
	{
		std::cout << "unable to read date from file" << std::endl;
		return;
	}

	bool isHeader = true;
	std::string line;
	while (std::getline(streamFile, line)) {
		if (line[0] != '#') {
			if (isHeader) { 
				header_ += line;
				if (prettyPrint_) { header_ += "\n"; }
			}
			else { footer_ += line; }
			continue;
		}
		isHeader = false;

		int splitIndx = line.find_first_of("=");

		std::string stringId = line.substr(0, splitIndx);
		std::string stringData = line.substr(splitIndx + 1, line.length());
		if (stringId.length() <= 1) { continue; }
		if (stringData.length() <= 0) { continue; }
		int id = std::stoi(stringId.substr(1));

		if (stringData[0] == ' ')
		{
			stringData = stringData.substr(1);
		}

		std::pair<std::string, std::string> stringPair(stringData.substr(0, stringData.find_first_of("(")), stringData);

		IfcClass currentClass(
			id,
			stringData.substr(0, stringData.find_first_of("(")),
			false,
			stringData
		);

		std::unique_ptr<IfcClass> uniqueClassPtr = std::make_unique<IfcClass>(currentClass);
		classStructure_.emplace(id, uniqueClassPtr.get());
		privateClassList_.emplace(id, std::move(uniqueClassPtr));
	}

	if (classStructure_.size () == privateClassList_.size() != 0)
	{
		isGood_ = true;
	}
	return;
}


void IfcFile::removeClass(int idx) {
	classStructure_.erase(idx);
	privateClassList_.erase(idx);
}

void IfcFile::restructureFile(const std::map<int, int>& referenceMap) {

	// update the object IDs
	std::map<int, std::unique_ptr<IfcClass>> newPrivateClassList;
	std::map<int, IfcClass*> newClassStructure;

	for (const std::pair<int, int>& currentPair : referenceMap)
	{
		int oldId = currentPair.first;
		int newId = currentPair.second;

		std::unique_ptr<IfcClass> currentClass = std::move(privateClassList_[oldId]);
		currentClass->setId(newId);
		newClassStructure.emplace(newId, currentClass.get());
		newPrivateClassList.emplace(newId, std::move(currentClass));
	}

	privateClassList_ = std::move(newPrivateClassList);
	classStructure_ = newClassStructure;

	// update the reference IDS


	for (const std::pair<int, IfcClass*>& currentPair : classStructure_)
	{
		IfcClass* currentClass = currentPair.second;
		currentClass->remapClassRelations(referenceMap);
	}

	return;
}

void IfcFile::roundFloats(int floatLength)
{
	std::cout << "[INFO] round floating point values to " << 1 / pow(10, floatLength) << "\n";
	// open the file as generic file and delete lines ad id
	for (auto fileIt = begin(); fileIt != end(); ++fileIt)
	{
		std::pair<int, IfcClass*> currentIdClassPair = *fileIt;
		IfcClass* currentClass = currentIdClassPair.second;
		currentClass->RoundFloatingValues(floatLength);
	}
	std::cout << "success" << std::endl;
	return;
}

void IfcFile::updateReference(const std::map<int, int>& referenceMap)
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

		std::pair<int, IfcClass*> currentIdClassPair = *fileIt;
		int referenceIdx = currentIdClassPair.first;
		if (referenceMap.find(referenceIdx) != referenceMap.end()) { continue; }

		IfcClass* referenceClass = currentIdClassPair.second;
		referenceClass->remapClassRelations(referenceMap);
	}
	std::cout << "processing objects: " << counter << " (" << "100%)\n";
	return;
}

void IfcFile::collapseClasses(int maxIt, int iteration)
{
	int counter = 0;
	int lineCount = getClassCount();

	std::cout << "\n[INFO] collapsing redundant classes, iteration: " << iteration << "\n";

	std::map<std::string, std::unordered_map<std::string, int>> uniqueItemMap;
	std::map<int, int> oldNewRefMap;

	for (auto fileIt = begin(); fileIt != end(); ++fileIt)
	{
		if (counter % 1000 == 0)
		{
			std::cout << "processing objects: " << counter << " (" << (counter * 100) / lineCount << "%)\r";
		}
		counter++;

		std::pair<int, IfcClass*> currentIdClassPair = *fileIt;
		int currentId = currentIdClassPair.first;

		IfcClass* currentClass = currentIdClassPair.second;
		std::string classType = currentClass->getClassType();
		std::string classData = currentClass->getData();

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

	std::map<int, int> IdRefMap;
	if (!oldNewRefMap.empty() && maxIt != iteration)
	{
		std::cout << "reduced objects to from " << counter << " to " << counter - oldNewRefMap.size() << "\n";
		iteration += 1;
		collapseClasses(maxIt, iteration);
	}
	return;
}

bool IfcFile::recalculateId(bool restructure)
{
	std::cout << "\n[INFO] recalcualating Ids\n";

	std::map<int, int> remappedId;
	if (restructure)
	{
		std::string delimiters = "(),";
		std::map<int, int> pairIdOccuranceList;

		for (auto fileIt = begin(); fileIt != end(); ++fileIt)
		{
			std::pair<int, IfcClass*> currentIdClassPair = *fileIt;
			int currentId = currentIdClassPair.first;
			pairIdOccuranceList[currentId] = 0;
		}

		for (auto fileIt = begin(); fileIt != end(); ++fileIt)
		{
			std::pair<int, IfcClass*> currentIdClassPair = *fileIt;

			std::vector<std::string> tokenizedString = currentIdClassPair.second->tokenizeData(delimiters);

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
			std::pair<int, IfcClass*> currentIdClassPair = *fileIt;
			int originId = currentIdClassPair.first;
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
	std::map<int, std::unordered_set<int>> indx2Relation;
	std::map<int, bool> evalMap;

	int IfcProjectId = -1;

	std::string delimiters = "(),";
	for (const std::pair<int, IfcClass*> idxClassPair : classStructure_)
	{
		int currentIdx = idxClassPair.first;
		IfcClass* currentClass = idxClassPair.second;

		if (IfcProjectId == -1)
		{
			std::string classType = currentClass->getClassType();
			std::transform(classType.begin(), classType.end(), classType.begin(), ::toupper);

			if (classType == "IFCPROJECT")
			{
				IfcProjectId = currentIdx;
			}
		}

		std::unordered_set<int> relatedId;
		if (indx2Relation.find(currentIdx) != indx2Relation.end())
		{
			relatedId = indx2Relation[currentIdx];
		}

		std::vector<std::string> tokenizedString = currentClass->tokenizeData(delimiters);
		for (const std::string& currentToken : tokenizedString)
		{
			if (currentToken.length() == 0) { continue; }
			if (currentToken[0] != '#') { continue; }
			int id = std::stoi(currentToken.substr(1));
			relatedId.insert(id);

			if (indx2Relation.find(id) == indx2Relation.end())
			{
				std::unordered_set<int> localId;
				localId.emplace(currentIdx);
				indx2Relation.emplace(id, localId);
				continue;
			}

			indx2Relation[id].emplace(currentIdx);

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
	for (std::pair<int, bool> currentOcc : evalMap)
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

	for (const std::pair<int, IfcClass*>& currentPair : classStructure_)
	{
		dumpedString += "#" + std::to_string(currentPair.first) + "=" + currentPair.second->getData();
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
