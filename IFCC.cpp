#define USE_IFC2x3
#define programVersion "0.3.0"

#include <fstream>
#include <iostream>
#include <filesystem>
#include <unordered_set>

#include <ifcparse/IfcFile.h>
#include <ifcparse/utils.h>
#include <ifcparse/IfcEntityInstanceData.h>
#include <ifcparse/IfcHierarchyHelper.h>

#include <boost/make_shared.hpp>

bool forcefullDelete(std::unique_ptr<IfcParse::IfcFile> theFile, std::filesystem::path pathToFile, std::unordered_set<int> toBeDeletedIdist)
{
	if (toBeDeletedIdist.empty()) 
	{
		std::cout << "no objects to delete" << std::endl; 
		return true;
	}

	// store the data to the temp file path
	std::cout << "\n[INFO] storing updated References\n";

	std::ofstream storageFile;
	storageFile.open(pathToFile);
	storageFile << *theFile;
	storageFile.close();

	// generate second temp file
	std::filesystem::path tempPath = pathToFile.parent_path().string() + "/" + pathToFile.stem().string() + std::string("_2") + pathToFile.extension().string();
	std::ofstream tempFile(tempPath);

	// open the file as generic file and delete lines ad id
	std::ifstream streamFile(pathToFile);
	int delCount = 0;
	if (streamFile.is_open()) {
		std::string line;
		int currentDeleteLoc = 0;

		while (std::getline(streamFile, line)) {
			std::string stringId = line.substr(0, line.find("="));

			if (stringId[0] != '#') { 
				tempFile << line << "\n";
				continue;
			}
			
			int currentId = std::stoi(stringId.substr(1, stringId.size()));

			bool ignore = false;
			if (toBeDeletedIdist.find(currentId) != toBeDeletedIdist.end())
			{
				ignore = true;

				delCount++;
				if (delCount % 1000 == 0)
				{
					std::cout << delCount << " objects removed from file\r";
				}
				continue;
			}
			if (!ignore)
			{
				tempFile << line << "\n";
			}
		}

		// Close the file stream once all lines have been
		streamFile.close();
	}
	else
	{
		return false;
	}
	tempFile.close();
	std::cout << delCount << " objects removed from file\n\n";

	std::filesystem::copy_file(tempPath, pathToFile, std::filesystem::copy_options::overwrite_existing);
	std::filesystem::remove(tempPath);
	return true;
}

std::vector<std::string> tokenizeString(const std::string& theString, const std::string& delimiters) {
	std::vector<std::string> tokens;

	size_t start = 0;
	size_t pos = 0;

	while ((pos = theString.find_first_of(delimiters, start)) != std::string::npos)
	{
		if (pos > start)
			tokens.push_back(theString.substr(start, pos - start));

		tokens.push_back(theString.substr(pos, 1));

		start = pos + 1;
	}

	if (start < theString.size()) { tokens.push_back(theString.substr(start)); }

	return tokens;
}

bool stringIsNum(const std::string& theString)
{
	bool isFirst = true;
	for (const char& currentChar : theString)
	{
		if (isFirst)
		{
			isFirst = false;

			if (!std::isdigit(currentChar) && currentChar != '-' && currentChar != '+' && currentChar != '.')
			{
				return false;
			}
			continue;
		}

		if (!std::isdigit(currentChar) && currentChar != '.')
		{
			return false;
		}
	}
	return true;
}

std::string roundStringFloats(const std::string& theString, int floatLength)
{
	size_t fPointLoc = theString.find_first_of('.');

	int maxNumLength = fPointLoc + floatLength + 1;

	if (theString.size() <= maxNumLength)
	{
		return theString;
	}
	return theString.substr(0, maxNumLength);
}

