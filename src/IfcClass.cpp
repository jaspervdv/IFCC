#include <IfcClass.hpp>
#include <map>

std::string IfcClass::roundStringFloats(const std::string& theString, int floatLength)
{
	std::stringstream stream;
	stream << std::fixed << std::setprecision(floatLength) << std::stod(theString);

	std::string roundedFloat = stream.str();

	size_t end = roundedFloat.find_last_not_of('0');
	if (end != std::string::npos)
		roundedFloat.erase(end + 1);

	return roundedFloat;
}

bool IfcClass::stringIsNum(const std::string& theString)
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
IfcClass::IfcClass(int id, const std::string& classType, bool hasGuid, const std::string& dataString)
{
	id_ = id;
	classType_ = classType;
	hasGuid_ = hasGuid;
	data_ = dataString;
}


std::vector<std::string> IfcClass::tokenizeData(const std::string& delimiters) {
	std::vector<std::string> tokens;

	size_t start = 0;
	size_t pos = 0;

	while ((pos = data_.find_first_of(delimiters, start)) != std::string::npos)
	{
		if (pos > start)
			tokens.push_back(data_.substr(start, pos - start));

		tokens.push_back(data_.substr(pos, 1));

		start = pos + 1;
	}

	if (start < data_.size()) { tokens.push_back(data_.substr(start)); }

	return tokens;
}

void IfcClass::RoundFloatingValues(int floatLength)
{
	std::vector<std::string> tokenizedString = tokenizeData(defaultDelimiters_);
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
	data_ = correctedString;
	return;
}

void IfcClass::remapClassRelations(const std::map<int, int>& referenceMap)
{
	std::vector<std::string> tokenizedString = tokenizeData(defaultDelimiters_);
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
	data_ = correctedString;
}
