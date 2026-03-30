#define USE_IFC2x3
#define programVersion "0.4.0"

#include <fstream>
#include <iostream>
#include <sstream>
#include <filesystem>
#include <unordered_set>
#include <unordered_map>
#include <map>
#include <string>

std::vector<std::string> tokenizeString(const std::string& theString, const std::string& delimiters);

struct IfcClass{
	int id;
	std::string classType;
	bool hasGuid;
	std::string data = "";
};

class IfcFile {
private:
	std::string header_ = "";
	std::string footer_ = "";
	std::map<int, IfcClass*> classStructure_;
	std::map<int, std::unique_ptr<IfcClass>> privateClassList_;
public:

	IfcFile(const std::string& filePath) {
		std::ifstream streamFile(filePath);
		if (streamFile.is_open()) {

			std::string line;

			bool isHeader = true;

			while (std::getline(streamFile, line)) {
				if (line[0] != '#') {
					if (isHeader) { header_ += line + "\n"; }
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

				IfcClass currentClass{
					id,
					stringData.substr(0, stringData.find_first_of("(")),
					false,
					stringData
				};
				std::unique_ptr<IfcClass> uniqueClassPtr = std::make_unique<IfcClass>(currentClass);
				classStructure_.emplace(id, uniqueClassPtr.get());
				privateClassList_.emplace(id, std::move(uniqueClassPtr));
			}
		}
	}

	std::string getHeader() const { return header_; }
	std::string getFooter() const { return footer_; }

	void removeClass(int idx) {
		classStructure_.erase(idx);
		privateClassList_.erase(idx);
	}

	void restructureFile(const std::map<int, int>& referenceMap) {

		// update the object IDs
		std::map<int, std::unique_ptr<IfcClass>> newPrivateClassList;
		std::map<int, IfcClass*> newClassStructure;

		for (const std::pair<int,int>& currentPair : referenceMap)
		{
			int oldId = currentPair.first;
			int newId = currentPair.second;

			std::unique_ptr<IfcClass> currentClass = std::move(privateClassList_[oldId]);
			currentClass->id = newId;
			newClassStructure.emplace(newId, currentClass.get());
			newPrivateClassList.emplace(newId, std::move(currentClass));
		}

		privateClassList_ = std::move(newPrivateClassList);
		classStructure_ = newClassStructure;

		// update the reference IDS

		std::string delimiters = "(),";

		for (const std::pair<int, IfcClass*>& currentPair : classStructure_)
		{
			IfcClass* currentClass = currentPair.second;
			std::string datastring = currentClass->data;

			std::vector<std::string> tokenizedString = tokenizeString(datastring, delimiters);
			std::string correctedString = tokenizedString[0];
			for (size_t i = 1; i < tokenizedString.size(); i++)
			{
				std::string currentSubString = tokenizedString[i];
				
				if (currentSubString[0] != '#')
				{
					correctedString += currentSubString;
					continue;
				}
				int id = std::stoi(currentSubString.substr(1));

				if (referenceMap.find(id) != referenceMap.end())
				{
					currentSubString = "#" + std::to_string(referenceMap.at(id));
				}
				correctedString += currentSubString;
			}
			currentClass->data = correctedString;
		}

		return;
	}

	std::map<int, IfcClass*>::iterator begin() {
		return classStructure_.begin();
	}

	std::map<int, IfcClass*>::iterator end() {
		return classStructure_.end();
	}

	int classCount() const {
		return classStructure_.size();
	}

	std::string dumptoString() const {
		std::string dumpedString = header_ + "\n";

		for (const std::pair<int, IfcClass*>& currentPair : classStructure_)
		{
			dumpedString += "#" + std::to_string(currentPair.first) + "=" + currentPair.second->data + "\n";
		}
		dumpedString += footer_;

		return dumpedString;
	}

};

/// returns a vector consisting out of all the elements that were split by the delimiters with the delimiters included in the vector
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

/// check if string is num
/// A num is any string value that consists completely out of digits or has a first char that is +, -, or . 
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

/// rounds the floats
std::string roundStringFloats(const std::string& theString, int floatLength)
{
	std::stringstream stream;
	stream << std::fixed << std::setprecision(floatLength) << std::stod(theString);

	std::string roundedFloat = stream.str();

	size_t end = roundedFloat.find_last_not_of('0');
	if (end != std::string::npos)
		roundedFloat.erase(end + 1);

	return roundedFloat;
}

void roundFloats(IfcFile& theFile, int floatLength)
{
	std::cout << "[INFO] round floating point values to " << 1 / pow(10, floatLength) << "\n";
	std::string delimiters = "(),";

	// open the file as generic file and delete lines ad id
	for (auto fileIt = theFile.begin(); fileIt != theFile.end(); ++ fileIt)
	{
		std::pair<int, IfcClass*> currentIdClassPair = *fileIt;
		IfcClass* currentClass = currentIdClassPair.second;

		std::vector<std::string> tokenizedString = tokenizeString(currentClass->data, delimiters);
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
		currentClass->data = correctedString;
	}
	std::cout << "success" << std::endl;
	return;
}

void updateReference(IfcFile& theFile, const std::map<int, int>& referenceMap)
{
	int counter = 0;
	int totalLineCount = theFile.classCount();

	std::string delimiters = "(),";

	for (auto fileIt = theFile.begin(); fileIt != theFile.end(); ++fileIt)
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
		std::string referenceData = referenceClass->data;

		std::vector<std::string> tokenizedRefData = tokenizeString(referenceData, delimiters);
		std::string correctedDataString = "";
		for (const std::string& currentToken : tokenizedRefData)
		{
			if (currentToken.length() == 0) { continue; }
			if (currentToken[0] != '#') 
			{
				correctedDataString += currentToken;
				continue; 
			}
			int id = std::stoi(currentToken.substr(1));
			if (referenceMap.find(id) == referenceMap.end())
			{
				correctedDataString += currentToken;
				continue;
			}
			correctedDataString += "#";
			correctedDataString += std::to_string(referenceMap.at(id));
		}

		referenceClass->data = correctedDataString;
	}
	std::cout << "processing objects: " << counter << " (" << "100%)\n";
	return;
}