void roundFloats(const std::filesystem::path& pathToFile, int floatLength)
{
	std::cout << "[INFO] round floating point values to " << 1 / pow(10, floatLength) << "\n";

	std::filesystem::path tempPath = pathToFile.parent_path().string() + "/" + pathToFile.stem().string() + std::string("_2") + pathToFile.extension().string();
	std::ofstream tempFile(tempPath);

	std::string delimiters = "(),";

	// open the file as generic file and delete lines ad id
	std::ifstream streamFile(pathToFile);
	int delCount = 0;
	if (streamFile.is_open()) {

		std::string line;
		while (std::getline(streamFile, line)) {
			if (line[0] != '#') {
				tempFile << line << "\n";
				continue;
			}
			std::vector<std::string> tokenizedString = tokenizeString(line, delimiters);
			
			std::string correctedString = "";
			for (const std::string& subString : tokenizedString)
			{
				if (!stringIsNum(subString))
				{
					correctedString += subString;
					continue;
				}
				correctedString += roundStringFloats(subString, floatLength);
			}
			tempFile << correctedString << "\n";
		}
		streamFile.close();
	}
	tempFile.close();
	std::filesystem::copy_file(tempPath, pathToFile, std::filesystem::copy_options::overwrite_existing);
	std::filesystem::remove(tempPath);
	std::cout << "success" << std::endl;
	return;
}

void updateReference(const std::set<IfcUtil::IfcBaseClass*>& baseClassList, const std::map<int, IfcUtil::IfcBaseClass*>& referenceMap)
{
	int counter = 0;
	int totalLineCount = baseClassList.size();

	for (IfcUtil::IfcBaseClass* reference : baseClassList)
	{
		if (counter % 1000 == 0)
		{
			std::cout << "processing objects: " << counter << " (" << (counter * 100) / totalLineCount << "%)\r";
		}
		counter++;

		IfcEntityInstanceData& referenceData = reference->data();
		if (referenceMap.find(referenceData.id()) != referenceMap.end()) { continue; }

		for (size_t i = 0; i < referenceData.getArgumentCount(); i++)
		{
			Argument* currentArgument = referenceData.getArgument(i);
			IfcUtil::ArgumentType currentArgumentType = currentArgument->type();

			if (currentArgumentType == IfcUtil::Argument_AGGREGATE_OF_ENTITY_INSTANCE)
			{
				aggregate_of_instance::ptr currentAggregate = currentArgument->operator aggregate_of_instance::ptr();
				aggregate_of_instance::ptr newAggregate = boost::make_shared< aggregate_of_instance>();
				newAggregate->reserve(currentAggregate->size());

				bool isNew = false;
				for (auto aggregateIt = currentAggregate->begin(); aggregateIt != currentAggregate->end(); ++aggregateIt)
				{
					IfcUtil::IfcBaseClass* argumentBaseclass = *aggregateIt;
					int currentId = argumentBaseclass->data().id();

					if (referenceMap.find(currentId) == referenceMap.end())
					{
						newAggregate->push(argumentBaseclass);
						continue;
					}
					newAggregate->push(referenceMap.at(currentId));

					isNew = true;
				}
				if (!isNew)
				{
					continue;
				}

				IfcWrite::IfcWriteArgument* writeArgument = new IfcWrite::IfcWriteArgument();
				writeArgument->set(newAggregate);

				referenceData.setArgument(i, writeArgument);
			}
			else if (currentArgumentType == IfcUtil::Argument_ENTITY_INSTANCE)
			{
				IfcUtil::IfcBaseClass* argumentBaseclass = currentArgument->operator IfcUtil::IfcBaseClass * ();
				int currentId = argumentBaseclass->data().id();

				if (referenceMap.find(currentId) == referenceMap.end())
				{
					continue;
				}
				IfcWrite::IfcWriteArgument* writeArgument = new IfcWrite::IfcWriteArgument();

				writeArgument->set(referenceMap.at(currentId));
				referenceData.setArgument(i, writeArgument);
			}
		}
	}
	std::cout << "processing objects: " << counter << " (" << "100%)\n";
	return;
}

bool hasGuid(IfcUtil::IfcBaseClass* baseClass)
{
	const IfcEntityInstanceData& currentDataItem = baseClass->data();

	if (currentDataItem.getArgumentCount() == 0) { return false; }
	Argument* currentFirstArg = currentDataItem.getArgument(0);
	if (currentFirstArg->type() == IfcUtil::Argument_STRING)
	{
		std::string stringCurrentFirstArg = currentFirstArg->toString();
		if (stringCurrentFirstArg.length() == 24)
		{
			if (stringCurrentFirstArg.find(" ") == 0)
			{
				return true;
			}
			return false;
		}
	}
	return false;
}

