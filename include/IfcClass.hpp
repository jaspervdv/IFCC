#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <map>
#include <unordered_map>

#ifndef IFCCLASS_IFCCLASS_H
#define IFCCLASS_IFCCLASS_H

class IfcClass {
private:
	int id_;
	int classType_;
	std::string data_ = "";

	inline static std::string defaultDelimiters_ = "(),";

	/// rounds the floats
	std::string roundStringFloats(const std::string& theString, int floatLength);

	/// check if string is num
	/// A num is any string value that consists completely out of digits or has a first char that is +, -, or . 
	bool stringIsNum(const std::string& theString);

public:

	IfcClass(int id, int classType, const std::string& dataString);

	/// returns a vector consisting out of all the elements that were split by the delimiters with the delimiters included in the vector
	std::vector<std::string> tokenizeData(const std::string& delimiters) const;

	/// rounds all the floating values in the class
	void RoundFloatingValues(int floatLenght);

	/// updates the class relations according to the reference map, if no relation IDs are present in the map nothing happens;
	void remapClassRelations(const std::unordered_map<int, int>& referenceMap);

	int getId() const { return id_; }
	void setId(int id) { id_ = id; }
	int getClassTypeIdx() const { return classType_; }
	std::string getData() const { return data_; }
	void setData(const std::string& dataString) { data_ = dataString; }
};

#endif