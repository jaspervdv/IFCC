#include <IfcClass.hpp>
#include <string>
#include <map>
#include <filesystem>

#ifndef IFCFILE_IFCFILE_H
#define IFCFILE_IFCFILE_H


class IfcFile {
private:
	std::string header_ = "";
	std::string footer_ = "";
	std::map<int, IfcClass*> classStructure_;
	std::map<int, std::unique_ptr<IfcClass>> privateClassList_;
	bool isGood_ = false;

	/// checks if filesystem path has an IFCZIP extension
	bool pathIsZip(const std::filesystem::path& filePath);

	/// unzippes an IFCZIP file and casts it to ifstream
	std::istringstream unZip(const std::string& filePaht);

	/// Zippes a file and stores it at the outputpath
	void storeFileZip(const std::filesystem::path& outputPath);

	/// stores an IFC file to the outputpath
	void storeFileIFC(const std::filesystem::path& outputPath);

public:

	IfcFile(const std::string& filePath);

	std::string getHeader() const { return header_; }
	std::string getFooter() const { return footer_; }
	std::map<int, IfcClass*>::iterator begin() { return classStructure_.begin(); }
	std::map<int, IfcClass*>::iterator end() { return classStructure_.end(); }
	int getClassCount() const { return classStructure_.size(); }

	/// removes a class at idx from the ifcfile class
	void removeClass(int idx);

	/// restructures the file according to the reference<old loc, new loc>
	void restructureFile(const std::map<int, int>& referenceMap);

	/// founds the floating values that are stored in the file to specified float precision
	void roundFloats(int floatLength);

	/// update the references according to the reference map <old loc, new loc>
	void updateReference(const std::map<int, int>& referenceMap);

	/// removes redundant classes and updates the references to not be dangling
	void collapseClasses(int iteration = 1);

	/// recalculate the ID so that the first object is 1 and every net ID is +1 of the one before 
	bool recalculateId(bool restructure);

	/// output all the IfcFile data to a single string
	std::string dumptoString() const;

	/// stores the file at the specified output path
	void storeFile(const std::filesystem::path& outputPath);

	bool isGood() { return isGood_; }
};

#endif