std::map<int, int> getIdRefCount(IfcParse::IfcFile* theFile)
{
	std::map<int, int> refMap;
	for (auto fileIt = theFile->begin(); fileIt != theFile->end(); ++fileIt)
	{
		auto indxObjectPair = *fileIt;
		int currentId = indxObjectPair.first;
		refMap.emplace(currentId, theFile->instances_by_reference(currentId)->size());
	}
	return refMap;
}


std::map<int, int> collapseClasses(const std::filesystem::path& pathToFile, std::unique_ptr<IfcParse::IfcFile> theFile = nullptr, std::unordered_set<int>* toBeDeletedIndx = nullptr, int iteration = 1)
{
	int counter = 0;
	std::ifstream inFile(pathToFile);
	int lineCount = std::count(std::istreambuf_iterator<char>(inFile), std::istreambuf_iterator<char>(), '\n');

	if (theFile == nullptr)
	{
		std::cout << "\nread file\n";
		theFile = std::make_unique<IfcParse::IfcFile>(pathToFile.string());
		if (!theFile->good()) { return std::map<int, int>(); }
		std::cout << "file successfully read\n";
	}

	if (toBeDeletedIndx == nullptr)
	{
		toBeDeletedIndx = new std::unordered_set<int>();
		toBeDeletedIndx->reserve(lineCount);
	}

	int currentDelSize = toBeDeletedIndx->size();
	std::map<std::string, std::unordered_map<std::string, IfcUtil::IfcBaseClass*>> uniqueItemMap;
	std::map<int, IfcUtil::IfcBaseClass*> oldNewRefMap;
	std::set<IfcUtil::IfcBaseClass*> fullBaseClassList;
	
	std::cout << "\n[INFO] collapsing redundant classes, iteration: " << iteration << "\n";

	for (auto fileIt = theFile->begin(); fileIt != theFile->end(); ++fileIt)
	{
		if (counter % 1000 == 0)
		{
			std::cout << "processing objects: " << counter << " (" << (counter * 100) /lineCount << "%)\r";
		}	
		counter++;

		auto indxObjectPair = *fileIt;
		int currentId = indxObjectPair.first;

		if (toBeDeletedIndx->find(currentId) != toBeDeletedIndx->end()) { continue; }

		IfcUtil::IfcBaseClass* currentItem = indxObjectPair.second;
		fullBaseClassList.emplace(currentItem);

		if (hasGuid(currentItem)) { continue; }
		
		std::string classStringRep = currentItem->data().toString();
		std::string classType = currentItem->data().type()->name();
		std::string dataClassComp = classStringRep.substr(classStringRep.find_first_of("("), classStringRep.size());

		if (uniqueItemMap.find(classType) == uniqueItemMap.end())
		{
			std::unordered_map<std::string, IfcUtil::IfcBaseClass*> newMap;
			newMap.emplace(dataClassComp, currentItem);
			uniqueItemMap.emplace(classType, newMap);
			continue;
		}

		auto [storedPropertyKey, inserted] = uniqueItemMap[classType].emplace(dataClassComp, currentItem);
		if (inserted) { continue; }

		IfcUtil::IfcBaseClass* uniqueItem = storedPropertyKey->second;
		oldNewRefMap.emplace(currentId, uniqueItem);
	}
	std::cout << "processing objects: " << counter << " (100%)\n";
	std::cout << "[INFO] Updating References\n";
	updateReference(fullBaseClassList, oldNewRefMap);
	
	for (auto remapPair : oldNewRefMap)
	{
		auto refs = theFile->instances_by_reference(remapPair.first);
		if (refs->size() != 0) {
			continue;
		}
		toBeDeletedIndx->emplace(remapPair.first);
	}

	std::map<int, int> IdRefMap;
	if (currentDelSize != toBeDeletedIndx->size())
	{
		std::cout << "reduced objects to from " << counter << " to " << counter - toBeDeletedIndx->size() << "\n";

		iteration += 1;
		IdRefMap = collapseClasses(pathToFile, std::move(theFile), toBeDeletedIndx,  iteration);
	}
	else
	{
		IdRefMap = getIdRefCount(theFile.get());
		forcefullDelete(std::move(theFile), pathToFile, *toBeDeletedIndx);
		delete toBeDeletedIndx;
		toBeDeletedIndx = nullptr;
	}
	return IdRefMap;
}

