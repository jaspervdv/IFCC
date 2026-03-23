#define USE_IFC2x3
#define programVersion "0.1.0"

#include <ifcparse/Ifc2x3.h>
#include <ifcparse/Ifc4x1.h>
#include <ifcparse/Ifc4x2.h>
#include <ifcparse/Ifc4.h>
#include <ifcparse/Ifc4x3.h>
#include <ifcparse/Ifc4x3_add1.h>
#include <ifcparse/Ifc4x3_add2.h>

#include <fstream>
#include <iostream>
#include <filesystem>
#include <ifcparse/IfcFile.h>
#include <ifcparse/utils.h>
#include <ifcparse/IfcEntityInstanceData.h>
#include <ifcparse/IfcHierarchyHelper.h>

#include <boost/make_shared.hpp>
#include <boost/optional/optional_io.hpp>

template <typename IfcSchema>
struct PropertyKey
{
	std::string name;
	std::string value;
	typename IfcSchema::IfcUnit* unit;
	boost::optional<std::string> description;

	bool operator==(const PropertyKey& other) const
	{
		return name == other.name &&
			value == other.value &&
			unit == other.unit &&
			description == other.description;
	}
};

struct PropertyKeyHash
{
	template <typename IfcSchema>
	size_t operator()(const PropertyKey<IfcSchema>& k) const
	{
		size_t h1 = std::hash<std::string>{}(k.name);
		size_t h2 = std::hash<std::string>{}(k.value);
		size_t h3 = std::hash<void*>{}(k.unit);
		size_t h4 = k.description ? std::hash<std::string>{}(*k.description) : 0;

		size_t hash = h1;
		hash ^= h2 + 0x9e3779b9 + (hash << 6) + (hash >> 2);
		hash ^= h3 + 0x9e3779b9 + (hash << 6) + (hash >> 2);
		hash ^= h4 + 0x9e3779b9 + (hash << 6) + (hash >> 2);
		return hash;
	}
};

struct IntPoint
{
	int64_t x;
	int64_t y;
	int64_t z;
	bool is3D;

	bool operator==(const IntPoint& other) const {
		if (is3D != other.is3D) { return false; }
		if (is3D) { return x == other.x && y == other.y && z == other.z; }
		return x == other.x && y == other.y;
	}
};

struct IntPointHash
{
	size_t operator()(const IntPoint& p) const
	{
		size_t h1 = std::hash<int64_t>{}(p.x);
		size_t h2 = std::hash<int64_t>{}(p.y);
		size_t h3 = std::hash<int64_t>{}(p.z);

		size_t hash = h1;
		hash ^= h2 + 0x9e3779b9 + (hash << 6) + (hash >> 2);
		hash ^= h3 + 0x9e3779b9 + (hash << 6) + (hash >> 2);
		return hash;
	}
};

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

template <typename T>
boost::optional<std::string> getDescription(T* prop) {
	if constexpr (std::is_same_v<T, Ifc2x3::IfcPropertySingleValue> ||
		std::is_same_v<T, Ifc4::IfcPropertySingleValue> ||
		std::is_same_v<T, Ifc4x1::IfcPropertySingleValue> ||
		std::is_same_v<T, Ifc4x2::IfcPropertySingleValue>)
	{
		return prop->Description();
	}
	else {
		return boost::none; 
	}
}

