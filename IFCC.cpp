#define USE_IFC2x3
#define programVersion "0.2.0"

#include <fstream>
#include <iostream>
#include <filesystem>
#include <ifcparse/IfcFile.h>
#include <ifcparse/utils.h>
#include <ifcparse/IfcEntityInstanceData.h>
#include <ifcparse/IfcHierarchyHelper.h>

#include <boost/make_shared.hpp>
#include <boost/optional/optional_io.hpp>

std::unique_ptr<IfcParse::IfcFile> forcefullDelete(std::unique_ptr<IfcParse::IfcFile> theFile, std::filesystem::path pathToFile, std::vector<int> toBeDeletedIdist)
{
	if (toBeDeletedIdist.empty()) 
	{
		std::cout << "no objects to delete" << std::endl; 
		return theFile;
	}

	std::sort(toBeDeletedIdist.begin(), toBeDeletedIdist.end());
	auto last = std::unique(toBeDeletedIdist.begin(), toBeDeletedIdist.end());
	toBeDeletedIdist.erase(last, toBeDeletedIdist.end());

	// store the data to the temp file path
	std::cout << "storing updated References\n";

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
		std::sort(toBeDeletedIdist.begin(), toBeDeletedIdist.end());

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
			for (size_t i = currentDeleteLoc; i < toBeDeletedIdist.size(); i++)
			{
				const int& deletorIndx = toBeDeletedIdist[i];

				if (currentId > deletorIndx) 
				{ 
					currentDeleteLoc = i;
					continue;
				}
				if (currentId < deletorIndx) { break; }

				ignore = true;

				delCount++;
				if (delCount % 100 == 0)
				{
					std::cout << delCount << " objects removed from file\r";
				}
				break;
			}
			if (!ignore)
			{
				tempFile << line << "\n";
			}
		}

		// Close the file stream once all lines have been
		// read.
		streamFile.close();
	}
	else
	{
		return false;
	}
	tempFile.close();
	std::cout << delCount << " objects removed from file\n";

	std::filesystem::copy_file(tempPath, pathToFile, std::filesystem::copy_options::overwrite_existing);
	std::filesystem::remove(tempPath);

	theFile = std::make_unique<IfcParse::IfcFile>(pathToFile.string());
	return theFile;
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
	std::cout << "succes" << std::endl;
	return;
}

std::unique_ptr<IfcParse::IfcFile> collapseClasses(std::unique_ptr<IfcParse::IfcFile> theFile, const std::filesystem::path& pathToFile, int iteration = 1)
{
	int maxBatchSize = 500000;
	bool isCut = false; 

	std::map<std::string, std::unordered_map<std::string, IfcUtil::IfcBaseClass*>> uniqueItemMap;
	std::map<int, std::string> storedOutputStrings;
	std::vector<int> toBeDeletedIndx;

	int counter = 0;

	std::cout << "\n[INFO] collapsing redundant classes, iteration: " << iteration << "\n";

	for (auto fileIt = theFile->begin(); fileIt != theFile->end(); ++fileIt)
	{
		if (counter % 100 == 0)
		{
			std::cout << "processing objects: " << counter << "\r";
		}
		
		counter++;

		auto t = *fileIt;

		IfcUtil::IfcBaseClass* test = t.second;
		std::string classStringRep = test->data().toString();
		
		std::string classType = test->data().type()->name();
		std::string dataClassComp = classStringRep.substr(classStringRep.find_first_of("("), classStringRep.size());

		if (uniqueItemMap.find(classType) == uniqueItemMap.end())
		{
			std::unordered_map<std::string, IfcUtil::IfcBaseClass*> newMap;
			newMap.emplace(dataClassComp, test);
			uniqueItemMap.emplace(classType, newMap);
			continue;
		}

		auto [storedPropertyKey, inserted] = uniqueItemMap[classType].emplace(dataClassComp, test);
		if (inserted) { continue; }

		IfcUtil::IfcBaseClass* uniqueItem = storedPropertyKey->second;

		aggregate_of_instance::ptr referencedBy = theFile->instances_by_reference(t.first);

		for (auto referenceIt = referencedBy->begin(); referenceIt != referencedBy->end(); ++referenceIt)
		{
			IfcUtil::IfcBaseClass* reference = *referenceIt;
			for (size_t i = 0; i < reference->data().getArgumentCount(); i++)
			{
				Argument* currentArgument = reference->data().getArgument(i);
				IfcUtil::ArgumentType currentArgumentType = currentArgument->type();

				if (currentArgumentType == IfcUtil::Argument_AGGREGATE_OF_ENTITY_INSTANCE)
				{
					aggregate_of_instance::ptr currentAggregate = currentArgument->operator aggregate_of_instance::ptr();
					aggregate_of_instance::ptr newAggregate = boost::make_shared< aggregate_of_instance>();

					for (auto aggregateIt = currentAggregate->begin(); aggregateIt != currentAggregate->end(); ++ aggregateIt)
					{
						IfcUtil::IfcBaseClass* argumentBaseclass = *aggregateIt;
						if (argumentBaseclass->data().id() != t.first)
						{
							newAggregate->push(argumentBaseclass);
							continue; 
						}
						newAggregate->push(uniqueItem);
					}

					IfcWrite::IfcWriteArgument* writeArgument = new IfcWrite::IfcWriteArgument();
					writeArgument->set(newAggregate);
					
					reference->data().setArgument(i, writeArgument);
				}
				
				if (currentArgumentType == IfcUtil::Argument_ENTITY_INSTANCE)
				{
					IfcUtil::IfcBaseClass* argumentBaseclass = currentArgument->operator IfcUtil::IfcBaseClass* ();
					if (argumentBaseclass->data().id() != t.first) { continue; }
					
					IfcWrite::IfcWriteArgument* writeArgument = new IfcWrite::IfcWriteArgument();
					writeArgument->set(uniqueItem);
					reference->data().setArgument(i, writeArgument);
				}
			}
		}

		auto refs = theFile->instances_by_reference(t.first);
		if (refs->size() != 0) {
			continue;
		}
		toBeDeletedIndx.emplace_back(t.first);

		if (toBeDeletedIndx.size() > maxBatchSize)
		{
			isCut = true;
			break;
		}

	}
	std::cout << "processing objects: " << counter << "\n";

	if (isCut)
	{
		std::cout << "max single batch delete size hit: " << maxBatchSize << "\n";
	}

	theFile = forcefullDelete(std::move(theFile), pathToFile, toBeDeletedIndx);
	if (!toBeDeletedIndx.empty())
	{
		iteration += 1;
		theFile = collapseClasses(std::move(theFile), pathToFile, iteration);
	}

	return theFile;
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

	std::cout << "\nread file\n";
	std::unique_ptr<IfcParse::IfcFile> ifcFile = std::make_unique<IfcParse::IfcFile>(filePath.string());
	ifcFile = std::make_unique<IfcParse::IfcFile>(filePath.string());
	if (!ifcFile->good()) { return 1; }
	std::cout << "file succesfully read\n";

	ifcFile = collapseClasses(std::move(ifcFile), filePath);

	std::ofstream storageFile;
	storageFile.open(outputPath);
	std::cout << "\n[INFO] Exporting file " << outputPath.string() << std::endl;
	storageFile << *ifcFile;
	std::cout << "[INFO] Exported succesfully" << std::endl;
	storageFile.close();

	std::filesystem::remove(localPath);

	return 0;
}

