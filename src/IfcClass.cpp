#include <IfcClass.hpp>
#include <map>

std::string IfcClass::roundStringFloats(const std::string& theString, int floatLength)
{
	if (theString.find('.') == std::string::npos) { return theString; }

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


std::vector<std::string> IfcClass::tokenizeData(const std::string& delimiters)
{
	std::vector<std::string> tokens;

	size_t start = 0;
	bool inQuotes = false;

	for (size_t i = 0; i < data_.size(); ++i)
	{
		char c = data_[i];

		if (c == '\'')
		{
			inQuotes = !inQuotes; 
		}

		// Only split if not a string
		if (!inQuotes && delimiters.find(c) != std::string::npos)
		{
			if (i > start)
				tokens.push_back(data_.substr(start, i - start));

			tokens.push_back(std::string(1, c)); // delimiter as token
			start = i + 1;
		}
	}

	// Add end
	if (start < data_.size())
	{
		tokens.push_back(data_.substr(start));
	}
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