template <typename IfcSchema>
void updatePset(IfcUtil::IfcBaseClass* currentPropertySetBase, int toBeReplacedId, typename IfcSchema::IfcPropertySingleValue* uniquePropertyValue)
{
	if (currentPropertySetBase->data().type()->name() == "IfcPropertySet")
	{
		IfcSchema::IfcPropertySet* currentPropertySet = currentPropertySetBase->as<IfcSchema::IfcPropertySet>();
		IfcSchema::IfcProperty::list::ptr propertyList = currentPropertySet->HasProperties();
		IfcSchema::IfcProperty::list::ptr newPropertyList = boost::make_shared<IfcSchema::IfcProperty::list>();

		for (auto it4 = propertyList->begin(); it4 != propertyList->end(); ++it4)
		{
			IfcSchema::IfcProperty* ttt = *it4;
			if (ttt->data().id() == toBeReplacedId)
			{
				newPropertyList->push(uniquePropertyValue);
				continue;
			}
			newPropertyList->push(ttt);
		}
		currentPropertySet->setHasProperties(newPropertyList);
	}
	else if (currentPropertySetBase->data().type()->name() == "IfcMaterialProperties")
	{
		if constexpr (std::is_same_v<IfcSchema, Ifc2x3>){ return; }
		else
		{
			IfcSchema::IfcMaterialProperties* currentPropertySet = currentPropertySetBase->as<IfcSchema::IfcMaterialProperties>();
			IfcSchema::IfcProperty::list::ptr propertyList = currentPropertySet->Properties();
			IfcSchema::IfcProperty::list::ptr newPropertyList = boost::make_shared<IfcSchema::IfcProperty::list>();

			for (auto it4 = propertyList->begin(); it4 != propertyList->end(); ++it4)
			{
				IfcSchema::IfcProperty* ttt = *it4;
				if (ttt->data().id() == toBeReplacedId)
				{
					newPropertyList->push(uniquePropertyValue);
					continue;
				}
				newPropertyList->push(ttt);
			}
			currentPropertySet->setProperties(newPropertyList);
		}	
	}
	else 
	{
		std::cout << currentPropertySetBase->data().type()->name() << std::endl;
	}
	
	return;
}

template <typename IfcSchema>
std::unique_ptr<IfcParse::IfcFile> collapseProperties(std::unique_ptr<IfcParse::IfcFile> theFile, const std::filesystem::path& pathToFile, std::vector<int>* tobedeletedID)
{
	std::cout << "[INFO] removing duplicate/redundant Pset objects\n";
	int currentDeleteSize = tobedeletedID->size();

	// fetch all the properties
	IfcSchema::IfcPropertySingleValue::list::ptr propertyValues = theFile->instances_by_type<IfcSchema::IfcPropertySingleValue>();
	int propListSise = propertyValues->size();
	int currentItemCount = 0;

	// collapse properties that are identical into a single item
	std::unordered_map<PropertyKey<IfcSchema>, IfcSchema::IfcPropertySingleValue*, PropertyKeyHash> processedItems;

	for (auto globalPropertyIt = propertyValues->begin(); globalPropertyIt != propertyValues->end(); ++globalPropertyIt)
	{
		if (currentItemCount % 100 == 0)
		{
			std::cout << currentItemCount << " of " << propListSise << " objects\r";
		}
		
		currentItemCount++;

		IfcSchema::IfcPropertySingleValue* currentPropertyValue = *globalPropertyIt;
		if (currentPropertyValue == nullptr) { continue; }

		PropertyKey<IfcSchema> key{
		currentPropertyValue->Name(),
		currentPropertyValue->NominalValue()->data().toString(),
		currentPropertyValue->Unit(),
		getDescription(currentPropertyValue)
		};

		// check if dub and store the unique data
		auto [storedPropertyKey, inserted] = processedItems.emplace(key, currentPropertyValue);
		if (inserted) { continue; }

		int currentId = currentPropertyValue->data().id();
		
		IfcSchema::IfcPropertySingleValue* uniquePropertyValue = storedPropertyKey->second;
		auto aggregateList = theFile->instances_by_reference(currentId);

		for (auto agregateIt = aggregateList->begin(); agregateIt != aggregateList->end(); ++agregateIt)
		{
			updatePset<IfcSchema>(*agregateIt, currentId, uniquePropertyValue);
		}

		auto refs = theFile->instances_by_reference(currentId);
		if (refs->size() != 0) {
			continue;
		}
		tobedeletedID->emplace_back(currentId);
	}
	std::cout << currentItemCount << " of " << propListSise << " objects\n";
	std::cout << tobedeletedID->size() - currentDeleteSize << " redundant objects found\n";

	return theFile;
} 