void collapseClasses(IfcFile& theFile, int iteration = 1)
{
	int counter = 0;
	int lineCount = theFile.classCount();
	std::unordered_set<int> toBeDeletedIndx;

	std::cout << "\n[INFO] collapsing redundant classes, iteration: " << iteration << "\n";

	std::map<std::string, std::unordered_map<std::string, int>> uniqueItemMap;
	std::map<int, int> oldNewRefMap;

	for (auto fileIt = theFile.begin(); fileIt != theFile.end(); ++fileIt)
	{
		if (counter % 1000 == 0)
		{
			std::cout << "processing objects: " << counter << " (" << (counter * 100) / lineCount << "%)\r";
		}
		counter++;

		std::pair<int, IfcClass*> currentIdClassPair = *fileIt;
		int currentId = currentIdClassPair.first;

		IfcClass* currentClass = currentIdClassPair.second;
		std::string classType = currentClass->classType;
		std::string classData = currentClass->data;

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
	updateReference(theFile, oldNewRefMap);

	for (auto remapPair : oldNewRefMap)
	{
		theFile.removeClass(remapPair.first);
	}

	std::map<int, int> IdRefMap;
	if (!oldNewRefMap.empty())
	{
		std::cout << "reduced objects to from " << counter << " to " << counter - oldNewRefMap.size() << "\n";
		iteration += 1;
		collapseClasses(theFile, iteration);
	}
	return;
}


bool recalculateId(IfcFile& theFile, bool restructure)
{
	std::cout << "\n[INFO] recalcualating Ids\n";

	std::map<int, int> remappedId;
	if (restructure)
	{
		std::string delimiters = "(),";
		std::map<int, int> pairIdOccuranceList;

		for (auto fileIt = theFile.begin(); fileIt != theFile.end(); ++fileIt)
		{
			std::pair<int, IfcClass*> currentIdClassPair = *fileIt;
			int currentId = currentIdClassPair.first;
			pairIdOccuranceList[currentId] = 0;
		}

		for (auto fileIt = theFile.begin(); fileIt != theFile.end(); ++fileIt)
		{
			std::pair<int, IfcClass*> currentIdClassPair = *fileIt;
			std::string classDataString = currentIdClassPair.second->data;

			std::vector<std::string> tokenizedString = tokenizeString(classDataString, delimiters);

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
		for (auto fileIt = theFile.begin(); fileIt != theFile.end(); ++fileIt)
		{
			std::pair<int, IfcClass*> currentIdClassPair = *fileIt;
			int originId = currentIdClassPair.first;
			remappedId.emplace(originId, currentId);
			currentId++;
		}
	}
	theFile.restructureFile(remappedId);

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

	printDefaultstartInfo();
	if (!getUserInput(argc, argv, &filePath, &outputPath)) { return 0; }

	std::cout << "\nInput path: " << filePath.string() << "\n";
	std::cout << "Output path: " << outputPath.string() << "\n";

	std::cout << "\n[INFO] read file\n";
	IfcFile theFile = IfcFile(filePath.string());
	std::cout << "file successfully read\n\n";

	int precisionPoint = 6;
	roundFloats(theFile, 6);
	collapseClasses(theFile);
	recalculateId(theFile, true);

	std::ofstream storageFile;
	storageFile.open(outputPath);
	std::cout << "\n[INFO] Exporting file " << outputPath.string() << std::endl;
	storageFile << theFile.dumptoString();
	std::cout << "[INFO] Exported successfully" << std::endl;
	storageFile.close();

	return 0;
}