bool restructureFile(const std::filesystem::path& pathToFile, std::map<int, int> idRefCountMap) {
	std::cout << "[INFO] restructure file\n";
	std::filesystem::path tempPath = pathToFile.parent_path().string() + "/" + pathToFile.stem().string() + std::string("_2") + pathToFile.extension().string();


	std::string delimiters = "=";
	std::ifstream streamFile(pathToFile);
	int currentId = 1;

	std::vector<std::string> fileStringVecHeader;
	std::vector<std::string> fileStringVecFooter;
	std::map<int, std::string> remappedId;
	if (streamFile.is_open()) {

		std::string line;
		bool isHeader = true;
		while (std::getline(streamFile, line)) {
			if (line[0] != '#') {
				if (isHeader)
				{
					fileStringVecHeader.emplace_back(line);
				}
				else
				{
					fileStringVecFooter.emplace_back(line);
				}
				continue;
			}
			isHeader = false;

			std::string stringId = line.substr(0, line.find_first_of("="));

			if (stringId.size() <= 1) { continue; }

			int id = std::stoi(stringId.substr(1));
			remappedId.emplace(id, line);
		}
		streamFile.close();
	}

	std::vector<std::pair<int, int>> vec(idRefCountMap.begin(), idRefCountMap.end());

	// sort by occurrence (value), descending
	std::sort(vec.begin(), vec.end(), [](const auto& a, const auto& b) {
		return a.second > b.second;
		});

	// extract only the ids
	std::vector<int> sortedIds;
	sortedIds.reserve(vec.size());

	for (const auto& [id, count] : vec) {
		sortedIds.push_back(id);
	}

	std::ofstream tempFile(tempPath);
	for (const std::string& line : fileStringVecHeader)
	{
		tempFile << line << "\n";
	}

	for (int currentId : sortedIds)
	{
		if (remappedId.find(currentId) == remappedId.end())
		{
			continue;
		}
		tempFile << remappedId[currentId] << "\n";
	}

	for (const std::string& line : fileStringVecFooter)
	{
		tempFile << line << "\n";
	}

	tempFile.close();
	std::filesystem::copy_file(tempPath, pathToFile, std::filesystem::copy_options::overwrite_existing);
	std::filesystem::remove(tempPath);
	std::cout << "success" << std::endl;
	return true;
}

bool recalculateId(const std::filesystem::path& pathToFile)
{
	std::cout << "[INFO] recalcualating Ids\n";
	std::filesystem::path tempPath = pathToFile.parent_path().string() + "/" + pathToFile.stem().string() + std::string("_2") + pathToFile.extension().string();
	std::map<std::string, std::string> remappedId;

	std::string delimiters = "=";
	std::ifstream streamFile(pathToFile);
	int currentId = 1;

	std::vector<std::string> fileStringVec;
	if (streamFile.is_open()) {

		std::string line;
		while (std::getline(streamFile, line)) {
			if (line[0] != '#') {
				fileStringVec.emplace_back(line);
				continue;
			}
			std::vector<std::string> tokenizedString = tokenizeString(line, delimiters);
			std::string IdSubString = tokenizedString[0];

			if (IdSubString[0] != '#') { continue; }

			std::string indxString = "#" + std::to_string(currentId);
			std::string correctedString = indxString;
			remappedId.emplace(IdSubString, indxString);
			currentId++;

			for (size_t i = 1; i < tokenizedString.size(); i++)
			{
				correctedString += tokenizedString[i];
			}
			fileStringVec.emplace_back(correctedString);
		}
		streamFile.close();
	}

	delimiters = "(),";
	std::ofstream tempFile(tempPath);
	for (const std::string& line : fileStringVec)
	{
		if (line[0] != '#') {
			tempFile << line << "\n";
			continue;
		}
		std::vector<std::string> tokenizedString = tokenizeString(line, delimiters);
		std::string correctedString = tokenizedString[0];
		for (size_t i = 1; i < tokenizedString.size(); i++)
		{
			std::string currentSubString = tokenizedString[i];
			if (remappedId.find(currentSubString) != remappedId.end())
			{
				currentSubString = remappedId.at(currentSubString);
			}
			correctedString += currentSubString;
		}
		tempFile << correctedString << "\n";
		streamFile.close();
	}
	tempFile.close();
	std::filesystem::copy_file(tempPath, pathToFile, std::filesystem::copy_options::overwrite_existing);
	std::filesystem::remove(tempPath);
	std::cout << "success" << std::endl;
	return true;

}

