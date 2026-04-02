#define USE_IFC2x3
#define programVersion "0.5.0"

#include <IfcFile.hpp>
#include <IfcClass.hpp>

#include <fstream>
#include <iostream>
#include <sstream>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

struct UserSettings {
	int floatPrecision = 6;
};

bool isValidIfcFile(const std::filesystem::path& filePath, bool isOutputPath = false)
{
	if (!isOutputPath)
	{
		if (!std::filesystem::exists(filePath)) { return false; }
	}

	std::string pathExtension = filePath.extension().string();
	std::transform(pathExtension.begin(), pathExtension.end(), pathExtension.begin(), ::toupper);

	if (isOutputPath)
	{
		if (pathExtension == ".IFCZIP") 
		{ 
			return true; 
		}
	}

	if (pathExtension == ".IFC") { return true; }
	return false;
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
	std::cout << "Usage: IFCC.exe 'IFC/IFCZIP target path' 'optional IFC output path'\nIf no output filepath is supplied the stem path is used with '_compressed' added\n";
	std::cout << "Outputpath can end with .ifc for ifc encoded output and .ifczip for zipped output.\n";
	std::cout << "Settings: \n\n";
	std::cout << "'--Pn'      set decimal size where n = an int for the size \n";
}

bool isPath(const std::string& currentString) {
	std::filesystem::path path = currentString;

	return path.has_filename() & path.has_extension();
}


bool getUserInput(int argc, char* argv[], std::filesystem::path* filePath, std::filesystem::path* outputPath, UserSettings* userSettings)
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
	if (!isValidIfcFile(*filePath, true))
	{
		std::cout << "invalid IFC input path\nUse --help for readme\n";
		return false;
	}

	*outputPath = "";
	if (argc > 2)
	{
		for (size_t i = 2; i < argc; i++)
		{
			std::string currentArg = std::string(argv[i]);

			// check if it is a valid file format
			if (currentArg[0] != ' - ' && isPath(currentArg))
			{
				std::filesystem::path outputPathInput = std::string(argv[i]);
				if (std::filesystem::exists(outputPathInput.parent_path()))
				{
					*outputPath = outputPathInput;
					continue;		
				}
				std::cout << "invalid IFC output path\nUse --help for readme\n";
				return false;
			}
			if (currentArg.size() > 3)
			{
				std::string settingType = currentArg.substr(0, 3);

				if (settingType == "--p")
				{
					try
					{
						userSettings->floatPrecision = std::stoi(currentArg.substr(3, currentArg.size() - 3));
						continue;
					}
					catch (const std::exception&)
					{
						// just do nothing
					}
				}
			}	

			std::cout << "Item '" << currentArg << "'is not reconized";;
			std::cout << "\nUse --help for readme\n";
			return false;
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
	else
	{
		if (!isValidIfcFile(*outputPath, true))
		{
			std::cout << "invalid IFC output path\nUse --help for readme\n";
			return false;
		}
	}


	return true;
}

int main(int argc, char* argv[])
{
	std::filesystem::path filePath = "";
	std::filesystem::path outputPath = "";

	printDefaultstartInfo();

	UserSettings userSettings;
	if (!getUserInput(argc, argv, &filePath, &outputPath, &userSettings)) { return 0; }

	std::cout << "\nInput path: " << filePath.string() << "\n";
	std::cout << "Output path: " << outputPath.string() << "\n";

	std::cout << "\n[INFO] read file\n";
	IfcFile theFile = IfcFile(filePath.string());
	if (!theFile.isGood()) { return 0; }
	std::cout << "file successfully read\n\n";

	theFile.roundFloats(userSettings.floatPrecision);
	theFile.collapseClasses();
	theFile.recalculateId(true);

	std::cout << "\n[INFO] Exporting file " << outputPath.string() << std::endl;
	theFile.storeFile(outputPath.string());
	std::cout << "[INFO] Exported successfully" << std::endl;
	return 0;
}

