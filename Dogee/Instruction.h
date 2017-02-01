#ifndef __DOGEE_INSTRUCTION_H_ 
#define __DOGEE_INSTRUCTION_H_ 

#include <vector>
#include <functional>

namespace Dogee{

	class Type
	{
	public:
		int TypeID;
		virtual void isa(Type&) = 0;
	};

	class Instruction
	{
	public:
		Type* type;
		virtual std::vector<Instruction>& GetParents()
		{
			//make compiler happy
			return *(std::vector<Instruction>*)nullptr;
		}
		virtual void EnumParents(std::function<bool(Instruction&)> func) = 0;
		virtual std::vector<Instruction>& GetChildren()
		{
			//make compiler happy
			return *(std::vector<Instruction>*)nullptr;
		}
		virtual void EnumChildren(std::function<bool(Instruction&)> func) = 0;
	};
}

#endif