template <typename IfcSchema>
void roundFloats(IfcParse::IfcFile* theFile, int floatingPointSize)
{
	double scale = pow(10, floatingPointSize - 1);
	std::cout << "[INFO] Rounding point coordinates to " << 1/scale << "\n";

	IfcSchema::IfcCartesianPoint::list::ptr pointList =  theFile->instances_by_type<IfcSchema::IfcCartesianPoint>();

	int pointListSize = pointList->size();
	int currentItemCount = 0;
	for (auto pointIt = pointList->begin(); pointIt != pointList->end(); ++ pointIt)
	{
		if (currentItemCount % 100 == 0)
		{
			std::cout << currentItemCount << " of " << pointListSize << " objects\r";
		}
		currentItemCount++;
		IfcSchema::IfcCartesianPoint* currentPoint = *pointIt;
		std::vector<double> currentCoords = currentPoint->Coordinates();

		for (double& coord : currentCoords)
		{
			coord = std::round(coord * scale ) / scale;
		}
		currentPoint->setCoordinates(currentCoords);
	}
	std::cout << currentItemCount << " of " << pointListSize << " objects\n";
	return;
}

template <typename IfcSchema>
std::unique_ptr<IfcParse::IfcFile> collapsePoints(std::unique_ptr<IfcParse::IfcFile> theFile, const std::filesystem::path& pathToFile, int floatingPointSize, std::vector<int>* tobedeletedID)
{
	std::cout << "[INFO] removing duplicate/redundant point objects\n";
	int currentDeleteSize = tobedeletedID->size();
	IfcSchema::IfcCartesianPoint::list::ptr pointList = theFile->instances_by_type<IfcSchema::IfcCartesianPoint>();

	double scale = pow(10, floatingPointSize - 1);
	int pointListSize = pointList->size();
	int currentItemCount = 0;

	std::unordered_map<IntPoint, IfcSchema::IfcCartesianPoint*, IntPointHash> processedItems;

	for (auto currentPointIt = pointList->begin(); currentPointIt != pointList->end(); ++currentPointIt)
	{
		if (currentItemCount % 100 == 0)
		{
			std::cout << currentItemCount << " of " << pointListSize << " objects\r";
		}
		currentItemCount++;

		IfcSchema::IfcCartesianPoint* currentPoint = *currentPointIt;
		std::vector<double> currentCoords = currentPoint->Coordinates();

		IntPoint roundedPoint = (currentCoords.size() == 2)
			? IntPoint{
				static_cast<int64_t>(std::round(currentCoords[0] * scale)),
				static_cast<int64_t>(std::round(currentCoords[1] * scale)),
				0,
				false
		}
			: IntPoint{
				static_cast<int64_t>(std::round(currentCoords[0] * scale)),
				static_cast<int64_t>(std::round(currentCoords[1] * scale)),
				static_cast<int64_t>(std::round(currentCoords[2] * scale)),
				true
		};

		auto [storedPoint, inserted] = processedItems.emplace(roundedPoint, currentPoint);
		if (inserted) { continue; }

		IfcSchema::IfcCartesianPoint* uniquePoint = storedPoint->second;
		auto aggregateList = theFile->instances_by_reference(currentPoint->data().id());

		for (auto agregateIt = aggregateList->begin(); agregateIt != aggregateList->end(); ++agregateIt)
		{
			IfcUtil::IfcBaseClass* referenceObject = *agregateIt;
			std::string objectTypeName = referenceObject->declaration().name();

			if (objectTypeName == "IfcAxis2Placement2D")
			{
				IfcSchema::IfcAxis2Placement2D* placementObject = referenceObject->as<IfcSchema::IfcAxis2Placement2D>();
				if (placementObject == nullptr) { continue; }
				placementObject->setLocation(uniquePoint);
				continue;
			}
			if (objectTypeName == "IfcAxis2Placement3D")
			{
				IfcSchema::IfcAxis2Placement3D* placementObject = referenceObject->as<IfcSchema::IfcAxis2Placement3D>();
				if (placementObject == nullptr) { continue; }
				placementObject->setLocation(uniquePoint);
				continue;
			}
			if (objectTypeName == "IfcBoundingBox")
			{
				IfcSchema::IfcBoundingBox* bBoxObject = referenceObject->as<IfcSchema::IfcBoundingBox>();
				if (bBoxObject == nullptr) { continue; }
				bBoxObject->setCorner(uniquePoint);
				continue;
			}
			if (objectTypeName == "IfcCartesianTransformationOperator3D")
			{
				IfcSchema::IfcCartesianTransformationOperator3D* transformatorObject = referenceObject->as<IfcSchema::IfcCartesianTransformationOperator3D>();
				if (transformatorObject == nullptr) { continue; }
				transformatorObject->setLocalOrigin(uniquePoint);
				continue;
			}
			if (objectTypeName == "IfcPolyline")
			{
				IfcSchema::IfcPolyline* polyLineObject = referenceObject->as<IfcSchema::IfcPolyline>();
				if (polyLineObject == nullptr) { continue; }

				IfcSchema::IfcCartesianPoint::list::ptr pointList = polyLineObject->Points();
				IfcSchema::IfcCartesianPoint::list::ptr newPointList = boost::make_shared< IfcSchema::IfcCartesianPoint::list>();

				for (auto loopPointIt = pointList->begin(); loopPointIt != pointList->end(); ++loopPointIt)
				{
					IfcSchema::IfcCartesianPoint* loopPoint = *loopPointIt;
					if (loopPoint->data().id() == currentPoint->data().id())
					{
						newPointList->push(uniquePoint);
						continue;
					}
					newPointList->push(loopPoint);
				}
				polyLineObject->setPoints(newPointList);
				continue;
			}
			if (objectTypeName == "IfcPolyLoop")
			{
				IfcSchema::IfcPolyLoop* polyLineObject = referenceObject->as<IfcSchema::IfcPolyLoop>();
				if (polyLineObject == nullptr) { continue; }

				IfcSchema::IfcCartesianPoint::list::ptr pointList = polyLineObject->Polygon();
				IfcSchema::IfcCartesianPoint::list::ptr newPointList = boost::make_shared< IfcSchema::IfcCartesianPoint::list>();

				for (auto loopPointIt = pointList->begin(); loopPointIt != pointList->end(); ++loopPointIt)
				{
					IfcSchema::IfcCartesianPoint* loopPoint = *loopPointIt;
					if (loopPoint->data().id() == currentPoint->data().id())
					{
						newPointList->push(uniquePoint);
						continue;
					}
					newPointList->push(loopPoint);
				}
				polyLineObject->setPolygon(newPointList);
				
				continue;
			}
			std::cout << referenceObject->declaration().name() << std::endl;
		}
		auto refs = theFile->instances_by_reference(currentPoint->data().id());
		if (refs->size() != 0) {
			continue;
		}
		tobedeletedID->emplace_back(currentPoint->data().id());
	}
	std::cout << currentItemCount << " of " << pointListSize << " objects\n";
	std::cout << tobedeletedID->size() - currentDeleteSize << " redundant objects found\n";
	return theFile;
}

