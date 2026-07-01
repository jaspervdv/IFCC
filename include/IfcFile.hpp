#include "IfcClass.hpp"

#include <string>
#include <map>
#include <filesystem>
#include <set>

#ifndef IFCFILE_IFCFILE_H
#define IFCFILE_IFCFILE_H


class IfcFile {
private:
	std::string header_ = "";
	std::string footer_ = "";
	std::map<int, IfcClass> privateClassList_;

	std::vector<std::string> classTypes_ = {};

	bool isGood_ = false;
	bool prettyPrint_ = false;

	/// checks if filesystem path has an IFCZIP extension
	bool pathIsZip(const std::filesystem::path& filePath);

	bool pathIsFrag(const std::filesystem::path& filePath);

	/// unzippes an IFCZIP file and casts it to ifstream
	std::string unZip(const std::string& filePaht);

	/// stores a Fragments file and stores it as the outputpath
	void storeFrag(const std::filesystem::path& outputPath);

	/// Zippes a file and stores it at the outputpath
	void storeFileZip(const std::filesystem::path& outputPath);

	/// searches for 7Zip and attempts to store it
	bool storeFile7Zip(const std::filesystem::path& outputPath, const std::filesystem::path& fileName);

	/// stores an IFC file to the outputpath
	void storeFileIFC(const std::filesystem::path& outputPath);

	/// break the datastring into single IFC data entry points
	std::vector<std::string_view> splitDataString(const std::string& dataString);

public:

	IfcFile() {};
	IfcFile(const std::string& filePath, bool prettyPrint);

	std::string getHeader() const { return header_; }
	std::string getFooter() const { return footer_; }
	std::map<int, IfcClass>::iterator begin() { return privateClassList_.begin(); }
	std::map<int, IfcClass>::iterator end() { return privateClassList_.end(); }
	int getClassCount() const { return privateClassList_.size(); }

	void setPrettyPrint(bool b) { prettyPrint_ = b; }

	/// inits the data from a datastring
	bool initFromString(const std::string& dataString);

	/// removes a class at idx from the ifcfile class
	void removeClass(int idx);

	/// restructures the file according to the reference<old loc, new loc>
	void restructureFile(const std::unordered_map<int, int>& referenceMap);

	/// founds the floating values that are stored in the file to specified float precision
	void roundFloats(int floatLength);

	/// update the references according to the reference map <old loc, new loc>
	void updateReference(const std::unordered_map<int, int>& referenceMap);

	/// removes redundant classes and updates the references to not be dangling
	void collapseClasses(int maxIt, int iteration = 1);

	/// recalculate the ID so that the first object is 1 and every net ID is +1 of the one before 
	bool recalculateId(bool restructure);

	void removingDangling();

	/// output all the IfcFile data to a single string
	std::string dumptoString() const;

	/// stores the file at the specified output path
	void storeFile(const std::filesystem::path& outputPath);

	bool isGood() { return isGood_; }
};

#endif