bool isValidIfcFile(const std::filesystem::path& filePath)
{
	if (!std::filesystem::exists(filePath)) { return false; } 

	std::string pathExtension = filePath.extension().string();
	std::transform(pathExtension.begin(), pathExtension.end(), pathExtension.begin(), ::toupper);
	if (pathExtension != ".IFC") { return false; }
	return true;
}

void printDefaultstartInfo() {
	std::cout << "\n"
		" _____ _____ ____ ____ \n"
		"|_   _|  __ / __ / ___|\n"
		"  | | | |_ | |  | | \n"
		" _| |_|  _|| |__| |___ \n"
		"|_____|_|   \\____\\____| version " << programVersion << "\n\n"
		"Compress IFC files with minimal data loss\n==================================================\n";
}

void helpOutput() {
	printDefaultstartInfo();
	std::cout << "Usage: IFCC.exe 'IFC target path' 'optional IFC output path'\nIf no output filepath is supplied the stem path is used with '_compressed' added";
}


bool getUserInput(int argc, char* argv[], std::filesystem::path* filePath, std::filesystem::path* outputPath)
{
	if (argc <= 1)
	{
		std::cout << "please supply arguments\nUse --help for readme\n";
		return false;
	}

	if (std::string(argv[1]) == "--help")
	{
		helpOutput();
		return false;
	}

	*filePath = std::string(argv[1]);
	if (!isValidIfcFile(*filePath))
	{
		std::cout << "invalid IFC input path\nUse --help for readme\n";
		return false;
	}

	*outputPath = "";
	if (argc > 2)
	{
		*outputPath = std::string(argv[2]);
		if (!std::filesystem::exists(outputPath->parent_path()))
		{
			std::cout << "invalid IFC output path\nUse --help for readme\n";
		}
	}
	if (*filePath == *outputPath)
	{
		std::cout << "Input path is same as output path\n not allowed\n";
		return false;
	}

	if (outputPath->string().size() == 0)
	{
		*outputPath = filePath->parent_path().string() + "/" + filePath->stem().string() + std::string("_compressed") + filePath->extension().string();
	}
	return true;
}


int main(int argc, char* argv[])
{
	std::filesystem::path filePath = "";
	std::filesystem::path outputPath = "";
	std::filesystem::path localPath = std::filesystem::path("./temp.ifc");

	printDefaultstartInfo();

	if (!getUserInput(argc, argv, &filePath, &outputPath)) { return 0; }

	std::cout << "\nInput path: " << filePath.string() << "\n";
	std::cout << "Output path: " << outputPath.string() << "\n\n";

	std::filesystem::copy(filePath, localPath, std::filesystem::copy_options::overwrite_existing);
	filePath = localPath;

	int precisionPoint = 6;
	roundFloats(filePath, 6);
	std::map<int, int> idRefCountMap = collapseClasses(filePath);
	restructureFile(filePath, idRefCountMap);
	recalculateId(filePath);

	std::ofstream storageFile;
	storageFile.open(outputPath);
	std::cout << "\n[INFO] Exporting file " << outputPath.string() << std::endl;
	std::filesystem::copy_file(localPath, outputPath, std::filesystem::copy_options::overwrite_existing);
	std::cout << "[INFO] Exported successfully" << std::endl;
	storageFile.close();

	std::filesystem::remove(localPath);

	return 0;
}

