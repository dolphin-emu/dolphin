#pragma once
#include <string>
#include <set>

class ExtInfo
{
public:
	ExtInfo() :players(0), onlinePlayers(0) {};
	~ExtInfo();

	std::string id;
	std::string title;
	std::string genre;
	std::string description;
	int onlinePlayers;
	int players;
	std::set<std::string> requiredControllers;
	std::set<std::string> optionalControllers;

private:

};