template <typename IfcSchema>
std::unique_ptr<IfcParse::IfcFile> collapseDirections(std::unique_ptr<IfcParse::IfcFile> theFile, const std::filesystem::path& pathToFile, int floatingPointSize, std::vector<int>* tobedeletedID)
{
	std::cout << "[INFO] removing duplicate/redundant direction objects\n";
	int currentDeleteSize = tobedeletedID->size();
	IfcSchema::IfcDirection::list::ptr directionList = theFile->instances_by_type<IfcSchema::IfcDirection>();

	double scale = pow(10, floatingPointSize - 1);
	int dirListSize = directionList->size();
	int currentItemCount = 0;

	std::unordered_map<IntPoint, IfcSchema::IfcDirection*, IntPointHash> processedItems;

	for (auto currentPointIt = directionList->begin(); currentPointIt != directionList->end(); ++currentPointIt)
	{
		if (currentItemCount % 100 == 0)
		{
			std::cout << currentItemCount << " of " << dirListSize << " objects\r";
		}
		currentItemCount++;

		IfcSchema::IfcDirection* currentDir = *currentPointIt;
		std::vector<double> currentCoords = currentDir->DirectionRatios();

		IntPoint roundedPoint = (currentCoords.size() == 2)
			? IntPoint{
				static_cast<int64_t>(std::round(currentCoords[0] * scale)),
				static_cast<int64_t>(std::round(currentCoords[1] * scale)),
				0,
				false
		}
			: IntPoint{
				static_cast<int64_t>(std::round(currentCoords[0] * scale)),
				static_cast<int64_t>(std::round(currentCoords[1] * scale)),
				static_cast<int64_t>(std::round(currentCoords[2] * scale)),
				true
		};

		auto [storedDir, inserted] = processedItems.emplace(roundedPoint, currentDir);
		if (inserted) { continue; }

		IfcSchema::IfcDirection* uniqueDir = storedDir->second;
		auto aggregateList = theFile->instances_by_reference(currentDir->data().id());

		for (auto agregateIt = aggregateList->begin(); agregateIt != aggregateList->end(); ++agregateIt)
		{
			IfcUtil::IfcBaseClass* referenceObject = *agregateIt;
			std::string objectTypeName = referenceObject->declaration().name();

			if (objectTypeName == "IfcAxis2Placement2D")
			{
				IfcSchema::IfcAxis2Placement2D* placementObject = referenceObject->as<IfcSchema::IfcAxis2Placement2D>();
				if (placementObject == nullptr) { continue; }
				placementObject->setRefDirection(uniqueDir);
				continue;
			}
			if (objectTypeName == "IfcAxis2Placement3D")
			{
				IfcSchema::IfcAxis2Placement3D* placementObject = referenceObject->as<IfcSchema::IfcAxis2Placement3D>();
				if (placementObject == nullptr) { continue; }
				if (placementObject->Axis() == currentDir) { placementObject->setAxis(uniqueDir); }
				else if (placementObject->RefDirection() == currentDir) { placementObject->setRefDirection(uniqueDir); }
				continue;
			}
			if (objectTypeName == "IfcCartesianTransformationOperator2D")
			{
				IfcSchema::IfcCartesianTransformationOperator2D* operatorObject = referenceObject->as<IfcSchema::IfcCartesianTransformationOperator2D>();
				if (operatorObject == nullptr) { continue; }
				if (operatorObject->Axis1() == currentDir) { operatorObject->setAxis1(uniqueDir); }
				if (operatorObject->Axis2() == currentDir) { operatorObject->setAxis2(uniqueDir); }
				continue;
			}
			if (objectTypeName == "IfcCartesianTransformationOperator3D")
			{
				IfcSchema::IfcCartesianTransformationOperator3D* operatorObject = referenceObject->as<IfcSchema::IfcCartesianTransformationOperator3D>();
				if (operatorObject == nullptr) { continue; }
				if (operatorObject->Axis1() == currentDir) { operatorObject->setAxis1(uniqueDir); }
				if (operatorObject->Axis2() == currentDir) { operatorObject->setAxis2(uniqueDir); }
				if (operatorObject->Axis3() == currentDir) { operatorObject->setAxis3(uniqueDir); }
				continue;
			}
			if (objectTypeName == "IfcExtrudedAreaSolid")
			{
				IfcSchema::IfcExtrudedAreaSolid* extrusionObject = referenceObject->as<IfcSchema::IfcExtrudedAreaSolid>();
				if (extrusionObject == nullptr) { continue; }
				extrusionObject->setExtrudedDirection(uniqueDir);
				continue;
			}	
			if (objectTypeName == "IfcGeometricRepresentationContext")
			{
				IfcSchema::IfcGeometricRepresentationContext* repObject = referenceObject->as<IfcSchema::IfcGeometricRepresentationContext>();
				if (repObject == nullptr) { continue; }
				repObject->setTrueNorth(uniqueDir);
				continue;
			}

		}
		auto refs = theFile->instances_by_reference(currentDir->data().id());
		if (refs->size() != 0) {
			continue;
		}
		tobedeletedID->emplace_back(currentDir->data().id());
	}
	std::cout << currentItemCount << " of " << dirListSize << " objects\n";
	std::cout << tobedeletedID->size() - currentDeleteSize << " redundant objects found\n";
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

void helpOutput() {
	std::cout << "IFCC V" << programVersion << "\nCompresses IFC files with minimal data loss\nUsage: IFCC.exe 'IFC target path' 'optional IFC output path'\nIf no output filepath is supplied the stem path is used with '_compressed' added";
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

template <typename IfcSchema>
std::unique_ptr<IfcParse::IfcFile> processFile(std::unique_ptr<IfcParse::IfcFile> theFile, const std::filesystem::path& pathToFile, int floatingPointSize)
{
	std::vector<int> tobedeletedID;
	theFile = collapseProperties<IfcSchema>(std::move(theFile), pathToFile, &tobedeletedID);
	roundFloats<IfcSchema>(theFile.get(), floatingPointSize);
	theFile = collapsePoints<IfcSchema>(std::move(theFile), pathToFile, floatingPointSize, &tobedeletedID);
	theFile = collapseDirections<IfcSchema>(std::move(theFile), pathToFile, floatingPointSize, &tobedeletedID);
	// delete the dubs that are dangling
	theFile = forcefullDelete(std::move(theFile), pathToFile, tobedeletedID);
	return theFile;
}


int main(int argc, char* argv[])
{
	std::filesystem::path filePath = "";
	std::filesystem::path outputPath = "";
	std::filesystem::path localPath = std::filesystem::path("./temp.ifc");

	if (!getUserInput(argc, argv, &filePath, &outputPath)) { return 0; }

	std::cout << "\nInput path: " << filePath.string() << "\n";
	std::cout << "Output path: " << outputPath.string() << "\n\n";

	std::filesystem::copy(filePath, localPath, std::filesystem::copy_options::overwrite_existing);
	filePath = localPath;

	std::cout << "read file\n";
	std::unique_ptr<IfcParse::IfcFile> ifcFile = std::make_unique<IfcParse::IfcFile>(filePath.string());
	ifcFile = std::make_unique<IfcParse::IfcFile>(filePath.string());
	if (!ifcFile->good()) { return 1; }
	std::cout << "file succesfully read\n";

	int precisionPoint =  6;
	std::string currentIfcVersion = ifcFile->schema()->name();

	if (currentIfcVersion == "IFC2X3") { ifcFile = processFile<Ifc2x3>(std::move(ifcFile), filePath, precisionPoint); }
	else if (currentIfcVersion == "IFC4") { ifcFile = processFile<Ifc4>(std::move(ifcFile), filePath, precisionPoint); }
	else if (currentIfcVersion == "IFC4X1") { ifcFile = processFile<Ifc4x1>(std::move(ifcFile), filePath, precisionPoint); }
	else if (currentIfcVersion == "IFC4X2") { ifcFile = processFile<Ifc4x2>(std::move(ifcFile), filePath, precisionPoint); }
	else if (currentIfcVersion == "IFC4X3") { ifcFile = processFile<Ifc4x3>(std::move(ifcFile), filePath, precisionPoint); }
	else if (currentIfcVersion == "IFC4X3_ADD1") { ifcFile = processFile<Ifc4x3_add1>(std::move(ifcFile), filePath, precisionPoint); }
	else if (currentIfcVersion == "IFC4X3_ADD2") { ifcFile = processFile<Ifc4x3_add2>(std::move(ifcFile), filePath, precisionPoint); }
	else
	{
		std::cout << "Ifc version is not supported: " << currentIfcVersion << "\n";
	}

	std::ofstream storageFile;
	storageFile.open(outputPath);
	std::cout << "[INFO] Exporting file " << outputPath.string() << std::endl;
	storageFile << *ifcFile;
	std::cout << "[INFO] Exported succesfully" << std::endl;
	storageFile.close();

	std::filesystem::remove(localPath);

	return 0;
}

