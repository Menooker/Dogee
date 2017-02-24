#include "DogeeMR.h"
#include <string>
#include <sstream>

using namespace Dogee;
class MyMR : public DMapReduce<int, std::string, std::string, int>
{
	DefBegin(DBaseAccumulator);
public:
	DefEnd();
	MyMR(ObjectKey key) :DMapReduce(key)
	{}

	MyMR(ObjectKey key, uint32_t num_users) :DMapReduce(key, num_users)
	{}

	void DoMap(int& key, std::string& value, DMapReduce::MAP& map)
	{
		std::stringstream buf(value);
		while (buf)
		{
			std::string str;
			buf >> str;
			map[str].push_back(1);
		}
	}

	void DoReduce(const std::string& key, const std::vector<int>& value)
	{

	}
};


DefGlobal(mr, Ref<MyMR>);

void mrtest()
{
	mr = NewObj<MyMR>(1);
	std::unordered_map<int, std::string> input = {
		{ 1, "On a dark desert highway" },
		{ 2, "cool wind in my hair" },
		{ 3, "Warm smell of colitas" },
		{ 4, "rising up through the air" },
		{ 5, "Up ahead in the distance" },
		{ 6, "I saw a shimmering light" },
		{ 7, "My head grew heavy and my sight grew dim" },
		{ 8, "I had to stop for the night" }
	};
	//auto& itr = input.begin();
//	itr->first
	mr->Map(input);
}